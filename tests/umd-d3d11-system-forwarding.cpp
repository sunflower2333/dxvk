// SPDX-License-Identifier: MIT
// CPU-only typed forwarding unit test. The injected core is a test stub, never
// an installed core or evidence of real device creation/rendering.
#include "umd-d3d11-system-front.cpp"
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace test {
UINT checks = 0, oldErrors = 0, newErrors = 0;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "forwarding check%u line%d: %s\n", checks, __LINE__, #value); std::exit(1); } } while (0)
int adapterCookie, runtimeCookie, coreCookie;
D3D10DDIARG_OPENADAPTER* expectedOpen;
const D3D10DDIARG_CALCPRIVATEDEVICESIZE* expectedSize;
D3D10DDIARG_CREATEDEVICE* expectedCreate;
const D3D10_2DDIARG_GETCAPS* expectedCaps;
UINT32* expectedCount;
UINT64* expectedVersions;
D3D10_2DDI_ADAPTERFUNCS alternative{};
D3D11DDI_CORELAYER_DEVICECALLBACKS liveCore{};
D3DDDI_DEVICECALLBACKS liveKernel{};
HRESULT createResult = E_FAIL, capsResult = S_OK;
UINT capValue = 0;
void APIENTRY oldError(D3D10DDI_HRTCORELAYER layer, HRESULT hr) {
  CHECK(layer.handle == &coreCookie && hr == E_FAIL); ++oldErrors;
}
void APIENTRY newError(D3D10DDI_HRTCORELAYER layer, HRESULT hr) {
  CHECK(layer.handle == &coreCookie && hr == E_FAIL); ++newErrors;
}
HRESULT APIENTRY query(HANDLE, const D3DDDICB_QUERYADAPTERINFO*) { return E_FAIL; }
void same(D3D10DDI_HADAPTER handle) { CHECK(handle.pDrvPrivate == &adapterCookie); }
void reentrantInfo() {
  auto snapshot = std::make_unique<VioGpuD11EntryInfo>(); snapshot->size = sizeof(*snapshot);
  CHECK(VioGpuDxvkD11ValidationInfo(snapshot.get()) == S_OK);
  CHECK(snapshot->eventCount && snapshot->events[snapshot->eventCount - 1].completed == 0);
}
SIZE_T APIENTRY size(D3D10DDI_HADAPTER handle, const D3D10DDIARG_CALCPRIVATEDEVICESIZE* args) {
  same(handle); CHECK(args == expectedSize); reentrantInfo(); return 0x350;
}
HRESULT APIENTRY create(D3D10DDI_HADAPTER handle, D3D10DDIARG_CREATEDEVICE* args) {
  same(handle); CHECK(args == expectedCreate);
  CHECK(args->pKTCallbacks == &liveKernel && args->p11UMCallbacks == &liveCore);
  CHECK(args->hRTCoreLayer.handle == &coreCookie);
  args->p11UMCallbacks->pfnSetErrorCb(args->hRTCoreLayer, E_FAIL);
  reentrantInfo(); return createResult;
}
HRESULT APIENTRY close(D3D10DDI_HADAPTER handle) { same(handle); reentrantInfo(); return S_OK; }
HRESULT APIENTRY versions(D3D10DDI_HADAPTER handle, UINT32* count, UINT64* output) {
  same(handle); CHECK(count == expectedCount && output == expectedVersions); reentrantInfo();
  if (!output) { *count = 1; return S_OK; }
  if (*count < 1) return E_OUTOFMEMORY;
  output[0] = D3D11_0_DDI_SUPPORTED; *count = 1; return S_OK;
}
HRESULT APIENTRY caps(D3D10DDI_HADAPTER handle, const D3D10_2DDIARG_GETCAPS* args) {
  same(handle); CHECK(args == expectedCaps); reentrantInfo();
  if (capsResult == S_OK) std::memcpy(args->pData, &capValue, sizeof(capValue));
  return capsResult;
}
HRESULT APIENTRY open(D3D10DDIARG_OPENADAPTER* args) {
  CHECK(args == expectedOpen && args->hRTAdapter.handle == &runtimeCookie);
  auto* originalOutput = args->pAdapterFuncs_2;
  reentrantInfo();
  // A runtime callback may mutate the descriptor. The core publishes through
  // the original captured output; the forwarder must do the same.
  args->pAdapterFuncs_2 = &alternative;
  originalOutput->pfnCalcPrivateDeviceSize = size; originalOutput->pfnCreateDevice = create;
  originalOutput->pfnCloseAdapter = close; originalOutput->pfnGetSupportedVersions = versions;
  originalOutput->pfnGetCaps = caps; args->hAdapter.pDrvPrivate = &adapterCookie;
  return S_OK;
}
BOOL CALLBACK inject(PINIT_ONCE, PVOID, PVOID*) { coreEntry = open; return TRUE; }
}

int main() {
  using namespace test;
  CHECK(OpenAdapter10_2(nullptr) == E_INVALIDARG);
  D3D10_2DDI_ADAPTERFUNCS functions{};
  D3DDDI_ADAPTERCALLBACKS callbacks{}; callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER request{};
  request.hRTAdapter.handle = &runtimeCookie; request.pAdapterCallbacks = &callbacks; request.pAdapterFuncs_2 = &functions;
  // Modern initial versions are deliberately ignored, as required by the DDI.
  request.Interface = 0xffffeeee; request.Version = 0xabcdef12; expectedOpen = &request;
  CHECK(InitOnceExecuteOnce(&coreOnce, inject, nullptr, nullptr) != FALSE);
  CHECK(OpenAdapter10_2(&request) == S_OK);
  CHECK(request.pAdapterFuncs_2 == &alternative && !alternative.pfnCreateDevice);
  CHECK(request.hAdapter.pDrvPrivate == &adapterCookie && functions.pfnCreateDevice && functions.pfnGetCaps);
  auto adapterHandle = request.hAdapter;
  UINT32 count = 0; expectedCount = &count; expectedVersions = nullptr;
  CHECK(functions.pfnGetSupportedVersions(adapterHandle, &count, nullptr) == S_OK && count == 1);
  UINT64 output = 0xdeadbeef; expectedVersions = &output; count = 0;
  CHECK(functions.pfnGetSupportedVersions(adapterHandle, &count, &output) == E_OUTOFMEMORY && count == 0 && output == 0xdeadbeef);
  count = 1;
  CHECK(functions.pfnGetSupportedVersions(adapterHandle, &count, &output) == S_OK && count == 1 && output == D3D11_0_DDI_SUPPORTED);
  UINT mask = 0xfeedbeef;
  D3D10_2DDIARG_GETCAPS capRequest{}; capRequest.Type = D3D11DDICAPS_3DPIPELINESUPPORT;
  capRequest.DataSize = sizeof(mask); capRequest.pData = &mask; expectedCaps = &capRequest;
  // A generic development response of zero must stay zero; the dedicated
  // validation core response of one must stay one. The frontend invents none.
  CHECK(functions.pfnGetCaps(adapterHandle, &capRequest) == S_OK && mask == 0);
  capValue = 1;
  CHECK(functions.pfnGetCaps(adapterHandle, &capRequest) == S_OK && mask == 1);
  capsResult = E_INVALIDARG; mask = 0xfeedbeef;
  CHECK(functions.pfnGetCaps(adapterHandle, &capRequest) == E_INVALIDARG && mask == 0xfeedbeef);
  D3D10DDIARG_CALCPRIVATEDEVICESIZE sizeRequest{};
  sizeRequest.Interface = D3D11_0_DDI_INTERFACE_VERSION; sizeRequest.Version = (D3D11_0_DDI_BUILD_VERSION << 16) | DXGI_RESOLVE_SHARED_RESOURCE;
  expectedSize = &sizeRequest;
  CHECK(functions.pfnCalcPrivateDeviceSize(adapterHandle, &sizeRequest) == 0x350);
  D3D10DDIARG_CREATEDEVICE createRequest{};
  createRequest.Interface = sizeRequest.Interface; createRequest.Version = sizeRequest.Version;
  createRequest.pKTCallbacks = &liveKernel; createRequest.p11UMCallbacks = &liveCore;
  createRequest.hRTCoreLayer.handle = &coreCookie; expectedCreate = &createRequest;
  liveCore.pfnSetErrorCb = oldError;
  CHECK(functions.pfnCreateDevice(adapterHandle, &createRequest) == E_FAIL && oldErrors == 1 && newErrors == 0);
  liveCore.pfnSetErrorCb = newError; createResult = S_OK;
  CHECK(functions.pfnCreateDevice(adapterHandle, &createRequest) == S_OK && oldErrors == 1 && newErrors == 1);
  CHECK(functions.pfnCloseAdapter(adapterHandle) == S_OK);
  CHECK(functions.pfnGetCaps(adapterHandle, &capRequest) == E_INVALIDARG && mask == 0xfeedbeef);
  auto snapshot = std::make_unique<VioGpuD11EntryInfo>(); snapshot->size = sizeof(*snapshot);
  CHECK(VioGpuDxvkD11ValidationInfo(snapshot.get()) == S_OK && snapshot->liveAdapters == 0 && !snapshot->overflow);
  for (UINT i = 0; i < snapshot->eventCount; ++i) CHECK(snapshot->events[i].completed && snapshot->events[i].sequence == i + 1);
  CHECK(snapshot->events[4].call == VioGpuD11Call::Caps && snapshot->events[4].caps == 0);
  CHECK(snapshot->events[5].call == VioGpuD11Call::Caps && snapshot->events[5].caps == 1);
  std::printf("SYSTEM_D3D11_FORWARDING_CPU_PASS checks=%u live_callback_updates=1 core_stub=1 driver_runtime_gpu=0\n", checks);
  return 0;
}
