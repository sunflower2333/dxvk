// SPDX-License-Identifier: MIT
// Actual typed adapter/device/DXGI entries, with a fixture-only WARP renderer
// and independently controlled runtime allocation callbacks. No public-runtime
// or hardware admission is substituted by this test backend.
#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_allocation.h"
#include <d3d9.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

static unsigned checks, residencyCalls, priorityCalls, deallocations, errors;
static DWORD caller;
static LUID selected{0x12345678, -39};
static uint64_t generation = 19;
static char adapterCookie, deviceCookie, coreCookie, contextCookie;
static HRESULT residencyResult = S_OK, priorityResult = S_OK;
static int invalidStatus = -1;
static UINT invalidStatusWord;
static unsigned statusMode;
static bool changeResidencyMetadata, changePriorityMetadata;
static std::vector<D3DKMT_HANDLE> expectedHandles;
static UINT expectedPriority;
static std::function<void()> callbackAction;
static std::unordered_map<HANDLE, D3DKMT_HANDLE> owned;
static D3DKMT_HANDLE nextHandle = 100;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "DXGI residency line=%d: %s\n", __LINE__, #value); std::abort(); \
} } while (0)
static void runtimeCaller() { CHECK(GetCurrentThreadId() == caller); }

static HRESULT APIENTRY adapterQuery(HANDLE adapter, const D3DDDICB_QUERYADAPTERINFO* request) {
  runtimeCaller(); CHECK(adapter == &adapterCookie && request && request->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(request->pPrivateDriverData);
  for (unsigned i = 0; i < 160; ++i) CHECK(bytes[i] == 0);
  auto set = [&](unsigned offset, uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes[offset + i] = uint8_t(value >> (8 * i));
  };
  set(0, 0x504d5644, 4); set(8, 128, 4); set(16, 3, 8); set(24, generation, 8);
  set(128, 0x44494c56, 4); set(132, 1, 4); set(136, 32, 4); set(140, 1, 4); set(152, 1, 4);
  std::memcpy(bytes + 144, &selected, sizeof(selected)); return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* request) {
  runtimeCaller(); CHECK(device == &deviceCookie && request && request->hResource);
  CHECK(request->NumAllocations == 1 && request->pAllocationInfo);
  const auto handle = nextHandle++;
  CHECK(owned.emplace(request->hResource, handle).second);
  request->pAllocationInfo->hAllocation = handle; request->hKMResource = handle + 1000;
  return S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* request) {
  runtimeCaller(); CHECK(device == &deviceCookie && request && request->hResource);
  CHECK(!request->NumAllocations && !request->HandleList && owned.erase(request->hResource) == 1);
  ++deallocations; return S_OK;
}
static HRESULT APIENTRY residency(HANDLE device, const D3DDDICB_QUERYRESIDENCY* request) {
  runtimeCaller(); ++residencyCalls;
  CHECK(device == &deviceCookie && request && !request->hResource);
  CHECK(request->NumAllocations == expectedHandles.size() && request->HandleList && request->pResidencyStatus);
  for (UINT i = 0; i < request->NumAllocations; ++i) CHECK(request->HandleList[i] == expectedHandles[i]);
  if (callbackAction) std::exchange(callbackAction, {})();
  for (UINT i = 0; i < request->NumAllocations; ++i) {
    if (int(i) == invalidStatus) std::memcpy(request->pResidencyStatus + i, &invalidStatusWord, sizeof(invalidStatusWord));
    else request->pResidencyStatus[i] = static_cast<D3DDDI_RESIDENCYSTATUS>(
      statusMode == 4 ? 3 - i % 3 : statusMode ? statusMode : 1 + i % 3);
  }
  if (changeResidencyMetadata) const_cast<D3DDDICB_QUERYRESIDENCY*>(request)->NumAllocations++;
  return residencyResult;
}
static HRESULT APIENTRY priority(HANDLE device, D3DDDICB_SETPRIORITY* request) {
  runtimeCaller(); ++priorityCalls;
  CHECK(device == &deviceCookie && request && !request->hResource && request->NumAllocations == 1);
  CHECK(request->HandleList && request->pPriorities && expectedHandles.size() == 1);
  CHECK(request->HandleList[0] == expectedHandles[0] && request->pPriorities[0] == expectedPriority);
  if (callbackAction) std::exchange(callbackAction, {})();
  if (changePriorityMetadata) request->hResource = &adapterCookie;
  return priorityResult;
}
static HRESULT APIENTRY wrongResidency(HANDLE, const D3DDDICB_QUERYRESIDENCY*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY wrongPriority(HANDLE, D3DDDICB_SETPRIORITY*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY lock(HANDLE, D3DDDICB_LOCK*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY unlock(HANDLE, const D3DDDICB_UNLOCK*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY createContext(HANDLE, D3DDDICB_CREATECONTEXT* request) {
  runtimeCaller(); request->hContext = &contextCookie; return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE, const D3DDDICB_DESTROYCONTEXT*) { runtimeCaller(); return S_OK; }
static HRESULT APIENTRY present(HANDLE, DXGIDDICB_PRESENT*) { CHECK(false); return E_FAIL; }
static void APIENTRY setError(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  runtimeCaller(); CHECK(runtime.handle == &coreCookie && FAILED(result)); ++errors;
}

HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend*) noexcept {
  CHECK(!std::memcmp(&luid, &selected, sizeof(luid)));
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

struct Storage {
  void* bytes;
  explicit Storage(SIZE_T size) : bytes(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) {
    CHECK(size && bytes);
  }
  void poison() { DWORD previous; CHECK(VirtualProtect(bytes, 1, PAGE_NOACCESS, &previous)); }
  ~Storage() { CHECK(VirtualFree(bytes, 0, MEM_RELEASE)); }
};

template<typename Table, bool dxgi11>
struct Fixture {
  using Desc = std::conditional_t<std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>,
    D3D11DDIARG_CREATERESOURCE, D3D10DDIARG_CREATERESOURCE>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  Storage kernelStorage{sizeof(D3DDDI_DEVICECALLBACKS)};
  D3D10DDI_HDEVICE device{storage.bytes};
  D3D10DDI_HADAPTER adapter{};
  D3D10_2DDI_ADAPTERFUNCS adapterFunctions{};
  Table table{};
  std::conditional_t<dxgi11, DXGI1_1_DDI_BASE_FUNCTIONS, DXGI_DDI_BASE_FUNCTIONS> dxgi{};
  DXGI_DDI_BASE_CALLBACKS dxgiCallbacks{};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  bool live = true;
  explicit Fixture(bool policyCallbacks = true) {
    generation = 19;
    D3DDDI_ADAPTERCALLBACKS adapterCallbacks{}; adapterCallbacks.pfnQueryAdapterInfoCb = adapterQuery;
    D3D10DDIARG_OPENADAPTER open{};
    open.hRTAdapter.handle = &adapterCookie; open.pAdapterCallbacks = &adapterCallbacks;
    open.pAdapterFuncs_2 = &adapterFunctions;
    CHECK(dxvk::umd::openAdapterForTest(&open, true) == S_OK); adapter = open.hAdapter;
    auto kernel = static_cast<D3DDDI_DEVICECALLBACKS*>(kernelStorage.bytes);
    kernel->pfnAllocateCb = allocate; kernel->pfnDeallocateCb = deallocate;
    kernel->pfnLockCb = lock; kernel->pfnUnlockCb = unlock;
    kernel->pfnCreateContextCb = createContext; kernel->pfnDestroyContextCb = destroyContext;
    if (policyCallbacks) { kernel->pfnQueryResidencyCb = residency; kernel->pfnSetPriorityCb = priority; }
    core10.pfnSetErrorCb = setError; core11.pfnSetErrorCb = setError;
    dxgiCallbacks.pfnPresentCb = present;
    D3D10DDIARG_CREATEDEVICE args{};
    UINT build;
    if constexpr (std::is_same_v<Table, D3D10DDI_DEVICEFUNCS>) {
      args.Interface = D3D10_0_DDI_INTERFACE_VERSION; build = D3D10_0_DDI_BUILD_VERSION;
      args.pDeviceFuncs = &table; args.pUMCallbacks = &core10;
    } else if constexpr (std::is_same_v<Table, D3D10_1DDI_DEVICEFUNCS>) {
      args.Interface = D3D10_1_DDI_INTERFACE_VERSION; build = D3D10_1_DDI_BUILD_VERSION;
      args.p10_1DeviceFuncs = &table; args.pUMCallbacks = &core10;
    } else {
      args.Interface = D3D11_0_DDI_INTERFACE_VERSION; build = D3D11_0_DDI_BUILD_VERSION;
      args.Flags = D3D11DDI_3DPIPELINELEVEL_11_0 << D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
      args.p11DeviceFuncs = &table; args.p11UMCallbacks = &core11;
    }
    args.Version = build << 16;
    if constexpr (dxgi11) args.Version |= std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>
      ? DXGI_RESOLVE_SHARED_RESOURCE : (VISTA_GOLD_PRODUCT_VER | DXGI_RESOLVE_SHARED_RESOURCE);
    CHECK(dxvk::umd::nativeDxgiUses1_1(args.Interface, args.Version) == dxgi11);
    args.hDrvDevice = device; args.hRTDevice.handle = &deviceCookie; args.hRTCoreLayer.handle = &coreCookie;
    args.pKTCallbacks = kernel; args.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgiCallbacks;
    if constexpr (dxgi11) args.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = &dxgi;
    else args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgi;
    CHECK(adapterFunctions.pfnCreateDevice(adapter, &args) == S_OK);
    CHECK(dxgi.pfnQueryResourceResidency && dxgi.pfnSetResourcePriority);
    // The original kernel table is a bounded creation-time snapshot. Neither
    // later entry replacement nor inaccessible original storage can alter it.
    kernel->pfnQueryResidencyCb = wrongResidency; kernel->pfnSetPriorityCb = wrongPriority;
    kernelStorage.poison();
  }
  void retire() {
    CHECK(live); live = false; table.pfnDestroyDevice(device); storage.poison();
  }
  ~Fixture() { if (live) retire(); CHECK(adapterFunctions.pfnCloseAdapter(adapter) == S_OK); }
  HRESULT query(const DXGI_DDI_HRESOURCE* resources, SIZE_T count, DXGI_DDI_RESIDENCY* output) {
    DXGI_DDI_ARG_QUERYRESOURCERESIDENCY args{};
    args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate);
    args.pResources = resources; args.Resources = count; args.pStatus = output;
    return dxgi.pfnQueryResourceResidency(&args);
  }
  HRESULT set(DXGI_DDI_HRESOURCE resource, UINT value) {
    DXGI_DDI_ARG_SETRESOURCEPRIORITY args{};
    args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate);
    args.hResource = resource; args.Priority = value;
    return dxgi.pfnSetResourcePriority(&args);
  }
};

template<typename F>
struct Texture {
  F& fixture;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  char runtime;
  bool live = true;
  Texture(F& f, UINT flags, bool opened = false)
  : fixture(f), storage([&] { typename F::Desc desc{};
      return f.table.pfnCalcPrivateResourceSize(f.device, &desc); }()), handle{storage.bytes} {
    if (opened) {
      dxvk::umd::AllocationInfo info; info.width = info.height = 2; info.pitch = 8; info.size = 16; info.format = 3;
      D3DDDI_OPENALLOCATIONINFO allocation{};
      allocation.hAllocation = 901; allocation.pPrivateDriverData = &info; allocation.PrivateDriverDataSize = sizeof(info);
      D3D10DDIARG_OPENRESOURCE args{}; args.NumAllocations = 1; args.pOpenAllocationInfo = &allocation;
      args.hKMResource.handle = 1901;
      f.table.pfnOpenResource(f.device, &args, handle, {&runtime});
    } else {
      D3D10DDI_MIPINFO mip{2, 2, 1, 2, 2, 1};
      typename F::Desc desc{}; desc.pMipInfoList = &mip;
      desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; desc.Usage = D3D10_DDI_USAGE_DEFAULT;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.MipLevels = desc.ArraySize = 1;
      desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
      if (flags == 1) desc.BindFlags |= D3D10_DDI_BIND_PRESENT;
      if (flags == 2) desc.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED;
      f.table.pfnCreateResource(f.device, &desc, handle, {&runtime});
    }
    CHECK(errors == 0);
  }
  DXGI_DDI_HRESOURCE dxgi() const { return reinterpret_cast<DXGI_DDI_HRESOURCE>(handle.pDrvPrivate); }
  D3DKMT_HANDLE allocation() const { const auto found = owned.find(const_cast<char*>(&runtime)); return found == owned.end() ? 901 : found->second; }
  void retire() { CHECK(live); live = false; fixture.table.pfnDestroyResource(fixture.device, handle); storage.poison(); }
  ~Texture() { if (live) retire(); }
};

template<typename Table, bool dxgi11>
static void profile() {
  using F = Fixture<Table, dxgi11>;
  // Zero is outside the three valid residency statuses while still inside
  // this SDK enum's representable range; it detects every partial write.
  constexpr auto sentinel = static_cast<DXGI_DDI_RESIDENCY>(0);
  {
    F f;
    Texture<F> a(f, 1), b(f, 2), opened(f, 0, true), backendOnly(f, 0);
    std::array<DXGI_DDI_HRESOURCE, 4> resources{a.dxgi(), b.dxgi(), opened.dxgi(), a.dxgi()};
    std::array<DXGI_DDI_RESIDENCY, 4> output{sentinel, sentinel, sentinel, sentinel};
    expectedHandles = {a.allocation(), b.allocation(), 901, a.allocation()};
    CHECK(f.query(resources.data(), resources.size(), output.data()) == S_NOT_RESIDENT);
    CHECK(output[0] == DXGI_DDI_RESIDENCY_FULLY_RESIDENT && output[1] == DXGI_DDI_RESIDENCY_RESIDENT_IN_SHARED_MEMORY
      && output[2] == DXGI_DDI_RESIDENCY_EVICTED_TO_DISK && output[3] == DXGI_DDI_RESIDENCY_FULLY_RESIDENT);
    for (unsigned mode : {1u, 2u, 3u}) {
      statusMode = mode;
      CHECK(f.query(resources.data(), resources.size(), output.data())
        == (mode == 1 ? S_OK : mode == 2 ? S_RESIDENT_IN_SHARED_MEMORY : S_NOT_RESIDENT));
      for (auto value : output) CHECK(value == static_cast<DXGI_DDI_RESIDENCY>(mode));
    }
    statusMode = 4;
    CHECK(f.query(resources.data(), resources.size(), output.data()) == S_NOT_RESIDENT);
    CHECK(output[0] == DXGI_DDI_RESIDENCY_EVICTED_TO_DISK && output[1] == DXGI_DDI_RESIDENCY_RESIDENT_IN_SHARED_MEMORY);
    statusMode = 0;
    for (const HRESULT result : {E_OUTOFMEMORY, DXGI_ERROR_WAS_STILL_DRAWING, S_FALSE}) {
      residencyResult = result; output.fill(sentinel);
      CHECK(f.query(resources.data(), resources.size(), output.data()) == (result == S_FALSE ? E_FAIL : result));
      for (auto value : output) CHECK(value == sentinel);
    }
    residencyResult = S_OK;
    for (int index = 0; index < 4; ++index) {
      invalidStatus = index;
      CHECK(f.query(resources.data(), resources.size(), output.data()) == E_FAIL);
      for (auto value : output) CHECK(value == sentinel);
    }
    for (UINT word : {4u, 0xffffffffu}) {
      invalidStatus = 3; invalidStatusWord = word;
      CHECK(f.query(resources.data(), resources.size(), output.data()) == E_FAIL);
      for (auto value : output) CHECK(value == sentinel);
    }
    invalidStatus = -1; invalidStatusWord = 0; changeResidencyMetadata = true;
    CHECK(f.query(resources.data(), resources.size(), output.data()) == E_FAIL);
    for (auto value : output) CHECK(value == sentinel);
    changeResidencyMetadata = false;
    const auto before = residencyCalls;
    CHECK(f.query(nullptr, 4, output.data()) == E_INVALIDARG);
    CHECK(f.query(resources.data(), 0, output.data()) == E_INVALIDARG);
    CHECK(f.query(resources.data(), 4, nullptr) == E_INVALIDARG);
    CHECK(f.query(resources.data(), SIZE_T(-1), output.data()) == DXGI_DDI_ERR_UNSUPPORTED);
    resources[3] = backendOnly.dxgi();
    CHECK(f.query(resources.data(), 4, output.data()) == DXGI_DDI_ERR_UNSUPPORTED);
    Storage inaccessible(1); inaccessible.poison();
    resources[3] = reinterpret_cast<DXGI_DDI_HRESOURCE>(inaccessible.bytes);
    CHECK(f.query(resources.data(), 4, output.data()) == E_INVALIDARG);
    CHECK(residencyCalls == before);
    for (auto value : output) CHECK(value == sentinel);
    resources[3] = a.dxgi();
    callbackAction = [&] {
      CHECK(f.query(resources.data(), 4, output.data()) == DXGI_ERROR_WAS_STILL_DRAWING);
      CHECK(f.set(a.dxgi(), 0) == DXGI_ERROR_WAS_STILL_DRAWING);
      resources.fill(0); // Original caller input may be overwritten by runtime reentry.
    };
    CHECK(f.query(resources.data(), 4, output.data()) == S_NOT_RESIDENT);
    expectedHandles = {b.allocation()};
    for (UINT value : {0u, 0x78000000u, 0xffffffffu}) {
      expectedPriority = value; CHECK(f.set(b.dxgi(), value) == S_OK);
    }
    expectedPriority = 0x1234;
    for (const HRESULT result : {E_OUTOFMEMORY, DXGI_ERROR_WAS_STILL_DRAWING, S_FALSE}) {
      priorityResult = result; CHECK(f.set(b.dxgi(), expectedPriority) == (result == S_FALSE ? E_FAIL : result));
    }
    priorityResult = S_OK; changePriorityMetadata = true;
    CHECK(f.set(b.dxgi(), expectedPriority) == E_FAIL); changePriorityMetadata = false;
    const auto priorityBefore = priorityCalls;
    CHECK(f.set(backendOnly.dxgi(), expectedPriority) == DXGI_DDI_ERR_UNSUPPORTED);
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(inaccessible.bytes), expectedPriority) == E_INVALIDARG);
    CHECK(priorityCalls == priorityBefore);
    callbackAction = [&] { CHECK(f.set(b.dxgi(), expectedPriority) == DXGI_ERROR_WAS_STILL_DRAWING); b.retire(); };
    CHECK(f.set(b.dxgi(), expectedPriority) == DXGI_ERROR_DEVICE_REMOVED);
  }
  CHECK(owned.empty());
  {
    F f(false); Texture<F> resource(f, 2);
    auto handle = resource.dxgi(); DXGI_DDI_RESIDENCY output = sentinel;
    CHECK(f.query(&handle, 1, &output) == DXGI_DDI_ERR_UNSUPPORTED && output == sentinel);
    CHECK(f.set(handle, 0) == DXGI_DDI_ERR_UNSUPPORTED);
  }
  {
    F f, foreign;
    Texture<F> resource(foreign, 2); auto handle = resource.dxgi(); DXGI_DDI_RESIDENCY output = sentinel;
    CHECK(f.query(&handle, 1, &output) == E_INVALIDARG && output == sentinel);
    CHECK(f.set(handle, 1) == E_INVALIDARG);
  }
  {
    F f; Texture<F> resource(f, 2);
    auto handle = resource.dxgi(); DXGI_DDI_RESIDENCY output = sentinel; expectedHandles = {resource.allocation()};
    callbackAction = [&] { resource.retire(); f.retire(); };
    CHECK(f.query(&handle, 1, &output) == DXGI_ERROR_DEVICE_REMOVED && output == sentinel);
    CHECK(f.query(&handle, 1, &output) == DXGI_ERROR_DEVICE_REMOVED && output == sentinel);
  }
  {
    F f; Texture<F> resource(f, 2);
    auto handle = resource.dxgi(); DXGI_DDI_RESIDENCY output = sentinel; expectedHandles = {resource.allocation()};
    callbackAction = [&] { ++generation; };
    CHECK(f.query(&handle, 1, &output) == DXGI_ERROR_DEVICE_REMOVED && output == sentinel);
  }
  CHECK(owned.empty() && !callbackAction && errors == 0);
}

int main() {
  caller = GetCurrentThreadId();
  profile<D3D10DDI_DEVICEFUNCS, false>(); profile<D3D10DDI_DEVICEFUNCS, true>();
  profile<D3D10_1DDI_DEVICEFUNCS, false>(); profile<D3D10_1DDI_DEVICEFUNCS, true>();
  profile<D3D11DDI_DEVICEFUNCS, false>(); profile<D3D11DDI_DEVICEFUNCS, true>();
  std::printf("DXGI residency/priority PASS checks=%u profiles=6 residency=%u priority=%u releases=%u\n",
    checks, residencyCalls, priorityCalls, deallocations);
}
