// Exercise the actual typed Present entry and its native destination branch.
// Resource setup is controlled; this is not AHB import or hardware pixel proof.
#include "../src/umd/umd_ddi.cpp"
#include <cstdlib>
#include <functional>
#include <set>

namespace {
unsigned checks = 0, presentCalls = 0, flushCalls = 0, forbiddenCalls = 0;
unsigned handoffs = 0;
unsigned displayModeCalls = 0;
unsigned idleWaits = 0, idleWaitsBefore = 0;
ID3D11Resource* waitSource = nullptr;
ID3D11Resource* waitDestination = nullptr;
HRESULT sourceWaitResult = S_OK, destinationWaitResult = S_OK;
std::function<void()> sourceWaitAction, destinationWaitAction;
std::string presentOrder;
bool flipCase = false;
unsigned handoffsBefore = 0;
bool explicitFlipDestination = false;
char deviceCookie, contextCookie, dxgiCookie;
HRESULT callbackResult = S_OK;
std::function<void()> presentAction;
HRESULT prepareResult = S_OK;
std::function<void()> prepareAction;
bool nativeOpenMode = false, failImage = false;
std::function<void()> imageAction;
std::function<void()> contextAction;
void* imageToken = nullptr;
HRESULT lastError = S_OK;
unsigned imports = 0, releases = 0;
unsigned openedCloses = 0;
DWORD openedCaller = 0;
std::set<HANDLE> openedRuntimeResources;
std::function<void()> openedCloseAction;
bool openedDeviceClosed = false;
unsigned terminalTokenReleases = 0;

std::map<uint64_t, uint64_t> importedKeys;
bool windowCreate = false;
bool allNativeRotation = false;
uint64_t nextSurfaceKey = 100;
std::set<uint64_t> multiSurfaceKeys;
unsigned capQueries = 0;
uint64_t identityGeneration = 2;
unsigned creationScenario = 0, modeQueries = 0, surfaceCreates = 0, surfaceFrees = 0, wrapperCreates = 0;
bool initialSharedFallback = false;
unsigned fallbackLocks = 0, fallbackUnlocks = 0;
std::vector<uint8_t> fallbackBacking;
std::array<uint32_t, 32> fallbackExpected{};
std::function<void()> modeQueryAction;
uint8_t commands[4096] = {};
D3DDDI_ALLOCATIONLIST allocationList[64] = {};
D3DDDI_PATCHLOCATIONLIST patchList[64] = {};
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "native Present line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
HRESULT APIENTRY closeOpened(HANDLE cookie, const D3DDDICB_DEALLOCATE* args) {
  CHECK(GetCurrentThreadId() == openedCaller && cookie == &deviceCookie && args);
  CHECK(args->hResource && !args->NumAllocations && !args->HandleList);
  CHECK(openedRuntimeResources.erase(args->hResource) == 1);
  ++openedCloses;
  if (openedCloseAction) std::exchange(openedCloseAction, {})();
  return S_OK;
}
struct Identity final : dxvk::umd::AdapterIdentitySource {
  HRESULT query(dxvk::umd::RuntimeIdentity& out, dxvk::umd::RuntimeQueryStage) const override {
    out = {}; out.luid[0] = 71; out.generation = identityGeneration; out.capabilities = 3; return S_OK;
  }
  HRESULT supportsPrimaryMode(const dxvk::umd::PrimaryShape& shape) const override {
    ++modeQueries;
    CHECK(shape.width == 8 && shape.height == 4 && shape.format == 87
      && shape.numerator == 165 && shape.denominator == 1 && !shape.source);
    if (modeQueryAction) std::exchange(modeQueryAction, {})();
    return creationScenario == 1 ? DXGI_ERROR_UNSUPPORTED : creationScenario == 2 ? S_FALSE
      : creationScenario == 8 ? DXGI_ERROR_DEVICE_REMOVED : S_OK;
  }
  HRESULT nativeEscape(void* data, UINT bytes) const override {
    if (bytes == 40) {
      if (!windowCreate) return DXGI_ERROR_UNSUPPORTED;
      ++capQueries;
      uint32_t packet[10]; std::memcpy(packet, data, sizeof(packet));
      CHECK(packet[0] == 0x504d5644 && !packet[1] && packet[2] == 40 && !packet[3]
        && packet[4] == 15 && !packet[5] && !packet[6] && !packet[7] && !packet[8] && !packet[9]);
      if (creationScenario == 1) return HRESULT_FROM_NT(NTSTATUS(0xc000000d));
      if (creationScenario == 12) return S_FALSE;
      if (creationScenario == 11 || creationScenario == 17) return E_FAIL;
      packet[6] = 2; packet[8] = creationScenario == 2 ? 0 : 1;
      if (creationScenario == 3) packet[2] = 39;
      if (creationScenario == 4) packet[8] = 3;
      if (creationScenario == 5) packet[6] = 3;
      if (creationScenario == 8) identityGeneration = 3;
      if (creationScenario == 13) packet[5] = 1;
      if (creationScenario == 14) packet[4] = 11;
      if (creationScenario == 15) packet[9] = 1;
      if (creationScenario == 16) packet[6] = 0;
      if (modeQueryAction) std::exchange(modeQueryAction, {})();
      std::memcpy(data, packet, sizeof(packet)); return S_OK;
    }
    CHECK(data && bytes == 128);
    auto& packet = *static_cast<dxvk::umd::NativeSurfaceInfo*>(data);
    if (packet.opcode == 9) { CHECK(packet.key == 13 || multiSurfaceKeys.erase(packet.key) == 1); ++surfaceFrees; return S_OK; }
    CHECK(packet.opcode == 8 && packet.width == 8 && packet.height == 4 && packet.fourcc == (windowCreate && creationScenario == 23 ? 0x34324241u : 0x34325241u));
    ++surfaceCreates; packet.key = 13; packet.size = 4096; packet.generation = 2; packet.resource = 19;
    if (allNativeRotation) { packet.key = ++nextSurfaceKey; CHECK(multiSurfaceKeys.insert(packet.key).second); }
    packet.stride = 32; packet.planes = packet.layoutFlags = 1;
    return S_OK;
  }
};
HRESULT APIENTRY forbiddenAllocate(HANDLE, D3DDDICB_ALLOCATE*) { ++forbiddenCalls; return E_FAIL; }
HRESULT APIENTRY forbiddenDeallocate(HANDLE, const D3DDDICB_DEALLOCATE*) { ++forbiddenCalls; return E_FAIL; }
HRESULT APIENTRY forbiddenLock(HANDLE, D3DDDICB_LOCK*) { ++forbiddenCalls; return E_FAIL; }
HRESULT APIENTRY forbiddenUnlock(HANDLE, const D3DDDICB_UNLOCK*) { ++forbiddenCalls; return E_FAIL; }
HRESULT APIENTRY initialSharedLock(HANDLE cookie, D3DDDICB_LOCK* args) {
  CHECK(initialSharedFallback && cookie == &deviceCookie && args && args->hAllocation == 31);
  CHECK(args->Flags.LockEntire && args->Flags.WriteOnly && !args->Flags.ReadOnly
    && !args->Flags.Discard && !args->Flags.IgnoreSync);
  CHECK(fallbackBacking.size() >= sizeof(fallbackExpected) + 32 && fallbackLocks == fallbackUnlocks);
  ++fallbackLocks; args->pData = fallbackBacking.data() + 16; return S_OK;
}
HRESULT APIENTRY initialSharedUnlock(HANDLE cookie, const D3DDDICB_UNLOCK* args) {
  CHECK(initialSharedFallback && cookie == &deviceCookie && args && args->NumAllocations == 1);
  CHECK(args->phAllocations && args->phAllocations[0] == 31 && fallbackLocks == fallbackUnlocks + 1);
  ++fallbackUnlocks; return S_OK;
}
HRESULT APIENTRY forbiddenRender(HANDLE, D3DDDICB_RENDER*);
HRESULT APIENTRY fixtureContext(HANDLE cookie, D3DDDICB_CREATECONTEXT* args) {
  CHECK(cookie == &deviceCookie && args && !args->hContext && args->EngineAffinity == 1);
  args->hContext = &contextCookie;
  if (nativeOpenMode) {
    args->pCommandBuffer = commands; args->CommandBufferSize = sizeof(commands);
    args->pAllocationList = allocationList; args->AllocationListSize = 64;
    args->pPatchLocationList = patchList; args->PatchLocationListSize = 64;
    if (contextAction) std::exchange(contextAction, {})();
  }
  return S_OK;
}
HRESULT APIENTRY fixtureContextClose(HANDLE cookie, const D3DDDICB_DESTROYCONTEXT* args) {
  CHECK(cookie == &deviceCookie && args && args->hContext == &contextCookie); return S_OK;
}
HRESULT APIENTRY fixturePresent(HANDLE cookie, DXGIDDICB_PRESENT* args) {
  CHECK(cookie == &deviceCookie && args && args->hContext == &contextCookie);
  CHECK(args->hSrcAllocation == 21 && args->hDstAllocation == (flipCase && !explicitFlipDestination ? 0u : 22u)
    && args->pDXGIContext == &dxgiCookie);
  CHECK(flushCalls > 0 && handoffs == handoffsBefore + unsigned(!flipCase)); ++presentCalls;
  CHECK(idleWaits == idleWaitsBefore + (flipCase ? 0u : 2u));
  CHECK(presentOrder == (flipCase ? "F" : "FSPFD"));
  presentOrder += 'C';
  if (presentAction) std::exchange(presentAction, {})();
  return callbackResult;
}
HRESULT APIENTRY fixtureDisplayMode(HANDLE cookie, D3DDDICB_SETDISPLAYMODE* args) {
  CHECK(cookie == &deviceCookie && args && args->hPrimaryAllocation == 21);
  CHECK(idleWaits == idleWaitsBefore + 1 && handoffs == handoffsBefore && presentOrder == "FS");
  ++displayModeCalls; presentOrder += 'M';
  if (presentAction) std::exchange(presentAction, {})();
  return callbackResult;
}
void run(unsigned scenario, bool flip = false, bool mode = false) {
  flipCase = flip; explicitFlipDestination = flip && scenario == 4;
  handoffsBefore = handoffs;
  idleWaitsBefore = idleWaits;
  presentOrder.clear();
  auto device = std::make_shared<Device>(true);
  const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
    D3D11_SDK_VERSION, &device->backend, nullptr, &device->context) == S_OK);
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid.LowPart = 71; identity->generation = 2; identity->capabilities = 3;
  identity->continuity = std::make_shared<Identity>();
  D3DDDI_DEVICECALLBACKS cb = {};
  cb.pfnAllocateCb = forbiddenAllocate; cb.pfnDeallocateCb = forbiddenDeallocate;
  cb.pfnLockCb = forbiddenLock; cb.pfnUnlockCb = forbiddenUnlock;
  cb.pfnCreateContextCb = fixtureContext; cb.pfnDestroyContextCb = fixtureContextClose;
  cb.pfnSetDisplayModeCb = fixtureDisplayMode;
  if (mode) cb.pfnRenderCb = forbiddenRender;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = fixturePresent;
  device->memory.initialize(&deviceCookie, cb, &dxgi, identity);
  Resource source, destination;
  for (auto resource : {&source, &destination}) {
    const bool primary = resource == &destination || (flip && scenario != 5) || (mode && scenario != 7);
    resource->owner = device.get(); resource->nativeFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    resource->present = std::make_shared<PresentSurface>();
    resource->present->native = std::make_shared<dxvk::umd::RuntimeAllocation>();
    dxvk::umd::AllocationInfo info;
    info.flags = primary; info.width = 8; info.height = 4; info.pitch = 32; info.size = 4096; info.format = 1;
    info.refreshNumerator = primary ? 165 : 0; info.refreshDenominator = primary;
    dxvk::umd::NativeResourceShare share; share.key = resource == &destination ? 12 : 11; share.stride = 32;
    CHECK(device->memory.adoptNative(*resource->present->native, resource == &destination ? 22 : 21, 0, info, share) == S_OK);
    D3D11_TEXTURE2D_DESC desc = {}; desc.Width = 8; desc.Height = 4; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> image;
    CHECK(device->backend->CreateTexture2D(&desc, nullptr, &image) == S_OK); resource->backend = image;
    CHECK(resourceStorage.emplace(resource, ResourceRecord{device.get(), ResourcePhase::Live,
      std::make_shared<const char>(0)}).second);
  }
  CHECK(deviceStorage.emplace(device.get(), DeviceRecord{DevicePhase::Live, device}).second);
  waitSource = source.backend.Get(); waitDestination = destination.backend.Get();
  if (mode) {
    waitDestination = nullptr;
    callbackResult = scenario == 1 ? E_OUTOFMEMORY : scenario == 2 ? S_FALSE : S_OK;
    sourceWaitResult = scenario == 4 ? E_OUTOFMEMORY : scenario == 5 ? S_FALSE : S_OK;
    if (scenario == 3) presentAction = [&] { CHECK(resourceStorage.erase(&source) == 1); };
    if (scenario == 6) sourceWaitAction = [&] { CHECK(resourceStorage.erase(&source) == 1); };
    DXGI_DDI_ARG_SETDISPLAYMODE args = {};
    args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.get());
    args.hResource = reinterpret_cast<DXGI_DDI_HRESOURCE>(&source);
    DXGI_DDI_BASE_FUNCTIONS table = {}; table.pfnSetDisplayMode = setDisplayMode;
    const auto before = displayModeCalls;
    const HRESULT expected = scenario == 1 || scenario == 4 ? E_OUTOFMEMORY
      : scenario == 2 || scenario == 5 ? E_FAIL : scenario == 3 || scenario == 6 ? DXGI_ERROR_DEVICE_REMOVED
      : scenario == 7 ? DXGI_DDI_ERR_UNSUPPORTED : S_OK;
    const auto actual = table.pfnSetDisplayMode(&args);
    std::printf("NATIVE_MODE scenario=%u actual=%08lx expected=%08lx callbacks=%u\n",
      scenario, (unsigned long)actual, (unsigned long)expected, displayModeCalls - before);
    CHECK(actual == expected && displayModeCalls == before + unsigned(scenario < 4));
    CHECK(idleWaits == idleWaitsBefore + unsigned(scenario != 7) && handoffs == handoffsBefore);
    CHECK(!sourceWaitAction && !presentAction && !forbiddenCalls);
    waitSource = nullptr;
    resourceStorage.erase(&source); resourceStorage.erase(&destination); deviceStorage.erase(device.get());
    CHECK(source.present->native->release() == S_OK && destination.present->native->release() == S_OK);
    source.backend.Reset(); destination.backend.Reset(); source.present.reset(); destination.present.reset();
    CHECK(device->close() == S_OK);
    return;
  }
  DXGI_DDI_ARG_PRESENT args = {};
  args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.get());
  args.hSurfaceToPresent = reinterpret_cast<DXGI_DDI_HRESOURCE>(&source);
  args.hDstResource = flip && !explicitFlipDestination ? 0 : reinterpret_cast<DXGI_DDI_HRESOURCE>(&destination);
  args.pDXGIContext = &dxgiCookie; args.Flags.Value = flip ? 2 : 1;
  if (flip && scenario == 6) args.Flags.Value = 3;
  if (flip && scenario == 7) args.Flags.Value = 0;
  if (flip && scenario == 8) args.Flags.Value = 6;
  callbackResult = scenario == 1 ? E_OUTOFMEMORY : scenario == 2 ? S_FALSE : S_OK;
  prepareResult = !flip && scenario == 4 ? E_OUTOFMEMORY : S_OK;
  sourceWaitResult = !flip && scenario == 6 ? E_OUTOFMEMORY : !flip && scenario == 7 ? S_FALSE : S_OK;
  destinationWaitResult = !flip && scenario == 9 ? E_OUTOFMEMORY : !flip && scenario == 10 ? S_FALSE : S_OK;
  if (scenario == 3) presentAction = [&] { CHECK(resourceStorage.erase(flip ? &source : &destination) == 1); };
  if (!flip && scenario == 5) prepareAction = [&] { CHECK(resourceStorage.erase(&destination) == 1); };
  if (!flip && scenario == 8) sourceWaitAction = [&] { CHECK(resourceStorage.erase(&source) == 1); };
  if (!flip && scenario == 12) sourceWaitAction = [&] { CHECK(resourceStorage.erase(&destination) == 1); };
  if (!flip && scenario == 11) destinationWaitAction = [&] { CHECK(resourceStorage.erase(&destination) == 1); };
  if (!flip && scenario == 13) destinationWaitAction = [&] { CHECK(resourceStorage.erase(&source) == 1); };
  const HRESULT expected = flip
    ? (scenario == 1 ? E_OUTOFMEMORY : scenario == 2 ? E_FAIL
       : scenario == 3 ? DXGI_ERROR_DEVICE_REMOVED : scenario == 5 ? DXGI_DDI_ERR_UNSUPPORTED
       : scenario >= 6 ? E_INVALIDARG : S_OK)
    : (scenario == 1 || scenario == 4 || scenario == 6 || scenario == 9 ? E_OUTOFMEMORY
       : scenario == 2 || scenario == 7 || scenario == 10 ? E_FAIL
       : scenario == 3 || scenario == 5 || scenario == 8 || scenario >= 11 ? DXGI_ERROR_DEVICE_REMOVED : S_OK);
  const auto oldPresent = presentCalls;
  DXGI_DDI_BASE_FUNCTIONS table = {}; table.pfnPresent = present;
  const HRESULT actual = table.pfnPresent(&args);
  std::printf("NATIVE_PRESENT scenario=%u flip=%u actual=%08lx expected=%08lx callbacks=%u\n",
    scenario, flip, (unsigned long)actual, (unsigned long)expected, presentCalls - oldPresent);
  CHECK(actual == expected && presentCalls == oldPresent + unsigned(scenario < (flip ? 5u : 4u)) && !forbiddenCalls);
  if (flip) CHECK(handoffs == handoffsBefore && idleWaits == idleWaitsBefore);
  else {
    const bool destinationPrepared = scenario < 6 || scenario == 9 || scenario == 10 || scenario == 11 || scenario == 13;
    const bool destinationWaited = scenario < 4 || scenario == 9 || scenario == 10 || scenario == 11 || scenario == 13;
    CHECK(handoffs == handoffsBefore + unsigned(destinationPrepared));
    CHECK(idleWaits == idleWaitsBefore + 1u + unsigned(destinationWaited));
    CHECK(presentOrder == (scenario < 4 ? "FSPFDC" : destinationWaited ? "FSPFD" : destinationPrepared ? "FSP" : "FS"));
  }
  CHECK(!sourceWaitAction && !destinationWaitAction);
  waitSource = waitDestination = nullptr;
  resourceStorage.erase(&source); resourceStorage.erase(&destination); deviceStorage.erase(device.get());
  CHECK(source.present->native->release() == S_OK && destination.present->native->release() == S_OK);
  source.backend.Reset(); destination.backend.Reset();
  source.present.reset(); destination.present.reset();
  CHECK(device->close() == S_OK);
}
void APIENTRY fixtureError(D3D10DDI_HRTCORELAYER, HRESULT hr) { lastError = hr; }
HRESULT APIENTRY forbiddenRender(HANDLE, D3DDDICB_RENDER*) { ++forbiddenCalls; return E_FAIL; }
HRESULT APIENTRY fixtureEscape(HANDLE, const D3DDDICB_ESCAPE* args) {
  CHECK(args && args->hDevice == &deviceCookie && args->hContext == &contextCookie);
  auto data = static_cast<uint8_t*>(args->pPrivateDriverData);
  auto get = [&](unsigned offset, unsigned bytes) { uint64_t value = 0; std::memcpy(&value, data + offset, bytes); return value; };
  auto put = [&](unsigned offset, uint64_t value, unsigned bytes) { std::memcpy(data + offset, &value, bytes); };
  CHECK(get(0,4) == 0x504d5644 && get(24,8) == 2);
  if (args->PrivateDriverDataSize == 64) {
    put(32,0x100000000ull,8); put(40,0x1000000,8); put(48,2,8); put(56,17,4); put(60,19,4);
  } else {
    CHECK(args->PrivateDriverDataSize == 88 && get(48,8) == 4096);
    const auto key = get(32,8), address = get(40,8), opcode = get(16,4);
    if (opcode == 6) {
      CHECK(!importedKeys.count(key)); importedKeys[key] = address; ++imports;
    } else {
      CHECK(opcode == 7 && importedKeys.at(key) == address); importedKeys.erase(key); ++releases;
    }
    put(56,17,4);
  }
  return S_OK;
}
const GUID tokenMarker = {0x737ad730,0xf931,0x4ec2,{0xa9,0x11,0x71,0x5d,0x30,0xe2,0x04,0x82}};
class TokenMarker final : public IUnknown {
public:
  TokenMarker(const dxvk::umd::RuntimeBackend& bridge, void* token) : bridge(bridge), token(token) { }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr; if (iid != __uuidof(IUnknown)) return E_NOINTERFACE;
    *out = this; AddRef(); return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() override { const auto count = --refs; if (!count) delete this; return count; }
private:
  ~TokenMarker() {
    const auto oldImports = imports, oldReleases = releases, oldCloses = openedCloses;
    const HRESULT hr = bridge.create.callbacks->release(bridge.create.owner, token);
    if (openedDeviceClosed) {
      // A parent backend object may outlive nested DestroyDevice. The closed
      // service rejects its late token call before dispatching runtime work.
      CHECK(hr == DXGI_ERROR_DEVICE_REMOVED);
      CHECK(importedKeys.empty() && openedRuntimeResources.empty());
      CHECK(imports == oldImports && releases == oldReleases && openedCloses == oldCloses);
      ++terminalTokenReleases;
    } else CHECK(hr == S_OK);
  }
  ULONG refs = 1;
  dxvk::umd::RuntimeBackend bridge;
  void* token;
};
void runOpen(unsigned scenario) {
  std::printf("NATIVE_OPEN scenario=%u\n", scenario);
  nativeOpenMode = true; prepareResult = S_OK; failImage = false;
  auto device = std::make_shared<Device>(true);
  const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
    D3D11_SDK_VERSION, &device->backend, nullptr, &device->context) == S_OK);
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid.LowPart = 71; identity->generation = 2; identity->capabilities = 3;
  identity->continuity = std::make_shared<Identity>();
  D3DDDI_DEVICECALLBACKS cb = {};
  cb.pfnAllocateCb = forbiddenAllocate; cb.pfnDeallocateCb = closeOpened;
  cb.pfnLockCb = forbiddenLock; cb.pfnUnlockCb = forbiddenUnlock;
  cb.pfnCreateContextCb = fixtureContext; cb.pfnDestroyContextCb = fixtureContextClose;
  cb.pfnEscapeCb = fixtureEscape; cb.pfnRenderCb = forbiddenRender;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = fixturePresent;
  D3D10DDI_CORELAYER_DEVICECALLBACKS core = {}; core.pfnSetErrorCb = fixtureError;
  device->callbacks10 = &core;
  device->memory.initialize(&deviceCookie, cb, &dxgi, identity, device->service);
  device->gpu = dxvk::umd::RuntimeGpu::create(&deviceCookie, cb, identity, device->service);
  CHECK(deviceStorage.emplace(device.get(), DeviceRecord{DevicePhase::Live, device}).second);
  dxvk::umd::RuntimeService::Scope caller(device->service.get());
  const D3D10DDI_HDEVICE h{device.get()};
  D3D10DDI_DEVICEFUNCS table = {};
  table.pfnOpenResource = deviceEntry<openResource>;
  table.pfnDestroyResource = deviceEntry<destroyResource>;
  table.pfnCreateRenderTargetView = deviceEntry<createTarget>;
  table.pfnDestroyRenderTargetView = deviceEntry<destroyTarget>;
  table.pfnCreateShaderResourceView = deviceEntry<createShaderView>;
  table.pfnDestroyShaderResourceView = deviceEntry<destroyShaderView>;
  alignas(Resource) unsigned char firstStorage[sizeof(Resource)], secondStorage[sizeof(Resource)];
  D3D10DDI_HRESOURCE first{firstStorage}, second{secondStorage};
  alignas(RenderTarget) unsigned char targetStorage[sizeof(RenderTarget)];
  alignas(ShaderView) unsigned char viewStorage[sizeof(ShaderView)];
  const D3D10DDI_HRENDERTARGETVIEW target{targetStorage};
  const D3D10DDI_HSHADERRESOURCEVIEW view{viewStorage};
  dxvk::umd::AllocationInfo info;
  info.flags = 0; info.width = 8; info.height = 4; info.pitch = 32; info.size = 4096; info.format = 1;
  dxvk::umd::NativeResourceShare share; share.key = 12; share.stride = 32;
  D3DDDI_OPENALLOCATIONINFO allocation = {};
  allocation.hAllocation = 21; allocation.pPrivateDriverData = &info; allocation.PrivateDriverDataSize = sizeof(info);
  D3D10DDIARG_OPENRESOURCE args = {};
  args.NumAllocations = 1; args.pOpenAllocationInfo = &allocation; args.hKMResource.handle = 41;
  args.pPrivateDriverData = &share; args.PrivateDriverDataSize = sizeof(share);
  lastError = S_OK;
  const auto oldImports = imports, oldReleases = releases, oldCloses = openedCloses;
  char firstRuntime, secondRuntime;
  openedCaller = GetCurrentThreadId();
  CHECK(openedRuntimeResources.empty());
  openedRuntimeResources.insert(&firstRuntime);
  if (scenario == 5) contextAction = [&] { table.pfnDestroyResource(h, first); };
  table.pfnOpenResource(h, &args, first, {&firstRuntime});
  if (scenario == 5) {
    CHECK(FAILED(lastError) && !resourceStorage.count(firstStorage) && imports == oldImports);
    CHECK(releases == oldReleases && importedKeys.empty());
    CHECK(openedCloses == oldCloses && openedRuntimeResources.erase(&firstRuntime) == 1);
    deviceStorage.erase(device.get()); CHECK(device->close() == S_OK);
    nativeOpenMode = false; return;
  }
  CHECK(lastError == S_OK && resourceStorage.count(firstStorage) && imports == oldImports + 1);
  if (scenario == 0) {
    D3D10DDIARG_CREATERENDERTARGETVIEW rt = {};
    rt.hDrvResource = first; rt.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    rt.Tex2D.ArraySize = 1;
    D3D10DDIARG_CREATESHADERRESOURCEVIEW sr = {};
    sr.hDrvResource = first; sr.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    sr.Tex2D.MipLevels = sr.Tex2D.ArraySize = 1;
    // Invalid views must leave caller storage unpublished. A typed sRGB view
    // of the opened linear Present buffer must retain the imported image.
    for (auto format : {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_TYPELESS,
                        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB}) {
      rt.Format = sr.Format = format;
      std::memset(targetStorage, 0xa5, sizeof(targetStorage));
      std::memset(viewStorage, 0xa5, sizeof(viewStorage));
      lastError = S_OK; table.pfnCreateRenderTargetView(h, &rt, target, {});
      const HRESULT rtResult = lastError;
      lastError = S_OK; table.pfnCreateShaderResourceView(h, &sr, view, {});
      if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        CHECK(rtResult == S_OK && lastError == S_OK && get(target)->backend && get(view)->backend);
      } else {
        CHECK(rtResult == E_INVALIDARG && lastError == E_INVALIDARG);
        CHECK(std::all_of(std::begin(targetStorage), std::end(targetStorage), [](auto b) { return b == 0xa5; }));
        CHECK(std::all_of(std::begin(viewStorage), std::end(viewStorage), [](auto b) { return b == 0xa5; }));
      }
    }
  }
  const auto token = imageToken;
  if (scenario != 0) allocation.hAllocation = 22;
  failImage = scenario == 2;
  if (scenario == 3) imageAction = [&] { table.pfnDestroyResource(h, second); };
  openedRuntimeResources.insert(&secondRuntime);
  table.pfnOpenResource(h, &args, second, {&secondRuntime});
  const bool failedOpen = scenario == 2 || scenario == 3;
  if (failedOpen) CHECK(openedCloses == oldCloses && openedRuntimeResources.erase(&secondRuntime) == 1);
  CHECK(FAILED(lastError) == failedOpen && bool(resourceStorage.count(secondStorage)) == !failedOpen);
  CHECK(imports == oldImports + 1 && imageToken == token);
  if (scenario >= 6) {
    // Keep independent storage alive beyond the runtime-private resource.
    // Terminal close must retire both runtime opens before this pin is dropped.
    auto pinnedFirst = get(first)->present;
    const auto oldTerminalReleases = terminalTokenReleases;
    std::weak_ptr<dxvk::umd::RuntimeGpu> gpuOwner = device->gpu;
    if (scenario == 6) {
      table.pfnDestroyResource(h, first);
      table.pfnDestroyResource(h, second);
      destroyDevice(h);
    } else {
      pinnedFirst.reset();
      openedCloseAction = [&] {
        if (scenario == 7) table.pfnDestroyResource(h, second);
        else {
          // DestroyDevice requires all child DDIs to have been destroyed;
          // their deferred backend objects can still pin runtime allocations.
          table.pfnDestroyResource(h, second);
          destroyDevice(h);
          openedDeviceClosed = true;
        }
      };
      table.pfnDestroyResource(h, first);
      if (scenario == 7) destroyDevice(h);
    }
    CHECK(!deviceStorage.count(device.get()) && resourceStorage.empty());
    CHECK(openedCloses == oldCloses + 2 && openedRuntimeResources.empty() && !openedCloseAction);
    pinnedFirst.reset();
    CHECK(openedCloses == oldCloses + 2 && importedKeys.empty() && !forbiddenCalls);
    CHECK(releases == oldReleases + 1);
    CHECK((terminalTokenReleases > oldTerminalReleases) == (scenario == 8));
    device->gpu.reset();
    CHECK(gpuOwner.expired());
    openedDeviceClosed = false;
    nativeOpenMode = false; return;
  }
  const auto bridge = device->gpu->backend();
  mwd_allocation retained = {};
  CHECK(bridge.create.callbacks->retain(bridge.create.owner, token, &retained) == S_OK && retained.handle == 21);
  const auto previousHandoffs = handoffs;
  if (scenario == 4) device->service->run([&] { table.pfnDestroyResource(h, first); });
  else table.pfnDestroyResource(h, first);
  CHECK(!resourceStorage.count(firstStorage) && handoffs == previousHandoffs + unsigned(scenario != 4));
  mwd_allocation after = {};
  const HRESULT alive = bridge.create.callbacks->retain(bridge.create.owner, token, &after);
  if (failedOpen) CHECK(FAILED(alive) && !after.token);
  else {
    CHECK(alive == S_OK && after.handle == (scenario == 0 ? 21u : 22u));
    CHECK(bridge.create.callbacks->release(bridge.create.owner, after.token) == S_OK);
    table.pfnDestroyResource(h, second);
  }
  CHECK(bridge.create.callbacks->release(bridge.create.owner, retained.token) == S_OK);
  if (scenario == 0) {
    CHECK(!importedKeys.empty() && releases == oldReleases);
    ComPtr<ID3D11Resource> viewedImage;
    get(view)->backend->GetResource(&viewedImage); CHECK(viewedImage); viewedImage.Reset();
    table.pfnDestroyRenderTargetView(h, target);
    CHECK(!importedKeys.empty() && releases == oldReleases);
    table.pfnDestroyShaderResourceView(h, view);
  }
  CHECK(importedKeys.empty() && releases == oldReleases + 1 && !forbiddenCalls);
  CHECK(openedCloses == oldCloses + (failedOpen ? 1u : 2u) && openedRuntimeResources.empty());
  CHECK(resourceStorage.empty()); deviceStorage.erase(device.get());
  CHECK(device->close() == S_OK);
  nativeOpenMode = false;
}
HRESULT APIENTRY createWrapper(HANDLE device, D3DDDICB_ALLOCATE* args) {
  CHECK(device == &deviceCookie && args && args->hResource == &dxgiCookie && args->NumAllocations == 1);
  CHECK(args->pAllocationInfo && args->pAllocationInfo->PrivateDriverDataSize == 80);
  const auto& info = *static_cast<const dxvk::umd::AllocationInfo*>(args->pAllocationInfo->pPrivateDriverData);
  const bool native = args->PrivateDriverDataSize == sizeof(dxvk::umd::NativeResourceShare);
  CHECK(info.width == 8 && info.height == 4 && info.pitch == 32 && info.flags == (native ? unsigned(!windowCreate) : 2u));
  CHECK(info.refreshNumerator == (native && !windowCreate ? 165u : 0u) && info.refreshDenominator == unsigned(native && !windowCreate));
  CHECK(args->pAllocationInfo->Flags.Primary == (native && !windowCreate));
  ++wrapperCreates;
  if (creationScenario == 10) return E_OUTOFMEMORY;
  if (initialSharedFallback) {
    CHECK(!native && fallbackBacking.empty() && info.size >= sizeof(fallbackExpected));
    fallbackBacking.assign(size_t(info.size) + 32, 0xa5);
  }
  args->pAllocationInfo->hAllocation = 31; args->hKMResource = 41;
  return S_OK;
}
HRESULT APIENTRY destroyWrapper(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  CHECK(device == &deviceCookie && args && args->hResource == &dxgiCookie && !args->NumAllocations);
  if (initialSharedFallback) {
    CHECK(fallbackLocks == fallbackUnlocks && !fallbackBacking.empty());
    CHECK(!std::memcmp(fallbackBacking.data() + 16, fallbackExpected.data(), sizeof(fallbackExpected)));
    for (size_t i = 0; i < fallbackBacking.size(); ++i)
      if (i < 16 || i >= 16 + sizeof(fallbackExpected)) CHECK(fallbackBacking[i] == 0xa5);
    fallbackBacking.clear();
  }
  return S_OK;
}
HRESULT APIENTRY unusedMode(HANDLE, D3DDDICB_SETDISPLAYMODE*);
unsigned rotationCalls = 0, rotationAllocations = 0, rotationReleases = 0;
HRESULT rotationResult = S_OK;
std::function<void()> rotationAction;
HANDLE expectedRotationRelease = nullptr;
HRESULT APIENTRY rotationAllocate(HANDLE cookie, D3DDDICB_ALLOCATE* args) {
  CHECK(cookie == &deviceCookie && args && args->hResource && args->NumAllocations == 1);
  args->pAllocationInfo->hAllocation = 80 + ++rotationAllocations;
  args->hKMResource = 0; // Real runtime09 also withheld this optional handle.
  return S_OK;
}
HRESULT APIENTRY rotationDeallocate(HANDLE cookie, const D3DDDICB_DEALLOCATE* args) {
  CHECK(cookie == &deviceCookie && args && args->hResource == expectedRotationRelease);
  CHECK(!args->NumAllocations && !args->HandleList); ++rotationReleases; return S_OK;
}
void runMixedRotation(unsigned scenario, bool allNative = false) {
  allNativeRotation = allNative;
  std::printf("MIXED_ROTATION scenario=%u\n", scenario);
  auto device = std::make_shared<Device>(true);
  const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
    D3D11_SDK_VERSION, &device->backend, nullptr, &device->context) == S_OK);
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid.LowPart = 71; identity->generation = 2; identity->capabilities = 3;
  identity->continuity = std::make_shared<Identity>(); device->adapter = identity;
  D3DDDI_DEVICECALLBACKS cb = {};
  cb.pfnAllocateCb = rotationAllocate; cb.pfnDeallocateCb = rotationDeallocate;
  cb.pfnLockCb = forbiddenLock; cb.pfnUnlockCb = forbiddenUnlock;
  cb.pfnCreateContextCb = fixtureContext; cb.pfnDestroyContextCb = fixtureContextClose;
  cb.pfnRenderCb = forbiddenRender; cb.pfnSetDisplayModeCb = unusedMode;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = fixturePresent;
  device->memory.initialize(&deviceCookie, cb, &dxgi, identity);
  CHECK(deviceStorage.emplace(device.get(), DeviceRecord{DevicePhase::Live, device}).second);
  Resource resources[3]; char runtimeCookies[3];
  const UINT count = scenario == 2 ? 3 : 2;
  const UINT nativeIndex = scenario == 1 ? 1 : 0;
  std::array<std::shared_ptr<PresentSurface>, 3> owners;
  std::array<ComPtr<ID3D11Resource>, 3> images;
  std::array<D3DKMT_HANDLE, 3> allocations{};
  DXGI_DDI_HRESOURCE handles[3] = {};
  for (UINT i = 0; i < count; ++i) {
    auto& resource = resources[i]; resource.owner = device.get();
    resource.nativeBindFlags = D3D10_DDI_BIND_PRESENT | D3D10_DDI_BIND_RENDER_TARGET;
    resource.nativeFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    resource.present = std::make_shared<PresentSurface>();
    if (i == nativeIndex || (allNative && scenario != 6)) {
      resource.present->native = std::make_shared<dxvk::umd::RuntimeAllocation>();
      CHECK(device->memory.allocateNative(*resource.present->native, &runtimeCookies[i], 8, 4,
        DXGI_FORMAT_B8G8R8A8_UNORM, i == nativeIndex,
        i == nativeIndex ? 165 : 0, i == nativeIndex ? 1 : 0) == S_OK);
    } else if (scenario == 6) {
      dxvk::umd::AllocationInfo info; info.flags = 2; info.width = 8; info.height = 4;
      info.pitch = 32; info.size = 128; info.format = 1;
      CHECK(device->memory.adopt(resource.present->allocation, 91, 0, info) == S_OK);
    } else {
      CHECK(device->memory.allocatePresent(resource.present->allocation, &runtimeCookies[i], 8, 4,
        DXGI_FORMAT_B8G8R8A8_UNORM) == S_OK);
    }
    D3D11_TEXTURE2D_DESC desc = {}; desc.Width = scenario == 7 && i == 1 ? 4 : 8;
    desc.Height = 4; desc.MipLevels = desc.ArraySize = 1; desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> image;
    CHECK(device->backend->CreateTexture2D(&desc, nullptr, &image) == S_OK);
    resource.backend = image; images[i] = image; owners[i] = resource.present;
    allocations[i] = resource.present->backing().handle();
    CHECK(resourceStorage.emplace(&resource, ResourceRecord{device.get(), ResourcePhase::Live,
      std::make_shared<const char>(0)}).second);
    handles[i] = reinterpret_cast<DXGI_DDI_HRESOURCE>(&resource);
  }
  DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES args = {};
  args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.get()); args.Resources = count; args.pResources = handles;
  const auto beforeCalls = rotationCalls;
  rotationResult = scenario == 3 ? E_OUTOFMEMORY : S_OK;
  if (scenario == 4) rotationAction = [&] { resourceStorage.erase(&resources[1]); };
  if (scenario == 5) handles[1] = handles[0];
  if (scenario == 8) resources[1].present->active = true;
  if (scenario == 9) device->rotationActive = true;
  if (scenario == 10) resources[nativeIndex].present->nativeRevoked = true;
  const HRESULT hr = rotateResourceIdentities(&args);
  const bool success = scenario <= 2;
  const HRESULT expected = success ? S_OK : scenario == 3 ? E_OUTOFMEMORY
    : scenario == 4 || scenario == 10 ? DXGI_ERROR_DEVICE_REMOVED
    : scenario == 8 || scenario == 9 ? DXGI_ERROR_WAS_STILL_DRAWING : E_INVALIDARG;
  std::printf("MIXED_ROTATION_RESULT actual=%08lx expected=%08lx\n", (unsigned long)hr, (unsigned long)expected);
  CHECK(hr == expected);
  CHECK(rotationCalls == beforeCalls + unsigned(scenario <= 4));
  resources[1].present->active = false; device->rotationActive = false;
  for (UINT i = 0; i < count; ++i) {
    const UINT donor = success ? (i + 1) % count : i;
    CHECK(resources[i].present == owners[donor] && resources[i].backend == images[i]);
    CHECK(resources[i].present->backing().handle() == allocations[donor]);
    CHECK(resources[i].present->backing().runtimeResource() == (scenario == 6 && i != nativeIndex ? nullptr : &runtimeCookies[i]));
    CHECK(bool(resources[i].present->native) == ((allNative && scenario != 6) || donor == nativeIndex));
  }
  for (UINT i = 0; i < count; ++i) {
    expectedRotationRelease = &runtimeCookies[i];
    CHECK(resources[i].present->backing().release() == S_OK);
    resourceStorage.erase(&resources[i]); resources[i].present.reset(); resources[i].backend.Reset();
    owners[i].reset(); images[i].Reset();
  }
  deviceStorage.erase(device.get()); CHECK(device->close() == S_OK);
  CHECK(resourceStorage.empty() && !forbiddenCalls && rotationAllocations == rotationReleases);
  CHECK(multiSurfaceKeys.empty()); allNativeRotation = false;
}
HRESULT APIENTRY unusedMode(HANDLE, D3DDDICB_SETDISPLAYMODE*) { CHECK(false); return E_FAIL; }
void runCreate(unsigned scenario, bool window = false, bool forceShared = false) {
  windowCreate = window || forceShared; identityGeneration = 2;
  initialSharedFallback = forceShared && window && scenario == 19;
  const auto beforeFallbackLocks = fallbackLocks, beforeFallbackUnlocks = fallbackUnlocks;
  std::printf("NATIVE_CREATE scenario=%u window=%u shared=%u\n", scenario, unsigned(window), unsigned(forceShared));
  creationScenario = scenario; nativeOpenMode = scenario != 9; failImage = scenario == 6;
  auto device = std::make_shared<Device>(true);
  const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
    D3D11_SDK_VERSION, &device->backend, nullptr, &device->context) == S_OK);
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid.LowPart = 71; identity->generation = 2; identity->capabilities = 3;
  identity->continuity = std::make_shared<Identity>(); device->adapter = identity;
  D3DDDI_DEVICECALLBACKS cb = {};
  cb.pfnAllocateCb = createWrapper; cb.pfnDeallocateCb = destroyWrapper;
  cb.pfnLockCb = initialSharedFallback ? initialSharedLock : forbiddenLock;
  cb.pfnUnlockCb = initialSharedFallback ? initialSharedUnlock : forbiddenUnlock;
  cb.pfnCreateContextCb = fixtureContext; cb.pfnDestroyContextCb = fixtureContextClose;
  cb.pfnEscapeCb = fixtureEscape; cb.pfnRenderCb = forbiddenRender; cb.pfnSetDisplayModeCb = unusedMode;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = fixturePresent;
  D3D10DDI_CORELAYER_DEVICECALLBACKS core = {}; core.pfnSetErrorCb = fixtureError; device->callbacks10 = &core;
  device->memory.initialize(&deviceCookie, cb, &dxgi, identity);
  device->gpu = dxvk::umd::RuntimeGpu::create(&deviceCookie, cb, identity, device->service);
  CHECK(deviceStorage.emplace(device.get(), DeviceRecord{DevicePhase::Live, device}).second);
  dxvk::umd::RuntimeService::Scope caller(device->service.get());
  const D3D10DDI_HDEVICE h{device.get()};
  alignas(Resource) unsigned char storage[sizeof(Resource)]; D3D10DDI_HRESOURCE handle{storage};
  DXGI_DDI_PRIMARY_DESC primary = {};
  primary.Flags = DXGI_DDI_PRIMARY_OPTIONAL; primary.DriverFlags = 0xdeadbeef;
  primary.ModeDesc.Width = 8; primary.ModeDesc.Height = 4; primary.ModeDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  primary.ModeDesc.RefreshRate = {165, 1};
  D3D10DDI_MIPINFO mip = {}; mip.TexelWidth = mip.PhysicalWidth = 8;
  mip.TexelHeight = mip.PhysicalHeight = 4; mip.TexelDepth = mip.PhysicalDepth = 1;
  D3D10DDIARG_CREATERESOURCE args = {}; args.pMipInfoList = &mip;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.MipLevels = args.ArraySize = 1;
  args.Usage = D3D10_DDI_USAGE_DEFAULT; args.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  args.SampleDesc.Count = 1; args.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT;
  args.pPrimaryDesc = window || scenario == 3 ? nullptr : &primary;
  if (forceShared || (!window && scenario == 4) || (window && scenario == 18)) args.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED;
  if (!window && scenario == 5) primary.ModeDesc.RefreshRate = {};
  uint32_t pixels[32] = {};
  for (unsigned i = 0; i < 32; ++i) pixels[i] = fallbackExpected[i] = 0xff123400u + i;
  D3D10_DDIARG_SUBRESOURCE_UP initial = {}; initial.pSysMem = pixels; initial.SysMemPitch = 32;
  if (window && scenario == 19) args.pInitialDataUP = &initial;
  if (window && scenario == 20) args.SampleDesc.Count = 4;
  if (window && scenario == 21) args.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
  if (window && scenario == 22) args.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
  if (window && scenario == 23) args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  if (window && scenario == 24) args.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  if (scenario == 7) modeQueryAction = [&] { deviceEntry<destroyResource>(h, handle); };
  const auto beforeCaps = capQueries;
  const auto beforeModes = modeQueries, beforeSurfaces = surfaceCreates, beforeWrappers = wrapperCreates;
  lastError = S_OK;
  deviceEntry<createResource>(h, &args, handle, D3D10DDI_HRTRESOURCE{&dxgiCookie});
  const bool queried = !window && !forceShared && (scenario <= 2 || scenario == 6 || scenario == 7 || scenario == 8 || scenario == 10);
  CHECK(modeQueries == beforeModes + unsigned(queried));
  const bool capQueried = windowCreate && scenario != 9
    && scenario != 19 && scenario != 20 && scenario != 21 && (scenario != 22 || forceShared);
  CHECK(capQueries == beforeCaps + unsigned(capQueried));
  const bool failed = window ? (scenario >= 3 && scenario <= 8) || (scenario >= 10 && scenario <= 17)
    : scenario == 6 || scenario == 7 || scenario == 8 || scenario == 10;
  const bool native = scenario == 0 || (window && (scenario == 18 || scenario >= 23))
    || (forceShared && scenario == 22);
  CHECK(FAILED(lastError) == failed && bool(resourceStorage.count(storage)) == !failed);
  CHECK(primary.DriverFlags == (window || failed || scenario == 3 ? 0xdeadbeefu : scenario == 0 && !forceShared ? 0u : 1u));
  CHECK(surfaceCreates == beforeSurfaces + unsigned(native || scenario == 6 || scenario == 10));
  CHECK(wrapperCreates == beforeWrappers + unsigned(window ? (!failed || scenario == 6 || scenario == 10) && (scenario != 22 || forceShared) : scenario != 7 && scenario != 8));
  if (!failed) {
    auto& resource = *reinterpret_cast<Resource*>(storage);
    if (window && scenario == 22 && !forceShared) CHECK(!resource.present);
    else CHECK(resource.present && bool(resource.present->native) == native);
    if (native && (args.MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED)) {
      CHECK(!resource.shared && !device->anySharedSurface && device->sharedSurfaces.empty());
      CHECK(resource.present->native->native() && !resource.present->native->info().flags);
      // Resolve submits the actual imported image; it never maps a staging
      // cache or pretends that an asynchronous GPU submission has completed.
      const auto previousFlushes = flushCalls;
      CHECK(resolveSharedResourceData(device.get(), reinterpret_cast<DXGI_DDI_HRESOURCE>(storage)) == S_OK);
      CHECK(flushCalls == previousFlushes + 1 && !forbiddenCalls);
    }
    deviceEntry<destroyResource>(h, handle);
    CHECK(lastError == S_OK);
  }
  CHECK(!resourceStorage.count(storage) && importedKeys.empty() && surfaceCreates == surfaceFrees);
  deviceStorage.erase(device.get()); CHECK(device->close() == S_OK);
  CHECK(fallbackLocks == beforeFallbackLocks + unsigned(initialSharedFallback));
  CHECK(fallbackUnlocks == beforeFallbackUnlocks + unsigned(initialSharedFallback));
  CHECK(fallbackBacking.empty() && !forbiddenCalls);
  initialSharedFallback = false;
  nativeOpenMode = false; failImage = false; windowCreate = false; identityGeneration = 2;
}
}
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL, ID3D11Device**, ID3D11DeviceContext**, const RuntimeBackend*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  presentOrder += 'F';
  ++flushCalls; context->Flush(); return S_OK;
}
HRESULT dxvk::umd::waitNativeTextureIdle(ID3D11DeviceContext* context, ID3D11Resource* resource) noexcept {
  CHECK(context && resource && !flipCase);
  CHECK(resource == waitSource || resource == waitDestination);
  CHECK(flushRuntimeSubmission(context) == S_OK);
  ++idleWaits;
  if (resource == waitSource) {
    CHECK(handoffs == handoffsBefore);
    presentOrder += 'S';
    if (sourceWaitAction) std::exchange(sourceWaitAction, {})();
    return sourceWaitResult;
  }
  CHECK(handoffs == handoffsBefore + 1);
  presentOrder += 'D';
  if (destinationWaitAction) std::exchange(destinationWaitAction, {})();
  return destinationWaitResult;
}
bool dxvk::umd::nativeSurfacesSupported(ID3D11Device*) noexcept { return nativeOpenMode; }
HRESULT dxvk::umd::createNativeTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, const RuntimeBackend& bridge,
    const mwd_allocation& imported, uint32_t, ID3D11Texture2D** out, bool) noexcept {
  CHECK(nativeOpenMode && out); *out = nullptr; imageToken = imported.token;
  if (imageAction) std::exchange(imageAction, {})();
  if (failImage) return E_OUTOFMEMORY;
  mwd_allocation retained = {};
  const HRESULT retainedResult = bridge.create.callbacks->retain(bridge.create.owner, imported.token, &retained);
  if (retainedResult != S_OK) return retainedResult;
  ComPtr<ID3D11Texture2D> image;
  const HRESULT hr = device->CreateTexture2D(&desc, nullptr, &image);
  CHECK(hr == S_OK);
  auto marker = new TokenMarker(bridge, retained.token);
  CHECK(image->SetPrivateDataInterface(tokenMarker, marker) == S_OK); marker->Release();
  *out = image.Detach(); return S_OK;
}
HRESULT dxvk::umd::rotateNativeTextureStorage(ID3D11DeviceContext* context, UINT count, ID3D11Texture2D* const* textures, const TextureStorageCommit& commit) noexcept {
  CHECK(context && count >= 2 && textures); ++rotationCalls;
  for (UINT i = 0; i < count; ++i) CHECK(textures[i]);
  if (rotationAction) std::exchange(rotationAction, {})();
  if (rotationResult != S_OK) return rotationResult;
  return commit ? commit([] {}) : S_OK;
}
HRESULT dxvk::umd::prepareNativeTextureForExternalWrite(ID3D11DeviceContext* context, ID3D11Resource* resource) noexcept {
  CHECK(context && resource); ++handoffs;
  if (waitDestination) {
    CHECK(resource == waitDestination && idleWaits == idleWaitsBefore + 1);
    presentOrder += 'P';
  } else CHECK(nativeOpenMode);
  if (prepareAction) std::exchange(prepareAction, {})();
  return prepareResult;
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  for (unsigned i = 0; i < 11; ++i) runMixedRotation(i);
  std::printf("PASS mixed rotation ownership checks=%u storage_calls=%u hardware_admission=0\n", checks, rotationCalls);
  for (unsigned i = 0; i < 14; ++i) run(i);
  CHECK(presentCalls == 4 && handoffs == 10 && idleWaits == 22 && !forbiddenCalls && resourceStorage.empty() && deviceStorage.empty());
  std::printf("PASS native destination Present checks=%u callbacks=%u cpu_transfers=0 hardware_admission=0\n", checks, presentCalls);
  for (unsigned i = 0; i < 9; ++i) run(i, true);
  CHECK(presentCalls == 9 && handoffs == 10 && idleWaits == 22 && !forbiddenCalls && resourceStorage.empty() && deviceStorage.empty());
  std::printf("PASS native Flip checks=%u callbacks=%u destination_writes=0 hardware_admission=0\n", checks, presentCalls);
  for (unsigned i = 0; i < 8; ++i) run(i, false, true);
  CHECK(displayModeCalls == 4 && idleWaits == 29 && !forbiddenCalls && resourceStorage.empty() && deviceStorage.empty());
  std::printf("PASS native SetDisplayMode checks=%u callbacks=%u cpu_transfers=0 hardware_admission=0\n", checks, displayModeCalls);
  for (unsigned i = 0; i < 9; ++i) runOpen(i);
  std::printf("PASS native OpenResource checks=%u imports=%u releases=%u hardware_admission=0\n", checks, imports, releases);
  for (unsigned i = 0; i < 11; ++i) runCreate(i);
  std::printf("PASS native primary creation checks=%u queries=%u surfaces=%u hardware_admission=0\n", checks, modeQueries, surfaceCreates);
  for (unsigned i = 0; i < 25; ++i) runCreate(i, true);
  std::printf("PASS native window creation checks=%u queries=%u surfaces=%u hardware_admission=0\n", checks, capQueries, surfaceCreates);
  for (unsigned i = 0; i < 20; ++i) runCreate(i, true, true);
  runCreate(22, true, true); // Shader/RT shared resource with no Present binding.
  runCreate(23, true, true); // RGBA shared backing.
  runCreate(0, false, true); // Optional shared primary retains NO_SCANOUT.
  std::printf("PASS native shared creation/Resolve checks=%u native_cpu_locks=%u initial_fallback_locks=%u hardware_admission=0\n", checks, forbiddenCalls, fallbackLocks);
  for (unsigned i = 0; i < 11; ++i) runMixedRotation(i, true);
  std::printf("PASS all-native primary/window rotation checks=%u hardware_admission=0\n", checks);
}
