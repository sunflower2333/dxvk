// SPDX-License-Identifier: MIT
// Actual typed DXGI tables and production primary publication. WARP replaces
// only the embedded renderer; runtime callbacks below are controlled owners.
// This fixture makes no hardware or ordinary-runtime admission claim.
#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_allocation.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

static std::atomic<unsigned> checks{0};
static unsigned snapshots, pixels, pairAllocations, renders, modes, presents, releases, contexts, contextCloses, locks, unlocks, unlockAttempts, unlockFailures;
static unsigned runtimeTerminalReleases, runtimeTerminalMapClosures;
static DWORD caller;
static bool runtimeValid = true, malformedRender, nonExactAllocate, allocationFails;
static unsigned malformedAllocation;
static HRESULT lastError = S_OK, modeResult = S_OK, renderResult = S_OK, lockResult = S_OK, unlockResult = S_OK;
static char deviceCookie, adapterCookie, coreCookie, contextCookie;
static LUID selected{0x13579024, -51};
static uint64_t generation = 23;
static std::function<void()> allocateBeforeAction, allocateBeforeContinuation, allocateAction, lockBeforeAction, lockAction, unlockAction, contextAction, renderAction, modeAction, queryAction;
static unsigned queryCountdown, lockCountdown = 1, unlockFailureCountdown;
static D3DKMT_HANDLE expectedHandle, nextHandle = 301;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "DXGI primary line=%d: %s error=%08lx\n", __LINE__, #value, static_cast<unsigned long>(lastError)); std::abort(); \
} } while (0)
static void callback() { CHECK(runtimeValid && GetCurrentThreadId() == caller); }
struct Backing {
  HANDLE runtime;
  dxvk::umd::AllocationInfo info;
  void* page;
  bool mapped = false;
  uint8_t* data() const { return static_cast<uint8_t*>(page) + 16; }
};
static std::unordered_map<D3DKMT_HANDLE, Backing> owned;
static std::vector<void*> retiredPages;
static std::array<uint8_t, 4096> commands[2];
static D3DDDI_ALLOCATIONLIST lists[2][8];
static D3DDDI_PATCHLOCATIONLIST patches[2][8];
static unsigned buffer;
static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* request) {
  callback(); CHECK(runtime == &adapterCookie && request && request->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(request->pPrivateDriverData);
  for (unsigned i = 0; i < 160; ++i) CHECK(bytes[i] == 0);
  auto put = [&](unsigned offset, uint64_t word, unsigned size) {
    for (unsigned i = 0; i < size; ++i) bytes[offset + i] = uint8_t(word >> (i * 8));
  };
  put(0, 0x504d5644, 4); put(8, 128, 4); put(16, 3, 8); put(24, generation, 8);
  put(128, 0x44494c56, 4); put(132, 1, 4); put(136, 32, 4); put(140, 1, 4); put(152, 1, 4);
  std::memcpy(bytes + 144, &selected, sizeof(selected));
  if (queryAction && --queryCountdown == 0) std::exchange(queryAction, {})();
  return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hResource && request->pAllocationInfo);
  CHECK(request->NumAllocations == 1 || request->NumAllocations == 2);
  if (allocateBeforeAction) {
    CHECK(!request->hKMResource && !request->pAllocationInfo[0].hAllocation
      && (request->NumAllocations == 1 || !request->pAllocationInfo[1].hAllocation));
    std::exchange(allocateBeforeAction, {})();
    // The reentrant owner canceled this request before any kernel ownership
    // was acquired. Do not invent outputs after that owner has returned.
    return DXGI_ERROR_DEVICE_REMOVED;
  }
  if (allocateBeforeContinuation) std::exchange(allocateBeforeContinuation, {})();
  if (allocationFails) return E_OUTOFMEMORY;
  if (malformedAllocation == 6) return S_OK; // Success status alone acquired nothing.
  for (UINT i = 0; i < request->NumAllocations; ++i) {
    auto& entry = request->pAllocationInfo[i]; dxvk::umd::AllocationInfo info;
    CHECK(entry.pPrivateDriverData && entry.PrivateDriverDataSize == 80);
    std::memcpy(&info, entry.pPrivateDriverData, 80);
    CHECK(info.magic == 0x504d5644 && !info.version && info.headerSize == 80 && !info.reserved);
    CHECK(info.alignment == 4096 && !info.requestedIova && !info.resetGeneration && !info.contextId);
    CHECK(info.width == 8 && info.height == 4 && info.pitch == 32 && info.size == 128);
    CHECK(info.format >= 1 && info.format <= 3 && !entry.VidPnSourceId);
    const bool primary = request->NumAllocations == 2 && i == 0;
    CHECK(info.flags == (primary ? 1u : 2u) && bool(entry.Flags.Primary) == primary);
    CHECK(info.refreshNumerator == (primary ? 60000u : 0u)
      && info.refreshDenominator == (primary ? 1001u : 0u));
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); CHECK(page);
    std::memset(page, 0xa5, 4096);
    const auto handle = nextHandle++; CHECK(owned.emplace(handle, Backing{request->hResource, info, page}).second);
    entry.hAllocation = handle;
    // A callback cannot redefine the UMD's retained format/shape/refresh.
    std::memset(entry.pPrivateDriverData, 0xff, 80);
  }
  request->hKMResource = nextHandle + 1000;
  if (request->NumAllocations == 2) ++pairAllocations;
  if (malformedAllocation == 1) request->pAllocationInfo[0].hAllocation = 0;
  if (malformedAllocation == 2) request->pAllocationInfo[1].hAllocation = 0;
  if (malformedAllocation == 3) request->pAllocationInfo[1].hAllocation = request->pAllocationInfo[0].hAllocation;
  if (malformedAllocation == 4) request->hKMResource = 0;
  if (malformedAllocation == 5) request->NumAllocations = 1;
  if (allocateAction) std::exchange(allocateAction, {})();
  return nonExactAllocate ? S_FALSE : S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hResource
    && !request->NumAllocations && !request->HandleList);
  unsigned count = 0;
  for (auto entry = owned.begin(); entry != owned.end();) {
    if (entry->second.runtime != request->hResource) { ++entry; continue; }
    auto& backing = entry->second;
    CHECK(!backing.mapped); // Every successful lock closes before deallocation.
    for (unsigned i = 0; i < 16; ++i) CHECK(static_cast<uint8_t*>(backing.page)[i] == 0xa5);
    for (unsigned i = 144; i < 4096; ++i) CHECK(static_cast<uint8_t*>(backing.page)[i] == 0xa5);
    DWORD previous; CHECK(VirtualProtect(backing.page, 4096, PAGE_NOACCESS, &previous));
    retiredPages.push_back(backing.page); entry = owned.erase(entry); ++count;
  }
  CHECK(count == 1 || count == 2); ++releases; return S_OK;
}
static void runtimeTerminalCleanup() {
  // This is the synthetic runtime's own device cleanup, not a UMD callback.
  // A nested DestroyDevice cannot know the suspended UnlockCb's result. The
  // UMD must report removal and make no late callback or mapped Deallocate.
  CHECK(!runtimeValid && GetCurrentThreadId() == caller && owned.size() == 2);
  for (auto& entry : owned) {
    auto& backing = entry.second;
    if (backing.mapped) { backing.mapped = false; ++runtimeTerminalMapClosures; }
    for (unsigned i = 0; i < 16; ++i) CHECK(static_cast<uint8_t*>(backing.page)[i] == 0xa5);
    for (unsigned i = 144; i < 4096; ++i) CHECK(static_cast<uint8_t*>(backing.page)[i] == 0xa5);
    DWORD previous; CHECK(VirtualProtect(backing.page, 4096, PAGE_NOACCESS, &previous));
    retiredPages.push_back(backing.page);
  }
  owned.clear(); ++runtimeTerminalReleases;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* request) {
  callback(); CHECK(device == &deviceCookie && request && owned.count(request->hAllocation));
  CHECK(owned.at(request->hAllocation).info.flags == 2); // Never map a primary.
  CHECK(request->Flags.LockEntire && !request->Flags.Discard && !request->Flags.IgnoreSync);
  if (lockBeforeAction) {
    CHECK(!request->pData); std::exchange(lockBeforeAction, {})();
    return DXGI_ERROR_DEVICE_REMOVED;
  }
  if (FAILED(lockResult)) return lockResult;
  auto& backing = owned.at(request->hAllocation);
  CHECK(!backing.mapped); backing.mapped = true; ++locks;
  request->pData = backing.data();
  if (lockAction && --lockCountdown == 0) std::exchange(lockAction, {})();
  return lockResult;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* request) {
  callback(); CHECK(device == &deviceCookie && request && request->NumAllocations == 1 && request->phAllocations);
  CHECK(owned.count(*request->phAllocations) && owned.at(*request->phAllocations).info.flags == 2);
  CHECK(owned.at(*request->phAllocations).mapped);
  ++unlockAttempts;
  const HRESULT result = unlockFailureCountdown && --unlockFailureCountdown == 0 ? E_OUTOFMEMORY : unlockResult;
  if (SUCCEEDED(result)) { owned.at(*request->phAllocations).mapped = false; ++unlocks; }
  else ++unlockFailures;
  if (unlockAction) std::exchange(unlockAction, {})();
  return result;
}
static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* request) {
  callback(); CHECK(device == &deviceCookie && request && !request->NodeOrdinal && request->EngineAffinity == 1
    && !request->Flags.Value && !request->pPrivateDriverData && !request->PrivateDriverDataSize);
  ++contexts; buffer = 0; request->hContext = &contextCookie;
  request->pCommandBuffer = commands[0].data(); request->CommandBufferSize = 4096;
  request->pAllocationList = lists[0]; request->AllocationListSize = 8;
  request->pPatchLocationList = patches[0]; request->PatchLocationListSize = 8;
  if (contextAction) std::exchange(contextAction, {})();
  return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hContext == &contextCookie);
  ++contextCloses; return S_OK;
}
static HRESULT APIENTRY render(HANDLE device, D3DDDICB_RENDER* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hContext == &contextCookie);
  CHECK(request->CommandLength == 64 && !request->CommandOffset && request->NumAllocations == 2
    && !request->NumPatchLocations && !request->Flags.Value);
  CHECK(request->NewCommandBufferSize == 4096 && request->NewAllocationListSize == 8
    && request->NewPatchLocationListSize == 8);
  std::array<uint32_t, 16> words; std::memcpy(words.data(), commands[buffer].data(), 64);
  CHECK((words == std::array<uint32_t, 16>{0x504d5644, 0, 64, 0, 2, 0, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0}));
  CHECK(lists[buffer][0].hAllocation != lists[buffer][1].hAllocation && !lists[buffer][0].WriteOperation
    && lists[buffer][1].WriteOperation && !lists[buffer][0].Reserved && !lists[buffer][1].Reserved);
  const auto& src = owned.at(lists[buffer][0].hAllocation); auto& dst = owned.at(lists[buffer][1].hAllocation);
  CHECK(src.runtime == dst.runtime && src.info.flags != dst.info.flags && !src.mapped && !dst.mapped);
  ++renders;
  if (renderResult == S_OK) std::memcpy(dst.data(), src.data(), 128);
  buffer ^= 1;
  request->pNewCommandBuffer = malformedRender ? nullptr : commands[buffer].data(); request->NewCommandBufferSize = 4096;
  request->pNewAllocationList = lists[buffer]; request->NewAllocationListSize = 8;
  request->pNewPatchLocationList = patches[buffer]; request->NewPatchLocationListSize = 8;
  if (renderAction) std::exchange(renderAction, {})();
  return renderResult;
}
static HRESULT APIENTRY mode(HANDLE device, D3DDDICB_SETDISPLAYMODE* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hPrimaryAllocation == expectedHandle);
  CHECK(owned.count(expectedHandle) && owned.at(expectedHandle).info.flags == 1);
  CHECK(!request->PrivateDriverFormatAttribute); ++modes;
  if (modeAction) std::exchange(modeAction, {})();
  if (modeResult == D3DDDIERR_INCOMPATIBLEPRIVATEFORMAT) request->PrivateDriverFormatAttribute = 71;
  return modeResult;
}
static HRESULT APIENTRY present(HANDLE device, DXGIDDICB_PRESENT* request) {
  callback(); CHECK(device == &deviceCookie && request && request->hSrcAllocation == expectedHandle);
  CHECK(request->hContext == &contextCookie && request->pDXGIContext == &adapterCookie
    && !request->hDstAllocation && owned.at(expectedHandle).info.flags == 2);
  ++presents; return S_OK;
}
static HRESULT APIENTRY wrongMode(HANDLE, D3DDDICB_SETDISPLAYMODE*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY wrongRender(HANDLE, D3DDDICB_RENDER*) { CHECK(false); return E_FAIL; }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  callback(); CHECK(runtime.handle == &coreCookie && FAILED(result)); lastError = result;
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
  SIZE_T capacity;
  explicit Storage(SIZE_T size) : bytes(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)), capacity(size) { CHECK(size && bytes); }
  void protect(DWORD access = PAGE_NOACCESS) { DWORD previous; CHECK(VirtualProtect(bytes, 1, access, &previous)); }
  ~Storage() { CHECK(VirtualFree(bytes, 0, MEM_RELEASE)); }
};
template<typename Table, bool modernDxgi>
struct Fixture {
  using Desc = std::conditional_t<std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>, D3D11DDIARG_CREATERESOURCE, D3D10DDIARG_CREATERESOURCE>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()}, kernelStorage{sizeof(D3DDDI_DEVICECALLBACKS)};
  D3D10DDI_HDEVICE device{storage.bytes}; Table table{};
  struct DxgiStorage {
    std::conditional_t<modernDxgi, DXGI1_1_DDI_BASE_FUNCTIONS, DXGI_DDI_BASE_FUNCTIONS> table{};
    uintptr_t guards[2]{0x13579, 0x24680};
  } dxgi;
  DXGI_DDI_BASE_CALLBACKS dxgiCallbacks{};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{}; D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  bool live = true;
  explicit Fixture(bool primaryCallbacks = true) {
    runtimeValid = true; generation = 23; lockCountdown = 1;
    auto kernel = static_cast<D3DDDI_DEVICECALLBACKS*>(kernelStorage.bytes);
    kernel->pfnAllocateCb = allocate; kernel->pfnDeallocateCb = deallocate;
    kernel->pfnLockCb = lock; kernel->pfnUnlockCb = unlock;
    kernel->pfnCreateContextCb = createContext; kernel->pfnDestroyContextCb = destroyContext;
    if (primaryCallbacks) { kernel->pfnRenderCb = render; kernel->pfnSetDisplayModeCb = mode; }
    dxgiCallbacks.pfnPresentCb = present; core10.pfnSetErrorCb = error; core11.pfnSetErrorCb = error;
    D3D10DDIARG_CREATEDEVICE args{};
    if constexpr (std::is_same_v<Table, D3D10DDI_DEVICEFUNCS>) {
      args.Interface = D3D10_0_DDI_INTERFACE_VERSION; args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
      args.pDeviceFuncs = &table; args.pUMCallbacks = &core10;
    } else if constexpr (std::is_same_v<Table, D3D10_1DDI_DEVICEFUNCS>) {
      args.Interface = D3D10_1_DDI_INTERFACE_VERSION; args.Version = D3D10_1_DDI_BUILD_VERSION << 16;
      args.p10_1DeviceFuncs = &table; args.pUMCallbacks = &core10;
    } else {
      args.Interface = D3D11_0_DDI_INTERFACE_VERSION; args.Version = D3D11_0_DDI_BUILD_VERSION << 16;
      args.Flags = D3D11DDI_3DPIPELINELEVEL_11_0 << D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
      args.p11DeviceFuncs = &table; args.p11UMCallbacks = &core11;
    }
    if constexpr (modernDxgi) args.Version |= std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>
      ? DXGI_RESOLVE_SHARED_RESOURCE : (VISTA_GOLD_PRODUCT_VER | DXGI_RESOLVE_SHARED_RESOURCE);
    CHECK(dxvk::umd::nativeDxgiUses1_1(args.Interface, args.Version) == modernDxgi);
    args.hDrvDevice = device; args.hRTDevice.handle = &deviceCookie; args.hRTCoreLayer.handle = &coreCookie;
    args.pKTCallbacks = kernel; args.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgiCallbacks;
    if constexpr (modernDxgi) args.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = &dxgi.table;
    else args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgi.table;
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->luid = selected; identity->runtime = &adapterCookie; identity->query = query;
    identity->generation = generation; identity->capabilities = 3;
    CHECK(dxvk::umd::createAdapterDevice(identity, &args) == S_OK);
    CHECK(dxgi.guards[0] == 0x13579 && dxgi.guards[1] == 0x24680);
    CHECK(dxgi.table.pfnGetGammaCaps && dxgi.table.pfnSetDisplayMode && dxgi.table.pfnPresent);
    CHECK(dxgi.table.pfnGetGammaCaps(nullptr) == E_INVALIDARG && dxgi.table.pfnSetDisplayMode(nullptr) == E_INVALIDARG);
    kernel->pfnSetDisplayModeCb = wrongMode; kernel->pfnRenderCb = wrongRender;
    kernelStorage.protect(); // The original kernel prefix is a bounded snapshot.
  }
  void retire() { CHECK(live); live = false; table.pfnDestroyDevice(device); storage.protect(); runtimeValid = false; }
  ~Fixture() {
    if (live) retire(); CHECK(owned.empty());
    for (auto page : retiredPages) CHECK(VirtualFree(page, 0, MEM_RELEASE)); retiredPages.clear();
    CHECK(!allocateBeforeAction && !allocateBeforeContinuation && !allocateAction && !lockBeforeAction && !lockAction
      && !unlockAction && !contextAction && !renderAction && !modeAction && !queryAction);
    CHECK(locks == unlocks + runtimeTerminalMapClosures);
  }
  HRESULT set(DXGI_DDI_HRESOURCE resource, UINT subresource = 0) {
    DXGI_DDI_ARG_SETDISPLAYMODE args{reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate), resource, subresource};
    return dxgi.table.pfnSetDisplayMode(&args);
  }
  HRESULT gamma(void* output) {
    DXGI_DDI_ARG_GET_GAMMA_CONTROL_CAPS args{};
    args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate);
    args.pGammaCapabilities = static_cast<DXGI_GAMMA_CONTROL_CAPABILITIES*>(output);
    return dxgi.table.pfnGetGammaCaps(&args);
  }
};
template<typename F>
struct Texture {
  F& fixture; Storage storage;
  D3D10DDI_HRESOURCE handle{storage.bytes}; char runtime{}; bool live = false;
  DXGI_DDI_PRIMARY_DESC primary{};
  D3DKMT_HANDLE allocation = 0;
  Texture(F& f, DXGI_FORMAT format, bool optional = false, unsigned malformed = 0)
  : fixture(f), storage(f.table.pfnCalcPrivateResourceSize(f.device, nullptr)) {
    D3D10DDI_MIPINFO shapes[2]{{8, 4, 1, 8, 4, 1}, {4, 2, 1, 4, 2, 1}};
    typename F::Desc args{}; args.pMipInfoList = shapes; args.MipLevels = args.ArraySize = 1;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Usage = D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT;
    args.Format = format; args.SampleDesc = {1, 0}; args.pPrimaryDesc = &primary;
    primary.Flags = optional ? DXGI_DDI_PRIMARY_OPTIONAL : 0; primary.DriverFlags = 0x12345678;
    primary.ModeDesc = {8, 4, format, {60000, 1001}, DXGI_DDI_MODE_SCANLINE_ORDER_PROGRESSIVE,
      DXGI_DDI_MODE_ROTATION_IDENTITY, DXGI_DDI_MODE_SCALING_UNSPECIFIED};
    switch (malformed) {
      case 1: primary.VidPnSourceId = 1; break;
      case 2: primary.Flags = DXGI_DDI_PRIMARY_STEREO; break;
      case 3: primary.Flags = DXGI_DDI_PRIMARY_INDIRECT; break;
      case 4: primary.ModeDesc.Rotation = DXGI_DDI_MODE_ROTATION_ROTATE90; break;
      case 5: primary.ModeDesc.ScanlineOrdering = DXGI_DDI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST; break;
      case 6: primary.ModeDesc.Scaling = DXGI_DDI_MODE_SCALING_CENTERED; break;
      case 7: primary.ModeDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
      case 8: primary.ModeDesc.Width = 9; break;
      case 9: primary.ModeDesc.RefreshRate.Denominator = 0; break;
      case 10: primary.Flags = 16; break;
      case 11: args.ArraySize = 2; break;
      case 12: args.MipLevels = 2; break;
      case 13: args.SampleDesc.Count = 2; break;
      case 14: args.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED; break;
      case 15: args.Usage = D3D10_DDI_USAGE_DYNAMIC; args.MapFlags = D3D10_DDI_CPU_ACCESS_WRITE; break;
      case 17: args.BindFlags = D3D10_DDI_BIND_RENDER_TARGET; break;
      case 18: shapes[0].PhysicalWidth = 9; break;
    }
    std::memset(storage.bytes, 0xa5, storage.capacity);
    lastError = S_OK; const auto before = owned.size();
    f.table.pfnCreateResource(f.device, &args, handle, {&runtime});
    if (malformed || nonExactAllocate || allocationFails || malformedAllocation || !f.live) {
      CHECK(primary.DriverFlags == 0x12345678 && owned.size() == before);
      for (SIZE_T i = 0; i < storage.capacity; ++i)
        CHECK(static_cast<uint8_t*>(storage.bytes)[i] == 0xa5);
      if (f.live) CHECK(FAILED(lastError));
      return;
    }
    CHECK(lastError == S_OK && primary.DriverFlags == (optional ? 1u : 0u)); live = true;
    for (const auto& entry : owned) if (entry.second.runtime == &runtime
        && entry.second.info.flags == (optional ? 2u : 1u)) allocation = entry.first;
    CHECK(allocation);
  }
  void clear(std::array<FLOAT, 4> color) {
    CHECK(live && fixture.live);
    D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = handle;
    args.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    args.Format = owned.at(allocation).info.format == 1 ? DXGI_FORMAT_B8G8R8A8_UNORM
      : owned.at(allocation).info.format == 2 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D = {0, 0, 1};
    Storage target(fixture.table.pfnCalcPrivateRenderTargetViewSize(fixture.device, &args));
    D3D10DDI_HRENDERTARGETVIEW view{target.bytes}; lastError = S_OK;
    fixture.table.pfnCreateRenderTargetView(fixture.device, &args, view, {}); CHECK(lastError == S_OK);
    fixture.table.pfnClearRenderTargetView(fixture.device, view, color.data()); CHECK(lastError == S_OK);
    fixture.table.pfnDestroyRenderTargetView(fixture.device, view);
  }
  ~Texture() { if (live && fixture.live) fixture.table.pfnDestroyResource(fixture.device, handle); }
  void retire() {
    CHECK(live && fixture.live); live = false;
    fixture.table.pfnDestroyResource(fixture.device, handle); storage.protect();
  }
};
static void save(const char* name, const void* data, size_t bytes) {
  FILE* file = nullptr; CHECK(fopen_s(&file, name, "wb") == 0 && file);
  CHECK(std::fwrite(data, 1, bytes, file) == bytes); CHECK(std::fclose(file) == 0);
}
static void capture(unsigned profile, unsigned image, D3DKMT_HANDLE handle, uint32_t expected) {
  const auto& backing = owned.at(handle); std::array<uint32_t, 32> actual, reference;
  std::memcpy(actual.data(), backing.data(), 128); reference.fill(expected);
  char path[100]; std::snprintf(path, sizeof(path), "primary-%u-%u.actual.bin", profile, image); save(path, actual.data(), 128);
  std::snprintf(path, sizeof(path), "primary-%u-%u.expected.bin", profile, image); save(path, reference.data(), 128);
  const uint32_t metadata[8]{profile, image, 8, 4, backing.info.format, backing.info.flags,
    backing.info.refreshNumerator, backing.info.refreshDenominator};
  std::snprintf(path, sizeof(path), "primary-%u-%u.metadata.bin", profile, image); save(path, metadata, sizeof(metadata));
  CHECK(actual == reference); ++snapshots; pixels += 32;
}
template<typename Table, bool dxgi11>
static void profile(unsigned number) {
  Fixture<Table, dxgi11> f;
  std::array<uint8_t, sizeof(DXGI_GAMMA_CONTROL_CAPABILITIES)> gamma; gamma.fill(0xa5);
  CHECK(f.gamma(gamma.data()) == DXGI_DDI_ERR_UNSUPPORTED);
  for (auto byte : gamma) CHECK(byte == 0xa5);
  CHECK(f.gamma(nullptr) == E_INVALIDARG); Storage guard(sizeof(DXGI_GAMMA_CONTROL_CAPABILITIES)); guard.protect();
  CHECK(f.gamma(guard.bytes) == DXGI_DDI_ERR_UNSUPPORTED);
  for (unsigned malformed = 1; malformed <= 18; ++malformed) {
    if (malformed == 16) continue; // Missing callbacks is a separate fixture.
    const auto oldPairs = pairAllocations, oldModes = modes;
    Texture<decltype(f)> bad(f, DXGI_FORMAT_R8G8B8A8_UNORM, false, malformed);
    CHECK(pairAllocations == oldPairs && modes == oldModes);
  }
  const DXGI_FORMAT formats[3]{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM};
  const std::array<FLOAT, 4> colors[3]{{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
  const uint32_t words[3]{0xff0000ff, 0xff00ff00, 0xff0000ff};
  for (unsigned image = 0; image < 3; ++image) {
    const auto oldModes = modes; Texture<decltype(f)> texture(f, formats[image]); CHECK(modes == oldModes);
    texture.clear(colors[image]); expectedHandle = texture.allocation;
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate), 1) == E_INVALIDARG);
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(&adapterCookie)) == E_INVALIDARG);
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == S_OK && modes == oldModes + 1);
    capture(number, image, texture.allocation, words[image]);
    for (const auto failure : {E_OUTOFMEMORY, D3DDDIERR_INCOMPATIBLEPRIVATEFORMAT, S_FALSE}) {
      modeResult = failure;
      CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == (FAILED(failure) ? failure : E_FAIL));
    }
    modeResult = S_OK;
    modeAction = [&] { CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == DXGI_ERROR_WAS_STILL_DRAWING); };
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == S_OK);
  }
  Texture<decltype(f)> optional(f, DXGI_FORMAT_R8G8B8A8_UNORM, true);
  optional.clear({0, 1, 1, 1}); expectedHandle = optional.allocation;
  const auto oldModes = modes, oldRenders = renders;
  CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(optional.handle.pDrvPrivate)) == DXGI_DDI_ERR_UNSUPPORTED);
  DXGI_DDI_ARG_PRESENT presentArgs{}; presentArgs.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);
  presentArgs.hSurfaceToPresent = reinterpret_cast<DXGI_DDI_HRESOURCE>(optional.handle.pDrvPrivate);
  presentArgs.pDXGIContext = &adapterCookie; presentArgs.Flags.Blt = 1;
  CHECK(f.dxgi.table.pfnPresent(&presentArgs) == S_OK && modes == oldModes && renders == oldRenders);
  capture(number, 3, optional.allocation, 0xffffff00);
}
static void retirement(unsigned point) {
  Fixture<D3D10_1DDI_DEVICEFUNCS, true> f;
  const auto oldReleases = releases, oldLocks = locks, oldUnlocks = unlocks;
  if (point == 0 || point == 7) {
    if (point == 0) allocateAction = [&] { f.retire(); };
    else allocateBeforeAction = [&] { f.retire(); };
    Texture<decltype(f)> failed(f, DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(!f.live && owned.empty() && releases == oldReleases + (point == 0 ? 1u : 0u));
    CHECK(locks == oldLocks && unlocks == oldUnlocks); return;
  }
  Texture<decltype(f)> texture(f, DXGI_FORMAT_R8G8B8A8_UNORM); texture.clear({1, 0, 1, 1});
  expectedHandle = texture.allocation;
  auto retire = [&] { f.retire(); };
  if (point == 1) lockAction = retire;
  if (point == 2) unlockAction = retire;
  if (point == 3) contextAction = retire;
  if (point == 4) renderAction = retire;
  if (point == 5) modeAction = retire;
  if (point == 6) { queryAction = retire; queryCountdown = 2; } // After synchronized LockCb.
  if (point == 8) { lockAction = retire; lockCountdown = 2; } // Successful completion-barrier lock.
  if (point == 9 || point == 10) {
    lockCountdown = 2;
    lockAction = [&] {
      texture.retire();
      if (point == 10) f.retire();
    };
  }
  if (point == 11) lockBeforeAction = retire; // No mapping acquired yet.
  CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == DXGI_ERROR_DEVICE_REMOVED);
  if (point == 2) {
    CHECK(!f.live && owned.size() == 2 && releases == oldReleases && lastError == D3DDDIERR_DEVICEREMOVED);
    for (const auto& entry : owned) CHECK(!entry.second.mapped);
    runtimeTerminalCleanup(); // Successful callback, status still in flight at DestroyDevice.
  }
  CHECK(releases == oldReleases + (point == 2 ? 0u : 1u) && locks - oldLocks == unlocks - oldUnlocks);
  CHECK(locks - oldLocks == (point == 11 ? 0u : point <= 4 || point == 6 ? 1u : 2u));
  if (point == 9) {
    CHECK(f.live && !texture.live && owned.empty());
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_INVALIDARG);
    return;
  }
  CHECK(!f.live && owned.empty());
  CHECK(f.gamma(reinterpret_cast<void*>(1)) == DXGI_ERROR_DEVICE_REMOVED);
}
static void pitchedTransfer() {
  runtimeValid = true;
  dxvk::umd::RuntimeMemory memory; dxvk::umd::RuntimeAllocation allocation;
  auto service = std::make_shared<dxvk::umd::RuntimeService>();
  dxvk::umd::RuntimeService::Scope scope(service.get());
  D3DDDI_DEVICECALLBACKS kernel{}; DXGI_DDI_BASE_CALLBACKS dxgi{};
  kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
  kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
  kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
  kernel.pfnRenderCb = render; kernel.pfnSetDisplayModeCb = mode; dxgi.pfnPresentCb = present;
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid = selected; identity->runtime = &adapterCookie; identity->query = query;
  identity->generation = generation; identity->capabilities = 3;
  memory.initialize(&deviceCookie, kernel, &dxgi, identity, service);
  char resource;
  CHECK(memory.allocatePrimary(allocation, &resource, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == S_OK);
  expectedHandle = allocation.handle();
  CHECK(memory.setDisplayMode(allocation) == E_INVALIDARG); // Not published yet.
  std::array<uint32_t, 40> upload; upload.fill(0xa5a5a5a5);
  for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 8; ++x)
    upload[y * 10 + x] = 0xff000000u | x | (y << 8) | ((x + y) << 16);
  renderAction = [&] { CHECK(memory.upload(allocation, upload.data(), 40) == DXGI_ERROR_WAS_STILL_DRAWING); };
  CHECK(memory.upload(allocation, upload.data(), 40) == S_OK);
  CHECK(memory.setDisplayMode(allocation) == S_OK);
  for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 8; ++x) {
    uint32_t word; std::memcpy(&word, owned.at(expectedHandle).data() + (y * 8 + x) * 4, 4);
    CHECK(word == (0xff000000u | x | (y << 8) | ((x + y) << 16)));
  }
  // The reverse scheduled copy must use primary as source, staging as
  // destination, then a blocking ReadOnly lock. Local pitch padding survives.
  std::array<uint32_t, 48> download; download.fill(0x39393939);
  CHECK(memory.download(allocation, download.data(), 48) == S_OK);
  for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 12; ++x)
    CHECK(download[y * 12 + x] == (x < 8 ? 0xff000000u | x | (y << 8) | ((x + y) << 16) : 0x39393939u));
  dxvk::umd::RuntimeAllocation moved(std::move(allocation)); CHECK(!allocation.handle() && moved.primary());
  CHECK(memory.closeDeviceAllocations() == S_OK && !moved.handle() && moved.release() == S_OK && owned.empty());
  service->close(); runtimeValid = false;
  for (auto page : retiredPages) CHECK(VirtualFree(page, 0, MEM_RELEASE)); retiredPages.clear();
}
static void mappedOwnerRetirement(bool moveOwner) {
  runtimeValid = true; lockCountdown = 2;
  dxvk::umd::RuntimeMemory memory; dxvk::umd::RuntimeAllocation allocation;
  auto service = std::make_shared<dxvk::umd::RuntimeService>();
  dxvk::umd::RuntimeService::Scope scope(service.get());
  D3DDDI_DEVICECALLBACKS kernel{}; DXGI_DDI_BASE_CALLBACKS dxgi{};
  kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
  kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
  kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
  kernel.pfnRenderCb = render; kernel.pfnSetDisplayModeCb = mode; dxgi.pfnPresentCb = present;
  memory.initialize(&deviceCookie, kernel, &dxgi, {}, service);
  char resource;
  CHECK(memory.allocatePrimary(allocation, &resource, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == S_OK);
  const auto oldReleases = releases, oldLocks = locks, oldUnlocks = unlocks;
  lockAction = [&] {
    // Retire a real successful completion-barrier mapping while LockCb is
    // still in progress. Both the pending request and ledger follow a move.
    if (moveOwner) {
      dxvk::umd::RuntimeAllocation moved(std::move(allocation));
      CHECK(moved.release() == S_OK && !moved.handle());
    } else CHECK(allocation.release() == S_OK);
    CHECK(owned.empty()); // Deallocate made pData PAGE_NOACCESS before return.
  };
  std::array<uint32_t, 32> upload{};
  CHECK(memory.upload(allocation, upload.data(), 32) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(!allocation.handle() && owned.empty() && releases == oldReleases + 1
    && locks == oldLocks + 2 && unlocks == oldUnlocks + 2 && !lockAction);
  CHECK(memory.close() == S_OK && memory.closeDeviceAllocations() == S_OK);
  service->close(); runtimeValid = false;
  for (auto page : retiredPages) CHECK(VirtualFree(page, 0, MEM_RELEASE)); retiredPages.clear();
}
static void failedUnlocks() {
  for (const unsigned stage : {1u, 2u}) {
    Fixture<D3D10_1DDI_DEVICEFUNCS, true> f;
    Texture<decltype(f)> texture(f, DXGI_FORMAT_R8G8B8A8_UNORM); texture.clear({1, 0, 1, 1});
    expectedHandle = texture.allocation;
    const auto oldFailures = unlockFailures, oldModes = modes, oldReleases = releases, oldLocks = locks;
    unlockFailureCountdown = stage;
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_OUTOFMEMORY);
    CHECK(unlockFailures == oldFailures + 1 && locks == oldLocks + stage
      && modes == oldModes && releases == oldReleases);
    unsigned mappings = 0; for (const auto& entry : owned) mappings += entry.second.mapped;
    CHECK(mappings == 1);
    // Close the retained mapping before any new LockCb or RenderCb. Both
    // callbacks reject an overlap with the still-owned previous mapping.
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == S_OK);
    for (const auto& entry : owned) CHECK(!entry.second.mapped);
  }
  {
    Fixture<D3D10_1DDI_DEVICEFUNCS, true> f;
    Texture<decltype(f)> texture(f, DXGI_FORMAT_R8G8B8A8_UNORM); texture.clear({1, 0, 0, 1});
    expectedHandle = texture.allocation; const auto oldReleases = releases;
    unlockResult = E_OUTOFMEMORY;
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_OUTOFMEMORY);
    texture.retire(); // Failed physical cleanup survives reclaimed resource bytes.
    CHECK(owned.size() == 2 && releases == oldReleases && lastError == E_OUTOFMEMORY);
    unlockResult = S_OK; f.retire();
    CHECK(owned.empty() && releases == oldReleases + 1 && locks == unlocks);
  }
}
static void directFailedUnlockMoveAndAllocation() {
  runtimeValid = true; lockCountdown = 1;
  dxvk::umd::RuntimeMemory memory; dxvk::umd::RuntimeAllocation allocation, neighbour;
  auto service = std::make_shared<dxvk::umd::RuntimeService>();
  dxvk::umd::RuntimeService::Scope scope(service.get());
  D3DDDI_DEVICECALLBACKS kernel{}; DXGI_DDI_BASE_CALLBACKS dxgi{};
  kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
  kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
  kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
  kernel.pfnRenderCb = render; kernel.pfnSetDisplayModeCb = mode; dxgi.pfnPresentCb = present;
  memory.initialize(&deviceCookie, kernel, &dxgi, {}, service);
  char resource, other;
  const auto oldReleases = releases;
  allocateBeforeContinuation = [&] { CHECK(allocation.release() == S_OK && owned.empty()); };
  CHECK(memory.allocatePrimary(allocation, &resource, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(owned.empty() && !allocation.handle() && releases == oldReleases + 1);
  allocationFails = true;
  allocateBeforeContinuation = [&] { CHECK(allocation.release() == S_OK && owned.empty()); };
  CHECK(memory.allocatePrimary(allocation, &resource, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == DXGI_ERROR_DEVICE_REMOVED);
  allocationFails = false; CHECK(owned.empty() && releases == oldReleases + 1);
  std::optional<dxvk::umd::RuntimeAllocation> staged;
  allocateBeforeContinuation = [&] { staged.emplace(std::move(allocation)); };
  CHECK(memory.allocatePrimary(allocation, &resource, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(staged && !staged->handle() && !allocation.handle() && owned.empty() && releases == oldReleases + 2);
  CHECK(memory.allocatePrimary(allocation, &resource, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == S_OK);
  CHECK(memory.allocatePrimary(neighbour, &other, 8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 60000, 1001) == S_OK);
  CHECK(allocation.canRotateWith(neighbour));
  std::array<uint32_t, 32> upload{};
  unlockResult = E_OUTOFMEMORY;
  CHECK(memory.upload(allocation, upload.data(), 32) == E_OUTOFMEMORY);
  CHECK(!allocation.canRotateWith(neighbour));
  const auto afterAllocation = releases, priorLocks = locks, priorRenders = renders;
  CHECK(memory.upload(allocation, upload.data(), 32) == E_OUTOFMEMORY);
  CHECK(memory.download(allocation, upload.data(), 32) == E_OUTOFMEMORY);
  CHECK(locks == priorLocks && renders == priorRenders);
  CHECK(allocation.release() == E_OUTOFMEMORY && allocation.handle() && releases == afterAllocation);
  std::optional<dxvk::umd::RuntimeAllocation> moved;
  unlockAction = [&] { moved.emplace(std::move(allocation)); };
  CHECK(allocation.release() == DXGI_ERROR_DEVICE_REMOVED && moved && moved->handle() && !allocation.handle());
  CHECK(!moved->canRotateWith(neighbour) && releases == afterAllocation && locks == priorLocks);
  unlockResult = S_OK;
  CHECK(moved->release() == S_OK && neighbour.release() == S_OK && owned.empty() && locks == unlocks);
  CHECK(memory.closeDeviceAllocations() == S_OK); service->close(); runtimeValid = false;
  for (auto page : retiredPages) CHECK(VirtualFree(page, 0, MEM_RELEASE)); retiredPages.clear();
}
static void terminalUnlockFailures() {
  for (const bool inFlight : {false, true}) {
    Fixture<D3D10_1DDI_DEVICEFUNCS, true> f;
    Texture<decltype(f)> texture(f, DXGI_FORMAT_R8G8B8A8_UNORM); texture.clear({0, 0, 1, 1});
    expectedHandle = texture.allocation;
    const auto oldReleases = releases, oldRuntime = runtimeTerminalReleases, oldClosures = runtimeTerminalMapClosures;
    unlockResult = E_OUTOFMEMORY;
    if (inFlight) unlockAction = [&] { f.retire(); };
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == (inFlight ? DXGI_ERROR_DEVICE_REMOVED : E_OUTOFMEMORY));
    if (!inFlight) f.retire();
    CHECK(!f.live && owned.size() == 2 && releases == oldReleases && lastError == D3DDDIERR_DEVICEREMOVED);
    unsigned mapped = 0; for (const auto& entry : owned) mapped += entry.second.mapped;
    CHECK(mapped == 1);
    unlockResult = S_OK;
    runtimeTerminalCleanup();
    CHECK(runtimeTerminalReleases == oldRuntime + 1 && runtimeTerminalMapClosures == oldClosures + 1);
  }
}
static void protectedPrimaryOutput() {
  Fixture<D3D10_1DDI_DEVICEFUNCS, true> f;
  Storage resource(f.table.pfnCalcPrivateResourceSize(f.device, nullptr)), desc(sizeof(DXGI_DDI_PRIMARY_DESC));
  auto primary = static_cast<DXGI_DDI_PRIMARY_DESC*>(desc.bytes);
  primary->ModeDesc = {8, 4, DXGI_FORMAT_R8G8B8A8_UNORM, {60000, 1001}, DXGI_DDI_MODE_SCANLINE_ORDER_PROGRESSIVE,
    DXGI_DDI_MODE_ROTATION_IDENTITY, DXGI_DDI_MODE_SCALING_UNSPECIFIED};
  primary->DriverFlags = 0xabcdef;
  D3D10DDI_MIPINFO shape{8, 4, 1, 8, 4, 1}; D3D10DDIARG_CREATERESOURCE args{};
  args.pMipInfoList = &shape; args.MipLevels = args.ArraySize = 1; args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  args.Format = DXGI_FORMAT_R8G8B8A8_UNORM; args.Usage = D3D10_DDI_USAGE_DEFAULT;
  args.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT;
  args.SampleDesc = {1, 0}; args.pPrimaryDesc = primary;
  char runtime;
  allocateAction = [&] { desc.protect(PAGE_READONLY); };
  lastError = S_OK; f.table.pfnCreateResource(f.device, &args, {resource.bytes}, {&runtime});
  CHECK(lastError == DXGI_DDI_ERR_UNSUPPORTED && primary->DriverFlags == 0xabcdef && owned.empty());
  // An unreadable input never reaches AllocateCb; the previous Blt fixture's
  // protected pPrimaryDesc behavior remains unchanged.
  desc.protect(); const auto oldPairs = pairAllocations;
  f.table.pfnCreateResource(f.device, &args, {resource.bytes}, {&runtime});
  CHECK(lastError == DXGI_DDI_ERR_UNSUPPORTED && pairAllocations == oldPairs && owned.empty());
}
int main() {
  caller = GetCurrentThreadId();
  profile<D3D10DDI_DEVICEFUNCS, false>(0); profile<D3D10DDI_DEVICEFUNCS, true>(1);
  profile<D3D10_1DDI_DEVICEFUNCS, false>(2); profile<D3D10_1DDI_DEVICEFUNCS, true>(3);
  profile<D3D11DDI_DEVICEFUNCS, false>(4); profile<D3D11DDI_DEVICEFUNCS, true>(5);
  for (unsigned point = 0; point <= 11; ++point) retirement(point);
  pitchedTransfer(); mappedOwnerRetirement(false); mappedOwnerRetirement(true); protectedPrimaryOutput();
  failedUnlocks(); directFailedUnlockMoveAndAllocation();
  {
    Fixture<D3D10DDI_DEVICEFUNCS, true> f;
    nonExactAllocate = true; Texture<decltype(f)> failed(f, DXGI_FORMAT_R8G8B8A8_UNORM); nonExactAllocate = false;
    allocationFails = true; Texture<decltype(f)> allocationFailure(f, DXGI_FORMAT_R8G8B8A8_UNORM); allocationFails = false;
    for (malformedAllocation = 1; malformedAllocation <= 6; ++malformedAllocation) {
      const auto beforeRelease = releases;
      Texture<decltype(f)> incomplete(f, DXGI_FORMAT_R8G8B8A8_UNORM);
      CHECK(releases == beforeRelease + (malformedAllocation == 6 ? 0u : 1u));
    }
    malformedAllocation = 0;
    CHECK(owned.empty());
    Texture<decltype(f)> texture(f, DXGI_FORMAT_R8G8B8A8_UNORM); texture.clear({1, 0, 0, 1}); expectedHandle = texture.allocation;
    lockResult = E_OUTOFMEMORY; CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_OUTOFMEMORY); lockResult = S_OK;
    renderResult = E_FAIL; CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_FAIL); renderResult = S_OK;
    const auto before = renders;
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_FAIL && renders == before);
  }
  {
    Fixture<D3D10DDI_DEVICEFUNCS, true> f;
    Texture<decltype(f)> texture(f, DXGI_FORMAT_R8G8B8A8_UNORM); texture.clear({1, 0, 0, 1}); expectedHandle = texture.allocation;
    malformedRender = true; CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == E_FAIL); malformedRender = false;
  }
  {
    Fixture<D3D10DDI_DEVICEFUNCS, true> f(false);
    Texture<decltype(f)> failed(f, DXGI_FORMAT_R8G8B8A8_UNORM, false, 16);
    CHECK(lastError == DXGI_DDI_ERR_UNSUPPORTED && owned.empty());
  }
  terminalUnlockFailures();
  CHECK(snapshots == 24 && pixels == 768 && presents == 6 && owned.empty()
    && contexts == contextCloses && locks == unlocks + runtimeTerminalMapClosures
    && unlockAttempts == unlocks + unlockFailures && runtimeTerminalReleases == 3 && runtimeTerminalMapClosures == 2);
  std::printf("DXGI primary/display PASS checks=%u profiles=6 snapshots=24 pixels=768 hardware_admission=0\n", checks.load());
}
