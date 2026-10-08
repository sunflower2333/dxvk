#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_contract.h"
#include "../src/umd/umd_runtime_identity.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <vector>

static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { \
  std::fprintf(stderr, "adapter check %u failed at line %d: %s\n", checks, __LINE__, #condition); \
  std::exit(1); } } while (0)

static char adapterCookie, deviceCookie, coreCookie, privateCookie;
static unsigned queryCalls, deviceCalls, destroyCalls;
static bool oldReply;
static HRESULT queryResult = S_OK, deviceResult = S_OK;
static bool throwAllocation;
static uint64_t generation = 7;
static UINT expectedInterface = D3D10_0_DDI_INTERFACE_VERSION;
static UINT expectedVersion = D3D10_0_DDI_BUILD_VERSION << 16;
static UINT expectedFlags;
static std::function<void()> queryHook, backendHook;
static D3DDDI_DEVICECALLBACKS expectedKernel = {};
static bool verifyKernel;
static const D3D10DDI_CORELAYER_DEVICECALLBACKS* expectedCore10;
static PFND3D10DDI_SETERROR_CB expectedError10;
static const D3D11DDI_CORELAYER_DEVICECALLBACKS* expectedCore11;
static PFND3D10DDI_SETERROR_CB expectedError11;
static const DXGI_DDI_BASE_CALLBACKS* expectedDxgiCallbacks;
static std::vector<std::shared_ptr<const dxvk::umd::AdapterIdentity>> devices;
static LUID expectedLuid = {0x12345678, -77};

static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
  queryCalls++;
  CHECK(runtime == &adapterCookie);
  CHECK(args && args->pPrivateDriverData && args->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (size_t i = 0; i < 160; i++) CHECK(bytes[i] == 0);
  if (FAILED(queryResult)) return queryResult;
  auto hook = std::move(queryHook); queryHook = nullptr;
  if (hook) hook();
  auto set = [bytes](size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; i++) bytes[offset+i] = uint8_t(value >> (i*8));
  };
  set(0,0x504d5644); set(8,128);
  set(24, static_cast<uint32_t>(generation)); set(28, static_cast<uint32_t>(generation >> 32));
  if (!oldReply) {
    set(128,0x44494c56); set(132,1); set(136,32); set(140,1); set(152,1);
    std::memcpy(bytes+144, &expectedLuid, sizeof(expectedLuid));
  }
  return S_OK;
}

static void APIENTRY setError(D3D10DDI_HRTCORELAYER, HRESULT) { CHECK(false); }
static void APIENTRY secondError(D3D10DDI_HRTCORELAYER, HRESULT) { CHECK(false); }
static void APIENTRY destroyStub(D3D10DDI_HDEVICE handle) {
  CHECK(handle.pDrvPrivate == &privateCookie);
  ++destroyCalls;
  CHECK(!devices.empty()); devices.pop_back();
}
static HRESULT APIENTRY rotateStub(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES*) { return E_NOTIMPL; }
static HRESULT APIENTRY resolveStub(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE*) { return E_NOTIMPL; }
static HRESULT APIENTRY presentStub(HANDLE, DXGIDDICB_PRESENT*) { return E_NOTIMPL; }
static HRESULT APIENTRY secondPresent(HANDLE, DXGIDDICB_PRESENT*) { return E_NOTIMPL; }
static HRESULT APIENTRY allocateStub(HANDLE, D3DDDICB_ALLOCATE*) { return E_NOTIMPL; }
static HRESULT APIENTRY deallocateStub(HANDLE, const D3DDDICB_DEALLOCATE*) { return E_NOTIMPL; }
static HRESULT APIENTRY lockStub(HANDLE, D3DDDICB_LOCK*) { return E_NOTIMPL; }
static HRESULT APIENTRY unlockStub(HANDLE, const D3DDDICB_UNLOCK*) { return E_NOTIMPL; }
static HRESULT APIENTRY createContextStub(HANDLE, D3DDDICB_CREATECONTEXT*) { return E_NOTIMPL; }
static HRESULT APIENTRY destroyContextStub(HANDLE, const D3DDDICB_DESTROYCONTEXT*) { return E_NOTIMPL; }

// Linked only into this CPU test, in place of the Vulkan-backed device
// implementation. These hooks are never exported by the development DLL.
extern "C" SIZE_T APIENTRY VioGpuDxvkPrivateDeviceSize() { return 256; }
HRESULT dxvk::umd::createAdapterDevice(
    const std::shared_ptr<const AdapterIdentity>& identity, D3D10DDIARG_CREATEDEVICE* args) {
  deviceCalls++;
  CHECK(identity && std::memcmp(&identity->luid, &expectedLuid, sizeof(LUID)) == 0);
  CHECK(identity->runtime == &adapterCookie);
  CHECK(args->hRTDevice.handle == &deviceCookie);
  CHECK(args->hRTCoreLayer.handle == &coreCookie);
  CHECK(args->hDrvDevice.pDrvPrivate == &privateCookie);
  CHECK(args->Interface == expectedInterface && args->Version == expectedVersion && args->Flags == expectedFlags);
  if (dxvk::umd::nativeInterface(args->Interface) == dxvk::umd::NativeInterface::D3D11) {
    CHECK(args->p11UMCallbacks == expectedCore11);
    CHECK(args->p11UMCallbacks->pfnSetErrorCb == expectedError11);
  } else {
    CHECK(args->pUMCallbacks == expectedCore10);
    CHECK(args->pUMCallbacks->pfnSetErrorCb == expectedError10);
  }
  if (verifyKernel) {
    CHECK(args->pKTCallbacks->pfnAllocateCb == expectedKernel.pfnAllocateCb);
    CHECK(args->pKTCallbacks->pfnDeallocateCb == expectedKernel.pfnDeallocateCb);
    CHECK(args->pKTCallbacks->pfnLockCb == expectedKernel.pfnLockCb);
    CHECK(args->pKTCallbacks->pfnUnlockCb == expectedKernel.pfnUnlockCb);
    CHECK(args->pKTCallbacks->pfnCreateContextCb == expectedKernel.pfnCreateContextCb);
    CHECK(args->pKTCallbacks->pfnDestroyContextCb == expectedKernel.pfnDestroyContextCb);
    CHECK(args->pKTCallbacks->pfnEscapeCb == expectedKernel.pfnEscapeCb);
    CHECK(args->pKTCallbacks->pfnRenderCb == expectedKernel.pfnRenderCb);
  }
  CHECK(args->DXGIBaseDDI.pDXGIBaseCallbacks == expectedDxgiCallbacks);
  if (throwAllocation) throw std::bad_alloc();
  if (FAILED(deviceResult)) return deviceResult;
  devices.push_back(identity);
  switch (dxvk::umd::nativeInterface(args->Interface)) {
    case dxvk::umd::NativeInterface::D3D10: args->pDeviceFuncs->pfnDestroyDevice = destroyStub; break;
    case dxvk::umd::NativeInterface::D3D10_1: args->p10_1DeviceFuncs->pfnDestroyDevice = destroyStub; break;
    case dxvk::umd::NativeInterface::D3D11: args->p11DeviceFuncs->pfnDestroyDevice = destroyStub; break;
    default: CHECK(false);
  }
  if (dxvk::umd::nativeDxgiUses1_1(args->Interface, args->Version)) {
    if (args->DXGIBaseDDI.pDXGIDDIBaseFunctions2)
      args->DXGIBaseDDI.pDXGIDDIBaseFunctions2->pfnResolveSharedResource = resolveStub;
  } else if (args->DXGIBaseDDI.pDXGIDDIBaseFunctions)
    args->DXGIBaseDDI.pDXGIDDIBaseFunctions->pfnRotateResourceIdentities = rotateStub;
  auto hook = std::move(backendHook); backendHook = nullptr;
  if (hook) hook();
  return deviceResult;
}

// Actual Windows guard pages establish the selected ABI boundary. A table
// write or old-runtime input read into the following page faults immediately.
class GuardedBytes {
public:
  explicit GuardedBytes(SIZE_T bytes) : m_bytes(bytes) {
    SYSTEM_INFO info = {}; GetSystemInfo(&info);
    m_page = info.dwPageSize;
    CHECK(bytes && bytes <= m_page);
    m_storage = VirtualAlloc(nullptr, 2 * m_page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(m_storage);
    DWORD old = 0;
    CHECK(VirtualProtect(static_cast<char*>(m_storage) + m_page, m_page, PAGE_NOACCESS, &old));
    m_data = static_cast<char*>(m_storage) + m_page - bytes;
    std::memset(m_data, 0, bytes);
  }
  ~GuardedBytes() { CHECK(VirtualFree(m_storage, 0, MEM_RELEASE)); }
  GuardedBytes(const GuardedBytes&) = delete;
  GuardedBytes& operator=(const GuardedBytes&) = delete;
  template<typename T> T* as() {
    CHECK(reinterpret_cast<uintptr_t>(m_data) % alignof(T) == 0);
    return reinterpret_cast<T*>(m_data);
  }
  void* data() { return m_data; }
  void fill(unsigned char value) { std::memset(m_data, value, m_bytes); }
  std::vector<unsigned char> snapshot() const {
    const auto bytes = static_cast<const unsigned char*>(m_data);
    return {bytes, bytes + m_bytes};
  }
  bool matches(const std::vector<unsigned char>& bytes) const {
    return bytes.size() == m_bytes && !std::memcmp(m_data, bytes.data(), m_bytes);
  }
private:
  SIZE_T m_bytes, m_page = 0;
  void* m_storage = nullptr;
  void* m_data = nullptr;
};

static UINT buildFor(UINT interfaceVersion) {
  switch (dxvk::umd::nativeInterface(interfaceVersion)) {
    case dxvk::umd::NativeInterface::D3D10: return D3D10_0_DDI_BUILD_VERSION;
    case dxvk::umd::NativeInterface::D3D10_1: return D3D10_1_DDI_BUILD_VERSION;
    case dxvk::umd::NativeInterface::D3D11: return D3D11_0_DDI_BUILD_VERSION;
    default: CHECK(false); return 0;
  }
}

static void nativeInterfaces();
static void modernExportNegatives();
static void validationNegotiation();

int main() {
  CHECK(VioGpuDxvkOpenAdapterForTest(nullptr) == E_INVALIDARG);
  CHECK(VioGpuDxvkOpenAdapter10_2ForTest(nullptr) == E_INVALIDARG);
  D3D10DDI_ADAPTERFUNCS functions = {};
  D3DDDI_ADAPTERCALLBACKS callbacks = {};
  callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER open = {};
  open.pAdapterFuncs = &functions;
  open.pAdapterCallbacks = &callbacks;
  open.hRTAdapter.handle = &adapterCookie;
  open.Interface = D3D10_0_DDI_INTERFACE_VERSION;
  open.Version = D3D10_0_DDI_BUILD_VERSION << 16;
  auto bad = open;
  bad.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  CHECK(VioGpuDxvkOpenAdapterForTest(&bad) == DXGI_ERROR_UNSUPPORTED);
  CHECK(!bad.hAdapter.pDrvPrivate && !functions.pfnCreateDevice && queryCalls == 0);
  bad = open; bad.Version -= 1;
  CHECK(VioGpuDxvkOpenAdapterForTest(&bad) == DXGI_ERROR_UNSUPPORTED && queryCalls == 0);
  bad = open; bad.pAdapterCallbacks = nullptr;
  CHECK(VioGpuDxvkOpenAdapterForTest(&bad) == E_INVALIDARG && queryCalls == 0);
  bad = open; bad.hRTAdapter = {};
  CHECK(VioGpuDxvkOpenAdapterForTest(&bad) == E_INVALIDARG && queryCalls == 0);
  oldReply = true;
  CHECK(VioGpuDxvkOpenAdapterForTest(&open) == DXGI_ERROR_UNSUPPORTED);
  CHECK(!open.hAdapter.pDrvPrivate && !functions.pfnCreateDevice);
  oldReply = false; queryResult = DXGI_ERROR_DEVICE_REMOVED;
  CHECK(VioGpuDxvkOpenAdapterForTest(&open) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(!open.hAdapter.pDrvPrivate && !functions.pfnCreateDevice);
  queryResult = S_OK;
  CHECK(VioGpuDxvkOpenAdapterForTest(&open) == S_OK);
  CHECK(open.hAdapter.pDrvPrivate && functions.pfnCalcPrivateDeviceSize
      && functions.pfnCreateDevice && functions.pfnCloseAdapter);
  D3D10DDIARG_CALCPRIVATEDEVICESIZE size = {open.Interface, open.Version, 0};
  CHECK(functions.pfnCalcPrivateDeviceSize(open.hAdapter, &size) == 256);
  CHECK(functions.pfnCalcPrivateDeviceSize({}, &size) == 0);
  CHECK(functions.pfnCalcPrivateDeviceSize(open.hAdapter, nullptr) == 0);
  size.Flags = D3D10DDI_CREATEDEVICE_FLAG_DISABLE_EXTRA_THREAD_CREATION;
  CHECK(functions.pfnCalcPrivateDeviceSize(open.hAdapter, &size) == 0);
  size.Flags = 0; size.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  CHECK(functions.pfnCalcPrivateDeviceSize(open.hAdapter, &size) == 0);
  D3D10DDI_CORELAYER_DEVICECALLBACKS core = {};
  core.pfnSetErrorCb = setError;
  D3DDDI_DEVICECALLBACKS kernel = {};
  D3D10DDI_DEVICEFUNCS table = {};
  D3D10DDIARG_CREATEDEVICE create = {};
  create.Interface = open.Interface; create.Version = open.Version;
  create.hRTDevice.handle = &deviceCookie; create.pKTCallbacks = &kernel;
  create.hRTCoreLayer.handle = &coreCookie; create.pUMCallbacks = &core;
  expectedCore10 = &core; expectedError10 = setError;
  create.hDrvDevice.pDrvPrivate = &privateCookie; create.pDeviceFuncs = &table;
  CHECK(functions.pfnCreateDevice({}, &create) == E_INVALIDARG && deviceCalls == 0);
  CHECK(functions.pfnCreateDevice(open.hAdapter, nullptr) == E_INVALIDARG && deviceCalls == 0);
  auto invalid = create; invalid.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == DXGI_ERROR_UNSUPPORTED && deviceCalls == 0);
  invalid = create; invalid.Flags = D3D10DDI_CREATEDEVICE_FLAG_DISABLE_EXTRA_THREAD_CREATION;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == DXGI_ERROR_UNSUPPORTED && deviceCalls == 0);
  invalid = create; invalid.Version -= 1;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == DXGI_ERROR_UNSUPPORTED && deviceCalls == 0);
  invalid = create; invalid.hRTDevice = {};
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == E_INVALIDARG && deviceCalls == 0);
  invalid = create; invalid.hRTCoreLayer = {};
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == E_INVALIDARG && deviceCalls == 0);
  invalid = create; invalid.hDrvDevice = {};
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == E_INVALIDARG && deviceCalls == 0);
  invalid = create; invalid.pKTCallbacks = nullptr;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == E_INVALIDARG && deviceCalls == 0);
  invalid = create; invalid.pUMCallbacks = nullptr;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == E_INVALIDARG && deviceCalls == 0);
  invalid = create; invalid.pDeviceFuncs = nullptr;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &invalid) == E_INVALIDARG && deviceCalls == 0);
  core.pfnSetErrorCb = nullptr;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &create) == E_INVALIDARG && deviceCalls == 0);
  core.pfnSetErrorCb = setError;
  deviceResult = DXGI_ERROR_UNSUPPORTED;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &create) == DXGI_ERROR_UNSUPPORTED);
  CHECK(devices.empty() && !table.pfnDestroyDevice);
  throwAllocation = true;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &create) == E_OUTOFMEMORY);
  CHECK(devices.empty() && !table.pfnDestroyDevice);
  throwAllocation = false; deviceResult = S_OK;
  CHECK(functions.pfnCreateDevice(open.hAdapter, &create) == S_OK && devices.size() == 1);
  CHECK(table.pfnDestroyDevice == destroyStub);
  CHECK(functions.pfnCreateDevice(open.hAdapter, &create) == S_OK && devices.size() == 2);
  std::weak_ptr<const dxvk::umd::AdapterIdentity> lifetime = devices.front();
  CHECK(functions.pfnCloseAdapter({}) == E_INVALIDARG);
  CHECK(functions.pfnCloseAdapter(open.hAdapter) == S_OK);
  CHECK(!lifetime.expired());
  devices.pop_back(); CHECK(!lifetime.expired());
  devices.clear(); CHECK(lifetime.expired());
  // Runtime revision is not a driver-build discriminator; higher builds are
  // compatible within this exact DDI interface.
  open.Version += (1 << 16) | 0x1234;
  CHECK(VioGpuDxvkOpenAdapterForTest(&open) == S_OK);
  CHECK(functions.pfnCloseAdapter(open.hAdapter) == S_OK);
  modernExportNegatives();
  nativeInterfaces();
  validationNegotiation();
  std::printf("adapter lifecycle PASS checks=%u; mock runtime and backend, no GPU\n", checks);
}

static D3D10DDI_HADAPTER openModern(D3D10_2DDI_ADAPTERFUNCS& functions, bool validation = false) {
  D3DDDI_ADAPTERCALLBACKS callbacks = {};
  callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER args = {};
  args.hRTAdapter.handle = &adapterCookie;
  args.Interface = args.Version = 0xffffffff;
  args.pAdapterCallbacks = &callbacks;
  args.pAdapterFuncs_2 = &functions;
  CHECK((validation ? VioGpuDxvkOpenAdapter11Fl10_0ForValidation(&args)
    : VioGpuDxvkOpenAdapter10_2ForTest(&args)) == S_OK);
  CHECK(args.hAdapter.pDrvPrivate && functions.pfnGetSupportedVersions && functions.pfnGetCaps);
  return args.hAdapter;
}

static HRESULT APIENTRY positiveQuery(HANDLE, const D3DDDICB_QUERYADAPTERINFO*) {
  ++queryCalls;
  return S_FALSE;
}

static void modernExportNegatives() {
  GuardedBytes adapterTable(sizeof(D3D10_2DDI_ADAPTERFUNCS));
  GuardedBytes openStorage(sizeof(D3D10DDIARG_OPENADAPTER));
  auto functions = adapterTable.as<D3D10_2DDI_ADAPTERFUNCS>();
  auto open = openStorage.as<D3D10DDIARG_OPENADAPTER>();
  D3DDDI_ADAPTERCALLBACKS callbacks = {}; callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER input = {};
  input.hRTAdapter.handle = &adapterCookie; input.pAdapterCallbacks = &callbacks;
  input.pAdapterFuncs_2 = functions; input.Interface = input.Version = 0xffffffff;
  const std::vector<unsigned char> zeroTable(sizeof(D3D10_2DDI_ADAPTERFUNCS), 0);
  const unsigned beforeQueries = queryCalls;
  for (unsigned invalid = 0; invalid < 4; ++invalid) {
    *open = input; adapterTable.fill(0xa5);
    if (invalid == 0) open->pAdapterFuncs_2 = nullptr;
    if (invalid == 1) open->pAdapterCallbacks = nullptr;
    if (invalid == 2) open->hRTAdapter = {};
    if (invalid == 3) callbacks.pfnQueryAdapterInfoCb = nullptr;
    CHECK(VioGpuDxvkOpenAdapter10_2ForTest(open) == E_INVALIDARG && !open->hAdapter.pDrvPrivate);
    CHECK(queryCalls == beforeQueries);
    if (invalid) CHECK(adapterTable.snapshot() == zeroTable);
    callbacks.pfnQueryAdapterInfoCb = query;
  }
  *open = input; callbacks.pfnQueryAdapterInfoCb = positiveQuery; adapterTable.fill(0xa5);
  CHECK(VioGpuDxvkOpenAdapter10_2ForTest(open) == E_FAIL && !open->hAdapter.pDrvPrivate);
  CHECK(queryCalls == beforeQueries + 1 && adapterTable.snapshot() == zeroTable);
  callbacks.pfnQueryAdapterInfoCb = query;
  *open = input;
  CHECK(VioGpuDxvkOpenAdapter10_2ForTest(open) == S_OK);
  CHECK(functions->pfnGetSupportedVersions && functions->pfnGetCaps && functions->pfnCreateDevice);

  GuardedBytes table(sizeof(D3D11DDI_DEVICEFUNCS)); table.fill(0x5a);
  GuardedBytes dxgi(sizeof(DXGI1_1_DDI_BASE_FUNCTIONS)); dxgi.fill(0x3c);
  const auto tableBefore = table.snapshot(), dxgiBefore = dxgi.snapshot();
  D3D11DDI_CORELAYER_DEVICECALLBACKS core = {}; core.pfnSetErrorCb = setError;
  D3DDDI_DEVICECALLBACKS kernel = {};
  D3D10DDIARG_CREATEDEVICE create = {};
  create.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  create.Version = D3D11_0_DDI_BUILD_VERSION << 16;
  create.Flags = 4; // Exact D3D11DDI_3DPIPELINELEVEL_11_0 encoding.
  create.hRTDevice.handle = &deviceCookie; create.hRTCoreLayer.handle = &coreCookie;
  create.hDrvDevice.pDrvPrivate = &privateCookie; create.pKTCallbacks = &kernel;
  create.p11UMCallbacks = &core; create.p11DeviceFuncs = table.as<D3D11DDI_DEVICEFUNCS>();
  create.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = dxgi.as<DXGI1_1_DDI_BASE_FUNCTIONS>();
  const unsigned beforeDevices = deviceCalls, beforeInvalidQueries = queryCalls;
  for (unsigned invalid = 0; invalid < 11; ++invalid) {
    auto request = create;
    if (invalid == 0) request.Interface = D3D11_1_DDI_INTERFACE_VERSION;
    if (invalid == 1) --request.Version;
    if (invalid == 2) request.Flags |= D3D10DDI_CREATEDEVICE_FLAG_DISABLE_EXTRA_THREAD_CREATION;
    if (invalid == 3) request.Flags |= 0x80000000;
    if (invalid == 4) request.hRTDevice = {};
    if (invalid == 5) request.hRTCoreLayer = {};
    if (invalid == 6) request.hDrvDevice = {};
    if (invalid == 7) request.pKTCallbacks = nullptr;
    if (invalid == 8) request.p11UMCallbacks = nullptr;
    if (invalid == 9) core.pfnSetErrorCb = nullptr;
    if (invalid == 10) request.p11DeviceFuncs = nullptr;
    CHECK(functions->pfnCreateDevice(open->hAdapter, &request)
      == (invalid < 4 ? DXGI_ERROR_UNSUPPORTED : E_INVALIDARG));
    core.pfnSetErrorCb = setError;
    CHECK(table.matches(tableBefore) && dxgi.matches(dxgiBefore));
    CHECK(deviceCalls == beforeDevices && queryCalls == beforeInvalidQueries);
  }
  CHECK(functions->pfnCloseAdapter(open->hAdapter) == S_OK);
}

enum class CreationCase {
  Success, BackendFailure, AllocationFailure, PositiveStatus,
  CloseBeforeBackend, CloseAfterBackend, ResetAfterBackend,
  QueryFailureAfterBackend, QueryExceptionAfterBackend
};

static void creationFixture(UINT interfaceVersion, UINT version, UINT flags, CreationCase action,
    bool validation = false) {
  D3D10_2DDI_ADAPTERFUNCS functions = {};
  auto adapter = openModern(functions, validation);
  const auto selected = dxvk::umd::nativeInterface(interfaceVersion);
  const bool newerDxgi = dxvk::umd::nativeDxgiUses1_1(interfaceVersion, version);
  SIZE_T tableBytes = sizeof(D3D10DDI_DEVICEFUNCS);
  if (selected == dxvk::umd::NativeInterface::D3D10_1) tableBytes = sizeof(D3D10_1DDI_DEVICEFUNCS);
  if (selected == dxvk::umd::NativeInterface::D3D11) tableBytes = sizeof(D3D11DDI_DEVICEFUNCS);
  GuardedBytes table(tableBytes);
  GuardedBytes dxgi(newerDxgi ? sizeof(DXGI1_1_DDI_BASE_FUNCTIONS) : sizeof(DXGI_DDI_BASE_FUNCTIONS));
  table.fill(0xa5); dxgi.fill(0x3c);
  const auto beforeTable = table.snapshot(), beforeDxgi = dxgi.snapshot();
  // ppfnRetrieveSubObject was introduced in D3D11.1. Its address must never be
  // read through an older creation contract, even in a current-SDK build.
  const SIZE_T createBytes = offsetof(D3D10DDIARG_CREATEDEVICE, ppfnRetrieveSubObject);
  GuardedBytes createStorage(createBytes);
  auto create = createStorage.as<D3D10DDIARG_CREATEDEVICE>();
  GuardedBytes coreStorage(sizeof(PFND3D10DDI_SETERROR_CB));
  GuardedBytes dxgiCallbackStorage(sizeof(PFNDDXGIDDI_PRESENTCB));
  auto dxgiCallbacks = dxgiCallbackStorage.as<DXGI_DDI_BASE_CALLBACKS>();
  dxgiCallbacks->pfnPresentCb = presentStub;
  expectedDxgiCallbacks = dxgiCallbacks;
  expectedKernel = {};
  expectedKernel.pfnAllocateCb = allocateStub; expectedKernel.pfnDeallocateCb = deallocateStub;
  expectedKernel.pfnLockCb = lockStub; expectedKernel.pfnUnlockCb = unlockStub;
  expectedKernel.pfnCreateContextCb = createContextStub;
  expectedKernel.pfnDestroyContextCb = destroyContextStub;
  const SIZE_T kernelBytes = offsetof(D3DDDI_DEVICECALLBACKS, pfnDestroyContextCb)
    + sizeof(expectedKernel.pfnDestroyContextCb);
  // All eight callbacks read by this adapter are in the historical prefix.
  static_assert(offsetof(D3DDDI_DEVICECALLBACKS, pfnRenderCb) < kernelBytes,
    "RenderCb must fit the historical callback prefix");
  static_assert(offsetof(D3DDDI_DEVICECALLBACKS, pfnEscapeCb) < kernelBytes,
    "EscapeCb must fit the historical callback prefix");
  GuardedBytes kernelStorage(kernelBytes);
  std::memcpy(kernelStorage.data(), &expectedKernel, kernelBytes);
  create->Interface = expectedInterface = interfaceVersion;
  create->Version = expectedVersion = version; create->Flags = expectedFlags = flags;
  create->hDrvDevice.pDrvPrivate = &privateCookie;
  create->hRTDevice.handle = &deviceCookie; create->hRTCoreLayer.handle = &coreCookie;
  create->pKTCallbacks = kernelStorage.as<D3DDDI_DEVICECALLBACKS>();
  create->DXGIBaseDDI.pDXGIBaseCallbacks = dxgiCallbacks;
  if (newerDxgi) create->DXGIBaseDDI.pDXGIDDIBaseFunctions2 = dxgi.as<DXGI1_1_DDI_BASE_FUNCTIONS>();
  else create->DXGIBaseDDI.pDXGIDDIBaseFunctions = dxgi.as<DXGI_DDI_BASE_FUNCTIONS>();
  if (selected == dxvk::umd::NativeInterface::D3D11) {
    auto core = coreStorage.as<D3D11DDI_CORELAYER_DEVICECALLBACKS>();
    core->pfnSetErrorCb = setError;
    create->p11UMCallbacks = expectedCore11 = core;
    expectedError11 = setError;
    create->p11DeviceFuncs = table.as<D3D11DDI_DEVICEFUNCS>();
  } else {
    auto core = coreStorage.as<D3D10DDI_CORELAYER_DEVICECALLBACKS>();
    core->pfnSetErrorCb = setError; create->pUMCallbacks = expectedCore10 = core;
    expectedError10 = setError;
    if (selected == dxvk::umd::NativeInterface::D3D10_1)
      create->p10_1DeviceFuncs = table.as<D3D10_1DDI_DEVICEFUNCS>();
    else create->pDeviceFuncs = table.as<D3D10DDI_DEVICEFUNCS>();
  }
  verifyKernel = true;
  const auto beforeCalls = deviceCalls, beforeDestroys = destroyCalls;
  HRESULT expected = S_OK;
  bool closed = false;
  switch (action) {
    case CreationCase::Success:
      queryHook = [&] {
        // Replace every caller argument and the callback storage during the
        // first identity query. Original output and ownership must survive.
        std::memset(create, 0, createBytes);
        std::memset(kernelStorage.data(), 0, kernelBytes);
        if (selected == dxvk::umd::NativeInterface::D3D11) {
          coreStorage.as<D3D11DDI_CORELAYER_DEVICECALLBACKS>()->pfnSetErrorCb = secondError;
          expectedError11 = secondError;
        } else {
          coreStorage.as<D3D10DDI_CORELAYER_DEVICECALLBACKS>()->pfnSetErrorCb = secondError;
          expectedError10 = secondError;
        }
        dxgiCallbacks->pfnPresentCb = secondPresent;
      };
      break;
    case CreationCase::BackendFailure: deviceResult = DXGI_ERROR_UNSUPPORTED; expected = deviceResult; break;
    case CreationCase::AllocationFailure: throwAllocation = true; expected = E_OUTOFMEMORY; break;
    case CreationCase::PositiveStatus: deviceResult = S_FALSE; expected = E_FAIL; break;
    case CreationCase::CloseBeforeBackend:
      queryHook = [&] { CHECK(functions.pfnCloseAdapter(adapter) == S_OK); closed = true; };
      expected = DXGI_ERROR_DEVICE_REMOVED; break;
    case CreationCase::CloseAfterBackend:
      backendHook = [&] { CHECK(functions.pfnCloseAdapter(adapter) == S_OK); closed = true; };
      expected = DXGI_ERROR_DEVICE_REMOVED; break;
    case CreationCase::ResetAfterBackend:
      backendHook = [] { ++generation; }; expected = DXGI_ERROR_DEVICE_REMOVED; break;
    case CreationCase::QueryFailureAfterBackend:
      backendHook = [] { queryResult = DXGI_ERROR_DEVICE_REMOVED; };
      expected = DXGI_ERROR_DEVICE_REMOVED; break;
    case CreationCase::QueryExceptionAfterBackend:
      backendHook = [] { queryHook = [] { throw std::bad_alloc(); }; };
      expected = E_OUTOFMEMORY; break;
  }
  CHECK(functions.pfnCreateDevice(adapter, create) == expected);
  CHECK(deviceCalls == beforeCalls + (action == CreationCase::CloseBeforeBackend ? 0u : 1u));
  if (action == CreationCase::Success) {
    CHECK(devices.size() == 1);
    PFND3D10DDI_DESTROYDEVICE destroy = nullptr;
    switch (selected) {
      case dxvk::umd::NativeInterface::D3D10: destroy = table.as<D3D10DDI_DEVICEFUNCS>()->pfnDestroyDevice; break;
      case dxvk::umd::NativeInterface::D3D10_1: destroy = table.as<D3D10_1DDI_DEVICEFUNCS>()->pfnDestroyDevice; break;
      case dxvk::umd::NativeInterface::D3D11: destroy = table.as<D3D11DDI_DEVICEFUNCS>()->pfnDestroyDevice; break;
      default: CHECK(false);
    }
    CHECK(destroy == destroyStub && destroyCalls == beforeDestroys);
    if (newerDxgi) CHECK(dxgi.as<DXGI1_1_DDI_BASE_FUNCTIONS>()->pfnResolveSharedResource == resolveStub);
    else CHECK(dxgi.as<DXGI_DDI_BASE_FUNCTIONS>()->pfnRotateResourceIdentities == rotateStub);
    destroy({&privateCookie});
  } else {
    CHECK(table.matches(beforeTable) && dxgi.matches(beforeDxgi));
    const bool constructed = action == CreationCase::PositiveStatus || action == CreationCase::CloseAfterBackend
      || action == CreationCase::ResetAfterBackend || action == CreationCase::QueryFailureAfterBackend
      || action == CreationCase::QueryExceptionAfterBackend;
    CHECK(destroyCalls == beforeDestroys + (constructed ? 1u : 0u));
  }
  CHECK(devices.empty());
  queryResult = deviceResult = S_OK; throwAllocation = false;
  queryHook = backendHook = nullptr; verifyKernel = false; expectedDxgiCallbacks = nullptr;
  CHECK(functions.pfnCloseAdapter(adapter) == (closed ? E_INVALIDARG : S_OK));
  // A later adapter allocation must never make this closed token valid again.
  const auto replacement = openModern(functions, validation);
  CHECK(replacement.pDrvPrivate != adapter.pDrvPrivate);
  CHECK(functions.pfnCloseAdapter(adapter) == E_INVALIDARG);
  CHECK(functions.pfnCloseAdapter(replacement) == S_OK);
}

static void nativeInterfaces() {
  const UINT interfaces[] = {D3D10_0_DDI_INTERFACE_VERSION, D3D10_1_DDI_INTERFACE_VERSION,
    D3D11_0_DDI_INTERFACE_VERSION};
  for (UINT interfaceVersion : interfaces) {
    const UINT build = buildFor(interfaceVersion);
    CHECK(!dxvk::umd::supportedNativeInterface(interfaceVersion, ((build - 1) << 16) | 0xffff));
    CHECK(dxvk::umd::supportedNativeInterface(interfaceVersion, ((build + 1) << 16) | 0xffff));
    for (UINT flags = 0; flags < 256; ++flags) {
      const bool expected = interfaceVersion == D3D11_0_DDI_INTERFACE_VERSION
        ? flags == 0 || flags == 2 || flags == 4 || flags == 0x10 || flags == 0x12 || flags == 0x14
        : flags == 0;
      CHECK(dxvk::umd::supportedNativeInterface(interfaceVersion, build << 16, flags) == expected);
    }
    CHECK(!dxvk::umd::runtimeSupportsNativeInterface(interfaceVersion));
    for (UINT revision : {0u, 0xffffu}) {
      const UINT version = (build << 16) | revision;
      CHECK(dxvk::umd::nativeDxgiUses1_1(interfaceVersion, version) == (revision != 0));
      const UINT flags[] = {0, 0x10, 2, 0x12, 4, 0x14};
      for (UINT flag : flags) {
        if (interfaceVersion != D3D11_0_DDI_INTERFACE_VERSION && flag) continue;
        for (CreationCase action : {CreationCase::Success, CreationCase::BackendFailure,
          CreationCase::AllocationFailure, CreationCase::PositiveStatus, CreationCase::CloseBeforeBackend,
          CreationCase::CloseAfterBackend, CreationCase::ResetAfterBackend,
          CreationCase::QueryFailureAfterBackend, CreationCase::QueryExceptionAfterBackend})
          creationFixture(interfaceVersion, version, flag, action);
      }
    }
  }
  CHECK(dxvk::umd::nativeFeatureLevel(D3D10_1_DDI_INTERFACE_VERSION) == D3D_FEATURE_LEVEL_10_1);
  CHECK(dxvk::umd::nativeFeatureLevel(D3D11_0_DDI_INTERFACE_VERSION, 4) == D3D_FEATURE_LEVEL_11_0);
  CHECK(!dxvk::umd::supportedNativeInterface(D3D10_0_x_DDI_INTERFACE_VERSION, 0xffffffff));
  CHECK(!dxvk::umd::supportedNativeInterface(D3D11_1_DDI_INTERFACE_VERSION, 0xffffffff));

  GuardedBytes adapterTable(sizeof(D3D10_2DDI_ADAPTERFUNCS));
  auto functions = adapterTable.as<D3D10_2DDI_ADAPTERFUNCS>();
  D3D10_2DDI_ADAPTERFUNCS alternate = {};
  D3DDDI_ADAPTERCALLBACKS callbacks = {}; callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER open = {};
  open.hRTAdapter.handle = &adapterCookie; open.pAdapterCallbacks = &callbacks;
  open.pAdapterFuncs_2 = functions; open.Interface = open.Version = 0xffffffff;
  queryHook = [&] {
    open.pAdapterFuncs_2 = &alternate; open.hRTAdapter.handle = &deviceCookie;
    open.pAdapterCallbacks = nullptr; callbacks.pfnQueryAdapterInfoCb = nullptr;
  };
  CHECK(VioGpuDxvkOpenAdapter10_2ForTest(&open) == S_OK);
  CHECK(functions->pfnCreateDevice && !alternate.pfnCreateDevice);
  UINT32 count = 0;
  GuardedBytes versionStorage(3 * sizeof(UINT64)); versionStorage.fill(0xa5);
  auto versions = versionStorage.as<UINT64>(); const auto versionBefore = versionStorage.snapshot();
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, nullptr) == S_OK && count == 3);
  count = 0;
  queryHook = [&] { count = UINT32_MAX; };
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, versions) == E_OUTOFMEMORY);
  CHECK(count == 3 && versionStorage.matches(versionBefore));
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, versions) == S_OK);
  CHECK(versions[0] == D3D10_0_DDI_SUPPORTED && versions[1] == D3D10_1_DDI_SUPPORTED
    && versions[2] == D3D11_0_DDI_SUPPORTED);
  GuardedBytes capsStorage(sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS)); capsStorage.fill(0xa5);
  UINT alternateCaps = 0x12345678;
  D3D10_2DDIARG_GETCAPS caps = {};
  caps.Type = D3D11DDICAPS_3DPIPELINESUPPORT; caps.pData = capsStorage.data();
  caps.DataSize = sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS);
  queryHook = [&] { caps.pData = &alternateCaps; caps.DataSize = 1; caps.Type = D3D11DDICAPS_SHADER; };
  CHECK(functions->pfnGetCaps(open.hAdapter, &caps) == S_OK);
  CHECK(capsStorage.as<D3D11DDI_3DPIPELINESUPPORT_CAPS>()->Caps == 0 && alternateCaps == 0x12345678);
  count = 77;
  queryHook = [&] {
    UINT32 nested = 99;
    CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &nested, nullptr) == DXGI_ERROR_WAS_STILL_DRAWING);
    CHECK(nested == 99);
  };
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, nullptr) == S_OK && count == 3);
  count = 77;
  queryHook = [&] { CHECK(functions->pfnCloseAdapter(open.hAdapter) == S_OK); };
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, nullptr) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(count == 77);
  const auto capsAdapter = openModern(*functions);
  caps.Type = D3D11DDICAPS_3DPIPELINESUPPORT;
  caps.pData = capsStorage.data(); caps.DataSize = sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS);
  capsStorage.fill(0xa5); const auto capsBefore = capsStorage.snapshot();
  queryHook = [&] { CHECK(functions->pfnCloseAdapter(capsAdapter) == S_OK); };
  CHECK(functions->pfnGetCaps(capsAdapter, &caps) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(capsStorage.matches(capsBefore));

  GuardedBytes legacyTable(sizeof(D3D10DDI_ADAPTERFUNCS));
  auto legacy = legacyTable.as<D3D10DDI_ADAPTERFUNCS>();
  callbacks.pfnQueryAdapterInfoCb = query;
  open = {}; open.hRTAdapter.handle = &adapterCookie; open.pAdapterCallbacks = &callbacks;
  open.pAdapterFuncs = legacy; open.Interface = D3D10_1_DDI_INTERFACE_VERSION;
  open.Version = D3D10_1_DDI_BUILD_VERSION << 16;
  CHECK(VioGpuDxvkOpenAdapterForTest(&open) == S_OK);
  D3D10DDIARG_CALCPRIVATEDEVICESIZE size = {open.Interface, open.Version, 0};
  CHECK(legacy->pfnCalcPrivateDeviceSize(open.hAdapter, &size) == 256);
  queryHook = [&] { size.Interface = size.Version = size.Flags = 0xffffffff; };
  CHECK(legacy->pfnCalcPrivateDeviceSize(open.hAdapter, &size) == 256);
  CHECK(size.Interface == 0xffffffff && size.Version == 0xffffffff && size.Flags == 0xffffffff);
  size.Flags = 0;
  size.Interface = D3D10_0_DDI_INTERFACE_VERSION; size.Version = D3D10_0_DDI_BUILD_VERSION << 16;
  CHECK(legacy->pfnCalcPrivateDeviceSize(open.hAdapter, &size) == 0);
  CHECK(legacy->pfnCloseAdapter(open.hAdapter) == S_OK);
  const auto beforeQueries = queryCalls;
  CHECK(OpenAdapter10(&open) == DXGI_ERROR_UNSUPPORTED && queryCalls == beforeQueries);
  CHECK(!open.hAdapter.pDrvPrivate && !legacy->pfnCreateDevice);
  open.pAdapterFuncs_2 = functions; open.Interface = open.Version = 0xffffffff;
  CHECK(OpenAdapter10_2(&open) == S_OK);
  count = 3; versionStorage.fill(0xa5);
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, versions) == S_OK && count == 0);
  CHECK(versionStorage.matches(versionBefore));
  CHECK(functions->pfnCloseAdapter(open.hAdapter) == S_OK);
}

static void validationNegotiation() {
  static_assert(D3D11_0_DDI_SUPPORTED == UINT64(0x000b000a00020000),
    "Validation selects the documented exact D3D11.0 build2 interface");
  static_assert(D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_0) == 1,
    "Validation publishes only the documented FL10.0 pipeline bit");
  static_assert(D3D11DDI_CREATEDEVICE_FLAG_SINGLETHREADED == 0x10,
    "Logical FL10.0 accepts only the caller's SINGLETHREADED flag");
  CHECK(VioGpuDxvkOpenAdapter11Fl10_0ForValidation(nullptr) == E_INVALIDARG);
  GuardedBytes adapterTable(sizeof(D3D10_2DDI_ADAPTERFUNCS));
  auto functions = adapterTable.as<D3D10_2DDI_ADAPTERFUNCS>();
  D3DDDI_ADAPTERCALLBACKS callbacks = {}; callbacks.pfnQueryAdapterInfoCb = query;
  D3D10DDIARG_OPENADAPTER open = {};
  open.pAdapterFuncs_2 = functions; open.pAdapterCallbacks = &callbacks;
  open.hRTAdapter.handle = &adapterCookie; open.Interface = open.Version = UINT32_MAX;
  const std::vector<unsigned char> zeroAdapter(sizeof(D3D10_2DDI_ADAPTERFUNCS), 0);
  for (unsigned invalid = 0; invalid < 4; ++invalid) {
    auto request = open; adapterTable.fill(0xa5);
    if (invalid == 0) request.pAdapterCallbacks = nullptr;
    if (invalid == 1) request.hRTAdapter = {};
    if (invalid == 2) callbacks.pfnQueryAdapterInfoCb = nullptr;
    if (invalid == 3) oldReply = true;
    const auto before = queryCalls;
    CHECK(VioGpuDxvkOpenAdapter11Fl10_0ForValidation(&request)
      == (invalid == 3 ? DXGI_ERROR_UNSUPPORTED : E_INVALIDARG));
    CHECK(!request.hAdapter.pDrvPrivate && adapterTable.snapshot() == zeroAdapter);
    CHECK(queryCalls == before + (invalid == 3 ? 1u : 0u));
    callbacks.pfnQueryAdapterInfoCb = query; oldReply = false;
  }
  CHECK(VioGpuDxvkOpenAdapter11Fl10_0ForValidation(&open) == S_OK);
  UINT32 count = 77;
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, nullptr) == S_OK && count == 1);
  GuardedBytes versions(sizeof(UINT64)); versions.fill(0xa5);
  const auto versionBefore = versions.snapshot();
  count = 0;
  queryHook = [&] { count = UINT32_MAX; };
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, versions.as<UINT64>()) == E_OUTOFMEMORY);
  CHECK(count == 1 && versions.matches(versionBefore));
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, versions.as<UINT64>()) == S_OK);
  CHECK(count == 1 && *versions.as<UINT64>() == D3D11_0_DDI_SUPPORTED);

  GuardedBytes capsStorage(sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS));
  D3D10_2DDIARG_GETCAPS caps = {};
  caps.pData = capsStorage.data(); caps.DataSize = sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS);
  for (auto type : {D3D11DDICAPS_3DPIPELINESUPPORT, D3D11DDICAPS_THREADING, D3D11DDICAPS_SHADER}) {
    caps.Type = type; capsStorage.fill(0xa5);
    CHECK(functions->pfnGetCaps(open.hAdapter, &caps) == S_OK);
    UINT bits = UINT32_MAX; std::memcpy(&bits, capsStorage.data(), sizeof(bits));
    CHECK(bits == (type == D3D11DDICAPS_3DPIPELINESUPPORT ? 1u : 0u));
  }
  caps.Type = D3D11DDICAPS_3DPIPELINESUPPORT;
  for (unsigned invalid = 0; invalid < 4; ++invalid) {
    auto request = caps; capsStorage.fill(0xa5); const auto before = capsStorage.snapshot();
    if (invalid == 0) request.DataSize -= 1;
    if (invalid == 1) request.DataSize += 1;
    if (invalid == 2) request.pInfo = &privateCookie;
    if (invalid == 3) request.Type = static_cast<D3D10_2DDICAPS_TYPE>(0xffffffff);
    const auto beforeQueries = queryCalls;
    CHECK(functions->pfnGetCaps(open.hAdapter, &request)
      == (invalid == 3 ? DXGI_ERROR_UNSUPPORTED : E_INVALIDARG));
    CHECK(capsStorage.matches(before) && queryCalls == beforeQueries);
  }
  UINT alternate = 0x12345678;
  queryHook = [&] { caps.pData = &alternate; caps.Type = D3D11DDICAPS_SHADER; caps.DataSize = 1; };
  CHECK(functions->pfnGetCaps(open.hAdapter, &caps) == S_OK);
  CHECK(capsStorage.as<D3D11DDI_3DPIPELINESUPPORT_CAPS>()->Caps == 1 && alternate == 0x12345678);
  caps.pData = capsStorage.data(); caps.Type = D3D11DDICAPS_3DPIPELINESUPPORT;
  caps.DataSize = sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS);

  GuardedBytes table(sizeof(D3D11DDI_DEVICEFUNCS)), dxgi(sizeof(DXGI1_1_DDI_BASE_FUNCTIONS));
  table.fill(0xa5); dxgi.fill(0x3c);
  const auto tableBefore = table.snapshot(), dxgiBefore = dxgi.snapshot();
  D3D11DDI_CORELAYER_DEVICECALLBACKS core = {}; core.pfnSetErrorCb = setError;
  DXGI_DDI_BASE_CALLBACKS dxgiCallbacks = {}; dxgiCallbacks.pfnPresentCb = presentStub;
  D3DDDI_DEVICECALLBACKS kernel = {};
  kernel.pfnAllocateCb = allocateStub; kernel.pfnDeallocateCb = deallocateStub;
  kernel.pfnLockCb = lockStub; kernel.pfnUnlockCb = unlockStub;
  kernel.pfnCreateContextCb = createContextStub; kernel.pfnDestroyContextCb = destroyContextStub;
  D3D10DDIARG_CREATEDEVICE create = {};
  create.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  create.Version = (D3D11_0_DDI_BUILD_VERSION << 16) | DXGI_RESOLVE_SHARED_RESOURCE;
  create.hRTDevice.handle = &deviceCookie; create.hRTCoreLayer.handle = &coreCookie;
  create.hDrvDevice.pDrvPrivate = &privateCookie; create.pKTCallbacks = &kernel;
  create.p11UMCallbacks = &core; create.p11DeviceFuncs = table.as<D3D11DDI_DEVICEFUNCS>();
  create.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgiCallbacks;
  create.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = dxgi.as<DXGI1_1_DDI_BASE_FUNCTIONS>();
  for (UINT flags = 0; flags < 256; ++flags) {
    D3D10DDIARG_CALCPRIVATEDEVICESIZE size = {create.Interface, create.Version, flags};
    CHECK(functions->pfnCalcPrivateDeviceSize(open.hAdapter, &size) == (flags == 0 || flags == 0x10 ? 256u : 0u));
  }
  const auto beforeCalls = deviceCalls, beforeQueries = queryCalls;
  for (unsigned invalid = 0; invalid < 19; ++invalid) {
    auto request = create; auto kernelCopy = kernel; request.pKTCallbacks = &kernelCopy;
    if (invalid == 0) { request.Interface = D3D10_0_DDI_INTERFACE_VERSION; request.Version = D3D10_0_DDI_BUILD_VERSION << 16; }
    if (invalid == 1) { request.Interface = D3D10_1_DDI_INTERFACE_VERSION; request.Version = D3D10_1_DDI_BUILD_VERSION << 16; }
    if (invalid == 2) request.Interface = D3D11_1_DDI_INTERFACE_VERSION;
    if (invalid == 3) request.Version = ((D3D11_0_DDI_BUILD_VERSION - 1) << 16) | 0xffff;
    if (invalid == 4) request.Flags = 2; // SDK pipeline10.1, never advertised here.
    if (invalid == 5) request.Flags = 4; // SDK pipeline11.0, never advertised here.
    if (invalid == 6) request.Flags = D3D10DDI_CREATEDEVICE_FLAG_DISABLE_EXTRA_THREAD_CREATION;
    if (invalid == 7) request.Flags = 0x80000000;
    if (invalid == 8) kernelCopy.pfnAllocateCb = nullptr;
    if (invalid == 9) kernelCopy.pfnDeallocateCb = nullptr;
    if (invalid == 10) kernelCopy.pfnLockCb = nullptr;
    if (invalid == 11) kernelCopy.pfnUnlockCb = nullptr;
    if (invalid == 12) kernelCopy.pfnCreateContextCb = nullptr;
    if (invalid == 13) kernelCopy.pfnDestroyContextCb = nullptr;
    if (invalid == 14) request.DXGIBaseDDI.pDXGIBaseCallbacks = nullptr;
    if (invalid == 15) dxgiCallbacks.pfnPresentCb = nullptr;
    if (invalid == 16) request.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = nullptr;
    if (invalid == 17) request.p11UMCallbacks = nullptr;
    if (invalid == 18) request.p11DeviceFuncs = nullptr;
    CHECK(functions->pfnCreateDevice(open.hAdapter, &request)
      == (invalid < 8 ? DXGI_ERROR_UNSUPPORTED : E_INVALIDARG));
    dxgiCallbacks.pfnPresentCb = presentStub;
    CHECK(table.matches(tableBefore) && dxgi.matches(dxgiBefore));
    CHECK(deviceCalls == beforeCalls && queryCalls == beforeQueries);
  }
  capsStorage.fill(0xa5); const auto capsBefore = capsStorage.snapshot();
  queryHook = [&] { CHECK(functions->pfnCloseAdapter(open.hAdapter) == S_OK); };
  CHECK(functions->pfnGetCaps(open.hAdapter, &caps) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(capsStorage.matches(capsBefore));
  CHECK(functions->pfnCreateDevice(open.hAdapter, &create) == E_INVALIDARG);

  for (UINT revision : {0u, UINT(DXGI_RESOLVE_SHARED_RESOURCE)}) {
    for (UINT flags : {0u, UINT(D3D11DDI_CREATEDEVICE_FLAG_SINGLETHREADED)}) {
      for (CreationCase action : {CreationCase::Success, CreationCase::BackendFailure,
        CreationCase::AllocationFailure, CreationCase::PositiveStatus, CreationCase::CloseBeforeBackend,
        CreationCase::CloseAfterBackend, CreationCase::ResetAfterBackend,
        CreationCase::QueryFailureAfterBackend, CreationCase::QueryExceptionAfterBackend})
        creationFixture(D3D11_0_DDI_INTERFACE_VERSION, (D3D11_0_DDI_BUILD_VERSION << 16) | revision,
          flags, action, true);
    }
  }
  // The tag belongs to an adapter token; it cannot spill into either old entry.
  const auto generic = openModern(*functions);
  capsStorage.fill(0xa5);
  CHECK(functions->pfnGetCaps(generic, &caps) == S_OK);
  CHECK(capsStorage.as<D3D11DDI_3DPIPELINESUPPORT_CAPS>()->Caps == 0);
  count = 77;
  CHECK(functions->pfnGetSupportedVersions(generic, &count, nullptr) == S_OK && count == 3);
  CHECK(functions->pfnCloseAdapter(generic) == S_OK);
  CHECK(OpenAdapter10_2(&open) == S_OK);
  count = 77;
  CHECK(functions->pfnGetSupportedVersions(open.hAdapter, &count, nullptr) == S_OK && count == 0);
  CHECK(functions->pfnGetCaps(open.hAdapter, &caps) == S_OK);
  CHECK(capsStorage.as<D3D11DDI_3DPIPELINESUPPORT_CAPS>()->Caps == 0);
  CHECK(functions->pfnCloseAdapter(open.hAdapter) == S_OK);
}
