#include "../src/umd/umd_runtime_gpu.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <array>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>

using dxvk::umd::RuntimeGpu;
static std::atomic<unsigned> checks{0};
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "runtime GPU check %u line %d: %s\n", checks.load(), __LINE__, #c); std::exit(1); } } while (0)
static std::atomic<unsigned> runtimeHookDepth{0}, errorHookDepth{0}, backendDrains{0};
static std::function<void()> errorHook;
static bool trackBackendDrain = false;
static bool preexistingWorker = false;
static HANDLE pendingSubmission = nullptr;
static ID3D11DeviceContext* inspectionContext = nullptr; // Borrowed test WARP context.
static std::mutex submissionMutex;
static std::atomic<unsigned> childDrains{0};
struct Fixture {
  char device, adapter, contextCookie, resourceCookie;
  LUID luid{0x13579024, -11};
  uint64_t generation = 73;
  std::shared_ptr<RuntimeGpu> gpu;
  dxvk::umd::RuntimeBackend bridge;
  std::shared_ptr<dxvk::umd::AdapterIdentity> identity;
  D3DDDI_DEVICECALLBACKS input = {};
  std::map<uint32_t, std::vector<uint8_t>> allocations;
  std::array<uint8_t, 65536> commands[2] = {};
  D3DDDI_ALLOCATIONLIST list[2][64] = {};
  D3DDDI_PATCHLOCATIONLIST patches[2][64] = {};
  unsigned allocationCalls = 0, deallocations = 0, locks = 0, unlocks = 0;
  unsigned contexts = 0, contextCloses = 0, renders = 0, callbackCalls = 0;
  unsigned presentAllocations = 0, presentDeallocations = 0;
  uint32_t nextHandle = 31, completedFence = 0;
  bool rename = false, nullMap = false, malformedContext = false, reset = false;
  bool allocationFails = false, deallocateFails = false, renderFails = false, replaceBad = false, unlockFails = false;
  bool nonExactAllocate = false, nonExactLock = false;
  bool checkQueryOwner = false;
  bool terminalBorrow = false;
  mwd_device_create_info borrowed = {};
  char retireAt = 0;
  void* pendingToken = nullptr;
  unsigned buffer = 0;
  HANDLE closedEvent = nullptr;
  DWORD runtimeThread = 0;
  bool runtimeValid = true;
  void checkRuntime() {
    if (runtimeThread) CHECK(runtimeValid && GetCurrentThreadId() == runtimeThread);
  }
  std::function<void(char)> outerHook;
  void retire(char point) {
    ++callbackCalls;
    if (retireAt == point) { retireAt = 0; CHECK(gpu->close() == S_OK); }
    if (outerHook) {
      auto callback = outerHook;
      ++runtimeHookDepth;
      callback(point);
      --runtimeHookDepth;
    }
  }
};
static Fixture* f;
static void put(void* ptr, unsigned offset, uint64_t value, unsigned size) {
  auto* bytes = static_cast<uint8_t*>(ptr);
  for (unsigned i = 0; i < size; ++i) bytes[offset+i] = uint8_t(value >> (i*8));
}
static uint64_t read(const void* ptr, unsigned offset, unsigned size) {
  const auto* bytes = static_cast<const uint8_t*>(ptr); uint64_t value = 0;
  for (unsigned i = 0; i < size; ++i) value |= uint64_t(bytes[offset+i]) << (i*8);
  return value;
}
static HRESULT APIENTRY query(HANDLE h, const D3DDDICB_QUERYADAPTERINFO* args) {
  f->checkRuntime();
  CHECK(h == &f->adapter && args->PrivateDriverDataSize == 160);
  auto* data = args->pPrivateDriverData;
  put(data, 0, 0x504d5644, 4); put(data, 8, 128, 4); put(data, 16, 3, 8);
  put(data, 24, f->generation + (f->reset ? 1 : 0), 8);
  put(data, 128, 0x44494c56, 4); put(data, 132, 1, 4); put(data, 136, 32, 4);
  put(data, 140, 1, 4); std::memcpy(static_cast<uint8_t*>(data)+144, &f->luid, 8); put(data, 152, 1, 4);
  if (f->checkQueryOwner) {
    f->checkQueryOwner = false;
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, f->pendingToken) == E_INVALIDARG);
  }
  f->retire('Q'); return S_OK;
}
static HRESULT APIENTRY createContext(HANDLE h, D3DDDICB_CREATECONTEXT* args) {
  f->checkRuntime();
  CHECK(h == &f->device && args->EngineAffinity == 1 && args->PrivateDriverDataSize == 32);
  CHECK(read(args->pPrivateDriverData, 0, 4) == 0x504d5644
    && read(args->pPrivateDriverData, 8, 4) == 32 && read(args->pPrivateDriverData, 16, 8) == f->generation);
  ++f->contexts; args->hContext = &f->contextCookie;
  args->pCommandBuffer = f->commands[0].data(); args->CommandBufferSize = 65536;
  args->pAllocationList = f->list[0]; args->AllocationListSize = 64;
  args->pPatchLocationList = f->patches[0]; args->PatchLocationListSize = 64;
  f->retire('C'); return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE h, const D3DDDICB_DESTROYCONTEXT* args) {
  f->checkRuntime();
  CHECK(h == &f->device && args->hContext == &f->contextCookie);
  ++f->contextCloses;
  if (f->terminalBorrow) CHECK(f->borrowed.callbacks->status(f->borrowed.owner) == S_OK);
  f->retire('D');
  if (f->closedEvent) CHECK(SetEvent(f->closedEvent));
  return S_OK;
}
static HRESULT APIENTRY escape(HANDLE h, const D3DDDICB_ESCAPE* args) {
  f->checkRuntime();
  CHECK(h == &f->adapter && args->hDevice == &f->device && args->hContext == &f->contextCookie);
  void* data = args->pPrivateDriverData;
  CHECK(read(data, 24, 8) == f->generation);
  if (args->PrivateDriverDataSize == 64) {
    CHECK(read(data, 16, 4) == 1);
    put(data, 32, 0x100000000ull, 8); put(data, 40, 0x1000000, 8);
    put(data, 48, f->generation, 8); put(data, 56, f->malformedContext ? 0 : 17, 4); put(data, 60, 19, 4);
  } else {
    CHECK(args->PrivateDriverDataSize == 56 && read(data, 16, 4) == 2);
    put(data, 32, f->completedFence, 8); put(data, 40, f->generation, 8); put(data, 48, 17, 4);
  }
  f->retire('E'); return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE h, D3DDDICB_ALLOCATE* args) {
  f->checkRuntime();
  if (args->hResource) {
    CHECK(h == &f->device && args->hResource == &f->resourceCookie && args->NumAllocations == 1);
    CHECK(args->pAllocationInfo && args->pAllocationInfo->PrivateDriverDataSize == sizeof(dxvk::umd::AllocationInfo));
    args->pAllocationInfo->hAllocation = 900; args->hKMResource = 901;
    ++f->presentAllocations;
    return S_OK;
  }
  CHECK(h == &f->device && !args->hResource && args->NumAllocations == 1 && !args->hKMResource);
  CHECK(args->pAllocationInfo->PrivateDriverDataSize == sizeof(dxvk::umd::AllocationInfo));
  dxvk::umd::AllocationInfo info;
  std::memcpy(&info, args->pAllocationInfo->pPrivateDriverData, sizeof(info));
  CHECK(info.magic == 0x504d5644 && info.headerSize == 80 && !info.version && !info.reserved);
  CHECK(info.contextId == 17 && info.resetGeneration == f->generation && info.flags == 6 && info.alignment == 4096);
  CHECK(info.requestedIova >= 0x100000000ull && info.requestedIova + info.size <= 0x101000000ull);
  ++f->allocationCalls;
  if (!f->allocationFails) {
    const uint32_t handle = f->nextHandle++;
    f->allocations.emplace(handle, std::vector<uint8_t>(size_t(info.size), 0x5a));
    args->pAllocationInfo->hAllocation = handle;
  }
  // Runtime callback output cannot rewrite the owner's bounds or generation.
  std::memset(args->pAllocationInfo->pPrivateDriverData, 0xff, sizeof(info));
  f->retire('A');
  return f->allocationFails ? E_OUTOFMEMORY : f->nonExactAllocate ? S_FALSE : S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE h, const D3DDDICB_DEALLOCATE* args) {
  f->checkRuntime();
  if (args->hResource) {
    CHECK(h == &f->device && args->hResource == &f->resourceCookie);
    CHECK(runtimeHookDepth == 0); // Never deallocate HRESOURCE inside RenderCb.
    ++f->presentDeallocations;
    return S_OK;
  }
  CHECK(h == &f->device && !args->hResource && args->NumAllocations == 1 && args->HandleList);
  CHECK(f->allocations.count(*args->HandleList) == 1);
  ++f->deallocations;
  if (!f->deallocateFails) f->allocations.erase(*args->HandleList);
  if (f->pendingToken) CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, f->pendingToken) == E_INVALIDARG);
  f->retire('F'); return f->deallocateFails ? E_FAIL : S_OK;
}
static HRESULT APIENTRY lock(HANDLE h, D3DDDICB_LOCK* args) {
  f->checkRuntime();
  CHECK(h == &f->device && args->Flags.LockEntire && !args->Flags.Discard && !args->Flags.IgnoreSync);
  CHECK(f->allocations.count(args->hAllocation) == 1); ++f->locks;
  if (f->rename) {
    auto record = f->allocations.extract(args->hAllocation);
    record.key() = f->nextHandle++; args->hAllocation = record.key(); f->allocations.insert(std::move(record));
  }
  args->pData = f->nullMap ? nullptr : f->allocations.at(args->hAllocation).data();
  f->retire('L'); return f->nonExactLock ? S_FALSE : S_OK;
}
static HRESULT APIENTRY unlock(HANDLE h, const D3DDDICB_UNLOCK* args) {
  f->checkRuntime();
  CHECK(h == &f->device && args->NumAllocations == 1 && f->allocations.count(*args->phAllocations) == 1);
  ++f->unlocks; f->retire('U'); return f->unlockFails ? E_FAIL : S_OK;
}
static HRESULT APIENTRY render(HANDLE h, D3DDDICB_RENDER* args) {
  f->checkRuntime();
  CHECK(h == &f->device && args->hContext == &f->contextCookie && args->NumAllocations == 1 && args->NumPatchLocations == 1);
  CHECK(args->pNewCommandBuffer == f->commands[f->buffer].data());
  const auto* packet = f->commands[f->buffer].data();
  CHECK(read(packet, 0, 4) == 0x504d5644 && read(packet, 8, 4) == args->CommandLength
    && read(packet, 24, 8) == f->generation && read(packet, 32, 4) == 64 && read(packet, 40, 4) == 96);
  CHECK(read(packet, 64+8, 8) == 0 && read(packet, 64+16, 8) == 4096);
  CHECK(f->allocations.count(f->list[f->buffer][0].hAllocation) == 1 && f->list[f->buffer][0].WriteOperation);
  CHECK(f->patches[f->buffer][0].PatchOffset == 96 && !f->patches[f->buffer][0].AllocationOffset);
  if (f->pendingToken) CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, f->pendingToken) == E_INVALIDARG);
  ++f->renders; ++f->completedFence; f->buffer ^= 1;
  args->pNewCommandBuffer = f->replaceBad ? nullptr : f->commands[f->buffer].data(); args->NewCommandBufferSize = 65536;
  args->pNewAllocationList = f->list[f->buffer]; args->NewAllocationListSize = 64;
  args->pNewPatchLocationList = f->patches[f->buffer]; args->NewPatchLocationListSize = 64;
  f->retire('R'); return f->renderFails ? E_FAIL : S_OK;
}
static std::unique_ptr<Fixture> setup() {
  auto result = std::make_unique<Fixture>(); f = result.get();
  f->input.pfnAllocateCb = allocate; f->input.pfnDeallocateCb = deallocate;
  f->input.pfnLockCb = lock; f->input.pfnUnlockCb = unlock;
  f->input.pfnCreateContextCb = createContext; f->input.pfnDestroyContextCb = destroyContext;
  f->input.pfnEscapeCb = escape; f->input.pfnRenderCb = render;
  f->identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  f->identity->luid = f->luid; f->identity->runtime.handle = &f->adapter;
  f->identity->query = query; f->identity->generation = f->generation; f->identity->capabilities = 3;
  f->gpu = RuntimeGpu::create(&f->device, f->input, f->identity);
  f->bridge = f->gpu->backend();
  return result;
}
static mwd_allocation buffer() {
  mwd_allocation a{};
  CHECK(f->bridge.create.callbacks->allocate(f->bridge.create.owner, 4096, 65536, 0, 6, &a) == S_OK);
  CHECK(a.token && a.handle && a.size == 4096 && a.address % 65536 == 0 && a.generation == 73 && a.flags == 6);
  return a;
}
static HRESULT send(const mwd_allocation& a) {
  const uint32_t stream[4] = {0,0,0x11223344,0x55667788};
  const mwd_reference ref{a.token, 0, 4096, 3, 0};
  return f->bridge.create.callbacks->submit(f->bridge.create.owner, stream, sizeof(stream), &ref, 1);
}

static bool earlyBackendFail = false;
static unsigned earlyBackends = 0;
// WARP's final private-data release observes the production DDI's real COM
// backend teardown. Its worker needs RuntimeGpu's actual recursive callback
// lock, so teardown on the interrupted callback stack cannot pass this test.
class DrainMarker final : public IUnknown {
public:
  explicit DrainMarker(const dxvk::umd::RuntimeBackend& value, const mwd_allocation& allocation)
  : bridge(value), allocation(allocation), persistent(preexistingWorker) {
    CHECK(ready && stop);
    if (persistent) {
      thread = CreateThread(nullptr, 0, completed, this, 0, nullptr);
      CHECK(thread && WaitForSingleObject(ready, 3000) == WAIT_OBJECT_0);
    }
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid != __uuidof(IUnknown)) return E_NOINTERFACE;
    *out = static_cast<IUnknown*>(this); AddRef(); return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG value = --references;
    if (!value) delete this;
    return value;
  }
private:
  ~DrainMarker() {
    CHECK(runtimeHookDepth == 0 && errorHookDepth == 0);
    CHECK(f->runtimeThread && GetCurrentThreadId() != f->runtimeThread);
    if (!persistent) thread = CreateThread(nullptr, 0, completed, this, 0, nullptr);
    CHECK(SetEvent(stop));
    CHECK(thread && WaitForSingleObject(thread, 3000) == WAIT_OBJECT_0);
    DWORD result = 1; CHECK(GetExitCodeThread(thread, &result) && result == 0);
    CloseHandle(thread); CloseHandle(ready); CloseHandle(stop);
    ++backendDrains;
  }
  static DWORD WINAPI completed(void* ptr) {
    auto marker = static_cast<DrainMarker*>(ptr);
    uint32_t fence = 0;
    const auto& b = marker->bridge;
    if (marker->persistent) {
      CHECK(b.create.callbacks->status(b.create.owner) == S_OK);
      CHECK(SetEvent(marker->ready));
      CHECK(WaitForSingleObject(marker->stop, 3000) == WAIT_OBJECT_0);
    }
    CHECK(b.create.callbacks->completed(b.create.owner, &fence) == S_OK);
    void* mapped = nullptr; uint32_t handle = 0;
    CHECK(b.create.callbacks->map(b.create.owner, marker->allocation.token, &mapped, &handle) == S_OK);
    CHECK(mapped && handle);
    CHECK(b.create.callbacks->unmap(b.create.owner, marker->allocation.token) == S_OK);
    CHECK(b.create.callbacks->release(b.create.owner, marker->allocation.token) == S_OK);
    return 0;
  }
  std::atomic<ULONG> references{1};
  dxvk::umd::RuntimeBackend bridge;
  mwd_allocation allocation;
  bool persistent;
  HANDLE thread = nullptr;
  HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
};
static const GUID drainGuid = {0x4fef0123, 0x4241, 0x4abd, {0x91,0x01,0x01,0x02,0x03,0x04,0x05,0x06}};
// Attached to the actual WARP resource reached through production DDI view
// binding. Its final release models Mesa's BO mutex, held across RenderCb.
class ChildDrainMarker final : public IUnknown {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid != __uuidof(IUnknown)) return E_NOINTERFACE;
    *out = static_cast<IUnknown*>(this); AddRef(); return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG value = --references;
    if (!value) delete this;
    return value;
  }
private:
  ~ChildDrainMarker() {
    CHECK(runtimeHookDepth == 0 && GetCurrentThreadId() != f->runtimeThread);
    std::lock_guard<std::mutex> lock(submissionMutex);
    ++childDrains;
  }
  std::atomic<ULONG> references{1};
};
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  ++earlyBackends;
  CHECK(runtime && runtime->owner && !std::memcmp(&luid, &f->luid, sizeof(luid)));
  CHECK(runtime->create.owner == runtime->owner.get() && mwd_callbacks_valid(runtime->create.callbacks));
  f->gpu = std::static_pointer_cast<RuntimeGpu>(runtime->owner); f->bridge = *runtime;
  // Real production CreateDevice has already copied these tables. A backend
  // BO allocation must work now, not only after the factory has returned.
  f->input = {};
  auto a = buffer();
  auto cb = runtime->create.callbacks; auto owner = runtime->create.owner;
  void* ptr = nullptr; uint32_t handle = 0;
  CHECK(cb->map(owner, a.token, &ptr, &handle) == S_OK);
  std::memset(ptr, 0x8c, 4096);
  CHECK(cb->unmap(owner, a.token) == S_OK && send(a) == S_OK);
  if (!trackBackendDrain) CHECK(cb->release(owner, a.token) == S_OK);
  *device = nullptr; *context = nullptr;
  if (earlyBackendFail) return E_FAIL;
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
    D3D11_SDK_VERSION, device, nullptr, context);
  inspectionContext = *context;
  if (hr == S_OK && trackBackendDrain) {
    auto marker = new DrainMarker(*runtime, a);
    CHECK((*device)->SetPrivateDataInterface(drainGuid, marker) == S_OK);
    marker->Release();
  }
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  CHECK(GetCurrentThreadId() != f->runtimeThread);
  context->Flush();
  if (pendingSubmission)
    CHECK(WaitForSingleObject(pendingSubmission, 3000) == WAIT_OBJECT_0);
  return S_OK;
}
static void APIENTRY setError(D3D10DDI_HRTCORELAYER, HRESULT) {
  CHECK(bool(errorHook));
  auto callback = errorHook;
  ++errorHookDepth;
  callback();
  --errorHookDepth;
}
static HRESULT APIENTRY unusedPresent(HANDLE, DXGIDDICB_PRESENT*) { return E_NOTIMPL; }

static void runtimeTeardown() {
  // D3D10 runtime calls stay on their DDI caller, and all device-owned
  // allocations/contexts are gone before DestroyDevice returns.
  for (unsigned mode = 0; mode < 2; ++mode) {
    auto scope = setup();
    f->gpu->close(); f->bridge = {}; f->gpu.reset();
    earlyBackendFail = false; trackBackendDrain = true; preexistingWorker = mode != 0;
    f->runtimeThread = GetCurrentThreadId();
    const unsigned before = backendDrains;
    D3D10DDI_DEVICEFUNCS table{};
    D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks{}; callbacks.pfnSetErrorCb = setError;
    auto kernel = f->input;
    void* storage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(storage);
    D3D10DDIARG_CREATEDEVICE args{};
    args.hDrvDevice.pDrvPrivate = storage; args.hRTDevice.handle = &f->device;
    args.pUMCallbacks = &callbacks; args.pKTCallbacks = &kernel; args.pDeviceFuncs = &table;
    DXGI_DDI_BASE_CALLBACKS dxgi{};
    dxgi.pfnPresentCb = unusedPresent;
    DXGI_DDI_BASE_FUNCTIONS dxgiFunctions{};
    args.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgi;
    args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgiFunctions;
    CHECK(dxvk::umd::createAdapterDevice(f->identity, &args) == S_OK);
    const auto bridge = f->bridge;
    CHECK(f->allocations.size() == 1);

    D3D10DDI_MIPINFO mip{2,2,1,2,2,1};
    D3D10DDIARG_CREATERESOURCE resourceArgs{};
    resourceArgs.pMipInfoList = &mip; resourceArgs.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    resourceArgs.Usage = D3D10_DDI_USAGE_DEFAULT;
    resourceArgs.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT;
    resourceArgs.Format = DXGI_FORMAT_R8G8B8A8_UNORM; resourceArgs.SampleDesc.Count = 1;
    resourceArgs.MipLevels = resourceArgs.ArraySize = 1;
    void* resourceStorage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    void* targetStorage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(resourceStorage && targetStorage);
    D3D10DDI_HRESOURCE resource{resourceStorage};
    D3D10DDI_HRENDERTARGETVIEW target{targetStorage};
    CHECK(table.pfnCalcPrivateResourceSize(args.hDrvDevice, &resourceArgs) <= 4096);
    table.pfnCreateResource(args.hDrvDevice, &resourceArgs, resource, {&f->resourceCookie});
    CHECK(f->presentAllocations == 1 && !f->presentDeallocations);
    D3D10DDIARG_CREATERENDERTARGETVIEW targetArgs{};
    targetArgs.hDrvResource = resource; targetArgs.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    targetArgs.Format = resourceArgs.Format; targetArgs.Tex2D.ArraySize = 1;
    CHECK(table.pfnCalcPrivateRenderTargetViewSize(args.hDrvDevice, &targetArgs) <= 4096);
    table.pfnCreateRenderTargetView(args.hDrvDevice, &targetArgs, target, {});
    table.pfnSetRenderTargets(args.hDrvDevice, &target, 1, 0, {});
    ID3D11RenderTargetView* actualView = nullptr;
    inspectionContext->OMGetRenderTargets(1, &actualView, nullptr);
    CHECK(actualView);
    ID3D11Resource* actualResource = nullptr;
    actualView->GetResource(&actualResource); CHECK(actualResource);
    auto marker = new ChildDrainMarker;
    CHECK(actualResource->SetPrivateDataInterface(drainGuid, marker) == S_OK);
    marker->Release(); actualResource->Release(); actualView->Release();
    table.pfnSetRenderTargets(args.hDrvDevice, nullptr, 0, 1, {});
    const unsigned childrenBefore = childDrains;
    DWORD oldProtection = 0;
    if (mode) {
      // The view owns the final backend reference after Resource destruction.
      table.pfnDestroyResource(args.hDrvDevice, resource);
      CHECK(VirtualProtect(resourceStorage, 4096, PAGE_NOACCESS, &oldProtection));
    } else {
      table.pfnDestroyRenderTargetView(args.hDrvDevice, target);
      CHECK(VirtualProtect(targetStorage, 4096, PAGE_NOACCESS, &oldProtection));
    }
    CHECK(childDrains == childrenBefore);

    // A backend CS/submission job can request runtime service between DDIs.
    // It must block without calling the runtime until the next permitted
    // caller pumps it, and native Flush must wait for this submission.
    struct Submission {
      dxvk::umd::RuntimeBackend bridge;
      HANDLE started = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    } submission{bridge};
    CHECK(submission.started);
    pendingSubmission = CreateThread(nullptr, 0, [](void* ptr) -> DWORD {
      auto& job = *static_cast<Submission*>(ptr);
      CHECK(SetEvent(job.started));
      const auto cb = job.bridge.create.callbacks; const auto owner = job.bridge.create.owner;
      mwd_allocation allocation{};
      CHECK(cb->allocate(owner, 4096, 4096, 0, 6, &allocation) == S_OK);
      void* data = nullptr; uint32_t handle = 0;
      CHECK(cb->map(owner, allocation.token, &data, &handle) == S_OK && data && handle);
      std::memset(data, 0x57, 4096);
      CHECK(cb->unmap(owner, allocation.token) == S_OK);
      const uint32_t commands[4] = {0, 0, 0x11223344, 0x55667788};
      const mwd_reference ref{allocation.token, 0, 4096, 3, 0};
      {
        std::lock_guard<std::mutex> lock(submissionMutex);
        CHECK(cb->submit(owner, commands, sizeof(commands), &ref, 1) == S_OK);
      }
      CHECK(cb->release(owner, allocation.token) == S_OK);
      return 0;
    }, &submission, 0, nullptr);
    CHECK(pendingSubmission && WaitForSingleObject(submission.started, 3000) == WAIT_OBJECT_0);
    const unsigned calls = f->callbackCalls;
    CHECK(WaitForSingleObject(pendingSubmission, 30) == WAIT_TIMEOUT);
    CHECK(f->callbackCalls == calls);
    bool nested = false;
    f->outerHook = [&](char point) {
      if (point != 'R' || nested) return;
      nested = true;
      D3D10DDI_COUNTER_INFO info{}; info.NumDetectableParallelUnits = 99;
      table.pfnCheckCounterInfo(args.hDrvDevice, &info);
      CHECK(info.NumDetectableParallelUnits == 0);
      std::thread sizeThread([&] {
        CHECK(table.pfnCalcPrivateResourceSize(args.hDrvDevice, nullptr) > 0);
      });
      sizeThread.join();
      if (mode) {
        table.pfnDestroyRenderTargetView(args.hDrvDevice, target);
        CHECK(VirtualProtect(targetStorage, 4096, PAGE_NOACCESS, &oldProtection));
      } else {
        table.pfnDestroyResource(args.hDrvDevice, resource);
        CHECK(VirtualProtect(resourceStorage, 4096, PAGE_NOACCESS, &oldProtection));
      }
      CHECK(childDrains == childrenBefore); // No COM release inside RenderCb.
      CHECK(f->presentDeallocations == (mode ? 1u : 0u));
    };
    const unsigned rendered = f->renders;
    table.pfnFlush(args.hDrvDevice);
    CHECK(nested && f->renders == rendered + 1 && f->allocations.size() == 1);
    CHECK(childDrains == childrenBefore + 1); // Retirement drained by Flush return.
    CHECK(f->presentDeallocations == 1);
    CHECK(WaitForSingleObject(pendingSubmission, 0) == WAIT_OBJECT_0);
    CloseHandle(pendingSubmission); pendingSubmission = nullptr;
    CloseHandle(submission.started); f->outerHook = {};
    table.pfnDestroyDevice(args.hDrvDevice);
    CHECK(backendDrains == before + 1 && f->contextCloses == 1 && f->allocations.empty());
    f->runtimeValid = false; // Handles and all service end immediately here.
    std::memset(&kernel, 0xcc, sizeof(kernel)); callbacks = {};
    DWORD previous = 0;
    CHECK(VirtualProtect(storage, 4096, PAGE_NOACCESS, &previous));
    CHECK(bridge.create.callbacks->status(bridge.create.owner) == DXGI_ERROR_DEVICE_REMOVED);
    uint32_t fence = 99;
    CHECK(bridge.create.callbacks->completed(bridge.create.owner, &fence) == DXGI_ERROR_DEVICE_REMOVED);
    D3D10DDI_COUNTER_INFO rejected{}; rejected.NumDetectableParallelUnits = 99;
    table.pfnCheckCounterInfo(args.hDrvDevice, &rejected);
    CHECK(rejected.NumDetectableParallelUnits == 99);
    table.pfnDestroyDevice(args.hDrvDevice);
    CHECK(VirtualFree(storage, 0, MEM_RELEASE));
    CHECK(VirtualFree(resourceStorage, 0, MEM_RELEASE));
    CHECK(VirtualFree(targetStorage, 0, MEM_RELEASE));
    inspectionContext = nullptr;
    trackBackendDrain = false;
  }
}

int main() {
  {
    auto scope = setup(); auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    CHECK(mwd_callbacks_valid(cb)); f->input = {};
    auto a = buffer(); auto b = buffer();
    CHECK(a.address != b.address && a.token != b.token && f->contexts == 1);
    f->pendingToken = a.token; f->checkQueryOwner = true;
    mwd_allocation alias{}; CHECK(cb->retain(owner, a.token, &alias) == S_OK && alias.handle == a.handle && f->allocationCalls == 2);
    f->pendingToken = nullptr;
    CHECK(cb->release(owner, a.token) == S_OK && f->deallocations == 0);
    f->rename = true; void* ptr = nullptr; uint32_t handle = 0;
    CHECK(cb->map(owner, alias.token, &ptr, &handle) == S_OK && handle != alias.handle);
    void* second = nullptr; uint32_t secondHandle = 0;
    CHECK(cb->map(owner, alias.token, &second, &secondHandle) == S_OK && ptr == second && handle == secondHandle && f->locks == 1);
    static_cast<uint8_t*>(ptr)[4095] = 0xa1;
    CHECK(cb->unmap(owner, alias.token) == S_OK && !f->unlocks);
    CHECK(cb->unmap(owner, alias.token) == S_OK && f->unlocks == 1);
    f->pendingToken = alias.token;
    f->checkQueryOwner = true;
    CHECK(send(alias) == S_OK);
    f->renderFails = true; CHECK(send(alias) == E_FAIL);
    f->renderFails = false; CHECK(send(alias) == S_OK && f->renders == 3);
    uint32_t fence = 0; CHECK(cb->completed(owner, &fence) == S_OK && fence == 3);
    CHECK(cb->release(owner, alias.token) == S_OK); f->pendingToken = nullptr;
    CHECK(cb->retain(owner, alias.token, &a) == E_INVALIDARG);
    auto reuse = buffer(); CHECK(reuse.token != alias.token && reuse.address == alias.address);
    CHECK(cb->release(owner, b.token) == S_OK && cb->release(owner, reuse.token) == S_OK);
    CHECK(f->gpu->close() == S_OK && f->allocations.empty() && f->contextCloses == 1);
    const unsigned before = f->callbackCalls;
    CHECK(cb->status(owner) == DXGI_ERROR_DEVICE_REMOVED && f->callbackCalls == before);
  }
  for (unsigned mode = 0; mode < 7; ++mode) {
    auto scope = setup(); auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    f->malformedContext = mode == 0; f->allocationFails = mode == 1;
    f->nonExactAllocate = mode == 2; f->nullMap = mode == 3; f->nonExactLock = mode == 4;
    mwd_allocation a{};
    HRESULT hr = cb->allocate(owner, 4096, 4096, 0, 6, &a);
    if (mode <= 2) CHECK(FAILED(hr) && !a.token && !a.handle);
    else {
      CHECK(hr == S_OK);
      if (mode <= 4) {
        void* ptr = nullptr; uint32_t handle = 0;
        CHECK(FAILED(cb->map(owner, a.token, &ptr, &handle)) && !ptr && !handle && f->unlocks == 1);
      } else if (mode == 5) {
        f->reset = true; CHECK(cb->status(owner) == DXGI_ERROR_DEVICE_REMOVED);
        f->reset = false; CHECK(cb->status(owner) == DXGI_ERROR_DEVICE_REMOVED && FAILED(send(a)));
      } else {
        f->replaceBad = true; CHECK(send(a) == E_FAIL && cb->status(owner) == DXGI_ERROR_DEVICE_REMOVED);
      }
    }
    CHECK(f->gpu->close() == S_OK && f->allocations.empty());
  }
  {
    auto scope = setup(); auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    auto a = buffer(); const unsigned before = f->callbackCalls;
    mwd_allocation out{};
    CHECK(cb->allocate(owner, UINT64_MAX, 4096, 0, 6, &out) == E_INVALIDARG);
    CHECK(cb->allocate(owner, 4096, 4097, 0, 6, &out) == E_INVALIDARG);
    CHECK(cb->retain(owner, reinterpret_cast<void*>(uintptr_t(0x12345)), &out) == E_INVALIDARG);
    CHECK(cb->allocate(owner, 4096, 4096, 1, 6, &out) == E_INVALIDARG);
    uint32_t stream[8] = {}; mwd_reference refs[2] = {{a.token, 0, 4096, 3, 0}, {a.token, 0, 4096, 3, 8}};
    CHECK(cb->submit(owner, stream, sizeof(stream), refs, 2) == E_INVALIDARG);
    refs[0].offset = UINT64_MAX; CHECK(cb->submit(owner, stream, sizeof(stream), refs, 1) == E_INVALIDARG);
    CHECK(f->callbackCalls == before && !f->renders);
    f->deallocateFails = true; CHECK(cb->release(owner, a.token) == E_FAIL);
    f->deallocateFails = false; CHECK(cb->release(owner, a.token) == S_OK);
    CHECK(f->gpu->close() == S_OK && f->allocations.empty());
  }
  {
    auto scope = setup();
    f->gpu->close(); f->bridge = {}; f->gpu.reset(); f->input.pfnRenderCb = nullptr;
    f->gpu = RuntimeGpu::create(&f->device, f->input, f->identity); f->bridge = f->gpu->backend();
    mwd_context_info context{};
    CHECK(f->bridge.create.callbacks->context(f->bridge.create.owner, &context) == DXGI_ERROR_UNSUPPORTED);
    CHECK(!context.context_id && !f->contexts && !f->allocationCalls && f->gpu->close() == S_OK);
  }
  {
    auto scope = setup(); auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    auto a = buffer(); f->nullMap = true; f->unlockFails = true;
    void* ptr = nullptr; uint32_t handle = 0;
    CHECK(cb->map(owner, a.token, &ptr, &handle) == E_FAIL && !ptr && !handle);
    f->nullMap = false;
    CHECK(cb->map(owner, a.token, &ptr, &handle) == E_FAIL && !ptr && !handle && f->locks == 1);
    f->unlockFails = false;
    CHECK(cb->release(owner, a.token) == S_OK && f->unlocks == 2 && f->allocations.empty());
    CHECK(f->gpu->close() == S_OK);
  }
  for (char point : {'Q', 'C', 'E', 'A', 'L', 'U', 'R', 'F'}) {
    auto scope = setup(); auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    mwd_allocation a{};
    if (point == 'L' || point == 'U' || point == 'R' || point == 'F') a = buffer();
    if (point == 'U') { void* ptr = nullptr; uint32_t handle = 0; CHECK(cb->map(owner, a.token, &ptr, &handle) == S_OK); }
    f->retireAt = point;
    HRESULT hr;
    if (point == 'L') { void* ptr = nullptr; uint32_t handle = 0; hr = cb->map(owner, a.token, &ptr, &handle); CHECK(!ptr && !handle); }
    else if (point == 'U') hr = cb->unmap(owner, a.token);
    else if (point == 'R') hr = send(a);
    else if (point == 'F') hr = cb->release(owner, a.token);
    else hr = cb->allocate(owner, 4096, 4096, 0, 6, &a);
    CHECK(hr == DXGI_ERROR_DEVICE_REMOVED);
    const unsigned before = f->callbackCalls;
    CHECK(cb->status(owner) == DXGI_ERROR_DEVICE_REMOVED && f->gpu->close() == S_OK && f->callbackCalls == before);
    // Emulated runtime owns final kernel teardown after recursive retirement.
    // This fixture never dereferences a map after retirement.
    f->allocations.clear();
  }
  for (bool failure : {false, true}) {
    auto scope = setup(); f->gpu->close(); f->bridge = {}; f->gpu.reset();
    earlyBackendFail = failure;
    D3D10DDI_DEVICEFUNCS table{}; D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks{}; callbacks.pfnSetErrorCb = setError;
    std::vector<std::max_align_t> storage((VioGpuDxvkPrivateDeviceSize() + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    D3D10DDIARG_CREATEDEVICE args{};
    args.hDrvDevice.pDrvPrivate = storage.data(); args.hRTDevice.handle = &f->device;
    args.pUMCallbacks = &callbacks; args.pKTCallbacks = &f->input; args.pDeviceFuncs = &table;
    const HRESULT hr = dxvk::umd::createAdapterDevice(f->identity, &args);
    if (failure) CHECK(hr == E_FAIL);
    else { CHECK(hr == S_OK && table.pfnDestroyDevice); table.pfnDestroyDevice(args.hDrvDevice); }
    CHECK(f->allocationCalls == 1 && f->renders == 1 && f->deallocations == 1 && f->contextCloses == 1 && f->allocations.empty());
    CHECK(f->bridge.create.callbacks->status(f->bridge.create.owner) == DXGI_ERROR_DEVICE_REMOVED);
  }
  {
    auto scope = setup(); auto a = buffer();
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, a.token) == S_OK);
    f->terminalBorrow = true; f->borrowed = f->bridge.create;
    f->bridge = {}; f->gpu.reset();
    CHECK(f->contextCloses == 1 && f->allocations.empty());
  }
  CHECK(earlyBackends == 2);
  runtimeTeardown();
  std::printf("PASS %u runtime GPU checks; ordinary caller dispatch, nested resource/view destruction and Flush retirement, shared owners, map/submit/reset and synchronous D3D10 teardown; backend-drains=%u child-drains=%u\n", checks.load(), backendDrains.load(), childDrains.load());
}
