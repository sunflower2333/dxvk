#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_contract.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <type_traits>

static unsigned checks, queries, backends, errors;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "native entry check failed line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)

#ifdef VIOGPU_TEST_COMPLETE_CONTRACT
// Linked only into this controlled lifetime fixture, never into the UMD DLL.
// Actual adapter, device, resource and destruction implementations are linked.
uint32_t dxvk::umd::runtimeMissingD3D10Requirements() noexcept { return 0; }
static constexpr bool complete = true;
#else
static constexpr bool complete = false;
#endif

static char adapterCookie, deviceCookie, coreCookie, resourceCookie, contextCookie, dxgiCookie;
static unsigned allocations, deallocations, presents, contextDestroys;
static uint32_t publishedPixels[4];
static LUID expected = {0x92345678, -81};
static uint64_t generation = 19, capabilities = 3;
static HRESULT queryResult = S_OK, backendResult = S_OK;
static D3D10DDI_HADAPTER active = {};
static D3D10_2DDI_ADAPTERFUNCS functions = {};
static D3D10DDIARG_CREATEDEVICE* activeCreate;
static D3D10DDI_DEVICEFUNCS* activeTable;
enum class Action { None, QueryAgain, Close, Reset, DuplicateCreate };
static Action queryAction, backendAction;
static bool destroyOnError;
static D3D10DDI_HRESOURCE activeResource = {};
static SIZE_T activeResourceBytes;
static HRESULT allocationResult = S_OK, deallocationResult = S_OK;
static bool zeroAllocation, destroyResourceOnDeallocate, destroyResourceOnError;
static bool cancelResourceCreation;

static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie);
  CHECK(args->NumAllocations == 1 && args->pAllocationInfo);
  args->pAllocationInfo[0].hAllocation = zeroAllocation ? 0 : 123;
  args->hKMResource = 456; ++allocations;
  if (cancelResourceCreation) {
    cancelResourceCreation = false;
    activeTable->pfnDestroyResource(activeCreate->hDrvDevice, activeResource);
    std::memset(activeResource.pDrvPrivate, 0xcc, activeResourceBytes);
  }
  return allocationResult;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie); ++deallocations;
  if (destroyResourceOnDeallocate) {
    destroyResourceOnDeallocate = false;
    activeTable->pfnDestroyResource(activeCreate->hDrvDevice, activeResource);
    // Model the runtime immediately reclaiming the retired resource storage.
    std::memset(activeResource.pDrvPrivate, 0xcc, activeResourceBytes);
  }
  return deallocationResult;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  CHECK(device == &deviceCookie && args && args->hAllocation == 123);
  args->pData = publishedPixels; return S_OK;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  CHECK(device == &deviceCookie && args && args->NumAllocations == 1); return S_OK;
}
static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
  CHECK(device == &deviceCookie && args); args->hContext = &contextCookie; return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
  CHECK(device == &deviceCookie && args && args->hContext == &contextCookie); ++contextDestroys;
  activeTable->pfnDestroyDevice(activeCreate->hDrvDevice); // Reenter the actual destructor.
  return E_FAIL; // Verify error callback is retained and invoked after retirement.
}
static HRESULT APIENTRY present(HANDLE device, DXGIDDICB_PRESENT* args) {
  CHECK(device == &deviceCookie && args && args->hSrcAllocation == 123);
  CHECK(args->hContext == &contextCookie && args->pDXGIContext == &dxgiCookie); ++presents;
  return S_OK;
}

static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
  ++queries;
  CHECK(runtime == &adapterCookie && args && args->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (unsigned i = 0; i < 160; ++i) CHECK(bytes[i] == 0);
  const Action action = queryAction; queryAction = Action::None;
  if (action == Action::QueryAgain) {
    UINT32 count = 77;
    CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == DXGI_ERROR_WAS_STILL_DRAWING);
    CHECK(count == 77);
  }
  if (action == Action::Close) CHECK(functions.pfnCloseAdapter(active) == S_OK);
  auto set = [bytes](unsigned offset, uint64_t value, unsigned length) {
    for (unsigned i = 0; i < length; ++i) bytes[offset+i] = uint8_t(value >> (8*i));
  };
  set(0, 0x504d5644, 4); set(8, 128, 4); set(16, capabilities, 8); set(24, generation, 8);
  set(128, 0x44494c56, 4); set(132, 1, 4); set(136, 32, 4); set(140, 1, 4); set(152, 1, 4);
  std::memcpy(bytes+144, &expected, sizeof(expected));
  return queryResult;
}

static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  CHECK(runtime.handle == &coreCookie && FAILED(hr)); ++errors;
  if (destroyResourceOnError) {
    destroyResourceOnError = false;
    activeTable->pfnDestroyResource(activeCreate->hDrvDevice, activeResource);
  }
  if (destroyOnError) {
    destroyOnError = false;
    activeTable->pfnDestroyDevice(activeCreate->hDrvDevice);
  }
}

HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context) noexcept {
  ++backends;
  CHECK(!std::memcmp(&luid, &expected, sizeof(luid)) && level == D3D_FEATURE_LEVEL_10_0);
  *device = nullptr; *context = nullptr;
  if (backendResult != S_OK) return backendResult;
  const Action action = backendAction; backendAction = Action::None;
  if (action == Action::DuplicateCreate) {
    D3D10DDI_DEVICEFUNCS untouched = *activeCreate->pDeviceFuncs;
    CHECK(functions.pfnCreateDevice(active, activeCreate) == E_INVALIDARG);
    CHECK(!std::memcmp(&untouched, activeCreate->pDeviceFuncs, sizeof(untouched)));
  }
  // The fixture backend is Microsoft's WARP only. The production DLL retains
  // the embedded DXVK factory and its exact LUID/Turnip selection policy.
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (action == Action::Reset) ++generation;
  if (action == Action::Close) CHECK(functions.pfnCloseAdapter(active) == S_OK);
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept {
  return E_NOTIMPL; // This fixture never calls the staging-resource busy DDI.
}

static void openAdapter() {
  D3DDDI_ADAPTERCALLBACKS callbacks = {};
  callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER args = {};
  args.Interface = 0xffffffff; args.Version = 0xffffffff;
  args.pAdapterFuncs_2 = &functions;
  args.pAdapterCallbacks = &callbacks;
  args.hRTAdapter.handle = &adapterCookie;
  CHECK(OpenAdapter10_2(&args) == S_OK);
  CHECK(args.hAdapter.pDrvPrivate && functions.pfnGetSupportedVersions
    && functions.pfnGetCaps && functions.pfnCreateDevice && functions.pfnCalcPrivateDeviceSize
    && functions.pfnCloseAdapter);
  active = args.hAdapter;
  // The runtime may release or overwrite its original callback table after
  // open. The exact function and opaque handle, not a pointer to this local,
  // must remain retained by the adapter.
  callbacks = {};
}

int main() {
  CHECK(OpenAdapter10_2(nullptr) == E_INVALIDARG);
  struct LegacyTable { D3D10DDI_ADAPTERFUNCS table; UINT64 canary; } legacy = {{}, 0xabcdef};
  D3DDDI_ADAPTERCALLBACKS legacyCallbacks = {}; legacyCallbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER legacyOpen = {};
  legacyOpen.Interface = D3D10_0_DDI_INTERFACE_VERSION;
  legacyOpen.Version = D3D10_0_DDI_BUILD_VERSION << 16;
  legacyOpen.pAdapterFuncs = &legacy.table; legacyOpen.pAdapterCallbacks = &legacyCallbacks;
  legacyOpen.hRTAdapter.handle = &adapterCookie;
  CHECK(OpenAdapter10(&legacyOpen) == (complete ? S_OK : DXGI_ERROR_UNSUPPORTED));
  CHECK(legacy.canary == 0xabcdef);
  if (complete) CHECK(legacy.table.pfnCloseAdapter(legacyOpen.hAdapter) == S_OK);
  else CHECK(!legacyOpen.hAdapter.pDrvPrivate && !legacy.table.pfnCreateDevice);
  openAdapter();
  UINT32 count = 31;
  CHECK(functions.pfnGetSupportedVersions(active, nullptr, nullptr) == E_INVALIDARG);
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == S_OK);
  CHECK(count == (complete ? 1u : 0u));
  UINT64 versions[2] = {0xaabbccdd, 0x778899};
  count = 0;
  CHECK(functions.pfnGetSupportedVersions(active, &count, versions)
    == (complete ? E_OUTOFMEMORY : S_OK));
  CHECK(versions[0] == 0xaabbccdd && versions[1] == 0x778899);
  count = 2;
  CHECK(functions.pfnGetSupportedVersions(active, &count, versions) == S_OK);
  CHECK(versions[0] == (complete ? D3D10_0_DDI_SUPPORTED : 0xaabbccdd));
  CHECK(versions[1] == 0x778899);

  D3D11DDI_3DPIPELINESUPPORT_CAPS pipelines = {};
  D3D10_2DDIARG_GETCAPS caps = {};
  caps.Type = D3D11DDICAPS_3DPIPELINESUPPORT;
  caps.pData = &pipelines; caps.DataSize = sizeof(pipelines);
  CHECK(functions.pfnGetCaps(active, &caps) == S_OK);
  CHECK(pipelines.Caps == (complete
    ? D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_0) : 0));
  caps.DataSize--; pipelines.Caps = 0x1234;
  CHECK(functions.pfnGetCaps(active, &caps) == E_INVALIDARG && pipelines.Caps == 0x1234);
  caps.DataSize++; caps.Type = static_cast<D3D10_2DDICAPS_TYPE>(0xffffffff);
  CHECK(functions.pfnGetCaps(active, &caps) == DXGI_ERROR_UNSUPPORTED && pipelines.Caps == 0x1234);
  queryAction = Action::QueryAgain;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == S_OK);

  D3D10DDIARG_CALCPRIVATEDEVICESIZE size = {};
  size.Interface = D3D10_0_DDI_INTERFACE_VERSION;
  size.Version = D3D10_0_DDI_BUILD_VERSION << 16;
  const SIZE_T bytes = functions.pfnCalcPrivateDeviceSize(active, &size);
  CHECK(complete ? bytes == VioGpuDxvkPrivateDeviceSize() : bytes == 0);
  std::vector<std::max_align_t> storage((VioGpuDxvkPrivateDeviceSize()+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
  D3D10DDI_CORELAYER_DEVICECALLBACKS core = {}; core.pfnSetErrorCb = error;
  D3DDDI_DEVICECALLBACKS kernel = {};
  kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
  kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
  kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = present;
  DXGI_DDI_BASE_FUNCTIONS dxgiFunctions = {};
  D3D10DDI_DEVICEFUNCS table = {};
  D3D10DDIARG_CREATEDEVICE create = {};
  create.Interface = size.Interface; create.Version = size.Version;
  create.hDrvDevice.pDrvPrivate = storage.data();
  create.hRTDevice.handle = &deviceCookie; create.hRTCoreLayer.handle = &coreCookie;
  create.pUMCallbacks = &core; create.pKTCallbacks = &kernel; create.pDeviceFuncs = &table;
  create.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgi;
  create.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgiFunctions;
  activeCreate = &create; activeTable = &table;
  auto invalid = create; invalid.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  CHECK(functions.pfnCreateDevice(active, &invalid) == DXGI_ERROR_UNSUPPORTED && backends == 0);
  if (!complete) {
    CHECK(functions.pfnCreateDevice(active, &create) == DXGI_ERROR_UNSUPPORTED && backends == 0);
  } else {
    auto savedKernel = kernel; auto savedDxgi = dxgi;
#define MISSING_CALLBACK(member) do { kernel.member = nullptr; \
    CHECK(functions.pfnCreateDevice(active, &create) == E_INVALIDARG && backends == 0); \
    kernel = savedKernel; } while (0)
    MISSING_CALLBACK(pfnAllocateCb); MISSING_CALLBACK(pfnDeallocateCb);
    MISSING_CALLBACK(pfnLockCb); MISSING_CALLBACK(pfnUnlockCb);
    MISSING_CALLBACK(pfnCreateContextCb); MISSING_CALLBACK(pfnDestroyContextCb);
#undef MISSING_CALLBACK
    dxgi.pfnPresentCb = nullptr;
    CHECK(functions.pfnCreateDevice(active, &create) == E_INVALIDARG && backends == 0);
    dxgi = savedDxgi;
    backendResult = E_OUTOFMEMORY;
    CHECK(functions.pfnCreateDevice(active, &create) == E_OUTOFMEMORY && !table.pfnDestroyDevice);
    backendResult = S_OK; backendAction = Action::DuplicateCreate;
    CHECK(functions.pfnCreateDevice(active, &create) == S_OK && table.pfnDestroyDevice);
    CHECK(functions.pfnCreateDevice(active, &create) == E_INVALIDARG);
    CHECK(functions.pfnCloseAdapter(active) == S_OK);
    CHECK(functions.pfnCloseAdapter(active) == E_INVALIDARG);
    // Device survives adapter close; original error callback and core handle
    // remain valid after the runtime overwrites the source callback structure.
    core.pfnSetErrorCb = nullptr; kernel = {}; dxgi = {};
    D3D10DDI_COUNTER_INFO counters = {};
    table.pfnCheckCounterInfo(create.hDrvDevice, &counters);
    CHECK(dxgiFunctions.pfnPresent);
    D3D10DDI_MIPINFO mip = {2,2,1,2,2,1};
    D3D10DDIARG_CREATERESOURCE desc = {};
    desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    desc.Usage = D3D10_DDI_USAGE_DEFAULT;
    desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.MipLevels = 1; desc.ArraySize = 1;
    uint32_t pixels[] = {0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff};
    std::remove_const_t<std::remove_reference_t<decltype(*desc.pInitialDataUP)>> initial = {};
    initial.pSysMem = pixels; initial.SysMemPitch = 8; initial.SysMemSlicePitch = 16;
    desc.pInitialDataUP = &initial;
    SIZE_T resourceBytes = table.pfnCalcPrivateResourceSize(create.hDrvDevice, &desc);
    std::vector<std::max_align_t> resourceStorage((resourceBytes+sizeof(std::max_align_t)-1)/sizeof(std::max_align_t));
    D3D10DDI_HRESOURCE resource = {resourceStorage.data()};
    activeResource = resource; activeResourceBytes = resourceBytes;
    table.pfnCreateResource(create.hDrvDevice, &desc, resource, {&resourceCookie});
    CHECK(allocations == 1 && errors == 0);
    DXGI_DDI_ARG_PRESENT presentation = {};
    presentation.hDevice = reinterpret_cast<UINT_PTR>(create.hDrvDevice.pDrvPrivate);
    presentation.hSurfaceToPresent = reinterpret_cast<UINT_PTR>(resource.pDrvPrivate);
    presentation.pDXGIContext = &dxgiCookie; presentation.Flags.Blt = 1;
    CHECK(dxgiFunctions.pfnPresent(&presentation) == S_OK && presents == 1);
    CHECK(!std::memcmp(pixels, publishedPixels, sizeof(pixels)));
    table.pfnCreateResource(create.hDrvDevice, &desc, resource, {&resourceCookie});
    CHECK(errors == 1 && allocations == 1); // Duplicate storage stays intact.
    deallocationResult = D3DDDIERR_DEVICEREMOVED;
    destroyResourceOnDeallocate = true; destroyResourceOnError = true;
    table.pfnDestroyResource(create.hDrvDevice, resource);
    CHECK(deallocations == 1 && errors == 2);
    for (SIZE_T i = 0; i < resourceBytes; ++i)
      CHECK(static_cast<unsigned char*>(resource.pDrvPrivate)[i] == 0xcc);
    table.pfnDestroyResource(create.hDrvDevice, resource);
    CHECK(deallocations == 1 && errors == 2);
    deallocationResult = S_OK;

    // Failed creation receives no DestroyResource. Its staged backend owner,
    // successful malformed allocation and storage reservation must all unwind.
    zeroAllocation = true;
    table.pfnCreateResource(create.hDrvDevice, &desc, resource, {&resourceCookie});
    CHECK(allocations == 2 && deallocations == 2 && errors == 3);
    for (SIZE_T i = 0; i < resourceBytes; ++i)
      CHECK(static_cast<unsigned char*>(resource.pDrvPrivate)[i] == 0xcc);
    zeroAllocation = false;
    allocationResult = E_OUTOFMEMORY;
    table.pfnCreateResource(create.hDrvDevice, &desc, resource, {&resourceCookie});
    CHECK(allocations == 3 && deallocations == 2 && errors == 4);
    allocationResult = S_OK;
    cancelResourceCreation = true;
    table.pfnCreateResource(create.hDrvDevice, &desc, resource, {&resourceCookie});
    CHECK(allocations == 4 && deallocations == 3 && errors == 5);
    for (SIZE_T i = 0; i < resourceBytes; ++i)
      CHECK(static_cast<unsigned char*>(resource.pDrvPrivate)[i] == 0xcc);
    // Reuse the same storage after all three failure modes without a destroy.
    table.pfnCreateResource(create.hDrvDevice, &desc, resource, {&resourceCookie});
    CHECK(allocations == 5 && deallocations == 3 && errors == 5);
    CHECK(dxgiFunctions.pfnPresent(&presentation) == S_OK && presents == 2);
    table.pfnDestroyResource(create.hDrvDevice, resource);
    CHECK(deallocations == 4 && errors == 5);
    destroyOnError = true;
    table.pfnDestroyDevice(create.hDrvDevice);
    CHECK(errors == 6 && contextDestroys == 1);
    table.pfnDestroyDevice(create.hDrvDevice); // Reentry already destroyed it.
    core.pfnSetErrorCb = error; kernel = savedKernel; dxgi = savedDxgi;
    openAdapter();
    table = {}; backendAction = Action::Reset;
    CHECK(functions.pfnCreateDevice(active, &create) == DXGI_ERROR_DEVICE_REMOVED && !table.pfnDestroyDevice);
    CHECK(functions.pfnCloseAdapter(active) == S_OK);
    openAdapter(); backendAction = Action::Close;
    CHECK(functions.pfnCreateDevice(active, &create) == DXGI_ERROR_DEVICE_REMOVED && !table.pfnDestroyDevice);
    openAdapter();
    CHECK(functions.pfnCreateDevice(active, &create) == S_OK);
    table.pfnDestroyDevice(create.hDrvDevice);
  }

  ++generation;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == DXGI_ERROR_DEVICE_REMOVED);
  --generation;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(functions.pfnCloseAdapter(active) == S_OK);
  openAdapter(); queryAction = Action::Close;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == E_INVALIDARG);
  openAdapter(); ++expected.LowPart;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(functions.pfnCloseAdapter(active) == S_OK);
  openAdapter(); ++capabilities;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(functions.pfnCloseAdapter(active) == S_OK);
  openAdapter(); queryResult = S_FALSE;
  CHECK(functions.pfnGetSupportedVersions(active, &count, nullptr) == E_FAIL);
  CHECK(functions.pfnCloseAdapter(active) == S_OK);
  queryResult = S_OK; generation = 0;
  D3D10DDIARG_OPENADAPTER zero = {};
  D3DDDI_ADAPTERCALLBACKS callback = {}; callback.pfnQueryAdapterInfoCb = query;
  zero.pAdapterFuncs_2 = &functions; zero.pAdapterCallbacks = &callback;
  zero.hRTAdapter.handle = &adapterCookie;
  CHECK(OpenAdapter10_2(&zero) == DXGI_ERROR_UNSUPPORTED && !zero.hAdapter.pDrvPrivate);
  std::printf("native production entry/lifetime PASS checks=%u complete-contract-fixture=%d backend-calls=%u; controlled WARP backend, not system UMD activation\n", checks, complete, backends);
}
