#include "umd_allocation.h"
#include "umd_residency_transaction.h"
#include "umd_primary_policy.h"
#include <cstring>
#include <limits>
#include <utility>

namespace dxvk::umd {
// Own the actual callback request and its output storage across moves,
// reentry and terminal cleanup. The resource cookie alone proves no physical
// acquisition, and a callback's in-flight mapped address must not become a
// CPU access after its owner has been retired.
struct PrimaryAllocationTransaction {
  D3DDDICB_ALLOCATE request = {};
  D3DDDI_ALLOCATIONINFO slots[2] = {};
  AllocationInfo metadata[2];
  RuntimeAllocation* owner = nullptr;
  bool failed = false;
  bool acquired() const {
    return !failed && (request.hKMResource || slots[0].hAllocation || slots[1].hAllocation);
  }
};
struct PrimaryLockTransaction {
  D3DDDICB_LOCK request = {};
  D3DKMT_HANDLE handle = 0;
  bool inCallback = true, acquired = false, consumed = false, unlocking = false;
  bool ownsLock() const {
    return !consumed && !unlocking && (inCallback
      ? request.pData && request.hAllocation == handle : acquired);
  }
};
namespace {
HRESULT completed(HRESULT hr) { return hr == S_OK || FAILED(hr) ? hr : E_FAIL; }
static_assert(offsetof(D3DDDI_DEVICECALLBACKS, pfnSetPriorityCb) == 2 * sizeof(PFND3DDDI_ALLOCATECB));
static_assert(offsetof(D3DDDI_DEVICECALLBACKS, pfnQueryResidencyCb) == 3 * sizeof(PFND3DDDI_ALLOCATECB));
static_assert(offsetof(D3DDDI_DEVICECALLBACKS, pfnSetDisplayModeCb) == 4 * sizeof(PFND3DDDI_ALLOCATECB));
}

RuntimeAllocation::~RuntimeAllocation() {
  release();
  if (m_owner && m_cleanupOwner) m_owner->retainFailedPrimary(*this);
}
RuntimeAllocation::RuntimeAllocation(RuntimeAllocation&& other) noexcept
: m_owner(std::exchange(other.m_owner, nullptr)),
  m_resource(std::exchange(other.m_resource, nullptr)),
  m_handle(std::exchange(other.m_handle, 0)),
  m_kernelResource(std::exchange(other.m_kernelResource, 0)),
  m_stagingHandle(std::exchange(other.m_stagingHandle, 0)),
  m_generation(std::exchange(other.m_generation, 0)),
  m_info(other.m_info), m_published(std::exchange(other.m_published, false)),
  m_opened(std::exchange(other.m_opened, false)) {
  m_locked = std::exchange(other.m_locked, false);
  m_pendingAllocation = std::move(other.m_pendingAllocation);
  m_pendingLock = std::move(other.m_pendingLock);
  m_cleanupOwner = std::move(other.m_cleanupOwner);
  if (m_pendingAllocation) m_pendingAllocation->owner = this;
  if (other.m_tracked) m_owner->replace(other, *this);
}
HRESULT RuntimeAllocation::release() { return m_owner ? m_owner->release(*this) : S_OK; }
bool RuntimeAllocation::canRotateWith(const RuntimeAllocation& other) const noexcept {
  const auto mapped = [](const RuntimeAllocation& value) {
    return value.m_locked || (value.m_pendingLock && !value.m_pendingLock->consumed);
  };
  return !mapped(*this) && !mapped(other) && !m_pendingAllocation && !other.m_pendingAllocation
    && m_owner == other.m_owner && bool(m_handle) == bool(other.m_handle)
    && bool(m_stagingHandle) == bool(other.m_stagingHandle)
    && m_opened == other.m_opened && m_generation == other.m_generation
    && m_info.flags == other.m_info.flags && m_info.format == other.m_info.format
    && m_info.width == other.m_info.width && m_info.height == other.m_info.height;
}
void RuntimeAllocation::swapIdentity(RuntimeAllocation& other) noexcept {
  using std::swap;
  swap(m_handle, other.m_handle);
  swap(m_kernelResource, other.m_kernelResource);
  swap(m_stagingHandle, other.m_stagingHandle);
  swap(m_generation, other.m_generation);
  swap(m_info, other.m_info);
  swap(m_published, other.m_published);
  // Owner, runtime resource and opened/owned lifetime belong to the private
  // resource object. canRotateWith excludes mixing their ownership kinds.
}
RuntimeMemory::~RuntimeMemory() { close(); }

HRESULT RuntimeMemory::retainFailedPrimary(RuntimeAllocation& allocation) {
  return call([&] {
    if (allocation.m_owner != this || !allocation.m_cleanupOwner) return E_INVALIDARG;
    auto storage = std::move(allocation.m_cleanupOwner);
    storage->~RuntimeAllocation();
    auto owned = new (storage.release()) RuntimeAllocation(std::move(allocation));
    owned->m_orphanNext = m_orphans; m_orphans = owned;
    return S_OK;
  });
}

void RuntimeMemory::track(RuntimeAllocation& allocation) noexcept {
  allocation.m_previous = nullptr; allocation.m_next = m_allocations;
  if (m_allocations) m_allocations->m_previous = &allocation;
  m_allocations = &allocation; allocation.m_tracked = true;
}
void RuntimeMemory::untrack(RuntimeAllocation& allocation) noexcept {
  if (!allocation.m_tracked) return;
  if (allocation.m_previous) allocation.m_previous->m_next = allocation.m_next;
  else m_allocations = allocation.m_next;
  if (allocation.m_next) allocation.m_next->m_previous = allocation.m_previous;
  allocation.m_previous = allocation.m_next = nullptr; allocation.m_tracked = false;
}
void RuntimeMemory::replace(RuntimeAllocation& previous, RuntimeAllocation& current) noexcept {
  current.m_previous = previous.m_previous; current.m_next = previous.m_next;
  current.m_tracked = true;
  if (current.m_previous) current.m_previous->m_next = &current;
  else m_allocations = &current;
  if (current.m_next) current.m_next->m_previous = &current;
  previous.m_previous = previous.m_next = nullptr; previous.m_tracked = false;
}

void RuntimeMemory::initialize(HANDLE device, const D3DDDI_DEVICECALLBACKS& kernel,
    const DXGI_DDI_BASE_CALLBACKS* dxgi, std::shared_ptr<const AdapterIdentity> identity,
    std::shared_ptr<RuntimeService> service) {
  m_device = device;
  m_callbacks.pfnAllocateCb = kernel.pfnAllocateCb;
  m_callbacks.pfnDeallocateCb = kernel.pfnDeallocateCb;
  // Both fields are in the original Vista prefix (slots2/3), not a newer tail.
  m_callbacks.pfnSetPriorityCb = kernel.pfnSetPriorityCb;
  m_callbacks.pfnQueryResidencyCb = kernel.pfnQueryResidencyCb;
  m_callbacks.pfnSetDisplayModeCb = kernel.pfnSetDisplayModeCb;
  m_callbacks.pfnRenderCb = kernel.pfnRenderCb;
  m_callbacks.pfnLockCb = kernel.pfnLockCb;
  m_callbacks.pfnUnlockCb = kernel.pfnUnlockCb;
  m_callbacks.pfnCreateContextCb = kernel.pfnCreateContextCb;
  m_callbacks.pfnDestroyContextCb = kernel.pfnDestroyContextCb;
  m_callbacks.pfnPresentCb = nullptr;
  m_dxgi = dxgi;
  m_identity = std::move(identity);
  m_service = std::move(service);
  m_removed = false;
  m_terminal = false;
}

void RuntimeMemory::initialize9(HANDLE device, const D3DDDI_DEVICECALLBACKS& kernel,
    std::shared_ptr<const AdapterIdentity> identity, std::shared_ptr<RuntimeService> service) {
  initialize(device, kernel, nullptr, std::move(identity), std::move(service));
  // The typed D3D9 callback is part of the common kernel table. DXGI has a
  // different callback ABI and retains its independently live table above.
  m_callbacks.pfnPresentCb = kernel.pfnPresentCb;
}

HRESULT RuntimeMemory::checkIdentity() {
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  // The older standalone allocation helper has no runtime adapter binding.
  // The actual native entry always supplies its immutable original identity.
  if (!m_identity) return S_OK;
  if (m_removed || !m_identity->available()) return DXGI_ERROR_DEVICE_REMOVED;
  if (m_querying) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_querying = true;
  struct QueryScope { bool& querying; ~QueryScope() { querying = false; } } scope{m_querying};
  RuntimeIdentity current;
  HRESULT hr = queryRuntimeIdentity(m_identity->runtime, m_identity->query, current);
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  if (!m_identity->available()) hr = DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK && (std::memcmp(current.luid.data(), &m_identity->luid, sizeof(LUID))
      || current.generation != m_identity->generation
      || current.capabilities != m_identity->capabilities))
    hr = DXGI_ERROR_DEVICE_REMOVED;
  if (hr == DXGI_ERROR_DEVICE_REMOVED) m_removed = true;
  return hr;
}

bool RuntimeMemory::available() const {
  return !m_terminal && m_device && m_callbacks.pfnAllocateCb && m_callbacks.pfnDeallocateCb
      && m_callbacks.pfnLockCb && m_callbacks.pfnUnlockCb
      && m_callbacks.pfnCreateContextCb && m_callbacks.pfnDestroyContextCb
      && (m_callbacks.pfnPresentCb || (m_dxgi && m_dxgi->pfnPresentCb));
}

HRESULT RuntimeMemory::allocateImpl(RuntimeAllocation& out, HANDLE resource,
    UINT width, UINT height, DXGI_FORMAT format) {
  if (!available()) return DXGI_ERROR_UNSUPPORTED;
  if (out.m_owner || !resource || !width || !height || width > 16384 || height > 16384)
    return E_INVALIDARG;
  AllocationInfo info;
  if (format == DXGI_FORMAT_B8G8R8A8_UNORM) info.format = 1;
  else if (format == DXGI_FORMAT_B8G8R8X8_UNORM) info.format = 2;
  else if (format == DXGI_FORMAT_R8G8B8A8_UNORM) info.format = 3;
  else return DXGI_ERROR_UNSUPPORTED;
  info.width = width; info.height = height; info.pitch = width * 4;
  info.size = uint64_t(info.pitch) * height;
  HRESULT hr = checkIdentity();
  if (FAILED(hr)) return hr;
  auto privateData = info;
  D3DDDI_ALLOCATIONINFO allocation = {};
  allocation.pPrivateDriverData = &privateData;
  allocation.PrivateDriverDataSize = sizeof(privateData);
  D3DDDICB_ALLOCATE request = {};
  request.hResource = resource;
  request.NumAllocations = 1;
  request.pAllocationInfo = &allocation;
  const HRESULT allocated = m_callbacks.pfnAllocateCb(m_device, &request);
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(allocated)) return allocated;
  hr = allocated == S_OK ? checkIdentity() : E_FAIL;
  if (FAILED(hr) || !allocation.hAllocation || !request.hKMResource) {
    // The callback reported allocation success: balance the resource even
    // when its returned handles are incomplete. Never expose a zero handle.
    D3DDDICB_DEALLOCATE cleanup = {};
    cleanup.hResource = resource;
    const HRESULT released = completed(m_callbacks.pfnDeallocateCb(m_device, &cleanup));
    return FAILED(released) ? released : FAILED(hr) ? hr : E_FAIL;
  }
  out.m_owner = this; out.m_resource = resource;
  out.m_handle = allocation.hAllocation; out.m_info = info;
  out.m_kernelResource = request.hKMResource;
  // KMD v0 requires zero generation/ContextId in non-native allocation wire
  // data. Keep adapter/reset ownership separately; do not change that ABI.
  out.m_generation = m_identity ? m_identity->generation : 0;
  out.m_published = false;
  out.m_opened = false;
  if (!m_callbacks.pfnPresentCb) track(out);
  return S_OK;
}

HRESULT RuntimeMemory::allocatePrimaryImpl(RuntimeAllocation& out, HANDLE resource,
    UINT width, UINT height, DXGI_FORMAT format, UINT numerator, UINT denominator) {
  if (!primaryAvailable()) return DXGI_ERROR_UNSUPPORTED;
  if (out.m_owner || !resource || !width || !height || width > 16384 || height > 16384
      || !numerator || !denominator) return E_INVALIDARG;
  AllocationInfo info;
  if (format == DXGI_FORMAT_B8G8R8A8_UNORM) info.format = 1;
  else if (format == DXGI_FORMAT_B8G8R8X8_UNORM) info.format = 2;
  else if (format == DXGI_FORMAT_R8G8B8A8_UNORM) info.format = 3;
  else return DXGI_ERROR_UNSUPPORTED;
  info.flags = 1; info.width = width; info.height = height; info.pitch = width * 4;
  info.size = uint64_t(info.pitch) * height;
  info.refreshNumerator = numerator; info.refreshDenominator = denominator;
  HRESULT hr = checkIdentity();
  if (hr != S_OK) return hr;
  std::shared_ptr<PrimaryAllocationTransaction> transaction;
  std::unique_ptr<RuntimeAllocation> cleanup;
  try {
    transaction = std::make_shared<PrimaryAllocationTransaction>();
    cleanup = std::make_unique<RuntimeAllocation>();
  }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  auto& metadata = transaction->metadata;
  metadata[0] = metadata[1] = info;
  metadata[1].flags = 2; metadata[1].refreshNumerator = metadata[1].refreshDenominator = 0;
  auto& allocations = transaction->slots;
  for (UINT i = 0; i < 2; ++i) {
    allocations[i].pPrivateDriverData = &metadata[i];
    allocations[i].PrivateDriverDataSize = sizeof(AllocationInfo);
  }
  allocations[0].Flags.Primary = 1;
  allocations[0].VidPnSourceId = 0;
  auto& request = transaction->request;
  request.hResource = resource; request.NumAllocations = 2; request.pAllocationInfo = allocations;
  // Reserve the construction owner before calling the runtime. Only actual
  // returned kernel handles prove acquisition: terminal retirement balances
  // those on this caller, and an all-zero canceled request needs no callback.
  out.m_owner = this; out.m_resource = resource; out.m_info = info;
  out.m_generation = m_identity ? m_identity->generation : 0;
  out.m_pendingAllocation = transaction;
  out.m_cleanupOwner = std::move(cleanup);
  transaction->owner = &out;
  track(out);
  const HRESULT allocated = m_callbacks.pfnAllocateCb(m_device, &request);
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  transaction->failed = FAILED(allocated);
  if (out.m_owner != this || out.m_pendingAllocation != transaction) {
    // Direct owner cancellation before callback outputs is not terminal
    // runtime retirement. Balance actual late acquisition on this still-live
    // caller; never assume callbacks remain valid after DestroyDevice.
    HRESULT released = S_OK;
    if (transaction->owner && transaction->owner->m_owner == this)
      released = releaseImpl(*transaction->owner);
    else if (transaction->acquired()) {
      D3DDDICB_DEALLOCATE request = {}; request.hResource = resource;
      released = completed(m_callbacks.pfnDeallocateCb(m_device, &request));
    }
    return FAILED(released) ? released : DXGI_ERROR_DEVICE_REMOVED;
  }
  if (FAILED(allocated)) {
    // A failed allocation acquired no ownership. Do not turn failure into a
    // DeallocateCb call; the construction-time reservation was local only.
    untrack(out); out.m_owner = nullptr; out.m_resource = nullptr; out.m_generation = 0;
    transaction->owner = nullptr;
    out.m_pendingAllocation.reset();
    return allocated;
  }
  out.m_handle = allocations[0].hAllocation;
  out.m_stagingHandle = allocations[1].hAllocation;
  out.m_kernelResource = request.hKMResource;
  hr = allocated == S_OK ? checkIdentity() : E_FAIL;
  if (hr == S_OK && (request.hResource != resource || request.NumAllocations != 2
      || request.pAllocationInfo != allocations || !out.m_handle || !out.m_stagingHandle
      || out.m_handle == out.m_stagingHandle || !out.m_kernelResource)) hr = E_FAIL;
  if (hr != S_OK) {
    if (m_terminal || out.m_owner != this) return DXGI_ERROR_DEVICE_REMOVED;
    const HRESULT released = releaseImpl(out);
    return FAILED(released) ? released : hr;
  }
  transaction->owner = nullptr; out.m_pendingAllocation.reset();
  return S_OK;
}

HRESULT RuntimeMemory::releaseImpl(RuntimeAllocation& allocation) {
  if (allocation.m_owner != this) return E_INVALIDARG;
  if (m_terminal) {
    const auto handle = allocation.m_stagingHandle ? allocation.m_stagingHandle : allocation.m_handle;
    const auto resource = allocation.m_resource;
    const bool opened = allocation.m_opened;
    const bool unresolvedUnlock = allocation.m_pendingLock && allocation.m_pendingLock->unlocking;
    const bool locked = allocation.m_locked || (allocation.m_pendingLock && allocation.m_pendingLock->ownsLock());
    const bool acquired = !allocation.m_pendingAllocation || allocation.m_pendingAllocation->acquired();
    if (allocation.m_pendingAllocation) allocation.m_pendingAllocation->owner = nullptr;
    if (allocation.m_pendingLock) allocation.m_pendingLock->consumed = true;
    const auto unlockCallback = m_callbacks.pfnUnlockCb;
    const auto deallocateCallback = m_callbacks.pfnDeallocateCb;
    const auto device = m_device;
    untrack(allocation);
    allocation.m_owner = nullptr; allocation.m_resource = nullptr;
    allocation.m_handle = 0; allocation.m_kernelResource = 0; allocation.m_stagingHandle = 0;
    allocation.m_generation = 0; allocation.m_published = false;
    allocation.m_opened = false; allocation.m_locked = false;
    allocation.m_pendingAllocation.reset(); allocation.m_pendingLock.reset();
    // A nested DestroyDevice cannot observe the return status of a suspended
    // UnlockCb. Do not guess that it succeeded, unlock twice, or deallocate a
    // potentially mapped entry. The actual runtime must finish its terminal
    // ownership; no callback can be deferred past this DestroyDevice return.
    HRESULT result = unresolvedUnlock ? DXGI_ERROR_WAS_STILL_DRAWING : S_OK;
    if (locked) {
      D3DDDICB_UNLOCK request = {}; request.NumAllocations = 1; request.phAllocations = &handle;
      result = completed(unlockCallback(device, &request));
    }
    // A failed unlock did not release the mapping. Report it and leave that
    // kernel ownership to actual runtime device cleanup, never deallocate a
    // mapped allocation or claim successful driver cleanup.
    if (!opened && acquired && SUCCEEDED(result)) {
      D3DDDICB_DEALLOCATE request = {}; request.hResource = resource;
      const HRESULT hr = completed(deallocateCallback(device, &request));
      if (FAILED(hr)) result = hr;
    }
    return result;
  }
  // A non-terminal callback can retire a mapped primary resource. Unlock its
  // staging entry while the resource remains in the ledger. Nested terminal
  // cleanup can then deallocate it, and this frame issues no late callback.
  if (allocation.m_releasing) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (allocation.m_stagingHandle && (allocation.m_locked
      || (allocation.m_pendingLock && !allocation.m_pendingLock->consumed))) {
    allocation.m_releasing = true;
    struct ReleaseScope { bool& flag; ~ReleaseScope() { flag = false; } } scope{allocation.m_releasing};
    const HRESULT hr = unlockPrimary(allocation);
    if (m_terminal || allocation.m_owner != this) return DXGI_ERROR_DEVICE_REMOVED;
    if (FAILED(hr)) return hr;
  }
  if (allocation.m_opened) {
    // A view, not an owner. Dropping it must not reach DeallocateCb: the
    // allocation outlives this resource and the creating process still uses
    // it. Retire the local state only.
    untrack(allocation);
    allocation.m_owner = nullptr; allocation.m_handle = 0;
    allocation.m_kernelResource = 0; allocation.m_generation = 0;
    allocation.m_published = false; allocation.m_opened = false;
    return S_OK;
  }
  D3DDDICB_DEALLOCATE request = {};
  request.hResource = allocation.m_resource;
  const auto deallocate = m_callbacks.pfnDeallocateCb;
  const HANDLE device = m_device;
  const bool acquired = !allocation.m_pendingAllocation || allocation.m_pendingAllocation->acquired();
  if (m_callbacks.pfnPresentCb) {
    // Typed9 serializes destruction with every DDI on this device. Preserve
    // ownership on a failed callback so DestroyResource can retry; guard even
    // a direct recursive release before invoking the runtime again.
    if (m_releasing9) return DXGI_ERROR_WAS_STILL_DRAWING;
    m_releasing9 = true;
    struct ReleaseScope { bool& flag; ~ReleaseScope() { flag = false; } } scope{m_releasing9};
    const HRESULT released = deallocate(device, &request);
    if (FAILED(released)) return released;
    // A callback-reported success freed the resource even if the status is
    // outside the exact-S_OK contract. Reject it without freeing twice.
    untrack(allocation);
    allocation.m_owner = nullptr; allocation.m_resource = nullptr;
    allocation.m_handle = 0; allocation.m_published = false;
    allocation.m_kernelResource = 0; allocation.m_generation = 0;
    return completed(released);
  }
  // Retire all user ownership before entering runtime code. A recursive
  // Release/DestroyResource must neither deallocate twice nor touch old state.
  untrack(allocation);
  if (allocation.m_pendingAllocation) allocation.m_pendingAllocation->owner = nullptr;
  allocation.m_owner = nullptr; allocation.m_resource = nullptr;
  allocation.m_handle = 0; allocation.m_published = false; allocation.m_stagingHandle = 0;
  allocation.m_kernelResource = 0; allocation.m_generation = 0;
  allocation.m_pendingAllocation.reset(); allocation.m_pendingLock.reset();
  return acquired ? completed(deallocate(device, &request)) : S_OK;
}

// Adopting is deliberately not allocating: no AllocateCb runs, so the balance
// this object normally keeps with DeallocateCb does not exist. The creating
// process owns the allocation; this device holds a view of it and the wire
// metadata that describes its pixels. Validate that metadata here rather than
// at every later transfer, because a hostile or mismatched private-data blob
// is the only thing standing between the open path and an out-of-bounds copy.
HRESULT RuntimeMemory::adoptImpl(RuntimeAllocation& out, D3DKMT_HANDLE allocation,
    D3DKMT_HANDLE kernelResource, const AllocationInfo& info) {
  if (!available()) return DXGI_ERROR_UNSUPPORTED;
  if (out.m_owner || !allocation) return E_INVALIDARG;
  if (info.magic != AllocationInfo{}.magic || info.version != AllocationInfo{}.version
      || info.headerSize != AllocationInfo{}.headerSize || info.reserved
      || !info.width || !info.height || info.width > 16384 || info.height > 16384
      || (info.format != 1 && info.format != 2 && info.format != 3)
      || info.pitch < uint64_t(info.width) * 4
      || uint64_t(info.pitch) * info.height > info.size)
    return E_INVALIDARG;
  // Only the two allocation kinds whose pixels this bridge can interpret. A
  // native or GPU-read-only allocation carries no CPU-visible linear image.
  if (info.flags != 1 && info.flags != 2) return DXGI_ERROR_UNSUPPORTED;
  const HRESULT hr = checkIdentity();
  if (FAILED(hr)) return hr;
  out.m_owner = this;
  out.m_resource = nullptr;
  out.m_handle = allocation;
  out.m_kernelResource = kernelResource;
  out.m_info = info;
  out.m_generation = m_identity ? m_identity->generation : 0;
  out.m_published = false;
  out.m_opened = true;
  if (!m_callbacks.pfnPresentCb) track(out);
  return S_OK;
}

// One body for both directions of the shared-surface copy. Publishing writes
// this device's cache into the kernel allocation; refreshing reads whatever
// the owning process last left there. The lock flags, the synchronization
// they imply and the balancing unlock are identical either way, and keeping
// them in one place is what stops the two directions drifting apart.
HRESULT RuntimeMemory::transferImpl(RuntimeAllocation& allocation, void* pixels,
    UINT rowPitch, bool publish) {
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  if (allocation.m_owner != this || !allocation.m_handle) return E_INVALIDARG;
  if (allocation.m_info.flags == 1 && !allocation.m_stagingHandle) return DXGI_ERROR_UNSUPPORTED;
  if (allocation.primary()) {
    const HRESULT pending = unlockPrimary(allocation);
    if (pending != S_OK) return pending;
  }
  if (publish) allocation.m_published = false;
  const auto& info = allocation.m_info;
  // Pitch is a stride, not a row length. They are equal for an allocation this
  // bridge created, which is why one value served as both until an adopted
  // allocation could arrive with padding: copying `pitch` bytes per row would
  // then carry that padding across and demand the local side be as wide.
  const uint64_t row = uint64_t(info.width) * 4;
  if (!pixels || rowPitch < row || info.pitch < row
      || uint64_t(info.height - 1) * rowPitch + row > std::numeric_limits<size_t>::max())
    return E_INVALIDARG;
  HRESULT hr = checkIdentity();
  if (FAILED(hr)) return hr;
  D3DDDICB_LOCK localLock = {};
  std::shared_ptr<PrimaryLockTransaction> pending;
  if (allocation.primary()) {
    try { pending = std::make_shared<PrimaryLockTransaction>(); }
    catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  }
  auto& lock = pending ? pending->request : localLock;
  const auto original = allocation.m_handle;
  const auto handle = allocation.m_stagingHandle ? allocation.m_stagingHandle : original;
  lock.hAllocation = handle;
  if (pending) { pending->handle = handle; allocation.m_pendingLock = pending; }
  lock.Flags.LockEntire = 1;
  lock.Flags.WriteOnly = publish ? 1 : 0;
  lock.Flags.ReadOnly = publish ? 0 : 1;
  // Do not discard or ignore synchronization: VidSch may still consume the
  // last Present. A successful synchronized lock precedes every CPU access.
  const HRESULT locked = m_callbacks.pfnLockCb(m_device, &lock);
  if (pending) {
    pending->inCallback = false;
    if (!pending->consumed) pending->acquired = SUCCEEDED(locked);
  }
  // Terminal destruction can complete inside the runtime callback. Its
  // cleanup invalidated this allocation and callback owner; never touch the
  // returned address or issue an Unlock against that retired owner.
  if (m_terminal || allocation.m_owner != this || allocation.m_handle != original)
    return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(locked)) { allocation.m_pendingLock.reset(); return locked; }
  if (!m_callbacks.pfnPresentCb) allocation.m_locked = true;
  // Any callback-reported success acquired a lock, even when its status is
  // outside the documented exact-S_OK contract. Balance before rejecting it.
  hr = locked == S_OK ? checkIdentity() : E_FAIL;
  if (m_terminal || allocation.m_owner != this || allocation.m_handle != original)
    return DXGI_ERROR_DEVICE_REMOVED;
  if (SUCCEEDED(hr) && (!lock.pData || lock.hAllocation != handle)) hr = E_FAIL;
  if (SUCCEEDED(hr)) {
    for (UINT y = 0; y < info.height; y++) {
      auto shared = static_cast<char*>(lock.pData) + size_t(y) * info.pitch;
      auto local = static_cast<char*>(pixels) + size_t(y) * rowPitch;
      std::memcpy(publish ? shared : local, publish ? local : shared, size_t(row));
    }
  }
  HRESULT unlocked;
  if (pending) unlocked = unlockPrimary(allocation);
  else {
    D3DDDICB_UNLOCK unlock = {}; unlock.NumAllocations = 1; unlock.phAllocations = &lock.hAllocation;
    allocation.m_locked = false;
    unlocked = completed(m_callbacks.pfnUnlockCb(m_device, &unlock));
  }
  if (m_terminal || allocation.m_owner != this || allocation.m_handle != original)
    return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(unlocked)) hr = unlocked;
  if (SUCCEEDED(hr)) hr = checkIdentity();
  if (publish) allocation.m_published = hr == S_OK;
  return hr;
}

HRESULT RuntimeMemory::uploadImpl(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch) {
  if (!allocation.primary()) return transferImpl(allocation, const_cast<void*>(pixels), rowPitch, true);
  if (m_primaryActive) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_primaryActive = true;
  struct CopyScope { bool& flag; ~CopyScope() { flag = false; } } scope{m_primaryActive};
  HRESULT hr = transferImpl(allocation, const_cast<void*>(pixels), rowPitch, true);
  allocation.m_published = false;
  if (hr == S_OK) hr = copyPrimary(allocation, true);
  if (hr == S_OK) hr = waitPrimary(allocation);
  if (!m_terminal && allocation.m_owner == this) allocation.m_published = hr == S_OK;
  return hr;
}

HRESULT RuntimeMemory::downloadImpl(RuntimeAllocation& allocation, void* pixels, UINT rowPitch) {
  if (!allocation.primary()) return transferImpl(allocation, pixels, rowPitch, false);
  if (m_primaryActive) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_primaryActive = true;
  struct CopyScope { bool& flag; ~CopyScope() { flag = false; } } scope{m_primaryActive};
  HRESULT hr = unlockPrimary(allocation);
  if (hr == S_OK) hr = copyPrimary(allocation, false);
  return hr == S_OK ? transferImpl(allocation, pixels, rowPitch, false) : hr;
}

HRESULT RuntimeMemory::copyPrimary(RuntimeAllocation& allocation, bool publish) {
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  if (allocation.m_owner != this || !allocation.primary()) return E_INVALIDARG;
  const auto primary = allocation.m_handle, staging = allocation.m_stagingHandle;
  auto live = [&] { return !m_terminal && allocation.m_owner == this
    && allocation.m_handle == primary && allocation.m_stagingHandle == staging; };
  HRESULT hr = checkIdentity();
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return hr;
  hr = ensureContext();
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return hr;
  hr = checkIdentity();
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return hr;
  const auto buffers = m_contextBuffers;
  if (!buffers.pCommandBuffer || buffers.CommandBufferSize < sizeof(PrimaryCopy)
      || !buffers.pAllocationList || buffers.AllocationListSize < 2
      || !buffers.pPatchLocationList || buffers.PatchLocationListSize < 2)
    return E_FAIL;
  PrimaryCopy copy; copy.width = allocation.m_info.width; copy.height = allocation.m_info.height;
  std::memcpy(buffers.pCommandBuffer, &copy, sizeof(copy));
  D3DDDI_ALLOCATIONLIST entries[2] = {};
  entries[0].hAllocation = publish ? staging : primary;
  entries[1].hAllocation = publish ? primary : staging; entries[1].WriteOperation = 1;
  std::memcpy(buffers.pAllocationList, entries, sizeof(entries));
  D3DDDICB_RENDER request = {};
  request.hContext = m_context; request.CommandLength = sizeof(copy); request.NumAllocations = 2;
  request.NewCommandBufferSize = buffers.CommandBufferSize;
  request.NewAllocationListSize = buffers.AllocationListSize;
  request.NewPatchLocationListSize = buffers.PatchLocationListSize;
  hr = m_callbacks.pfnRenderCb(m_device, &request);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  // A submission consumed the old runtime buffers. Never reuse them when
  // the callback fails or supplies malformed replacements.
  m_contextBuffers.pCommandBuffer = nullptr; m_contextBuffers.CommandBufferSize = 0;
  m_contextBuffers.pAllocationList = nullptr; m_contextBuffers.AllocationListSize = 0;
  m_contextBuffers.pPatchLocationList = nullptr; m_contextBuffers.PatchLocationListSize = 0;
  if (hr != S_OK) return completed(hr);
  if (request.hContext != m_context || request.CommandLength != sizeof(copy)
      || request.CommandOffset || request.NumAllocations != 2 || request.NumPatchLocations
      || request.Flags.Value || !request.pNewCommandBuffer
      || request.NewCommandBufferSize < sizeof(copy) || !request.pNewAllocationList
      || request.NewAllocationListSize < 2 || !request.pNewPatchLocationList
      || request.NewPatchLocationListSize < 2) return E_FAIL;
  m_contextBuffers.pCommandBuffer = request.pNewCommandBuffer;
  m_contextBuffers.CommandBufferSize = request.NewCommandBufferSize;
  m_contextBuffers.pAllocationList = request.pNewAllocationList;
  m_contextBuffers.AllocationListSize = request.NewAllocationListSize;
  m_contextBuffers.pPatchLocationList = request.pNewPatchLocationList;
  m_contextBuffers.PatchLocationListSize = request.NewPatchLocationListSize;
  hr = checkIdentity();
  return live() ? hr : DXGI_ERROR_DEVICE_REMOVED;
}

HRESULT RuntimeMemory::waitPrimary(RuntimeAllocation& allocation) {
  if (m_terminal || allocation.m_owner != this) return DXGI_ERROR_DEVICE_REMOVED;
  const HRESULT previous = unlockPrimary(allocation);
  if (previous != S_OK) return previous;
  const auto primary = allocation.m_handle, staging = allocation.m_stagingHandle;
  std::shared_ptr<PrimaryLockTransaction> pending;
  try { pending = std::make_shared<PrimaryLockTransaction>(); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  auto& request = pending->request;
  pending->handle = staging; request.hAllocation = staging; request.Flags.LockEntire = 1;
  allocation.m_pendingLock = pending;
  // A synchronized read/write staging lock waits for the scheduled source
  // read. It is a completion barrier, never a CPU map of the primary.
  const HRESULT locked = m_callbacks.pfnLockCb(m_device, &request);
  pending->inCallback = false;
  if (!pending->consumed) pending->acquired = SUCCEEDED(locked);
  auto live = [&] { return !m_terminal && allocation.m_owner == this
    && allocation.m_handle == primary && allocation.m_stagingHandle == staging; };
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(locked)) { allocation.m_pendingLock.reset(); return locked; }
  allocation.m_locked = true;
  HRESULT hr = locked == S_OK && request.hAllocation == staging ? checkIdentity() : E_FAIL;
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  const HRESULT unlocked = unlockPrimary(allocation);
  if (!live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(unlocked)) hr = unlocked;
  if (hr == S_OK) hr = checkIdentity();
  return live() ? hr : DXGI_ERROR_DEVICE_REMOVED;
}

HRESULT RuntimeMemory::unlockPrimary(RuntimeAllocation& allocation) {
  if (m_terminal || allocation.m_owner != this) return DXGI_ERROR_DEVICE_REMOVED;
  auto pending = allocation.m_pendingLock;
  if (!pending) return allocation.m_locked ? E_FAIL : S_OK;
  if (pending->consumed) { allocation.m_pendingLock.reset(); return S_OK; }
  if (pending->unlocking) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (!pending->ownsLock()) return DXGI_ERROR_WAS_STILL_DRAWING;
  const auto staging = allocation.m_stagingHandle;
  if (!staging || pending->handle != staging) return E_FAIL;
  // The record follows a moved owner during this callback. An attempted
  // unlock is not a successful unlock: retain it on failure. While in flight,
  // nested successful terminal cleanup must not unlock the same entry twice.
  allocation.m_locked = false; pending->unlocking = true;
  D3DDDICB_UNLOCK request = {}; request.NumAllocations = 1; request.phAllocations = &staging;
  const HRESULT hr = m_callbacks.pfnUnlockCb(m_device, &request);
  pending->unlocking = false;
  if (!pending->consumed && SUCCEEDED(hr)) {
    pending->consumed = true; pending->acquired = false;
  }
  if (m_terminal || allocation.m_owner != this || allocation.m_stagingHandle != staging)
    return DXGI_ERROR_DEVICE_REMOVED;
  if (pending->consumed && allocation.m_pendingLock == pending) allocation.m_pendingLock.reset();
  return completed(hr);
}

HRESULT RuntimeMemory::queryResidencyImpl(const D3DKMT_HANDLE* allocations, UINT count,
    D3DDDI_RESIDENCYSTATUS* output, const std::function<bool()>& live) {
  if (!m_device || !m_callbacks.pfnQueryResidencyCb) return DXGI_DDI_ERR_UNSUPPORTED;
  if (m_policyActive) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_policyActive = true;
  struct PolicyScope { bool& active; ~PolicyScope() { active = false; } } scope{m_policyActive};
  return residencyTransaction(allocations, count, output,
    [&](const D3DKMT_HANDLE* handles, D3DDDI_RESIDENCYSTATUS* staged, UINT size) {
      HRESULT hr = checkIdentity();
      if (hr != S_OK) return hr;
      if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
      D3DDDICB_QUERYRESIDENCY request = {};
      request.NumAllocations = size; request.HandleList = handles; request.pResidencyStatus = staged;
      hr = m_callbacks.pfnQueryResidencyCb(m_device, &request);
      if (hr != S_OK) return hr;
      if (request.hResource || request.NumAllocations != size || request.HandleList != handles
          || request.pResidencyStatus != staged) return E_FAIL;
      return checkIdentity();
    }, [&] { return !m_removed && (!live || live()); },
    [](const D3DDDI_RESIDENCYSTATUS& value) {
      // Validate the raw ABI word before loading it as an enum. A malformed
      // callback can write a value outside this C++ enum's representable range.
      static_assert(sizeof(value) == sizeof(UINT));
      UINT word;
      std::memcpy(&word, &value, sizeof(word));
      return word == D3DDDI_RESIDENCYSTATUS_RESIDENTINGPUMEMORY
        || word == D3DDDI_RESIDENCYSTATUS_RESIDENTINSHAREDMEMORY
        || word == D3DDDI_RESIDENCYSTATUS_NOTRESIDENT;
    }, HRESULT(E_INVALIDARG), HRESULT(DXGI_DDI_ERR_UNSUPPORTED), HRESULT(E_FAIL), HRESULT(DXGI_ERROR_DEVICE_REMOVED));
}

HRESULT RuntimeMemory::setPriorityImpl(D3DKMT_HANDLE allocation, UINT priority,
    const std::function<bool()>& live) {
  if (!allocation) return E_INVALIDARG;
  if (!m_device || !m_callbacks.pfnSetPriorityCb) return DXGI_DDI_ERR_UNSUPPORTED;
  if (m_policyActive) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_policyActive = true;
  struct PolicyScope { bool& active; ~PolicyScope() { active = false; } } scope{m_policyActive};
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  HRESULT hr = checkIdentity();
  if (hr != S_OK) return hr;
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  D3DDDICB_SETPRIORITY request = {};
  request.NumAllocations = 1; request.HandleList = &allocation; request.pPriorities = &priority;
  hr = m_callbacks.pfnSetPriorityCb(m_device, &request);
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return completed(hr);
  if (request.hResource || request.NumAllocations != 1 || request.HandleList != &allocation
      || request.pPriorities != &priority) return E_FAIL;
  return checkIdentity();
}

HRESULT RuntimeMemory::ensureContext() {
  if (m_context) return S_OK;
  if (m_creatingContext) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_creatingContext = true;
  struct ContextScope { bool& flag; ~ContextScope() { flag = false; } } scope{m_creatingContext};
  m_contextBuffers = {};
  auto& request = m_contextBuffers;
  request.EngineAffinity = 1;
  const HRESULT created = m_callbacks.pfnCreateContextCb(m_device, &request);
  // closeImpl also examines this owned request while CreateContextCb is in
  // progress. A nested terminal call can destroy an already returned context
  // before its original runtime owner disappears.
  if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(created)) return created;
  if (created != S_OK || !request.hContext || request.NodeOrdinal || request.EngineAffinity != 1
      || request.Flags.Value || request.pPrivateDriverData || request.PrivateDriverDataSize) {
    if (request.hContext) {
      D3DDDICB_DESTROYCONTEXT cleanup = {};
      cleanup.hContext = request.hContext;
      m_contextBuffers = {};
      const HRESULT released = completed(m_callbacks.pfnDestroyContextCb(m_device, &cleanup));
      if (m_terminal) return DXGI_ERROR_DEVICE_REMOVED;
      if (FAILED(released)) return released;
    }
    return E_FAIL;
  }
  m_context = request.hContext;
  return S_OK;
}

HRESULT RuntimeMemory::presentImpl(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args,
    const std::function<bool()>& live) {
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (!available()) return DXGI_ERROR_UNSUPPORTED;
  if (m_primaryActive) return DXGI_ERROR_WAS_STILL_DRAWING;
  // Initial windowed blit path only. Primary/flip, opened shared resources,
  // stereo and explicit destinations require their own ownership support.
  if (source.m_owner != this || !source.m_handle || !source.m_published
      || args.SrcSubResourceIndex || args.DstSubResourceIndex
      || args.hDstResource || !args.pDXGIContext || args.Flags.Value != 1)
    return E_INVALIDARG;
  HRESULT hr = checkIdentity();
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  hr = ensureContext();
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  hr = checkIdentity();
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  DXGIDDICB_PRESENT request = {};
  request.hSrcAllocation = source.m_handle;
  request.pDXGIContext = args.pDXGIContext;
  request.hContext = m_context;
  // The opaque DXGI context carries the runtime's destination and timing.
  // Do not substitute a global scanout escape or override its sync interval.
  const auto present = m_dxgi ? m_dxgi->pfnPresentCb : nullptr;
  if (!present) return DXGI_ERROR_UNSUPPORTED;
  hr = completed(present(m_device, &request));
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  hr = checkIdentity();
  return live && !live() ? DXGI_ERROR_DEVICE_REMOVED : hr;
}

HRESULT RuntimeMemory::present9Impl(RuntimeAllocation& source, const D3DDDIARG_PRESENT& args,
    const std::function<bool()>& live) {
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (!available9()) return DXGI_ERROR_UNSUPPORTED;
  // Initial windowed source blits only. Destination index and flip interval
  // are reserved without a destination or flip; do not validate those fields.
  if (source.m_owner != this || !source.m_handle || !source.m_published
      || !args.hSrcResource || args.SrcSubResourceIndex || args.hDstResource
      || args.Flags.Value != 1) return E_INVALIDARG;
  HRESULT hr = checkIdentity();
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  // The synchronized linear allocation is a standard CPU-visible source.
  // KMD presents it on a separately owned GDI context, not the Vulkan context
  // whose native allocation identities and command stream are different.
  hr = ensureContext();
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  hr = checkIdentity();
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  D3DDDICB_PRESENT request = {};
  request.hSrcAllocation = source.m_handle;
  request.hContext = m_context;
  const auto present = m_callbacks.pfnPresentCb;
  hr = completed(present(m_device, &request));
  if (live && !live()) return DXGI_ERROR_DEVICE_REMOVED;
  if (FAILED(hr)) return hr;
  hr = checkIdentity();
  return live && !live() ? DXGI_ERROR_DEVICE_REMOVED : hr;
}

HRESULT RuntimeMemory::closeImpl() {
  const auto context = m_context ? m_context : m_creatingContext ? m_contextBuffers.hContext : nullptr;
  if (!context) return S_OK;
  D3DDDICB_DESTROYCONTEXT request = {};
  request.hContext = context;
  const auto destroy = m_callbacks.pfnDestroyContextCb;
  const HANDLE device = m_device;
  m_context = nullptr;
  m_contextBuffers = {};
  return completed(destroy(device, &request));
}

HRESULT RuntimeMemory::setDisplayModeImpl(RuntimeAllocation& allocation,
    const std::function<bool()>& live) {
  auto valid = [&] { return !m_terminal && allocation.m_owner == this
    && (!live || live()); };
  if (!valid()) return DXGI_ERROR_DEVICE_REMOVED;
  if (!primaryAvailable()) return DXGI_ERROR_UNSUPPORTED;
  if (!allocation.primary() || allocation.m_opened || !allocation.m_published) return E_INVALIDARG;
  if (m_primaryActive) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_primaryActive = true;
  struct ModeScope { bool& flag; ~ModeScope() { flag = false; } } scope{m_primaryActive};
  const auto primary = allocation.m_handle;
  HRESULT hr = checkIdentity();
  if (!valid() || allocation.m_handle != primary) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return hr;
  D3DDDICB_SETDISPLAYMODE request = {}; request.hPrimaryAllocation = primary;
  hr = m_callbacks.pfnSetDisplayModeCb(m_device, &request);
  if (!valid() || allocation.m_handle != primary) return DXGI_ERROR_DEVICE_REMOVED;
  // INCOMPATIBLEPRIVATEFORMAT requires an actual conversion before retrying.
  // v0 uses one exact format, so preserve the real failure and do not fake a
  // successful mode switch or reuse the returned attribute without conversion.
  if (hr != S_OK) return completed(hr);
  if (request.hPrimaryAllocation != primary) return E_FAIL;
  hr = checkIdentity();
  return valid() && allocation.m_handle == primary ? hr : DXGI_ERROR_DEVICE_REMOVED;
}

HRESULT RuntimeMemory::closeDeviceAllocationsImpl() {
  if (m_terminal) return S_OK;
  // Typed9 preserves ownership after a failed DeallocateCb so its synchronous
  // DestroyResource can retry. This terminal modern path must not change it.
  if (m_callbacks.pfnPresentCb) return DXGI_ERROR_UNSUPPORTED;
  m_terminal = true;
  HRESULT result = S_OK;
  while (m_allocations) {
    // releaseImpl detaches and zeros before entering runtime code. A nested
    // callback can release or move any remaining node; reload the actual head.
    const HRESULT hr = releaseImpl(*m_allocations);
    if (FAILED(hr) && SUCCEEDED(result)) result = hr;
  }
  while (m_orphans) {
    auto retired = m_orphans; m_orphans = retired->m_orphanNext;
    delete retired; // Terminal release already detached its callback owner.
  }
  const HRESULT context = closeImpl();
  if (FAILED(context)) result = context;
  m_device = nullptr; m_dxgi = nullptr; m_callbacks = {};
  return result;
}
}
