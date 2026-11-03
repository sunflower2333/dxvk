#include "../src/umd/umd_allocation.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>
#include <atomic>
#include <thread>

using dxvk::umd::RuntimeAllocation;
using dxvk::umd::RuntimeMemory;
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { \
  std::fprintf(stderr, "allocation check %u line %d: %s\n", checks, __LINE__, #c); \
  std::exit(1); } } while (0)

static char deviceCookie, resourceCookie, contextCookie, dxgiCookie;
static std::vector<char> calls;
static unsigned char backing[24];
static HRESULT allocateResult = S_OK, lockResult = S_OK, unlockResult = S_OK;
static HRESULT contextResult = S_OK, presentResult = S_OK, deallocateResult = S_OK;
static bool zeroAllocation, zeroResource, nullLock, changedLock, zeroContext, readLock;
static UINT requestedFormat = 3;
static RuntimeAllocation* releaseReentry;
static RuntimeMemory* closeReentry;
static char adapterCookie, resetDuring;
static uint64_t generation = 42, capabilities = 3;
static LUID luid = {0x12345678, -97};
static HRESULT queryResult = S_OK;
static unsigned queries;
static RuntimeMemory* queryReentry;
static bool explicitResidency;
static unsigned residencyReferences, residentCalls;
static HRESULT pagingResult = S_OK, residentResult = S_OK, waitResult = S_OK, evictResult = S_OK;
static bool nullPagingSync, trimOnce, malformedResident, zeroPagingFence;
static RuntimeMemory* terminalResidencyReentry;
static RuntimeMemory* closeResidencyReentry;
static bool* invalidateResidency;
static std::atomic<bool> dmaPending{false};
static std::vector<std::thread> completionThreads;
static HRESULT completionResult = S_OK;
static bool completionTimeout;
static RuntimeMemory* terminalCompletionReentry;
static bool* invalidateCompletion;
static D3DKMT_HANDLE expectedDestination;
static UINT residentCount() { return expectedDestination && expectedDestination != 123 ? 2 : 1; }

static HRESULT APIENTRY signalCompletion(HANDLE device,
    const D3DDDICB_SIGNALSYNCHRONIZATIONOBJECT2* args) {
  calls.push_back('S');
  CHECK(device == &deviceCookie && args && args->hContext == &contextCookie);
  CHECK(args->Flags.Value == 2 && !args->ObjectCount && !args->BroadcastContextCount);
  CHECK(args->CpuEventHandle && dmaPending.load());
  for (auto handle : args->ObjectHandleArray) CHECK(!handle);
  if (terminalCompletionReentry) {
    auto memory = std::exchange(terminalCompletionReentry, nullptr);
    CHECK(memory->closeDeviceAllocations() == S_OK);
    return S_OK;
  }
  if (invalidateCompletion) {
    *std::exchange(invalidateCompletion, nullptr) = false;
    return S_OK;
  }
  if (completionResult != S_OK || completionTimeout) return completionResult;
  HANDLE event = nullptr;
  CHECK(DuplicateHandle(GetCurrentProcess(), args->CpuEventHandle,
    GetCurrentProcess(), &event, 0, FALSE, DUPLICATE_SAME_ACCESS));
  // Submission and signal-queueing do not complete the DMA. The caller must
  // wait for this scheduler-owned completion before dropping residency.
  completionThreads.emplace_back([event] {
    Sleep(15);
    dmaPending.store(false, std::memory_order_release);
    if (!SetEvent(event) || !CloseHandle(event)) std::abort();
  });
  return S_OK;
}

struct ResidencyIdentity final : dxvk::umd::AdapterIdentitySource {
  HRESULT query(dxvk::umd::RuntimeIdentity& out, dxvk::umd::RuntimeQueryStage) const override {
    std::memcpy(out.luid.data(), &luid, sizeof(luid));
    out.generation = generation; out.capabilities = capabilities;
    return S_OK;
  }
  bool explicitResidency() const override { return true; }
};
static HRESULT APIENTRY createPaging(HANDLE device, D3DDDICB_CREATEPAGINGQUEUE* args) {
  calls.push_back('G'); CHECK(device == &deviceCookie && args);
  CHECK(args->Priority == D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL);
  CHECK(!args->hPagingQueue && !args->hSyncObject);
  args->hPagingQueue = 751; args->hSyncObject = nullPagingSync ? 0 : 752;
  return pagingResult;
}
static HRESULT APIENTRY destroyPaging(HANDLE device, const D3DDDI_DESTROYPAGINGQUEUE* args) {
  calls.push_back('Q'); CHECK(device == &deviceCookie && args && args->hPagingQueue == 751);
  CHECK(!residencyReferences); return S_OK;
}
static HRESULT APIENTRY makeResident(HANDLE device, D3DDDI_MAKERESIDENT* args) {
  calls.push_back('M'); CHECK(device == &deviceCookie && args && args->hPagingQueue == 751);
  CHECK(args->NumAllocations == residentCount() && args->AllocationList && *args->AllocationList == 123);
  if (residentCount() == 2) CHECK(args->AllocationList[1] == expectedDestination);
  CHECK(!args->Flags.MustSucceed && args->Flags.CantTrimFurther == (residentCalls != 0));
  ++residentCalls;
  if (trimOnce && residentCalls == 1) return E_OUTOFMEMORY;
  if (residentResult == S_OK || residentResult == E_PENDING) {
    CHECK(!residencyReferences); residencyReferences += args->NumAllocations;
    if (residentResult == E_PENDING) args->PagingFenceValue = zeroPagingFence ? 0 : 91;
  }
  if (malformedResident) args->NumAllocations = 2;
  if (closeResidencyReentry) {
    auto memory = std::exchange(closeResidencyReentry, nullptr);
    CHECK(memory->close() == DXGI_ERROR_WAS_STILL_DRAWING);
  }
  if (terminalResidencyReentry) {
    auto memory = std::exchange(terminalResidencyReentry, nullptr);
    CHECK(memory->closeDeviceAllocations() == S_OK);
  }
  if (invalidateResidency) *std::exchange(invalidateResidency, nullptr) = false;
  return residentResult;
}
static HRESULT APIENTRY waitPaging(HANDLE device, const D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMCPU* args) {
  calls.push_back('W'); CHECK(device == &deviceCookie && args && args->ObjectCount == 1);
  CHECK(*args->ObjectHandleArray == 752 && *args->FenceValueArray == 91 && args->hAsyncEvent);
  CHECK(residencyReferences == residentCount());
  if (waitResult == S_OK) CHECK(SetEvent(args->hAsyncEvent));
  return waitResult;
}
static HRESULT APIENTRY evict(HANDLE device, D3DDDICB_EVICT* args) {
  calls.push_back('E'); CHECK(device == &deviceCookie && args && args->NumAllocations == residentCount());
  CHECK(args->AllocationList && *args->AllocationList == 123 && !args->Flags.Value);
  if (residentCount() == 2) CHECK(args->AllocationList[1] == expectedDestination);
  CHECK(residencyReferences == residentCount());
  CHECK(!dmaPending.load(std::memory_order_acquire));
  if (SUCCEEDED(evictResult)) residencyReferences -= args->NumAllocations;
  return evictResult;
}

static HRESULT APIENTRY query(HANDLE adapter, const D3DDDICB_QUERYADAPTERINFO* args) {
  CHECK(adapter == &adapterCookie && args && args->PrivateDriverDataSize == 160);
  ++queries;
  if (queryReentry) {
    auto memory = std::exchange(queryReentry, nullptr);
    RuntimeAllocation nested;
    CHECK(memory->allocate(nested, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
      == DXGI_ERROR_WAS_STILL_DRAWING);
  }
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (unsigned i = 0; i < 160; ++i) CHECK(!bytes[i]);
  auto set = [bytes](unsigned offset, uint64_t value, unsigned length) {
    for (unsigned i = 0; i < length; ++i) bytes[offset+i] = uint8_t(value >> (8*i));
  };
  set(0, 0x504d5644, 4); set(8, 128, 4); set(16, capabilities, 8); set(24, generation, 8);
  set(128, 0x44494c56, 4); set(132, 1, 4); set(136, 32, 4); set(140, 1, 4); set(152, 1, 4);
  std::memcpy(bytes + 144, &luid, sizeof(luid));
  return queryResult;
}

static void maybeReset(char phase) { if (resetDuring == phase) ++generation; }

static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  calls.push_back('A');
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie);
  CHECK(args->NumAllocations == 1 && args->pAllocationInfo);
  auto& allocation = args->pAllocationInfo[0];
  CHECK(!allocation.hAllocation && !allocation.Flags.Primary);
  CHECK(allocation.PrivateDriverDataSize == 80 && allocation.pPrivateDriverData);
  dxvk::umd::AllocationInfo info;
  std::memcpy(&info, allocation.pPrivateDriverData, sizeof(info));
  CHECK(info.magic == 0x504d5644 && info.version == 0 && info.headerSize == 80 && !info.reserved);
  CHECK(info.size == 24 && info.alignment == 4096 && info.flags == 2);
  CHECK(info.width == 3 && info.height == 2 && info.pitch == 12 && info.format == requestedFormat);
  CHECK(!info.requestedIova && !info.resetGeneration && !info.contextId);
  CHECK(!info.refreshNumerator && !info.refreshDenominator);
  // Output private data must not redefine the bridge's trusted copy bounds.
  std::memset(allocation.pPrivateDriverData, 0xff, sizeof(info));
  allocation.hAllocation = zeroAllocation ? 0 : 123;
  args->hKMResource = zeroResource ? 0 : 456;
  maybeReset('A');
  return allocateResult;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  calls.push_back('D');
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie);
  CHECK(!args->NumAllocations && !args->HandleList);
  // Runtime destruction retires outstanding OS references even if the last
  // Evict failed or a MakeResident callback entered terminal cleanup.
  if (explicitResidency && SUCCEEDED(deallocateResult)) residencyReferences = 0;
  if (explicitResidency && SUCCEEDED(deallocateResult)) dmaPending = false;
  if (releaseReentry) {
    auto allocation = std::exchange(releaseReentry, nullptr);
    CHECK(!allocation->handle() && !allocation->kernelResource()
      && !allocation->runtimeResource() && !allocation->generation());
    CHECK(allocation->release() == S_OK);
  }
  return deallocateResult;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  calls.push_back('L');
  CHECK(device == &deviceCookie && args && args->hAllocation == 123);
  CHECK(args->Flags.LockEntire && bool(args->Flags.WriteOnly) == !readLock
    && bool(args->Flags.ReadOnly) == readLock);
  CHECK(!args->Flags.Discard && !args->Flags.IgnoreSync && !args->Flags.DonotWait);
  CHECK(!args->NumPages && !args->pPages && !args->pData);
  args->pData = nullLock ? nullptr : backing;
  if (changedLock) args->hAllocation = 124;
  maybeReset('L');
  return lockResult;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  calls.push_back('U');
  CHECK(device == &deviceCookie && args && args->NumAllocations == 1 && args->phAllocations);
  CHECK(*args->phAllocations == (changedLock ? 124 : 123));
  maybeReset('U');
  return unlockResult;
}
static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
  calls.push_back('C');
  CHECK(device == &deviceCookie && args && args->EngineAffinity == 1 && !args->NodeOrdinal);
  CHECK(!args->pPrivateDriverData && !args->PrivateDriverDataSize);
  args->hContext = zeroContext ? nullptr : &contextCookie;
  maybeReset('C');
  return contextResult;
}
static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
  calls.push_back('X');
  CHECK(device == &deviceCookie && args && args->hContext == &contextCookie);
  if (closeReentry) {
    auto memory = std::exchange(closeReentry, nullptr);
    CHECK(memory->close() == S_OK);
  }
  return S_OK;
}
static HRESULT APIENTRY present(HANDLE device, DXGIDDICB_PRESENT* args) {
  calls.push_back('P');
  CHECK(device == &deviceCookie && args && args->hSrcAllocation == 123 && args->hDstAllocation == expectedDestination);
  CHECK(args->pDXGIContext == &dxgiCookie && args->hContext == &contextCookie);
  CHECK(!args->BroadcastContextCount);
  if (explicitResidency) CHECK(residencyReferences == residentCount());
  if (explicitResidency && presentResult == S_OK) dmaPending = true;
  maybeReset('P');
  return presentResult;
}
static HRESULT APIENTRY presentReplacement(HANDLE device, DXGIDDICB_PRESENT* args) {
  calls.push_back('Q');
  return present(device, args);
}

static void sequence(const char* expected) {
  CHECK(calls.size() == std::strlen(expected));
  CHECK(calls.empty() || std::memcmp(calls.data(), expected, calls.size()) == 0);
  calls.clear();
}

int main() {
  D3DDDI_DEVICECALLBACKS callbacks = {};
  callbacks.pfnAllocateCb = allocate; callbacks.pfnDeallocateCb = deallocate;
  callbacks.pfnLockCb = lock; callbacks.pfnUnlockCb = unlock;
  callbacks.pfnCreateContextCb = createContext; callbacks.pfnDestroyContextCb = destroyContext;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = present;
  RuntimeMemory memory;
  RuntimeAllocation allocation;
  CHECK(!memory.available());
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == DXGI_ERROR_UNSUPPORTED);
  memory.initialize(&deviceCookie, callbacks, &dxgi);
  CHECK(memory.available());
  CHECK(memory.allocate(allocation, nullptr, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 0, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 0, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 16385, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R16G16B16A16_FLOAT) == DXGI_ERROR_UNSUPPORTED);
  sequence("");
  allocateResult = E_OUTOFMEMORY;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_OUTOFMEMORY);
  CHECK(!allocation.handle()); sequence("A"); allocateResult = S_OK;
  allocateResult = S_FALSE;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_FAIL);
  CHECK(!allocation.handle()); sequence("AD"); allocateResult = S_OK;
  zeroAllocation = true;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_FAIL);
  CHECK(!allocation.handle()); sequence("AD"); zeroAllocation = false;
  zeroResource = true;
  // DWM's shared BGRA atlas returns a valid allocation but no hKMResource.
  // Its original resource cookie still owns synchronized transfers/release.
  requestedFormat = 1;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_B8G8R8A8_UNORM) == S_OK);
  CHECK(allocation.handle() == 123 && !allocation.kernelResource()
    && allocation.runtimeResource() == &resourceCookie); sequence("A");
  unsigned char sharedPixels[32], sharedReadback[32];
  for (unsigned i = 0; i < sizeof(sharedPixels); ++i) sharedPixels[i] = uint8_t(0x37 + i);
  std::memset(sharedReadback, 0xe7, sizeof(sharedReadback));
  CHECK(memory.upload(allocation, sharedPixels, 16) == S_OK); sequence("LU");
  readLock = true;
  CHECK(memory.download(allocation, sharedReadback, 16) == S_OK); sequence("LU");
  readLock = false;
  for (unsigned y = 0; y < 2; ++y) {
    CHECK(!std::memcmp(sharedPixels + 16 * y, sharedReadback + 16 * y, 12));
    for (unsigned x = 12; x < 16; ++x) CHECK(sharedReadback[16 * y + x] == 0xe7);
  }
  RuntimeAllocation movedShared(std::move(allocation));
  CHECK(!allocation.handle() && movedShared.handle() == 123 && !movedShared.kernelResource());
  releaseReentry = &movedShared;
  CHECK(movedShared.release() == S_OK); sequence("D");
  CHECK(movedShared.release() == S_OK); sequence("");
  requestedFormat = 3;
  CHECK(memory.allocatePresent(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == S_OK);
  CHECK(allocation.handle() == 123 && !allocation.kernelResource()
    && allocation.runtimeResource() == &resourceCookie); sequence("A");
  CHECK(allocation.release() == S_OK); sequence("D");
  zeroAllocation = true;
  CHECK(memory.allocatePresent(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_FAIL);
  CHECK(!allocation.handle()); sequence("AD"); zeroAllocation = false; zeroResource = false;
  zeroAllocation = true; deallocateResult = D3DDDIERR_DEVICEREMOVED;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
    == D3DDDIERR_DEVICEREMOVED);
  CHECK(!allocation.handle()); sequence("AD"); zeroAllocation = false; deallocateResult = S_OK;
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == S_OK);
  CHECK(allocation.handle() == 123); sequence("A");
  CHECK(allocation.kernelResource() == 456 && allocation.runtimeResource() == &resourceCookie);
  CHECK(memory.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == E_INVALIDARG);
  DXGI_DDI_ARG_PRESENT args = {};
  args.pDXGIContext = &dxgiCookie; args.Flags.Blt = 1;
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("");
  unsigned char pixels[32];
  for (unsigned i = 0; i < sizeof(pixels); i++) pixels[i] = static_cast<unsigned char>(i);
  CHECK(memory.upload(allocation, nullptr, 16) == E_INVALIDARG);
  CHECK(memory.upload(allocation, pixels, 11) == E_INVALIDARG); sequence("");
  CHECK(memory.upload(allocation, pixels, 16) == S_OK); sequence("LU");
  CHECK(!std::memcmp(backing, pixels, 12) && !std::memcmp(backing + 12, pixels + 16, 12));
  CHECK(memory.present(allocation, args) == S_OK); sequence("CP");
  CHECK(memory.present(allocation, args) == S_OK); sequence("P");
  // DXGI can relocate its callback entry points between DDI calls. The UMD
  // must follow the runtime-owned table rather than a creation-time copy.
  dxgi.pfnPresentCb = presentReplacement;
  CHECK(memory.present(allocation, args) == S_OK); sequence("QP");
  dxgi.pfnPresentCb = nullptr;
  CHECK(!memory.available());
  CHECK(memory.present(allocation, args) == DXGI_ERROR_UNSUPPORTED); sequence("");
  dxgi.pfnPresentCb = present;
  auto bad = args; bad.Flags.Flip = 1;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.SrcSubResourceIndex = 1;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.DstSubResourceIndex = 1;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.hDstResource = reinterpret_cast<UINT_PTR>(&resourceCookie);
  CHECK(memory.present(allocation, bad) == E_INVALIDARG);
  bad = args; bad.pDXGIContext = nullptr;
  CHECK(memory.present(allocation, bad) == E_INVALIDARG); sequence("");
  presentResult = DXGI_ERROR_DEVICE_REMOVED;
  CHECK(memory.present(allocation, args) == DXGI_ERROR_DEVICE_REMOVED); sequence("P"); presentResult = S_OK;
  lockResult = DXGI_DDI_ERR_WASSTILLDRAWING;
  CHECK(memory.upload(allocation, pixels, 16) == DXGI_DDI_ERR_WASSTILLDRAWING);
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("L"); lockResult = S_OK;
  lockResult = S_FALSE;
  CHECK(memory.upload(allocation, pixels, 16) == E_FAIL); sequence("LU"); lockResult = S_OK;
  nullLock = true;
  CHECK(memory.upload(allocation, pixels, 16) == E_FAIL);
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("LU"); nullLock = false;
  changedLock = true;
  CHECK(memory.upload(allocation, pixels, 16) == E_FAIL); sequence("LU"); changedLock = false;
  unlockResult = DXGI_ERROR_DEVICE_REMOVED;
  CHECK(memory.upload(allocation, pixels, 16) == DXGI_ERROR_DEVICE_REMOVED);
  CHECK(memory.present(allocation, args) == E_INVALIDARG); sequence("LU"); unlockResult = S_OK;
  CHECK(memory.upload(allocation, pixels, 16) == S_OK); sequence("LU");
  CHECK(memory.close() == S_OK); sequence("X");
  contextResult = E_OUTOFMEMORY;
  CHECK(memory.present(allocation, args) == E_OUTOFMEMORY); sequence("C"); contextResult = S_OK;
  contextResult = S_FALSE;
  CHECK(memory.present(allocation, args) == E_FAIL); sequence("CX"); contextResult = S_OK;
  zeroContext = true;
  CHECK(memory.present(allocation, args) == E_FAIL); sequence("C"); zeroContext = false;
  CHECK(memory.present(allocation, args) == S_OK); sequence("CP");
  RuntimeMemory other;
  other.initialize(&deviceCookie, callbacks, &dxgi);
  CHECK(other.upload(allocation, pixels, 16) == E_INVALIDARG);
  CHECK(other.present(allocation, args) == E_INVALIDARG);
  CHECK(other.release(allocation) == E_INVALIDARG); sequence("");
  releaseReentry = &allocation;
  CHECK(allocation.release() == S_OK && !allocation.handle()); sequence("D");
  CHECK(allocation.release() == S_OK); sequence("");
  requestedFormat = 1;
  {
    RuntimeAllocation bgra;
    CHECK(memory.allocate(bgra, &resourceCookie, 3, 2, DXGI_FORMAT_B8G8R8A8_UNORM) == S_OK);
  }
  sequence("AD");
  closeReentry = &memory;
  CHECK(memory.close() == S_OK); sequence("X");
  CHECK(memory.close() == S_OK); sequence("");
  callbacks.pfnUnlockCb = nullptr;
  other.initialize(&deviceCookie, callbacks, &dxgi);
  CHECK(!other.available());
  CHECK(other.allocate(allocation, &resourceCookie, 3, 2, DXGI_FORMAT_B8G8R8A8_UNORM) == DXGI_ERROR_UNSUPPORTED);
  sequence("");

  callbacks.pfnUnlockCb = unlock; requestedFormat = 3;
  auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
  identity->luid = luid; identity->generation = generation;
  identity->capabilities = capabilities; identity->runtime = &adapterCookie;
  identity->query = query;
  for (char phase : {'A', 'L', 'U', 'C', 'P'}) {
    RuntimeMemory bound;
    bound.initialize(&deviceCookie, callbacks, &dxgi, identity);
    RuntimeAllocation owned;
    generation = identity->generation;
    resetDuring = phase == 'A' ? 'A' : 0;
    queryReentry = &bound;
    HRESULT hr = bound.allocate(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (phase == 'A') {
      CHECK(hr == DXGI_ERROR_DEVICE_REMOVED && !owned.handle()); sequence("AD");
    } else {
      CHECK(hr == S_OK && owned.generation() == identity->generation);
      CHECK(owned.kernelResource() == 456 && owned.runtimeResource() == &resourceCookie);
      sequence("A");
      resetDuring = phase;
      std::memset(backing, 0x5a, sizeof(backing));
      hr = bound.upload(owned, pixels, 16);
      if (phase == 'L' || phase == 'U') {
        CHECK(hr == DXGI_ERROR_DEVICE_REMOVED); sequence("LU");
        if (phase == 'L') for (auto value : backing) CHECK(value == 0x5a);
      } else {
        CHECK(hr == S_OK); sequence("LU");
        CHECK(bound.present(owned, args) == DXGI_ERROR_DEVICE_REMOVED);
        sequence(phase == 'C' ? "C" : "CP");
      }
      // Callback success followed by reset never admits another publication,
      // even if the callback later reports the old generation again.
      generation = identity->generation; resetDuring = 0;
      CHECK(bound.upload(owned, pixels, 16) == DXGI_ERROR_DEVICE_REMOVED); sequence("");
      RuntimeAllocation moved(std::move(owned));
      CHECK(!owned.handle() && moved.handle() == 123 && moved.kernelResource() == 456);
      CHECK(moved.release() == S_OK); sequence("D");
      CHECK(bound.close() == S_OK); sequence(phase == 'C' || phase == 'P' ? "X" : "");
    }
    resetDuring = 0; generation = identity->generation;
    CHECK(bound.allocate(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
      == DXGI_ERROR_DEVICE_REMOVED); sequence("");
  }
  for (unsigned mismatch = 0; mismatch < 3; ++mismatch) {
    RuntimeMemory bound;
    bound.initialize(&deviceCookie, callbacks, &dxgi, identity);
    RuntimeAllocation owned;
    if (mismatch == 0) ++luid.LowPart;
    if (mismatch == 1) ++capabilities;
    if (mismatch == 2) queryResult = S_FALSE;
    CHECK(bound.allocate(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM)
      == (mismatch == 2 ? E_FAIL : DXGI_ERROR_DEVICE_REMOVED)); sequence("");
    luid = identity->luid; capabilities = identity->capabilities; queryResult = S_OK;
  }
  CHECK(queries > 20);
  callbacks.pfnCreatePagingQueueCb = createPaging; callbacks.pfnDestroyPagingQueueCb = destroyPaging;
  callbacks.pfnMakeResidentCb = makeResident; callbacks.pfnEvictCb = evict;
  callbacks.pfnWaitForSynchronizationObjectFromCpuCb = waitPaging;
  callbacks.pfnSignalSynchronizationObject2Cb = signalCompletion;
  identity->continuity = std::make_shared<ResidencyIdentity>();
  explicitResidency = true;
  const char* residencySequences[] = {"CGMPSE", "CGMWPSE", "CGM", "CGMMPSE", "CGMM",
    "CGMWE", "CGMPSE", "CG", "CG", "CGMDQX", "CGME", "CGMPSE", "CGME", "CGMPSE", "C", "CGM", "CGME",
    "CGMPS", "CGMPS", "C", "CGMPS", "CGMPSDQX", "CGMPS", "CGMPE"};
  for (unsigned scenario = 0; scenario < 24; ++scenario) {
    pagingResult = residentResult = waitResult = evictResult = S_OK;
    presentResult = completionResult = S_OK; completionTimeout = false;
    nullPagingSync = trimOnce = malformedResident = zeroPagingFence = false;
    residentCalls = residencyReferences = 0;
    bool live = true;
    HRESULT expected = S_OK;
    RuntimeMemory bound;
    RuntimeAllocation owned;
    if (scenario == 1) residentResult = E_PENDING;
    if (scenario == 2) { residentResult = E_FAIL; expected = E_FAIL; }
    if (scenario == 3) trimOnce = true;
    if (scenario == 4) { residentResult = E_OUTOFMEMORY; expected = E_OUTOFMEMORY; }
    if (scenario == 5) { residentResult = E_PENDING; waitResult = E_FAIL; expected = E_FAIL; }
    if (scenario == 6) { evictResult = E_FAIL; expected = E_FAIL; }
    if (scenario == 7) { pagingResult = E_FAIL; expected = E_FAIL; }
    if (scenario == 8) { nullPagingSync = true; expected = E_FAIL; }
    if (scenario == 9) { terminalResidencyReentry = &bound; expected = DXGI_ERROR_DEVICE_REMOVED; }
    if (scenario == 10) { malformedResident = true; expected = E_FAIL; }
    if (scenario == 11) { evictResult = S_FALSE; expected = E_FAIL; }
    if (scenario == 12) { invalidateResidency = &live; expected = DXGI_ERROR_DEVICE_REMOVED; }
    if (scenario == 13) closeResidencyReentry = &bound;
    if (scenario == 14) { callbacks.pfnMakeResidentCb = nullptr; expected = DXGI_ERROR_UNSUPPORTED; }
    if (scenario == 15) { residentResult = S_FALSE; expected = E_FAIL; }
    if (scenario == 16) { residentResult = E_PENDING; zeroPagingFence = true; expected = E_FAIL; }
    if (scenario == 17) { completionResult = E_FAIL; expected = E_FAIL; }
    if (scenario == 18) { completionResult = S_FALSE; expected = E_FAIL; }
    if (scenario == 19) { callbacks.pfnSignalSynchronizationObject2Cb = nullptr; expected = DXGI_ERROR_UNSUPPORTED; }
    if (scenario == 20) { completionTimeout = true; expected = HRESULT_FROM_WIN32(ERROR_TIMEOUT); }
    if (scenario == 21) { terminalCompletionReentry = &bound; expected = DXGI_ERROR_DEVICE_REMOVED; }
    if (scenario == 22) { invalidateCompletion = &live; expected = DXGI_ERROR_DEVICE_REMOVED; }
    if (scenario == 23) { presentResult = E_FAIL; expected = E_FAIL; }
    bound.initialize(&deviceCookie, callbacks, &dxgi, identity);
    callbacks.pfnMakeResidentCb = makeResident;
    callbacks.pfnSignalSynchronizationObject2Cb = signalCompletion;
    CHECK(bound.allocatePresent(owned, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == S_OK);
    CHECK(bound.upload(owned, pixels, 16) == S_OK); sequence("ALU");
    CHECK(bound.present(owned, args, [&] { return live; }) == expected);
    sequence(residencySequences[scenario]);
    if (scenario == 7 || scenario == 8) {
      // A failed queue with nonzero outputs must never be reused as ready.
      CHECK(bound.present(owned, args) == E_FAIL); sequence("");
    }
    if (scenario == 6 || scenario == 11 || scenario == 17 || scenario == 18
        || scenario == 20 || scenario == 22) {
      CHECK(bound.upload(owned, pixels, 16) == DXGI_ERROR_DEVICE_REMOVED); sequence("");
    }
    if (scenario == 17 || scenario == 18 || scenario == 20 || scenario == 22) {
      CHECK(dmaPending.load() && residencyReferences == 1);
    }
    if (expected == S_OK) {
      CHECK(!residencyReferences);
      RuntimeAllocation moved(std::move(owned));
      residentCalls = 0; trimOnce = false;
      CHECK(bound.present(moved, args) == S_OK);
      sequence(residentResult == E_PENDING ? "MWPSE" : "MPSE");
      CHECK(moved.release() == S_OK); sequence("D");
    } else {
      CHECK(owned.release() == S_OK); sequence(scenario == 9 || scenario == 21 ? "" : "D");
    }
    CHECK(!residencyReferences);
    CHECK(bound.close() == S_OK); sequence(scenario == 9 || scenario == 21 ? "" : scenario == 14 || scenario == 19 ? "X" : "QX");
  }
  // A known destination must be resident together with the source, including
  // a paging wait. Invalidated resources cannot reach Present, and aliased
  // source/destination handles must not double-acquire an allocation.
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    pagingResult = residentResult = waitResult = evictResult = presentResult = completionResult = S_OK;
    nullPagingSync = trimOnce = malformedResident = zeroPagingFence = completionTimeout = false;
    residentCalls = residencyReferences = 0;
    RuntimeMemory bound, foreign;
    RuntimeAllocation source, destination, other;
    bound.initialize(&deviceCookie, callbacks, &dxgi, identity);
    foreign.initialize(&deviceCookie, callbacks, &dxgi, identity);
    CHECK(bound.allocatePresent(source, &resourceCookie, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM) == S_OK);
    CHECK(bound.upload(source, pixels, 16) == S_OK); sequence("ALU");
    CHECK(bound.adopt(destination, 789, 0, source.info()) == S_OK);
    CHECK(foreign.adopt(other, 790, 0, source.info()) == S_OK); sequence("");
    auto request = args; request.hDstResource = reinterpret_cast<UINT_PTR>(&resourceCookie);
    CHECK(bound.present(source, request, {}, &other) == E_INVALIDARG);
    CHECK(bound.present(source, args, {}, &destination) == E_INVALIDARG); sequence("");
    bool live = true;
    if (scenario == 1) residentResult = E_PENDING;
    if (scenario == 2) invalidateResidency = &live;
    expectedDestination = scenario == 3 ? 123 : 789;
    CHECK(bound.present(source, request, [&] { return live; }, scenario == 3 ? &source : &destination)
      == (scenario == 2 ? DXGI_ERROR_DEVICE_REMOVED : S_OK));
    sequence(scenario == 1 ? "CGMWPSE" : scenario == 2 ? "CGME" : "CGMPSE");
    CHECK(!dmaPending.load() && !residencyReferences);
    CHECK(destination.release() == S_OK && other.release() == S_OK); sequence("");
    CHECK(source.release() == S_OK); sequence("D");
    CHECK(bound.close() == S_OK && foreign.close() == S_OK); sequence("QX");
    expectedDestination = 0;
  }
  explicitResidency = false;
  for (auto& thread : completionThreads) thread.join();
  std::printf("runtime allocation/presentation PASS checks=%u; mock callbacks, no GPU\n", checks);
}
