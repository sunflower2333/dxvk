#include "umd_kernel_identity.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace dxvk::umd {
namespace {
HRESULT exactStatus(NTSTATUS status) {
  if (status == 0) return S_OK;
  return status < 0 ? HRESULT_FROM_NT(status) : E_FAIL;
}

void traceKernelQuery(RuntimeQueryStage stage, NTSTATUS status, HRESULT result,
    UINT bytes, const RuntimeIdentity& identity) noexcept {
  const DWORD savedError = GetLastError();
  const int savedErrno = errno;
  char flag[2] = {};
  if (GetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE", flag, sizeof(flag)) == 1
      && flag[0] == '1') {
    static std::atomic<uint64_t> sequence{0};
    const auto next = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    if (next <= 64) {
      uint32_t low = 0, high = 0;
      std::memcpy(&low, identity.luid.data(), sizeof(low));
      std::memcpy(&high, identity.luid.data() + 4, sizeof(high));
      std::fprintf(stderr,
        "VIOGPU_KERNEL_QUERY stage=%u sequence=%llu status=%08x hr=%08x"
        " requested_bytes=160 observed_bytes=%u identity_valid=%u"
        " luid=%08x:%08x generation=%llu capabilities=%llx\n",
        unsigned(stage), static_cast<unsigned long long>(next), unsigned(uint32_t(status)),
        unsigned(uint32_t(result)), unsigned(bytes), unsigned(result == S_OK),
        unsigned(high), unsigned(low), static_cast<unsigned long long>(identity.generation),
        static_cast<unsigned long long>(identity.capabilities));
      std::fflush(stderr);
    }
  }
  SetLastError(savedError);
  errno = savedErrno;
}

class KernelIdentity final : public AdapterIdentitySource {
public:
  explicit KernelIdentity(const KernelIdentityApi& api) : m_api(api) { }
  ~KernelIdentity() override {
    if (m_handle) {
      D3DKMT_CLOSEADAPTER args = {};
      args.hAdapter = m_handle;
      m_api.close(&args);
    }
  }
  HRESULT open(const RuntimeIdentity& initial) {
    D3DKMT_OPENADAPTERFROMLUID args = {};
    static_assert(sizeof(args.AdapterLuid) == sizeof(initial.luid));
    std::memcpy(&args.AdapterLuid, initial.luid.data(), sizeof(args.AdapterLuid));
    HRESULT hr = exactStatus(m_api.open(&args));
    if (FAILED(hr)) return hr;
    m_handle = args.hAdapter;
    if (!m_handle) return E_FAIL;
    RuntimeIdentity observed;
    hr = query(observed, RuntimeQueryStage::OpenAdapter);
    if (FAILED(hr)) return hr;
    // Bind only the adapter and generation returned by the genuine runtime
    // callback. An unrelated adapter or a reset during OpenAdapter is rejected.
    if (observed.luid != initial.luid || observed.generation != initial.generation
        || observed.capabilities != initial.capabilities) return DXGI_ERROR_DEVICE_REMOVED;
    D3DKMT_QUERYADAPTERINFO model = {};
    model.hAdapter = m_handle; model.Type = KMTQAITYPE_DRIVERVERSION;
    model.pPrivateDriverData = &m_driverVersion; model.PrivateDriverDataSize = sizeof(m_driverVersion);
    hr = exactStatus(m_api.query(&model));
    if (hr == S_OK && (!m_driverVersion || model.PrivateDriverDataSize != sizeof(m_driverVersion)))
      hr = E_FAIL;
    if (hr == S_OK) m_initial = initial;
    return hr;
  }
  bool explicitResidency() const override { return m_driverVersion >= KMT_DRIVERVERSION_WDDM_2_0; }
  HRESULT supportsPrimaryMode(const PrimaryShape& shape) const override {
    PrimaryPlan plan;
    if (!m_handle || !m_api.modes || primaryPlan(shape, plan, true) != PrimaryStatus::Valid
        || !plan.scanout || !shape.numerator || !shape.denominator) return DXGI_ERROR_UNSUPPORTED;
    D3DDDIFORMAT format;
    switch (shape.format) {
      case 28: format = D3DDDIFMT_A8B8G8R8; break;
      case 87: case 91: format = D3DDDIFMT_A8R8G8B8; break;
      case 88: format = D3DDDIFMT_X8R8G8B8; break;
      default: return DXGI_ERROR_UNSUPPORTED;
    }
    auto current = [&]() {
      RuntimeIdentity identity;
      const HRESULT hr = query(identity, RuntimeQueryStage::Continuity);
      if (hr != S_OK) return hr == HRESULT_FROM_NT(NTSTATUS(0xc00002b6))
        ? DXGI_ERROR_DEVICE_REMOVED : hr;
      return identity.luid == m_initial.luid && identity.generation == m_initial.generation
        && identity.capabilities == m_initial.capabilities ? S_OK : DXGI_ERROR_DEVICE_REMOVED;
    };
    HRESULT hr = current();
    if (hr != S_OK) return hr;
    try {
      D3DKMT_GETDISPLAYMODELIST request = {};
      request.hAdapter = m_handle; request.VidPnSourceId = shape.source;
      auto statusResult = [](NTSTATUS status) {
        return status == NTSTATUS(0xc00002b6) ? DXGI_ERROR_DEVICE_REMOVED : exactStatus(status);
      };
      hr = statusResult(m_api.modes(&request));
      if (hr != S_OK) return hr;
      // Bound both allocation and retries while topology changes underneath
      // enumeration. No old mode list is cached across resource creations.
      for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const UINT capacity = request.ModeCount;
        if (!capacity || capacity > 4096) return DXGI_ERROR_UNSUPPORTED;
        std::vector<D3DKMT_DISPLAYMODE> modes(capacity);
        request.pModeList = modes.data();
        const NTSTATUS status = m_api.modes(&request);
        if (status == NTSTATUS(0xc0000023)) continue; // STATUS_BUFFER_TOO_SMALL
        hr = statusResult(status);
        if (hr != S_OK) return hr;
        if (!request.ModeCount || request.ModeCount > capacity) return E_FAIL;
        hr = current();
        if (hr != S_OK) return hr;
        for (UINT i = 0; i < request.ModeCount; ++i) {
          const auto& mode = modes[i];
          if (mode.Width == shape.modeWidth && mode.Height == shape.modeHeight && mode.Format == format
              && mode.RefreshRate.Numerator && mode.RefreshRate.Denominator
              && uint64_t(mode.RefreshRate.Numerator) * shape.denominator
                == uint64_t(shape.numerator) * mode.RefreshRate.Denominator
              && mode.ScanLineOrdering == D3DDDI_VSSLO_PROGRESSIVE
              && mode.DisplayOrientation == D3DDDI_ROTATION_IDENTITY && mode.DisplayFixedOutput <= 1)
            return S_OK;
        }
        return DXGI_ERROR_UNSUPPORTED;
      }
      return DXGI_ERROR_UNSUPPORTED;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
      catch (...) { return E_FAIL; }
  }
  HRESULT nativeEscape(void* data, UINT bytes) const override {
    if (!m_handle || !m_api.escape) return DXGI_ERROR_UNSUPPORTED;
    if (!data || (bytes != 128 && bytes != 64 && bytes != 40)) return E_INVALIDARG;
    D3DKMT_ESCAPE request = {};
    request.hAdapter = m_handle; request.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    request.pPrivateDriverData = data; request.PrivateDriverDataSize = bytes;
    return exactStatus(m_api.escape(&request));
  }
  HRESULT query(RuntimeIdentity& result, RuntimeQueryStage stage) const override {
    result = {};
    std::array<uint8_t, RuntimeIdentityReplySize> reply = {};
    D3DKMT_QUERYADAPTERINFO args = {};
    args.hAdapter = m_handle;
    args.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    args.pPrivateDriverData = reply.data();
    args.PrivateDriverDataSize = static_cast<UINT>(reply.size());
    const NTSTATUS status = m_api.query(&args);
    HRESULT hr = exactStatus(status);
    if (SUCCEEDED(hr))
      hr = readRuntimeIdentity(reply.data(), args.PrivateDriverDataSize, result)
        ? S_OK : DXGI_ERROR_UNSUPPORTED;
    traceKernelQuery(stage, status, hr, args.PrivateDriverDataSize, result);
    return hr;
  }
private:
  const KernelIdentityApi m_api;
  D3DKMT_HANDLE m_handle = 0;
  UINT m_driverVersion = 0;
  RuntimeIdentity m_initial;
};
}

HRESULT openKernelIdentityWithApi(const RuntimeIdentity& initial,
    const KernelIdentityApi& api, std::shared_ptr<const AdapterIdentitySource>& source) {
  source.reset();
  if (!api.open || !api.query || !api.close || initial.luid == AdapterLuid{} || !initial.generation)
    return E_INVALIDARG;
  try {
    auto identity = std::make_shared<KernelIdentity>(api);
    const HRESULT hr = identity->open(initial);
    if (hr == S_OK) source = std::move(identity);
    return hr;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT openKernelIdentity(const RuntimeIdentity& initial,
    std::shared_ptr<const AdapterIdentitySource>& source) {
  return openKernelIdentityWithApi(initial,
    {D3DKMTOpenAdapterFromLuid, D3DKMTQueryAdapterInfo, D3DKMTCloseAdapter, D3DKMTEscape,
     D3DKMTGetDisplayModeList}, source);
}
}
