#include "../src/umd/umd_allocation.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <functional>
using namespace dxvk::umd;
static unsigned checks, allocations, deallocations, modes, creates, frees, publishes;
static char deviceCookie, resourceCookie;
static unsigned scenario;
static bool primary;
static unsigned freeFailures = 0, freeAttempts = 0;
static std::function<void()> freeAction;
static RuntimeMemory* activeMemory;
static RuntimeAllocation* activeAllocation;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "native allocation line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
struct Source final : AdapterIdentitySource {
  RuntimeIdentity current;
  Source() { current.luid[0] = 71; current.generation = 2; current.capabilities = 3; }
  HRESULT query(RuntimeIdentity& out, RuntimeQueryStage) const override { out = current; return S_OK; }
  HRESULT nativeEscape(void* data, UINT bytes) const override {
    if (bytes == 40) {
      auto& p = *static_cast<NativeSurfacePublish*>(data);
      CHECK(p.key == 123 && p.opcode == 11); ++publishes; return S_OK;
    }
    CHECK(bytes == 128);
    auto& p = *static_cast<NativeSurfaceInfo*>(data);
    CHECK(p.headerSize == 128 && p.magic == 0x504d5644);
    if (p.opcode == 9) {
      CHECK(p.key == 123 && p.expected == 2); ++freeAttempts;
      if (freeAction) std::exchange(freeAction, {})();
      if (freeFailures) { --freeFailures; return E_FAIL; }
      ++frees; return S_OK;
    }
    CHECK(p.opcode == 8 && p.width == 64 && p.height == 48 && p.fourcc == 0x34325241);
    ++creates; p.key = 123; p.size = 12288; p.generation = 2; p.resource = 19;
    p.stride = 256; p.planes = p.layoutFlags = 1; return S_OK;
  }
};
static std::shared_ptr<Source> source;
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  ++allocations;
  CHECK(device == &deviceCookie && args && args->hResource == &resourceCookie);
  CHECK(args->NumAllocations == 1 && args->pAllocationInfo && !args->hKMResource);
  CHECK(args->PrivateDriverDataSize == 48 && args->pPrivateDriverData);
  const auto share = *static_cast<const NativeResourceShare*>(args->pPrivateDriverData);
  CHECK(validNativeResourceShare(share, 64) && share.key == 123 && share.stride == 256);
  auto& slot = args->pAllocationInfo[0];
  CHECK(slot.PrivateDriverDataSize == 80 && slot.pPrivateDriverData && !slot.hAllocation);
  const auto info = *static_cast<const AllocationInfo*>(slot.pPrivateDriverData);
  CHECK(info.flags == unsigned(primary) && info.width == 64 && info.height == 48
    && info.pitch == 256 && info.size == 12288 && !info.contextId && !info.resetGeneration);
  CHECK(slot.Flags.Primary == primary && slot.Flags.Value == unsigned(primary) && !slot.VidPnSourceId);
  CHECK(info.refreshNumerator == (primary ? 165u : 0u) && info.refreshDenominator == unsigned(primary));
  if (scenario == 1) return E_OUTOFMEMORY;
  if (scenario == 6) CHECK(activeAllocation->release() == S_OK);
  if (scenario == 8) CHECK(activeMemory->closeDeviceAllocations() == S_OK);
  slot.hAllocation = scenario == 3 ? 0 : 32; args->hKMResource = 44;
  if (scenario == 4) ++source->current.generation;
  if (scenario == 5) const_cast<NativeResourceShare*>(static_cast<const NativeResourceShare*>(args->pPrivateDriverData))->stride = 260;
  if (scenario == 7) CHECK(activeAllocation->release() == S_OK);
  if (scenario == 9) CHECK(activeMemory->closeDeviceAllocations() == S_OK);
  if (scenario == 10) {
    CHECK(activeMemory->closeDeviceAllocations() == S_OK);
    freeFailures = 1;
  }
  return scenario == 2 ? S_FALSE : S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  ++deallocations; CHECK(device == &deviceCookie && args->hResource == &resourceCookie);
  CHECK(!args->NumAllocations && !args->HandleList && creates > frees); return S_OK;
}
static HRESULT APIENTRY lock(HANDLE, D3DDDICB_LOCK*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY unlock(HANDLE, const D3DDDICB_UNLOCK*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY createContext(HANDLE, D3DDDICB_CREATECONTEXT*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY destroyContext(HANDLE, const D3DDDICB_DESTROYCONTEXT*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY render(HANDLE, D3DDDICB_RENDER*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY present(HANDLE, DXGIDDICB_PRESENT*) { CHECK(false); return E_FAIL; }
static HRESULT APIENTRY mode(HANDLE device, D3DDDICB_SETDISPLAYMODE* args) {
  ++modes; CHECK(device == &deviceCookie && args->hPrimaryAllocation == 32 && !publishes); return S_OK;
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  for (scenario = 0; scenario < 10; ++scenario) for (bool scanout : {false, true}) {
    std::printf("NATIVE_ALLOCATION_CASE scenario=%u primary=%u\n", scenario, scanout);
    primary = scanout;
    source = std::make_shared<Source>();
    auto identity = std::make_shared<AdapterIdentity>();
    std::memcpy(&identity->luid, source->current.luid.data(), sizeof(LUID));
    identity->generation = 2; identity->capabilities = 3; identity->continuity = source;
    D3DDDI_DEVICECALLBACKS cb = {};
    cb.pfnAllocateCb = allocate; cb.pfnDeallocateCb = deallocate;
    cb.pfnLockCb = lock; cb.pfnUnlockCb = unlock;
    cb.pfnCreateContextCb = createContext; cb.pfnDestroyContextCb = destroyContext;
    cb.pfnRenderCb = render; cb.pfnSetDisplayModeCb = mode;
    DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = present;
    RuntimeMemory memory; memory.initialize(&deviceCookie, cb, &dxgi, identity);
    RuntimeAllocation allocation;
    activeMemory = &memory; activeAllocation = &allocation;
    const auto oldFree = frees, oldDeallocate = deallocations;
    const HRESULT hr = memory.allocateNative(allocation, &resourceCookie, 64, 48,
      DXGI_FORMAT_B8G8R8A8_UNORM, primary, primary ? 165 : 0, primary ? 1 : 0);
    if (scenario == 0) {
      CHECK(hr == S_OK && allocation.native() && allocation.primary() == primary);
      CHECK(allocation.handle() == 32 && allocation.generation() == 2 && allocation.info().pitch == 256);
      CHECK(memory.upload(allocation, "x", 256) == DXGI_ERROR_UNSUPPORTED);
      CHECK(memory.download(allocation, nullptr, 256) == DXGI_ERROR_UNSUPPORTED);
      CHECK(memory.markNativeSubmitted(allocation) == S_OK);
      if (primary) CHECK(memory.setDisplayMode(allocation) == S_OK);
      RuntimeAllocation opened;
      CHECK(memory.adoptNative(opened, 63, 77, allocation.info(), allocation.nativeShare()) == S_OK);
      CHECK(opened.opened() && opened.native() && opened.primary() == primary);
      CHECK(opened.release() == S_OK && !opened.native() && deallocations == oldDeallocate);
      for (unsigned invalid = 0; invalid < 10; ++invalid) {
        auto info = allocation.info(); auto share = allocation.nativeShare();
        if (invalid == 0) ++info.magic;
        if (invalid == 1) info.flags = 2;
        if (invalid == 2) ++info.size;
        if (invalid == 3) ++info.requestedIova;
        if (invalid == 4) ++info.resetGeneration;
        if (invalid == 5) ++info.contextId;
        if (invalid == 6) ++share.stride;
        if (invalid == 7) share.key = 0;
        if (invalid == 8) share.flags = 0;
        if (invalid == 9) ++share.reserved2[1];
        CHECK(memory.adoptNative(opened, 63, 77, info, share) == E_INVALIDARG && !opened.native());
      }
      RuntimeAllocation moved(std::move(allocation));
      CHECK(!allocation.handle() && !allocation.native() && moved.handle() == 32 && moved.native());
      CHECK(moved.release() == S_OK && !moved.native());
    } else {
      CHECK(FAILED(hr) && !allocation.handle() && !allocation.native());
      if (scenario == 1) CHECK(hr == E_OUTOFMEMORY);
    }
    CHECK(allocation.release() == S_OK);
    CHECK(frees == oldFree + 1);
    CHECK(deallocations == oldDeallocate + ((scenario == 1 || scenario == 8) ? 0 : 1));
    CHECK(memory.closeDeviceAllocations() == S_OK);
  }
  CHECK(modes == 1);
  scenario = 0; primary = false;
  source = std::make_shared<Source>();
  auto identity = std::make_shared<AdapterIdentity>();
  std::memcpy(&identity->luid, source->current.luid.data(), sizeof(LUID));
  identity->generation = 2; identity->capabilities = 3; identity->continuity = source;
  D3DDDI_DEVICECALLBACKS cb = {};
  cb.pfnAllocateCb = allocate; cb.pfnDeallocateCb = deallocate;
  cb.pfnLockCb = lock; cb.pfnUnlockCb = unlock;
  cb.pfnCreateContextCb = createContext; cb.pfnDestroyContextCb = destroyContext;
  cb.pfnRenderCb = render; cb.pfnSetDisplayModeCb = mode;
  DXGI_DDI_BASE_CALLBACKS dxgi = {}; dxgi.pfnPresentCb = present;
  RuntimeMemory memory; memory.initialize(&deviceCookie, cb, &dxgi, identity);
  RuntimeAllocation allocation;
  activeMemory = &memory; activeAllocation = &allocation;
  CHECK(memory.allocateNative(allocation, &resourceCookie, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, false) == S_OK);
  const auto oldFrees = frees, oldAttempts = freeAttempts, oldDeallocate = deallocations;
  freeFailures = 1;
  CHECK(allocation.release() == E_FAIL);
  CHECK(!allocation.handle() && !allocation.native() && frees == oldFrees
    && freeAttempts == oldAttempts + 1 && deallocations == oldDeallocate + 1);
  CHECK(memory.closeDeviceAllocations() == S_OK);
  CHECK(frees == oldFrees + 1 && freeAttempts == oldAttempts + 2 && deallocations == oldDeallocate + 1);
  RuntimeMemory retry;
  retry.initialize(&deviceCookie, cb, &dxgi, identity);
  RuntimeAllocation first, next;
  CHECK(retry.allocateNative(first, &resourceCookie, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, false) == S_OK);
  const auto heldCreates = creates, heldAllocations = allocations, heldDeallocate = deallocations;
  freeFailures = 2;
  CHECK(first.release() == E_FAIL);
  // A refusal during drain retains the record and admits no new AHB/wrapper.
  freeAction = [&] { CHECK(NativeSurfaceLease::drainRetired() == DXGI_ERROR_WAS_STILL_DRAWING); };
  CHECK(retry.allocateNative(next, &resourceCookie, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, false) == E_FAIL);
  CHECK(!next.handle() && !next.native() && creates == heldCreates && allocations == heldAllocations);
  CHECK(retry.allocateNative(next, &resourceCookie, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, false) == S_OK);
  CHECK(creates == heldCreates + 1 && allocations == heldAllocations + 1);
  freeFailures = 1;
  // Report the failure even when terminal drain then succeeds. Retrying the
  // terminal call must not use the expired runtime cookie or free twice.
  CHECK(retry.closeDeviceAllocations() == E_FAIL);
  CHECK(!next.handle() && creates == frees && deallocations == heldDeallocate + 2);
  CHECK(retry.closeDeviceAllocations() == S_OK && deallocations == heldDeallocate + 2);
  RuntimeMemory reentrant;
  reentrant.initialize(&deviceCookie, cb, &dxgi, identity);
  RuntimeAllocation queued, duringDrain, afterDrain;
  CHECK(reentrant.allocateNative(queued, &resourceCookie, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, false) == S_OK);
  CHECK(reentrant.allocateNative(duringDrain, &resourceCookie, 64, 48, DXGI_FORMAT_B8G8R8A8_UNORM, false) == S_OK);
  freeFailures = 1;
  CHECK(queued.release() == E_FAIL);
  const auto beforeDrainCreates = creates, beforeDrainAllocations = allocations;
  freeAction = [&] {
    freeFailures = 1;
    CHECK(duringDrain.release() == E_FAIL);
  };
  // FREE of queued A succeeds, but its callback retires B unsuccessfully.
  // That newly queued B must still prevent admission of a third AHB.
  CHECK(reentrant.allocateNative(afterDrain, &resourceCookie, 64, 48,
    DXGI_FORMAT_B8G8R8A8_UNORM, false) == DXGI_ERROR_WAS_STILL_DRAWING);
  CHECK(!afterDrain.handle() && creates == beforeDrainCreates && allocations == beforeDrainAllocations);
  CHECK(reentrant.allocateNative(afterDrain, &resourceCookie, 64, 48,
    DXGI_FORMAT_B8G8R8A8_UNORM, false) == S_OK);
  CHECK(reentrant.closeDeviceAllocations() == S_OK && creates == frees);
  RuntimeMemory suspended;
  suspended.initialize(&deviceCookie, cb, &dxgi, identity);
  RuntimeAllocation late;
  activeMemory = &suspended; activeAllocation = &late; scenario = 10;
  const auto lateFrees = frees, lateDeallocations = deallocations;
  CHECK(FAILED(suspended.allocateNative(late, &resourceCookie, 64, 48,
    DXGI_FORMAT_B8G8R8A8_UNORM, false)));
  CHECK(!late.handle() && !late.native() && frees == lateFrees && deallocations == lateDeallocations + 1);
  // The final local lease outlived terminal close inside AllocateCb. Its
  // destructor retained failed ownership without using that runtime again.
  CHECK(suspended.closeDeviceAllocations() == S_OK && creates == frees);
  CHECK(deallocations == lateDeallocations + 1);
  std::printf("PASS native allocation %u checks creates=%u frees=%u wrappers=%u/%u\n",
    checks, creates, frees, allocations, deallocations);
}
