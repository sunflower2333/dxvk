#include "../src/umd/umd_runtime_gpu.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_api.h"
#include "umd-runtime-diagnostics.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <array>
#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <stdexcept>

using dxvk::umd::RuntimeGpu;
static std::atomic<unsigned> checks{0};
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "runtime GPU check %u line %d: %s\n", checks.load(), __LINE__, #c); std::exit(1); } } while (0)
static std::atomic<unsigned> runtimeHookDepth{0}, errorHookDepth{0}, backendDrains{0};
static std::function<void()> errorHook;
static bool trackBackendDrain = false;
static bool preexistingWorker = false;
static HANDLE pendingSubmission = nullptr;
static ID3D11DeviceContext* inspectionContext = nullptr; // Borrowed test WARP context.
static std::function<void()> onFlush;
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
  unsigned queueCreates = 0, queueDestroys = 0, makes = 0, evicts = 0, pagingWaits = 0;
  unsigned gpuPagingWaits = 0;
  uint64_t queuedPagingFence = 0;
  HRESULT gpuPagingResult = S_OK;
  unsigned residencyFailure = 0;
  bool residencyPending = false;
  std::map<uint32_t, unsigned> resident;
  uint64_t pagingTarget = 0, pagingDone = 0;
  std::set<uint32_t> nativeHandles{901,902};
  std::map<uint64_t,uint64_t> nativeImports;
  unsigned nativeImportCalls = 0, nativeReleaseCalls = 0;
  HRESULT nativeImportResult = S_OK, nativeReleaseResult = S_OK;
  bool malformedNative = false;
  uint32_t expectedNativeHandle = 901, expectedNativeAccess = 2;
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
  f->nativeImports.clear(); // Emulated confirmed kernel context retirement.
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
  } else if (args->PrivateDriverDataSize == 88) {
    const auto opcode = read(data,16,4), key = read(data,32,8), address = read(data,40,8);
    CHECK((opcode == 6 || opcode == 7) && key && !(address & 4095) && address >= 0x100000000ull);
    CHECK(read(data,0,4) == 0x504d5644 && read(data,4,4) == 0 && read(data,8,4) == 88);
    CHECK(read(data,12,4) == 0 && read(data,20,4) == 0 && read(data,48,8) == 8192);
    for (unsigned i = 56; i < 88; i += 4) CHECK(read(data,i,4) == 0);
    HRESULT result;
    if (opcode == 6) {
      ++f->nativeImportCalls;
      CHECK(!f->nativeImports.count(key)); f->nativeImports[key] = address;
      result = f->nativeImportResult;
    } else {
      ++f->nativeReleaseCalls;
      CHECK(f->nativeImports.count(key) && f->nativeImports[key] == address);
      result = f->nativeReleaseResult;
      if (result == S_OK) f->nativeImports.erase(key);
    }
    put(data,56,opcode == 6 && f->malformedNative ? 18 : 17,4);
    f->retire(opcode == 6 ? 'i' : 'r'); return result;
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
  CHECK(h == &f->device && args->hContext == &f->contextCookie && args->NumPatchLocations == 1);
  CHECK(args->pNewCommandBuffer == f->commands[f->buffer].data());
  const auto* packet = f->commands[f->buffer].data();
  const bool native = read(packet,20,4) == 1;
  const unsigned streamOffset = native ? 136 : 96;
  CHECK(args->NumAllocations == (native ? 2 : 1));
  CHECK(read(packet, 0, 4) == 0x504d5644 && read(packet, 8, 4) == args->CommandLength
    && read(packet, 24, 8) == f->generation && read(packet, 32, 4) == 64 && read(packet, 40, 4) == streamOffset);
  CHECK(read(packet, 64+8, 8) == 0 && read(packet, 64+16, 8) == 4096);
  CHECK(f->allocations.count(f->list[f->buffer][0].hAllocation) == 1 && f->list[f->buffer][0].WriteOperation);
  CHECK(f->patches[f->buffer][0].PatchOffset == streamOffset && !f->patches[f->buffer][0].AllocationOffset);
  if (native) {
    CHECK(read(packet,36,4) == 1 && read(packet,48,4) == 96 && read(packet,52,4) == 1
      && read(packet,56,4) == 2 && read(packet,60,4) == 0);
    const auto key = read(packet,96,8);
    CHECK(f->nativeImports.count(key) && read(packet,104,8) == f->nativeImports[key]);
    CHECK(read(packet,112,8) == 8192 && read(packet,120,8) == f->generation
      && read(packet,128,4) == f->expectedNativeAccess && read(packet,132,4) == 2);
    CHECK(f->list[f->buffer][1].hAllocation == f->expectedNativeHandle
      && bool(f->list[f->buffer][1].WriteOperation) == bool(f->expectedNativeAccess & 2));
    if (f->identity->explicitResidency()) CHECK(f->resident[f->expectedNativeHandle] == 1);
  }
  if (f->pendingToken) CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, f->pendingToken) == E_INVALIDARG);
  if (f->identity->explicitResidency()) {
    CHECK(f->resident[f->list[f->buffer][0].hAllocation] == 1);
    CHECK(f->pagingDone == f->pagingTarget || f->queuedPagingFence == f->pagingTarget);
  }
  ++f->renders; ++f->completedFence; f->buffer ^= 1;
  args->pNewCommandBuffer = f->replaceBad ? nullptr : f->commands[f->buffer].data(); args->NewCommandBufferSize = 65536;
  args->pNewAllocationList = f->list[f->buffer]; args->NewAllocationListSize = 64;
  args->pNewPatchLocationList = f->patches[f->buffer]; args->NewPatchLocationListSize = 64;
  f->retire('R'); return f->renderFails ? E_FAIL : S_OK;
}
class ResidencyIdentity final : public dxvk::umd::AdapterIdentitySource {
public:
  bool explicitResidency() const override { return true; }
  HRESULT query(dxvk::umd::RuntimeIdentity& result, dxvk::umd::RuntimeQueryStage stage) const override {
    return dxvk::umd::queryRuntimeIdentity(&f->adapter, ::query, result, stage);
  }
};
static HRESULT APIENTRY createPaging(HANDLE h, D3DDDICB_CREATEPAGINGQUEUE* args) {
  f->checkRuntime(); CHECK(h == &f->device && args->Priority == D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL);
  ++f->queueCreates; args->hPagingQueue = 77; args->hSyncObject = 78;
  f->retire('q'); return S_OK;
}
static HRESULT APIENTRY destroyPaging(HANDLE h, const D3DDDI_DESTROYPAGINGQUEUE* args) {
  f->checkRuntime(); CHECK(h == &f->device && args->hPagingQueue == 77);
  CHECK(f->allocations.empty()); ++f->queueDestroys; f->retire('d'); return S_OK;
}
static HRESULT APIENTRY makeResident(HANDLE h, D3DDDI_MAKERESIDENT* args) {
  f->checkRuntime(); CHECK(h == &f->device && args->hPagingQueue == 77 && args->NumAllocations == 1);
  CHECK(args->PriorityList == nullptr && !args->Flags.MustSucceed);
  const auto handle = args->AllocationList[0];
  CHECK((f->allocations.count(handle) == 1 || f->nativeHandles.count(handle)) && !f->resident[handle]);
  ++f->makes; f->retire('m');
  if (f->residencyFailure == 1 || (f->residencyFailure == 2 && !args->Flags.CantTrimFurther)) {
    args->NumBytesToTrim = 4096; return E_OUTOFMEMORY;
  }
  if (f->residencyFailure == 3) return E_FAIL;
  ++f->resident[handle];
  if (f->residencyPending) { args->PagingFenceValue = ++f->pagingTarget; return E_PENDING; }
  return S_OK;
}
static HRESULT APIENTRY evict(HANDLE h, D3DDDICB_EVICT* args) {
  f->checkRuntime(); CHECK(h == &f->device && args->NumAllocations == 1 && !args->Flags.Value);
  CHECK(f->resident[args->AllocationList[0]] == 1);
  ++f->evicts; f->retire('e');
  if (f->residencyFailure == 5) return E_FAIL;
  --f->resident[args->AllocationList[0]]; return S_OK;
}
static HRESULT APIENTRY waitPaging(HANDLE h, const D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMCPU* args) {
  f->checkRuntime(); CHECK(h == &f->device && args->ObjectCount == 1);
  CHECK(args->ObjectHandleArray[0] == 78 && args->FenceValueArray[0] == f->pagingTarget);
  CHECK(args->hAsyncEvent && !args->Flags.Value);
  ++f->pagingWaits; f->retire('w');
  if (f->residencyFailure == 4) return E_FAIL;
  f->pagingDone = args->FenceValueArray[0]; CHECK(SetEvent(args->hAsyncEvent)); return S_OK;
}
static HRESULT APIENTRY waitPagingGpu(HANDLE h, const D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMGPU* args) {
  f->checkRuntime();
  CHECK(h == &f->device && args->hContext == &f->contextCookie && args->ObjectCount == 1);
  CHECK(args->ObjectHandleArray[0] == 78 && args->MonitoredFenceValueArray[0] == f->pagingTarget);
  for (unsigned i = 1; i < 8; ++i) CHECK(args->Reserved[i] == 0);
  ++f->gpuPagingWaits; f->retire('g');
  // Scheduling a dependency does not complete the monitored CPU fence.
  if (f->gpuPagingResult == S_OK) f->queuedPagingFence = args->MonitoredFenceValueArray[0];
  return f->gpuPagingResult;
}
static std::unique_ptr<Fixture> setup(bool residency = false, bool gpuPaging = false, bool cpuPaging = true) {
  auto result = std::make_unique<Fixture>(); f = result.get();
  f->input.pfnAllocateCb = allocate; f->input.pfnDeallocateCb = deallocate;
  f->input.pfnLockCb = lock; f->input.pfnUnlockCb = unlock;
  f->input.pfnCreateContextCb = createContext; f->input.pfnDestroyContextCb = destroyContext;
  f->input.pfnEscapeCb = escape; f->input.pfnRenderCb = render;
  f->identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  f->identity->luid = f->luid; f->identity->runtime = &f->adapter;
  f->identity->query = query; f->identity->generation = f->generation; f->identity->capabilities = 3;
  if (residency) {
    f->identity->continuity = std::make_shared<ResidencyIdentity>();
    f->input.pfnCreatePagingQueueCb = createPaging; f->input.pfnDestroyPagingQueueCb = destroyPaging;
    f->input.pfnMakeResidentCb = makeResident; f->input.pfnEvictCb = evict;
    f->input.pfnWaitForSynchronizationObjectFromCpuCb = cpuPaging ? waitPaging : nullptr;
    f->input.pfnWaitForSynchronizationObjectFromGpuCb = gpuPaging ? waitPagingGpu : nullptr;
  }
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
  CHECK(!runtime->disableExtraThreads || GetCurrentThreadId() == f->runtimeThread);
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
  level = dxvk::umd::implementationFeatureLevel(level);
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
    runtime->disableExtraThreads ? D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS : 0, &level, 1,
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
  CHECK((GetCurrentThreadId() == f->runtimeThread) == f->bridge.disableExtraThreads);
  if (onFlush) onFlush();
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

static unsigned amortizedCalls = 0, alternateAmortizedCalls = 0;
static std::function<void()> onAmortized;
static void APIENTRY amortized(D3D10DDI_HRTCORELAYER runtime) {
  CHECK(runtime.handle == f && GetCurrentThreadId() == f->runtimeThread
    && runtimeHookDepth == 0 && errorHookDepth == 0);
  ++amortizedCalls;
  if (auto hook = std::exchange(onAmortized, {})) hook();
}
static void APIENTRY alternateAmortized(D3D10DDI_HRTCORELAYER runtime) {
  ++alternateAmortizedCalls; amortized(runtime);
}

static void successfulSubmissionAmortization() {
  auto fixture = setup();
  CHECK(f->gpu->close() == S_OK); f->bridge = {}; f->gpu.reset();
  auto service = std::make_shared<dxvk::umd::RuntimeService>();
  f->runtimeThread = GetCurrentThreadId();
  unsigned calls = 0;
  mwd_allocation a{};
  service->setAmortizedProcessing([&] {
    CHECK(GetCurrentThreadId() == f->runtimeThread && runtimeHookDepth == 0);
    ++calls;
    // Releasing the submitted allocation would fail while its pending guard
    // is still held. An amortized callback must run after submission returns.
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, a.token) == S_OK);
    a.token = nullptr;
  });
  f->gpu = RuntimeGpu::create(&f->device, f->input, f->identity, service);
  f->bridge = f->gpu->backend();
  service->run([&] { a = buffer(); });
  CHECK(calls == 0);
  f->renderFails = true;
  service->run([&] { CHECK(send(a) == E_FAIL); });
  CHECK(calls == 0);
  f->renderFails = false;
  service->run([&] { CHECK(send(a) == S_OK && send(a) == S_OK); });
  CHECK(calls == 1 && f->renders == 3 && !a.token && f->allocations.empty());
  service->run([] {});
  CHECK(calls == 1);
  // Terminal KMD callbacks belong to this caller, as in Device::close().
  // Backend jobs use service->run(), but direct RuntimeGpu::close() does not.
  CHECK(f->gpu->close() == S_OK);
  service->close();
  service->requestAmortizedProcessing(); service->run([] {});
  CHECK(calls == 1 && f->allocations.empty() && f->contextCloses == 1);
}

static void runtimeAmortizedProcessing(bool disableThreadCreation) {
  auto fixture = setup();
  CHECK(f->gpu->close() == S_OK); f->bridge = {}; f->gpu.reset();
  f->runtimeThread = GetCurrentThreadId(); earlyBackendFail = false;
  D3D11DDI_DEVICEFUNCS table{};
  auto kernel = f->input;
  void* storage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  void* queryStorage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  void* callbackStorage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  CHECK(storage && queryStorage && callbackStorage);
  auto* callbacks = static_cast<D3D11DDI_CORELAYER_DEVICECALLBACKS*>(callbackStorage);
  callbacks->pfnSetErrorCb = setError;
  callbacks->pfnPerformAmortizedProcessingCb = amortized;
  D3D10DDIARG_CREATEDEVICE args{};
  args.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  args.Version = D3D11_0_DDI_BUILD_VERSION << 16;
  args.Flags = UINT(D3D11DDI_3DPIPELINELEVEL_10_0)
    << D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
  if (disableThreadCreation) args.Flags |= D3D10DDI_CREATEDEVICE_FLAG_DISABLE_EXTRA_THREAD_CREATION;
  args.hDrvDevice.pDrvPrivate = storage; args.hRTDevice.handle = &f->device;
  args.hRTCoreLayer.handle = f;
  args.p11UMCallbacks = callbacks; args.pKTCallbacks = &kernel; args.p11DeviceFuncs = &table;
  CHECK(dxvk::umd::createAdapterDevice(f->identity, &args) == S_OK);
  // Native D3D11 records and submits on its DDI caller even when the runtime
  // permits helper threads. The callbacks below also verify the real thread
  // identity during nested Flush, retirement and final device destruction.
  CHECK(f->bridge.disableExtraThreads);
  CHECK(table.pfnFlush && table.pfnDestroyDevice);
  const D3D10DDIARG_CREATEQUERY queryArgs{D3D10DDI_QUERY_EVENT, 0};
  D3D10DDI_HQUERY queryHandle{queryStorage};
  CHECK(table.pfnCalcPrivateQuerySize(args.hDrvDevice, &queryArgs) <= 4096);
  table.pfnCreateQuery(args.hDrvDevice, &queryArgs, queryHandle, {});
  const unsigned before = amortizedCalls, rendered = f->renders;
  onFlush = [&] {
    auto a = buffer();
    CHECK(send(a) == S_OK && send(a) == S_OK);
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, a.token) == S_OK);
  };
  onAmortized = [&] {
    onFlush = {}; // The nested empty Flush must not submit another command.
    table.pfnDestroyQuery(args.hDrvDevice, queryHandle);
    DWORD previous = 0;
    CHECK(VirtualProtect(queryStorage, 4096, PAGE_NOACCESS, &previous));
    // Runtime amortized processing can synchronously reenter Flush. It must
    // remain serialized, release retired children and never recurse into the
    // amortized callback or read reclaimed query bytes.
    table.pfnFlush(args.hDrvDevice);
    CHECK(amortizedCalls == before + 1);
  };
  table.pfnFlush(args.hDrvDevice);
  CHECK(!onAmortized && amortizedCalls == before + 1 && f->renders == rendered + 2);
  callbacks->pfnPerformAmortizedProcessingCb = alternateAmortized;
  const unsigned alternateBefore = alternateAmortizedCalls;
  table.pfnFlush(args.hDrvDevice); // Empty Flush still drains runtime work.
  CHECK(alternateAmortizedCalls == alternateBefore + 1 && amortizedCalls == before + 2);
  onAmortized = [&] {
    table.pfnDestroyDevice(args.hDrvDevice);
    DWORD previous = 0;
    CHECK(VirtualProtect(storage, 4096, PAGE_NOACCESS, &previous));
    f->runtimeValid = false;
    CHECK(VirtualProtect(callbackStorage, 4096, PAGE_NOACCESS, &previous));
    // Reading either runtime callback slot after this point is invalid.
    throw std::runtime_error("retired runtime callback table");
  };
  table.pfnFlush(args.hDrvDevice);
  CHECK(!onAmortized && amortizedCalls == before + 3 && f->allocations.empty());
  const auto closedCount = amortizedCalls;
  table.pfnFlush(args.hDrvDevice); table.pfnDestroyDevice(args.hDrvDevice);
  CHECK(amortizedCalls == closedCount);
  CHECK(VirtualFree(queryStorage, 0, MEM_RELEASE) && VirtualFree(storage, 0, MEM_RELEASE)
    && VirtualFree(callbackStorage, 0, MEM_RELEASE));
  inspectionContext = nullptr;
  std::printf("D3D11 amortized processing PASS: successful submissions coalesced; empty Flush, live callback slot, nested Flush/query retirement and reclaimed device storage\n");
}

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
    args.Interface = D3D10_0_DDI_INTERFACE_VERSION;
    args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
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
      static DWORD WINAPI run(void* ptr) {
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
      }
    } submission{bridge};
    CHECK(submission.started);
    pendingSubmission = CreateThread(nullptr, 0, Submission::run, &submission, 0, nullptr);
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

static void gpuPagingDependencies() {
  for (bool cpuAvailable : {true, false}) {
    auto scope = setup(true, true, cpuAvailable); f->runtimeThread = GetCurrentThreadId();
    f->residencyPending = true; f->input = {}; // callbacks are device-owned copies
    auto a = buffer();
    CHECK(send(a) == S_OK);
    CHECK(f->gpuPagingWaits == 1 && f->pagingWaits == 0 && f->pagingDone == 0);
    CHECK(send(a) == S_OK && f->gpuPagingWaits == 1); // same dependency already queued
    auto b = buffer();
    CHECK(send(b) == S_OK && f->gpuPagingWaits == 2 && f->queuedPagingFence == 2);
    CHECK(f->pagingDone == 0 && f->pagingWaits == 0);
    CHECK(f->gpu->close() == S_OK && f->queueDestroys == 1 && f->deallocations == 2);
  }
  for (HRESULT result : {E_FAIL, S_FALSE}) {
    auto scope = setup(true, true); f->residencyPending = true; auto a = buffer();
    f->gpuPagingResult = result;
    CHECK(send(a) == E_FAIL && f->renders == 0 && f->pagingWaits == 0 && !f->queuedPagingFence);
    f->gpuPagingResult = S_OK;
    CHECK(send(a) == S_OK && f->gpuPagingWaits == 2 && f->pagingWaits == 0);
    CHECK(f->gpu->close() == S_OK);
  }
  {
    auto scope = setup(true, true); f->residencyPending = true; auto a = buffer();
    f->retireAt = 'g';
    CHECK(send(a) == DXGI_ERROR_DEVICE_REMOVED && f->renders == 0 && f->pagingWaits == 0);
    CHECK(f->gpu->close() == S_OK && f->queueDestroys == 0);
    f->allocations.clear(); // runtime final device destruction owns residuals
  }
}
static void explicitResidency() {
  gpuPagingDependencies();
  for (bool pending : {false, true}) {
    auto scope = setup(true); f->runtimeThread = GetCurrentThreadId();
    f->residencyPending = pending; f->input = {}; // copied callback ownership
    auto a = buffer(), b = buffer();
    CHECK(f->queueCreates == 1 && f->makes == 2 && f->pagingWaits == 0);
    CHECK(send(a) == S_OK && send(b) == S_OK);
    CHECK(f->pagingWaits == (pending ? 1u : 0u));
    CHECK(f->bridge.create.callbacks->retain(f->bridge.create.owner, a.token,
      &b) == S_OK);
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, a.token) == S_OK && f->evicts == 0);
    CHECK(f->gpu->close() == S_OK);
    CHECK(f->evicts == 2 && f->deallocations == 2 && f->queueDestroys == 1);
  }
  for (unsigned failure : {1u, 2u, 3u}) {
    auto scope = setup(true); f->residencyFailure = failure;
    mwd_allocation a{};
    const HRESULT hr = f->bridge.create.callbacks->allocate(f->bridge.create.owner,4096,4096,0,6,&a);
    CHECK(hr == (failure == 1 ? E_OUTOFMEMORY : failure == 2 ? S_OK : E_FAIL));
    CHECK(f->makes == (failure == 3 ? 1u : 2u));
    CHECK(bool(a.token) == (failure == 2));
    CHECK(f->gpu->close() == S_OK);
    CHECK(f->allocations.empty() && f->deallocations == 1 && f->evicts == (failure == 2 ? 1u : 0u));
  }
  {
    auto scope = setup(true); f->residencyPending = true; auto a = buffer();
    f->residencyFailure = 4; CHECK(send(a) == E_FAIL && f->renders == 0);
    f->residencyFailure = 0; CHECK(send(a) == S_OK && f->pagingWaits == 2);
    CHECK(f->gpu->close() == S_OK);
  }
  {
    auto scope = setup(true); auto a = buffer(); f->residencyFailure = 5;
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner,a.token) == S_OK);
    CHECK(f->evicts == 1 && f->deallocations == 1 && f->allocations.empty());
    CHECK(f->gpu->close() == S_OK && f->evicts == 1);
  }
  for (char point : {'q', 'm', 'e', 'w'}) {
    auto scope = setup(true); mwd_allocation a{};
    if (point == 'e' || point == 'w') { f->residencyPending = true; a = buffer(); }
    f->retireAt = point;
    HRESULT hr = point == 'w' ? send(a) : point == 'e'
      ? f->bridge.create.callbacks->release(f->bridge.create.owner,a.token)
      : f->bridge.create.callbacks->allocate(f->bridge.create.owner,4096,4096,0,6,&a);
    CHECK(hr == DXGI_ERROR_DEVICE_REMOVED);
    CHECK(f->gpu->close() == S_OK && f->queueDestroys == 0);
    f->allocations.clear(); // runtime final device destruction owns residuals
  }
}
static void inlineServiceLifetime() {
  dxvk::umd::RuntimeService service(false, true);
  service.allowDeferredCalls();
  const DWORD caller = GetCurrentThreadId();
  unsigned released = 0, callbacks = 0;
  struct Retired final : dxvk::umd::RuntimeService::Retirement {
    DWORD caller; unsigned& released;
    Retired(DWORD t, unsigned& n) : caller(t), released(n) { }
    void release() noexcept override {
      CHECK(GetCurrentThreadId() == caller); ++released;
    }
  };
  service.setAmortizedProcessing([&] {
    CHECK(GetCurrentThreadId() == caller && released == 1);
    ++callbacks;
    service.run([&] { service.requestAmortizedProcessing(); });
    CHECK(callbacks == 1);
  });
  CHECK(service.run([&] {
    CHECK(GetCurrentThreadId() == caller);
    CHECK(service.invoke([&] {
      service.run([&] { service.retire(new Retired(caller, released)); });
      CHECK(released == 0);
      return S_OK;
    }) == S_OK);
    service.requestAmortizedProcessing();
    return 73;
  }) == 73);
  CHECK(released == 1 && callbacks == 1);
  service.setAmortizedProcessing({});
  bool caught = false;
  try { service.run([&] {
    service.retire(new Retired(caller, released));
    throw std::runtime_error("inline cleanup");
  }); } catch (const std::runtime_error&) { caught = true; }
  CHECK(caught && released == 2);
  HRESULT foreignResult = S_OK;
  std::thread foreign([&] { foreignResult = service.invoke([] { CHECK(false); return S_OK; }); });
  foreign.join();
  CHECK(foreignResult == DXGI_ERROR_UNSUPPORTED);
  service.close();
  CHECK(service.run([&] { return service.invoke([] { CHECK(false); return S_OK; }); }) == DXGI_ERROR_DEVICE_REMOVED);
}

static void nativeSurfaceImport() {
  for (unsigned scenario = 0; scenario < 3; ++scenario) {
    auto scope = setup(true, true);
    auto firstOwner = std::make_shared<int>(1), secondOwner = std::make_shared<int>(2);
    mwd_allocation first{}, second{};
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, firstOwner, &first) == S_OK);
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, secondOwner, &second) == S_OK);
    const auto cb = f->bridge.create.callbacks; const auto owner = f->bridge.create.owner;
    CHECK(cb->release(owner, first.token) == S_OK && f->makes == 1);
    if (scenario) f->residencyFailure = 5;
    if (scenario != 1) f->outerHook = [&](char point) {
      if (point != 'e') return;
      f->outerHook = {}; CHECK(f->gpu->revokeNativeSurface(firstOwner) == S_OK);
    };
    CHECK(cb->release(owner, second.token) == (scenario ? E_FAIL : S_OK));
    CHECK(f->evicts == 1 && !f->outerHook && f->resident[901] == unsigned(scenario != 0));
    if (scenario) {
      f->residencyFailure = 0;
      CHECK(cb->release(owner, second.token) == S_OK && f->evicts == 2);
    }
    CHECK(!f->resident[901] && f->nativeReleaseCalls == 1 && f->gpu->close() == S_OK);
  }
  {
    auto scope = setup(); auto backing = std::make_shared<int>(1);
    mwd_allocation imported{}, retained{};
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, backing, &imported) == S_OK);
    f->outerHook = [&](char point) {
      if (point != 'Q') return;
      f->outerHook = {}; CHECK(f->gpu->revokeNativeSurface(backing) == S_OK);
    };
    const auto cb = f->bridge.create.callbacks; const auto owner = f->bridge.create.owner;
    CHECK(cb->retain(owner, imported.token, &retained) == DXGI_ERROR_DEVICE_REMOVED);
    CHECK(!retained.token && !retained.handle && !f->outerHook);
    CHECK(cb->release(owner, imported.token) == S_OK && f->gpu->close() == S_OK);
  }
  for (bool retry : {false, true}) {
    auto scope = setup(true, true);
    auto firstOwner = std::make_shared<int>(1), secondOwner = std::make_shared<int>(2);
    mwd_allocation first{}, next{};
    if (retry) {
      CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, firstOwner, &first) == S_OK);
      f->residencyFailure = 2;
    }
    const auto beforeMakes = f->makes;
    f->outerHook = [&](char point) {
      if (point != (retry ? 'm' : 'q')) return;
      f->outerHook = {};
      CHECK(f->gpu->revokeNativeSurface(secondOwner) == S_OK);
    };
    CHECK(f->gpu->importNativeSurface(902, 0x12345678, 8192, secondOwner, &next) == DXGI_ERROR_DEVICE_REMOVED);
    CHECK(!next.token && !f->outerHook && f->makes == beforeMakes + unsigned(retry));
    // No MakeResident is legal after queue-create revocation, nor a retry
    // after the first OOM callback has retired this runtime open.
    if (first.token) CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, first.token) == S_OK);
    CHECK(f->gpu->close() == S_OK);
  }
  {
    auto scope = setup();
    auto firstOwner = std::make_shared<int>(1), secondOwner = std::make_shared<int>(2);
    mwd_allocation first{}, second{};
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, firstOwner, &first) == S_OK);
    // Distinct OpenResource objects may describe the same kernel allocation.
    // Their host mapping and Vulkan BO must still have one physical identity.
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, secondOwner, &second) == S_OK);
    CHECK(first.token == second.token && first.address == second.address && f->nativeImportCalls == 1);
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, first.token) == S_OK);
    CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner, second.token) == S_OK);
    CHECK(f->nativeReleaseCalls == 1 && f->gpu->close() == S_OK);
  }
  for (bool sameHandle : {false, true}) {
    auto scope = setup(true, true);
    auto firstOwner = std::make_shared<int>(1), secondOwner = std::make_shared<int>(2);
    mwd_allocation first{}, second{}, retained{};
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, firstOwner, &first) == S_OK);
    CHECK(f->gpu->importNativeSurface(sameHandle ? 901 : 902, 0x12345678, 8192, secondOwner, &second) == S_OK);
    CHECK(first.token == second.token && f->nativeImportCalls == 1 && f->makes == (sameHandle ? 1u : 2u));
    auto a = buffer();
    auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    uint32_t stream[4] = {};
    mwd_reference refs[2] = {{first.token,0,8192,2,MWD_NATIVE_SURFACE_REFERENCE},{a.token,0,4096,3,0}};
    CHECK(cb->submit(owner, stream, sizeof(stream), refs, 2) == S_OK);
    const unsigned oldEvicts = f->evicts;
    CHECK(f->gpu->revokeNativeSurface(firstOwner) == S_OK && f->evicts == oldEvicts);
    if (!sameHandle) {
      f->nativeHandles.erase(901); f->resident.erase(901); // Runtime retires this open.
      f->expectedNativeHandle = 902;
    }
    CHECK(cb->retain(owner, first.token, &retained) == S_OK && retained.handle == f->expectedNativeHandle);
    CHECK(cb->submit(owner, stream, sizeof(stream), refs, 2) == S_OK);
    if (!sameHandle) {
      auto reusedOwner = std::make_shared<int>(3); mwd_allocation reused{};
      f->nativeHandles.insert(901);
      CHECK(f->gpu->importNativeSurface(901, 0x12345679, 8192, reusedOwner, &reused) == S_OK);
      CHECK(reused.token != first.token && reused.address != first.address);
      CHECK(cb->submit(owner, stream, sizeof(stream), refs, 2) == S_OK);
      CHECK(cb->release(owner, reused.token) == S_OK);
    }
    const auto beforeLast = f->callbackCalls;
    CHECK(f->gpu->revokeNativeSurface(secondOwner) == S_OK);
    f->nativeHandles.erase(f->expectedNativeHandle); f->resident.erase(f->expectedNativeHandle);
    CHECK(FAILED(cb->submit(owner, stream, sizeof(stream), refs, 2)) && f->callbackCalls == beforeLast);
    CHECK(cb->release(owner, retained.token) == S_OK && cb->release(owner, first.token) == S_OK);
    CHECK(cb->release(owner, second.token) == S_OK && cb->release(owner, a.token) == S_OK);
    CHECK(f->nativeImports.empty() && f->gpu->close() == S_OK);
  }
  for (char revokeAt : {'Q','g','R','m'}) {
    auto scope = setup(true, true);
    auto firstOwner = std::make_shared<int>(1), secondOwner = std::make_shared<int>(2);
    mwd_allocation first{}, second{};
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, firstOwner, &first) == S_OK);
    auto a = buffer();
    auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    auto installHook = [&] {
      f->outerHook = [&](char point) {
        if (point != revokeAt) return;
        f->outerHook = {};
        CHECK(f->gpu->revokeNativeSurface(revokeAt == 'm' ? secondOwner : firstOwner) == S_OK);
      };
    };
    if (revokeAt == 'm') installHook();
    f->residencyPending = true;
    const HRESULT added = f->gpu->importNativeSurface(902, 0x12345678, 8192, secondOwner, &second);
    if (revokeAt == 'm') {
      CHECK(added == DXGI_ERROR_DEVICE_REMOVED && !second.token);
      f->nativeHandles.erase(902); f->resident.erase(902);
    } else {
      CHECK(added == S_OK);
      installHook();
      if (revokeAt != 'R') f->expectedNativeHandle = 902;
    }
    uint32_t stream[4] = {};
    mwd_reference refs[2] = {{first.token,0,8192,2,MWD_NATIVE_SURFACE_REFERENCE},{a.token,0,4096,3,0}};
    CHECK(cb->submit(owner, stream, sizeof(stream), refs, 2) == S_OK);
    CHECK(!f->outerHook && f->nativeImportCalls == 1);
    if (revokeAt != 'm') {
      f->nativeHandles.erase(901); f->resident.erase(901); f->expectedNativeHandle = 902;
      CHECK(cb->submit(owner, stream, sizeof(stream), refs, 2) == S_OK);
      CHECK(cb->release(owner, second.token) == S_OK);
    }
    CHECK(cb->release(owner, first.token) == S_OK && cb->release(owner, a.token) == S_OK);
    CHECK(f->gpu->close() == S_OK);
  }
  for (bool residency : {false,true}) {
    auto scope = setup(residency,true);
    f->residencyPending = residency;
    auto backing = std::make_shared<int>(1), otherBacking = std::make_shared<int>(2);
    std::weak_ptr<void> lifetime = backing;
    mwd_allocation imported{}, unused{}, alias{}, retained{};
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, backing, &imported) == S_OK);
    CHECK(imported.token && imported.handle == 901 && imported.size == 8192
      && imported.generation == 73 && imported.flags == (4u | MWD_ALLOCATION_NATIVE_SURFACE));
    CHECK(f->gpu->importNativeSurface(901, 0x12345678, 8192, backing, &alias) == S_OK
      && alias.token == imported.token && f->nativeImportCalls == 1);
    CHECK(f->gpu->importNativeSurface(902, 0x12345679, 8192, otherBacking, &unused) == S_OK);
    auto cb = f->bridge.create.callbacks; auto owner = f->bridge.create.owner;
    CHECK(cb->retain(owner, imported.token, &retained) == S_OK && retained.token == imported.token);
    void* ptr = reinterpret_cast<void*>(1); uint32_t handle = 1;
    CHECK(cb->map(owner, imported.token, &ptr, &handle) == E_INVALIDARG && !ptr && !handle && !f->locks);
    auto a = buffer();
    CHECK(a.address >= unused.address + unused.size || a.address + a.size <= imported.address);
    uint32_t stream[4] = {}; mwd_reference refs[2] = {
      {imported.token,0,8192,2,MWD_NATIVE_SURFACE_REFERENCE}, {a.token,0,4096,3,0}};
    CHECK(cb->submit(owner,stream,sizeof(stream),refs,2) == S_OK);
    CHECK(f->renders == 1 && f->nativeImports.size() == 2 && !f->locks && !f->unlocks);
    // A live, unused native image is absent from the exact reference table.
    CHECK(send(a) == S_OK);
    f->expectedNativeAccess = 1; refs[0].flags = 1;
    CHECK(cb->submit(owner,stream,sizeof(stream),refs,2) == S_OK);
    const unsigned callbacks = f->callbackCalls;
    refs[0].patch_offset = 0; CHECK(cb->submit(owner,stream,sizeof(stream),refs,2) == E_INVALIDARG);
    refs[0].patch_offset = MWD_NATIVE_SURFACE_REFERENCE; refs[0].length = 4096;
    CHECK(cb->submit(owner,stream,sizeof(stream),refs,2) == E_INVALIDARG);
    refs[0].length = 8192; CHECK(cb->submit(owner,stream,sizeof(stream),refs,1) == E_INVALIDARG);
    refs[1] = refs[0]; CHECK(cb->submit(owner,stream,sizeof(stream),refs,2) == E_INVALIDARG);
    CHECK(f->callbackCalls == callbacks);
    CHECK(cb->release(owner,alias.token) == S_OK && cb->release(owner,retained.token) == S_OK
      && f->nativeReleaseCalls == 0);
    backing.reset(); CHECK(!lifetime.expired());
    f->nativeReleaseResult = E_FAIL;
    CHECK(cb->release(owner,imported.token) == E_FAIL && !lifetime.expired());
    f->nativeReleaseResult = S_OK;
    CHECK(cb->release(owner,imported.token) == S_OK && lifetime.expired());
    CHECK(cb->release(owner,unused.token) == S_OK && cb->release(owner,a.token) == S_OK);
    CHECK(f->nativeImports.empty() && f->deallocations == 1 && f->gpu->close() == S_OK);
    if (residency) CHECK(f->makes == 3 && f->evicts == 3 && f->gpuPagingWaits == 1 && f->pagingDone == 0);
  }
  for (unsigned failure = 0; failure < 4; ++failure) {
    auto scope = setup(); auto backing = std::make_shared<int>(1);
    if (failure == 0) f->nativeImportResult = E_FAIL;
    if (failure == 1) f->nativeImportResult = S_FALSE;
    if (failure == 2) f->malformedNative = true;
    if (failure == 3) { f->nativeImportResult = E_FAIL; f->nativeReleaseResult = E_FAIL; }
    mwd_allocation out{};
    CHECK(f->gpu->importNativeSurface(901,0x12345678,8192,backing,&out) == E_FAIL);
    CHECK(!out.token && !out.handle && f->nativeReleaseCalls == 1 && !f->deallocations);
    if (failure == 3) {
      auto a = buffer(); CHECK(a.address >= 0x100002000ull);
      CHECK(f->bridge.create.callbacks->release(f->bridge.create.owner,a.token) == S_OK);
      CHECK(f->gpu->close() == E_FAIL && f->nativeImports.empty() && backing.use_count() == 1);
    } else CHECK(f->nativeImports.empty() && backing.use_count() == 1 && f->gpu->close() == S_OK);
  }
  {
    auto scope = setup(); auto backing = std::make_shared<int>(1); mwd_allocation out{};
    CHECK(f->gpu->importNativeSurface(0,1,8192,backing,&out) == E_INVALIDARG);
    CHECK(f->gpu->importNativeSurface(901,0,8192,backing,&out) == E_INVALIDARG);
    CHECK(f->gpu->importNativeSurface(901,UINT64_MAX,8192,backing,&out) == E_INVALIDARG);
    CHECK(f->gpu->importNativeSurface(901,1,8193,backing,&out) == E_INVALIDARG);
    CHECK(f->gpu->importNativeSurface(901,1,8192,{},&out) == E_INVALIDARG);
    CHECK(!f->callbackCalls && !out.token);
    CHECK(f->gpu->importNativeSurface(901,1,8192,backing,&out) == S_OK);
    mwd_allocation alias{};
    CHECK(f->gpu->importNativeSurface(902,1,8192,backing,&alias) == E_INVALIDARG);
    CHECK(f->gpu->importNativeSurface(901,1,4096,backing,&alias) == E_INVALIDARG);
    CHECK(f->gpu->importNativeSurface(901,2,8192,backing,&alias) == E_INVALIDARG);
    CHECK(f->nativeImportCalls == 1 && f->gpu->close() == S_OK);
  }
  {
    auto scope = setup(); auto backing = std::make_shared<int>(1); mwd_allocation out{};
    f->retireAt = 'i';
    CHECK(f->gpu->importNativeSurface(901,1,8192,backing,&out) == DXGI_ERROR_DEVICE_REMOVED);
    CHECK(!out.token && f->nativeReleaseCalls == 0 && f->contextCloses == 0);
    f->nativeImports.clear(); // Runtime's terminal teardown, not a UMD callback.
  }
}

int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--native-surface") == 0) {
    nativeSurfaceImport();
    std::printf("PASS %u native surface runtime controls; CPU callback fixture\n", checks.load());
    return 0;
  }
  inlineServiceLifetime();
  checks += runtimeDiagnosticsControls();
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
    args.Interface = D3D10_0_DDI_INTERFACE_VERSION;
    args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
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
  nativeSurfaceImport();
  explicitResidency();
  runtimeTeardown();
  successfulSubmissionAmortization();
  runtimeAmortizedProcessing(false);
  runtimeAmortizedProcessing(true);
  std::printf("PASS %u runtime GPU checks; ordinary caller dispatch, nested resource/view destruction and Flush retirement, shared owners, map/submit/reset and synchronous D3D10 teardown; backend-drains=%u child-drains=%u\n", checks.load(), backendDrains.load(), childDrains.load());
}
