// SPDX-License-Identifier: MIT
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_contract.h"
#include "../src/umd/umd_format.h"
#include "../src/umd/umd_result.h"
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, replacementCallbacks, backendCalls;
static DWORD callerThread;
static HRESULT lastError = S_OK;
static const LUID expectedLuid = {0x58410217, -72};
static ComPtr<ID3D11Device> createdBackend;
static PFND3D10DDI_DESTROYDEVICE retire = nullptr;
static D3D10DDI_HDEVICE retirementDevice{};
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "D3D10 format failure line %d: %s\n", __LINE__, #value); std::abort(); \
} } while (0)

static_assert(D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT == 32, "native DDI sample-count bound");
static_assert(std::is_same_v<decltype(D3D10DDI_DEVICEFUNCS::pfnCreateShaderResourceView),
  PFND3D10DDI_CREATESHADERRESOURCEVIEW>, "exact 10.0 view ABI");
static_assert(std::is_same_v<decltype(D3D10_1DDI_DEVICEFUNCS::pfnCreateShaderResourceView),
  PFND3D10_1DDI_CREATESHADERRESOURCEVIEW>, "exact 10.1 view ABI");
static_assert(std::is_same_v<decltype(D3D10_1DDI_DEVICEFUNCS::pfnCreateBlendState),
  PFND3D10_1DDI_CREATEBLENDSTATE>, "exact 10.1 blend ABI");

static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(hr));
  ++callbacks; lastError = hr;
}
static void APIENTRY replacementError(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  ++replacementCallbacks; error(runtime, hr);
  // Output must be fully staged before the error callback retires the device.
  if (retire) { const auto destroy = retire; retire = nullptr; destroy(retirementDevice); }
}

// Source-linked CPU fixture: only the private factory is substituted with
// Microsoft's WARP. It does not register/activate a D3D10 runtime adapter.
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!std::memcmp(&luid, &expectedLuid, sizeof(luid)) && !runtime);
  ++backendCalls; *device = nullptr; *context = nullptr;
  level = implementationFeatureLevel(level);
  const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hr == S_OK) createdBackend = *device;
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

struct Output {
  UINT before = 0x17284569, value = 0xdeadbeef, after = 0x83416729;
  void check() { CHECK(before == 0x17284569 && after == 0x83416729); }
};

// Inject failures into the same header used by the production callbacks,
// including partial writes, positive non-S_OK statuses and exceptions.
static void queryTransactions() {
  using namespace dxvk::umd;
  const HRESULT statuses[] = {S_OK, S_FALSE, E_FAIL, E_INVALIDARG, E_OUTOFMEMORY, DXGI_ERROR_DEVICE_REMOVED};
  const UINT counts[] = {0, 1, 2, 3, 4, 16, 32, 33, UINT(-1)};
  const UINT mutations[] = {0, 1, 7, UINT(-1)};
  for (HRESULT status : statuses) {
    for (UINT mutation : mutations) {
      unsigned calls = 0; Output output;
      const HRESULT hr = queryNativeFormatCaps(DXGI_FORMAT_R8G8B8A8_UNORM, &output.value,
        [&](DXGI_FORMAT format, UINT* staged) {
          ++calls; CHECK(format == DXGI_FORMAT_R8G8B8A8_UNORM && *staged == 0 && output.value == 0);
          *staged = mutation; return status;
        });
      CHECK(calls == 1);
      CHECK(hr == (status == S_OK ? S_OK : status == E_INVALIDARG || SUCCEEDED(status) ? E_FAIL : status));
      CHECK(output.value == (status == S_OK ? nativeFormatCaps(mutation) : 0)); output.check();
      for (UINT count : counts) {
        output = {}; calls = 0;
        const HRESULT result = queryNativeMultisampleLevels(DXGI_FORMAT_R8G8B8A8_UNORM, count,
          &output.value, [&](DXGI_FORMAT format, UINT queriedCount, UINT* staged) {
            ++calls; CHECK(format == DXGI_FORMAT_R8G8B8A8_UNORM && queriedCount == count);
            CHECK(*staged == 0 && output.value == 0); *staged = mutation; return status;
          });
        const bool validCount = count && count <= 32;
        CHECK(calls == (validCount ? 1u : 0u));
        CHECK(result == (!validCount || status == S_OK ? S_OK : FAILED(status) ? status : E_FAIL));
        CHECK(output.value == (validCount && status == S_OK ? count == 1 ? 1u : mutation : 0u)); output.check();
      }
    }
  }
  unsigned calls = 0; Output output;
  auto unchecked = [&](DXGI_FORMAT, UINT*) { ++calls; return E_FAIL; };
  CHECK(queryNativeFormatCaps(DXGI_FORMAT_R8G8B8A8_UNORM, nullptr, unchecked) == E_INVALIDARG && calls == 0);
  CHECK(queryNativeFormatCaps(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, &output.value, unchecked) == S_OK);
  CHECK(output.value == D3D10_DDI_FORMAT_SUPPORT_NOT_SUPPORTED && calls == 0); output.check();
  CHECK(queryNativeMultisampleLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 1, nullptr,
    [&](DXGI_FORMAT, UINT, UINT*) { ++calls; return S_OK; }) == E_INVALIDARG && calls == 0);
  output = {}; bool caught = false;
  try {
    queryNativeFormatCaps(DXGI_FORMAT_R8G8B8A8_UNORM, &output.value,
      [](DXGI_FORMAT, UINT* staged) -> HRESULT { *staged = UINT(-1); throw std::bad_alloc(); });
  } catch (const std::bad_alloc&) { caught = true; }
  CHECK(caught && output.value == 0); output.check();
  output = {}; caught = false;
  try {
    queryNativeMultisampleLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 1, &output.value,
      [](DXGI_FORMAT, UINT, UINT* staged) -> HRESULT { *staged = UINT(-1); throw std::bad_alloc(); });
  } catch (const std::bad_alloc&) { caught = true; }
  CHECK(caught && output.value == 0); output.check();

  CHECK(nativeFormatCaps(D3D11_FORMAT_SUPPORT_BLENDABLE | D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET) == 0);
  CHECK(nativeFormatCaps(D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER | D3D11_FORMAT_SUPPORT_BUFFER
    | D3D11_FORMAT_SUPPORT_DISPLAY | D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW
    | D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT | D3D11_FORMAT_SUPPORT_DECODER_OUTPUT) == 0);
  CHECK(nativeFormatCaps(UINT(-1)) == 31);
}

#include "umd-d3d10-device-table.h"

template<typename Table> struct Fixture {
  static constexpr uint64_t canary = 0x7814f25391abcdeFull;
  std::vector<uint64_t> words;
  D3D10DDI_HDEVICE device;
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};
  Table table{};
  uint64_t tableCanary = canary;
  ComPtr<ID3D11Device> backend;
  Fixture() : words((VioGpuDxvkPrivateDeviceSize() + 7) / 8 + 1), device{words.data()} {
    words.back() = canary; core.pfnSetErrorCb = error;
    HRESULT hr = E_FAIL;
    if constexpr (std::is_same_v<Table, D3D10DDI_DEVICEFUNCS>)
      hr = VioGpuDxvkCreateDdiTestDevice(&expectedLuid, device, {&core}, &core, &table);
    else hr = VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid, device, {&core}, &core, &table);
    CHECK(hr == S_OK && createdBackend); backend = createdBackend; createdBackend.Reset();
    checkMandatoryTable(table);
    CHECK(tableCanary == canary && words.back() == canary);
  }
  ~Fixture() { backend.Reset(); table.pfnDestroyDevice(device); CHECK(tableCanary == canary && words.back() == canary); }
  void expect(HRESULT hr) { CHECK(lastError == dxvk::umd::ddiResult(hr)); lastError = S_OK; }
  void exercise() {
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
      DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_BC1_UNORM};
    for (DXGI_FORMAT format : formats) {
      UINT publicCaps = 0; const HRESULT hr = backend->CheckFormatSupport(format, &publicCaps);
      CHECK(hr == S_OK);
      Output output; table.pfnCheckFormatSupport(device, format, &output.value); expect(S_OK);
      CHECK(output.value == dxvk::umd::nativeFormatCaps(publicCaps)); CHECK((output.value & ~31u) == 0); output.check();
      for (UINT count = 0; count <= 33; ++count) {
        UINT qualities = 0; const HRESULT status = count && count <= 32
          ? backend->CheckMultisampleQualityLevels(format, count, &qualities) : S_OK;
        output = {}; table.pfnCheckMultisampleQualityLevels(device, format, count, &output.value);
        expect(status == S_OK || FAILED(status) ? status : E_FAIL);
        CHECK(output.value == (status == S_OK && count && count <= 32 ? count == 1 ? 1u : qualities : 0u));
        output.check();
      }
    }
    Output output; table.pfnCheckFormatSupport(device, DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, &output.value);
    expect(S_OK); CHECK(output.value == D3D10_DDI_FORMAT_SUPPORT_NOT_SUPPORTED); output.check();
    core.pfnSetErrorCb = replacementError; const auto before = replacementCallbacks;
    table.pfnCheckFormatSupport(device, DXGI_FORMAT_R8G8B8A8_UNORM, nullptr); expect(E_INVALIDARG);
    table.pfnCheckMultisampleQualityLevels(device, DXGI_FORMAT_R8G8B8A8_UNORM, 1, nullptr); expect(E_INVALIDARG);
    output = {}; table.pfnCheckFormatSupport(device, DXGI_FORMAT(-1), &output.value);
    expect(E_FAIL); CHECK(output.value == 0); output.check();
    for (UINT count : {1u, 4u}) {
      output = {}; table.pfnCheckMultisampleQualityLevels(device, DXGI_FORMAT(-1), count, &output.value);
      expect(E_INVALIDARG); CHECK(output.value == 0); output.check();
    }
    CHECK(replacementCallbacks == before + 5);
    // The final callback retires the owning device while its DDI is pinned.
    retire = table.pfnDestroyDevice; retirementDevice = device;
    output = {}; table.pfnCheckFormatSupport(device, DXGI_FORMAT(-1), &output.value);
    expect(E_FAIL); CHECK(!retire && output.value == 0); output.check();
    CHECK(tableCanary == canary && words.back() == canary);
  }
};

int main() {
  callerThread = GetCurrentThreadId();
  CHECK(dxvk::umd::runtimeMissingD3D10Requirements() != 0);
  CHECK(dxvk::umd::runtimeMissingD3D10_1Requirements() != 0);
  queryTransactions();
  { Fixture<D3D10DDI_DEVICEFUNCS> fixture; fixture.exercise(); }
  { Fixture<D3D10_1DDI_DEVICEFUNCS> fixture; fixture.exercise(); }
  CHECK(backendCalls == 2 && callbacks == 12 && replacementCallbacks == 12);
  std::printf("native D3D10/10.1 format queries verified checks=%u callbacks=%u backend_calls=%u hardware_admission=0\n",
    checks, callbacks, backendCalls);
}
