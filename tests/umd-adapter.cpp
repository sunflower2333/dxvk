#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_runtime_identity.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

static unsigned checks;
#define CHECK(condition) do { checks++; if (!(condition)) { \
  std::fprintf(stderr, "adapter check %u failed at line %d: %s\n", checks, __LINE__, #condition); \
  std::exit(1); } } while (0)

static char adapterCookie, deviceCookie, coreCookie, privateCookie;
static unsigned queryCalls, deviceCalls;
static bool oldReply;
static HRESULT queryResult = S_OK, deviceResult = S_OK;
static bool throwAllocation;
static std::vector<std::shared_ptr<const dxvk::umd::AdapterIdentity>> devices;
static LUID expectedLuid = {0x12345678, -77};

static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
  queryCalls++;
  CHECK(runtime == &adapterCookie);
  CHECK(args && args->pPrivateDriverData && args->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (size_t i = 0; i < 160; i++) CHECK(bytes[i] == 0);
  if (FAILED(queryResult)) return queryResult;
  auto set = [bytes](size_t offset, uint32_t value) {
    for (size_t i = 0; i < 4; i++) bytes[offset+i] = uint8_t(value >> (i*8));
  };
  set(0,0x504d5644); set(8,128);
  if (!oldReply) {
    set(128,0x44494c56); set(132,1); set(136,32); set(140,1); set(152,1);
    std::memcpy(bytes+144, &expectedLuid, sizeof(expectedLuid));
  }
  return S_OK;
}

static void APIENTRY setError(D3D10DDI_HRTCORELAYER, HRESULT) { CHECK(false); }
static void APIENTRY destroyStub(D3D10DDI_HDEVICE) {}

// Linked only into this CPU test, in place of the Vulkan-backed device
// implementation. These hooks are never exported by the development DLL.
extern "C" SIZE_T APIENTRY VioGpuDxvkPrivateDeviceSize() { return 256; }
HRESULT dxvk::umd::createAdapterDevice(
    const std::shared_ptr<const AdapterIdentity>& identity, D3D10DDIARG_CREATEDEVICE* args) {
  deviceCalls++;
  CHECK(identity && std::memcmp(&identity->luid, &expectedLuid, sizeof(LUID)) == 0);
  CHECK(identity->runtime.handle == &adapterCookie);
  CHECK(args->hRTDevice.handle == &deviceCookie);
  CHECK(args->hRTCoreLayer.handle == &coreCookie);
  CHECK(args->hDrvDevice.pDrvPrivate == &privateCookie);
  CHECK(args->pUMCallbacks->pfnSetErrorCb == setError);
  if (throwAllocation) throw std::bad_alloc();
  if (FAILED(deviceResult)) return deviceResult;
  devices.push_back(identity);
  args->pDeviceFuncs->pfnDestroyDevice = destroyStub;
  return S_OK;
}

int main() {
  CHECK(VioGpuDxvkOpenAdapterForTest(nullptr) == E_INVALIDARG);
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
  std::printf("adapter lifecycle PASS checks=%u; mock runtime and backend, no GPU\n", checks);
}
