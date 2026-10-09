#include "umd_adapter.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
const char* stageName(dxvk::umd::RuntimeQueryStage stage) noexcept {
  using Stage = dxvk::umd::RuntimeQueryStage;
  switch (stage) {
    case Stage::OpenAdapter: return "open-adapter";
    case Stage::GetSupportedVersions: return "get-supported-versions";
    case Stage::GetCapsThreading: return "get-caps-threading";
    case Stage::GetCapsShader: return "get-caps-shader";
    case Stage::GetCapsPipelines: return "get-caps-pipelines";
    case Stage::CalcPrivateDeviceSize: return "calc-private-device-size";
    case Stage::CreateDeviceBeforeBackend: return "create-device-before-backend";
    case Stage::CreateDeviceAfterBackend: return "create-device-after-backend";
    default: return "continuity";
  }
}

bool queryTraceEnabled() noexcept {
#ifdef _WIN32
  const DWORD saved = GetLastError();
  char flag[2] = {};
  const DWORD size = GetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE", flag, sizeof(flag));
  SetLastError(saved);
  return size == 1 && flag[0] == '1';
#else
  const char* flag = std::getenv("DXVK_UMD_ADAPTER_QUERY_TRACE");
  return flag && flag[0] == '1' && flag[1] == 0;
#endif
}

class QueryTrace {
public:
  QueryTrace(HANDLE runtime, PFND3DDDI_QUERYADAPTERINFOCB callback,
      dxvk::umd::RuntimeQueryStage stage) noexcept
  : m_runtime(reinterpret_cast<uintptr_t>(runtime)),
    m_callback(reinterpret_cast<uintptr_t>(callback)), m_stage(stageName(stage)) {
    if (queryTraceEnabled()) {
      static std::atomic<uint64_t> sequence{0};
      const uint64_t next = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
      // Optional diagnostics are finite even when a completion worker polls.
      if (next <= 64) m_sequence = next;
    }
  }

  void entered() const noexcept {
    emit("enter", false, 0, 0, dxvk::umd::RuntimeIdentityReplySize, false, {});
  }
  void returned(HRESULT callbackResult, HRESULT result, UINT bytes,
      bool valid, const dxvk::umd::RuntimeIdentity& identity) const noexcept {
    emit("return", true, callbackResult, result, bytes, valid, identity);
  }
  void threw(UINT bytes) const noexcept {
    emit("throw", false, 0, 0, bytes, false, {});
  }

private:
  void emit(const char* event, bool returned, HRESULT callbackResult,
      HRESULT result, UINT bytes, bool valid,
      const dxvk::umd::RuntimeIdentity& identity) const noexcept {
    if (!m_sequence) return;
    const int savedErrno = errno;
#ifdef _WIN32
    const DWORD savedError = GetLastError();
#endif
    uint32_t luidLow = 0, luidHigh = 0;
    std::memcpy(&luidLow, identity.luid.data(), sizeof(luidLow));
    std::memcpy(&luidHigh, identity.luid.data() + 4, sizeof(luidHigh));
    std::fprintf(stderr,
      "VIOGPU_ADAPTER_QUERY event=%s stage=%s sequence=%llu runtime=%llx callback=%llx"
      " callback_result_known=%u callback_hr=%08x result_known=%u hr=%08x"
      " requested_bytes=160 observed_bytes=%u identity_valid=%u"
      " luid=%08x:%08x generation=%llu capabilities=%llx\n",
      event, m_stage, static_cast<unsigned long long>(m_sequence),
      static_cast<unsigned long long>(m_runtime), static_cast<unsigned long long>(m_callback),
      unsigned(returned), unsigned(uint32_t(callbackResult)), unsigned(returned),
      unsigned(uint32_t(result)), unsigned(bytes), unsigned(valid),
      unsigned(luidHigh), unsigned(luidLow), static_cast<unsigned long long>(identity.generation),
      static_cast<unsigned long long>(identity.capabilities));
    std::fflush(stderr);
#ifdef _WIN32
    SetLastError(savedError);
#endif
    errno = savedErrno;
  }
  uintptr_t m_runtime, m_callback;
  const char* m_stage;
  uint64_t m_sequence = 0;
};
}

HRESULT dxvk::umd::queryRuntimeIdentity(HANDLE runtime,
    PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result, RuntimeQueryStage stage) {
  result = {};
  if (!runtime || !query) return E_INVALIDARG;
  std::array<uint8_t, RuntimeIdentityReplySize> reply = {};
  D3DDDICB_QUERYADAPTERINFO args = {};
  args.pPrivateDriverData = reply.data();
  args.PrivateDriverDataSize = static_cast<UINT>(reply.size());
  const QueryTrace trace(runtime, query, stage);
  trace.entered();
  HRESULT hr;
  try {
    hr = query(runtime, &args);
  } catch (...) {
    trace.threw(args.PrivateDriverDataSize);
    throw;
  }
  if (hr != S_OK) {
    const HRESULT status = FAILED(hr) ? hr : E_FAIL;
    trace.returned(hr, status, args.PrivateDriverDataSize, false, result);
    return status;
  }
  const HRESULT status = readRuntimeIdentity(reply.data(), args.PrivateDriverDataSize, result)
    ? S_OK : DXGI_ERROR_UNSUPPORTED;
  trace.returned(hr, status, args.PrivateDriverDataSize, status == S_OK, result);
  return status;
}

HRESULT dxvk::umd::queryRuntimeIdentity(D3D10DDI_HRTADAPTER runtime,
    PFND3DDDI_QUERYADAPTERINFOCB query, RuntimeIdentity& result, RuntimeQueryStage stage) {
  return queryRuntimeIdentity(runtime.handle, query, result, stage);
}

extern "C" HRESULT APIENTRY VioGpuDxvkQueryRuntimeAdapterLuid(
    D3D10DDI_HRTADAPTER runtime, PFND3DDDI_QUERYADAPTERINFOCB query, LUID* luid) {
  if (!luid) return E_INVALIDARG;
  *luid = {};
  if (!runtime.handle || !query) return E_INVALIDARG;
  std::array<uint8_t, dxvk::umd::RuntimeIdentityReplySize> reply = {};
  D3DDDICB_QUERYADAPTERINFO args = {};
  args.pPrivateDriverData = reply.data();
  args.PrivateDriverDataSize = static_cast<UINT>(reply.size());
  // The opaque runtime handle is passed back only to its own callback.
  const HRESULT hr = query(runtime.handle, &args);
  if (FAILED(hr)) return hr;
  dxvk::umd::AdapterLuid identity;
  if (!dxvk::umd::readProposedRuntimeIdentity(reply.data(), args.PrivateDriverDataSize, identity))
    return DXGI_ERROR_UNSUPPORTED;
  static_assert(sizeof(*luid) == sizeof(identity));
  std::memcpy(luid, identity.data(), sizeof(*luid));
  return S_OK;
}
