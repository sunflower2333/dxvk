#include "umd_allocation.h"
#include <cstring>
#include <limits>
#include <utility>

namespace dxvk::umd {
namespace {
HRESULT completed(HRESULT hr) { return hr == S_OK || FAILED(hr) ? hr : E_FAIL; }
}

RuntimeAllocation::~RuntimeAllocation() { release(); }
RuntimeAllocation::RuntimeAllocation(RuntimeAllocation&& other) noexcept
: m_owner(std::exchange(other.m_owner, nullptr)),
  m_resource(std::exchange(other.m_resource, nullptr)),
  m_handle(std::exchange(other.m_handle, 0)),
  m_kernelResource(std::exchange(other.m_kernelResource, 0)),
  m_generation(std::exchange(other.m_generation, 0)),
  m_info(other.m_info), m_published(std::exchange(other.m_published, false)),
  m_opened(std::exchange(other.m_opened, false)) {}
HRESULT RuntimeAllocation::release() { return m_owner ? m_owner->release(*this) : S_OK; }
RuntimeMemory::~RuntimeMemory() { close(); }

void RuntimeMemory::initialize(HANDLE device, const D3DDDI_DEVICECALLBACKS& kernel,
    const DXGI_DDI_BASE_CALLBACKS* dxgi, std::shared_ptr<const AdapterIdentity> identity,
    std::shared_ptr<RuntimeService> service) {
  m_device = device;
  m_callbacks.pfnAllocateCb = kernel.pfnAllocateCb;
  m_callbacks.pfnDeallocateCb = kernel.pfnDeallocateCb;
  m_callbacks.pfnLockCb = kernel.pfnLockCb;
  m_callbacks.pfnUnlockCb = kernel.pfnUnlockCb;
  m_callbacks.pfnCreateContextCb = kernel.pfnCreateContextCb;
  m_callbacks.pfnDestroyContextCb = kernel.pfnDestroyContextCb;
  m_dxgi.pfnPresentCb = dxgi ? dxgi->pfnPresentCb : nullptr;
  m_identity = std::move(identity);
  m_service = std::move(service);
  m_removed = false;
}

HRESULT RuntimeMemory::checkIdentity() {
  // The older standalone allocation helper has no runtime adapter binding.
  // The actual native entry always supplies its immutable original identity.
  if (!m_identity) return S_OK;
  if (m_removed) return DXGI_ERROR_DEVICE_REMOVED;
  if (m_querying) return DXGI_ERROR_WAS_STILL_DRAWING;
  m_querying = true;
  struct QueryScope { bool& querying; ~QueryScope() { querying = false; } } scope{m_querying};
  RuntimeIdentity current;
  HRESULT hr = queryRuntimeIdentity(m_identity->runtime, m_identity->query, current);
  if (hr == S_OK && (std::memcmp(current.luid.data(), &m_identity->luid, sizeof(LUID))
      || current.generation != m_identity->generation
      || current.capabilities != m_identity->capabilities))
    hr = DXGI_ERROR_DEVICE_REMOVED;
  if (hr == DXGI_ERROR_DEVICE_REMOVED) m_removed = true;
  return hr;
}

bool RuntimeMemory::available() const {
  return m_device && m_callbacks.pfnAllocateCb && m_callbacks.pfnDeallocateCb
      && m_callbacks.pfnLockCb && m_callbacks.pfnUnlockCb
      && m_callbacks.pfnCreateContextCb && m_callbacks.pfnDestroyContextCb
      && m_dxgi.pfnPresentCb;
}

HRESULT RuntimeMemory::allocateImpl(RuntimeAllocation& out, HANDLE resource,
    UINT width, UINT height, DXGI_FORMAT format) {
  if (!available()) return DXGI_ERROR_UNSUPPORTED;
  if (out.m_owner || !resource || !width || !height || width > 16384 || height > 16384)
    return E_INVALIDARG;
  AllocationInfo info;
  if (format == DXGI_FORMAT_B8G8R8A8_UNORM) info.format = 1;
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
  return S_OK;
}

HRESULT RuntimeMemory::releaseImpl(RuntimeAllocation& allocation) {
  if (allocation.m_owner != this) return E_INVALIDARG;
  if (allocation.m_opened) {
    // A view, not an owner. Dropping it must not reach DeallocateCb: the
    // allocation outlives this resource and the creating process still uses
    // it. Retire the local state only.
    allocation.m_owner = nullptr; allocation.m_handle = 0;
    allocation.m_kernelResource = 0; allocation.m_generation = 0;
    allocation.m_published = false; allocation.m_opened = false;
    return S_OK;
  }
  D3DDDICB_DEALLOCATE request = {};
  request.hResource = allocation.m_resource;
  const auto deallocate = m_callbacks.pfnDeallocateCb;
  const HANDLE device = m_device;
  // Retire all user ownership before entering runtime code. A recursive
  // Release/DestroyResource must neither deallocate twice nor touch old state.
  allocation.m_owner = nullptr; allocation.m_resource = nullptr;
  allocation.m_handle = 0; allocation.m_published = false;
  allocation.m_kernelResource = 0; allocation.m_generation = 0;
  return completed(deallocate(device, &request));
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
      || (info.format != 1 && info.format != 3)
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
  return S_OK;
}

// One body for both directions of the shared-surface copy. Publishing writes
// this device's cache into the kernel allocation; refreshing reads whatever
// the owning process last left there. The lock flags, the synchronization
// they imply and the balancing unlock are identical either way, and keeping
// them in one place is what stops the two directions drifting apart.
HRESULT RuntimeMemory::transferImpl(RuntimeAllocation& allocation, void* pixels,
    UINT rowPitch, bool publish) {
  if (allocation.m_owner != this || !allocation.m_handle) return E_INVALIDARG;
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
  D3DDDICB_LOCK lock = {};
  lock.hAllocation = allocation.m_handle;
  lock.Flags.LockEntire = 1;
  lock.Flags.WriteOnly = publish ? 1 : 0;
  lock.Flags.ReadOnly = publish ? 0 : 1;
  // Do not discard or ignore synchronization: VidSch may still consume the
  // last Present. A successful synchronized lock precedes every CPU access.
  const HRESULT locked = m_callbacks.pfnLockCb(m_device, &lock);
  if (FAILED(locked)) return locked;
  // Any callback-reported success acquired a lock, even when its status is
  // outside the documented exact-S_OK contract. Balance before rejecting it.
  hr = locked == S_OK ? checkIdentity() : E_FAIL;
  if (SUCCEEDED(hr) && (!lock.pData || lock.hAllocation != allocation.m_handle)) hr = E_FAIL;
  if (SUCCEEDED(hr)) {
    for (UINT y = 0; y < info.height; y++) {
      auto shared = static_cast<char*>(lock.pData) + size_t(y) * info.pitch;
      auto local = static_cast<char*>(pixels) + size_t(y) * rowPitch;
      std::memcpy(publish ? shared : local, publish ? local : shared, size_t(row));
    }
  }
  D3DDDICB_UNLOCK unlock = {};
  unlock.NumAllocations = 1;
  unlock.phAllocations = &lock.hAllocation;
  const HRESULT unlocked = completed(m_callbacks.pfnUnlockCb(m_device, &unlock));
  if (FAILED(unlocked)) hr = unlocked;
  if (SUCCEEDED(hr)) hr = checkIdentity();
  if (publish) allocation.m_published = hr == S_OK;
  return hr;
}

HRESULT RuntimeMemory::uploadImpl(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch) {
  return transferImpl(allocation, const_cast<void*>(pixels), rowPitch, true);
}

HRESULT RuntimeMemory::downloadImpl(RuntimeAllocation& allocation, void* pixels, UINT rowPitch) {
  return transferImpl(allocation, pixels, rowPitch, false);
}

HRESULT RuntimeMemory::ensureContext() {
  if (m_context) return S_OK;
  D3DDDICB_CREATECONTEXT request = {};
  request.EngineAffinity = 1;
  const HRESULT created = m_callbacks.pfnCreateContextCb(m_device, &request);
  if (FAILED(created)) return created;
  if (created != S_OK || !request.hContext) {
    if (request.hContext) {
      D3DDDICB_DESTROYCONTEXT cleanup = {};
      cleanup.hContext = request.hContext;
      const HRESULT released = completed(m_callbacks.pfnDestroyContextCb(m_device, &cleanup));
      if (FAILED(released)) return released;
    }
    return E_FAIL;
  }
  m_context = request.hContext;
  return S_OK;
}

HRESULT RuntimeMemory::presentImpl(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args) {
  if (!available()) return DXGI_ERROR_UNSUPPORTED;
  // Initial windowed blit path only. Primary/flip, opened shared resources,
  // stereo and explicit destinations require their own ownership support.
  if (source.m_owner != this || !source.m_handle || !source.m_published
      || args.SrcSubResourceIndex || args.DstSubResourceIndex
      || args.hDstResource || !args.pDXGIContext || args.Flags.Value != 1)
    return E_INVALIDARG;
  HRESULT hr = checkIdentity();
  if (FAILED(hr)) return hr;
  hr = ensureContext();
  if (FAILED(hr)) return hr;
  hr = checkIdentity();
  if (FAILED(hr)) return hr;
  DXGIDDICB_PRESENT request = {};
  request.hSrcAllocation = source.m_handle;
  request.pDXGIContext = args.pDXGIContext;
  request.hContext = m_context;
  // The opaque DXGI context carries the runtime's destination and timing.
  // Do not substitute a global scanout escape or override its sync interval.
  hr = completed(m_dxgi.pfnPresentCb(m_device, &request));
  return FAILED(hr) ? hr : checkIdentity();
}

HRESULT RuntimeMemory::closeImpl() {
  if (!m_context) return S_OK;
  D3DDDICB_DESTROYCONTEXT request = {};
  request.hContext = m_context;
  const auto destroy = m_callbacks.pfnDestroyContextCb;
  const HANDLE device = m_device;
  m_context = nullptr;
  return completed(destroy(device, &request));
}
}
