#include "umd_allocation.h"
#include <cstring>
#include <limits>

namespace dxvk::umd {
namespace {
HRESULT completed(HRESULT hr) { return hr == S_OK || FAILED(hr) ? hr : E_FAIL; }
}

RuntimeAllocation::~RuntimeAllocation() { release(); }
HRESULT RuntimeAllocation::release() { return m_owner ? m_owner->release(*this) : S_OK; }
RuntimeMemory::~RuntimeMemory() { close(); }

void RuntimeMemory::initialize(HANDLE device, const D3DDDI_DEVICECALLBACKS& kernel,
    const DXGI_DDI_BASE_CALLBACKS* dxgi) {
  m_device = device;
  m_callbacks.pfnAllocateCb = kernel.pfnAllocateCb;
  m_callbacks.pfnDeallocateCb = kernel.pfnDeallocateCb;
  m_callbacks.pfnLockCb = kernel.pfnLockCb;
  m_callbacks.pfnUnlockCb = kernel.pfnUnlockCb;
  m_callbacks.pfnCreateContextCb = kernel.pfnCreateContextCb;
  m_callbacks.pfnDestroyContextCb = kernel.pfnDestroyContextCb;
  m_dxgi.pfnPresentCb = dxgi ? dxgi->pfnPresentCb : nullptr;
}

bool RuntimeMemory::available() const {
  return m_device && m_callbacks.pfnAllocateCb && m_callbacks.pfnDeallocateCb
      && m_callbacks.pfnLockCb && m_callbacks.pfnUnlockCb
      && m_callbacks.pfnCreateContextCb && m_callbacks.pfnDestroyContextCb
      && m_dxgi.pfnPresentCb;
}

HRESULT RuntimeMemory::allocate(RuntimeAllocation& out, HANDLE resource,
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
  auto privateData = info;
  D3DDDI_ALLOCATIONINFO allocation = {};
  allocation.pPrivateDriverData = &privateData;
  allocation.PrivateDriverDataSize = sizeof(privateData);
  D3DDDICB_ALLOCATE request = {};
  request.hResource = resource;
  request.NumAllocations = 1;
  request.pAllocationInfo = &allocation;
  HRESULT hr = completed(m_callbacks.pfnAllocateCb(m_device, &request));
  if (FAILED(hr)) return hr;
  if (!allocation.hAllocation || !request.hKMResource) {
    // The callback reported allocation success: balance the resource even
    // when its returned handles are incomplete. Never expose a zero handle.
    D3DDDICB_DEALLOCATE cleanup = {};
    cleanup.hResource = resource;
    m_callbacks.pfnDeallocateCb(m_device, &cleanup);
    return E_FAIL;
  }
  out.m_owner = this; out.m_resource = resource;
  out.m_handle = allocation.hAllocation; out.m_info = info;
  out.m_published = false;
  return S_OK;
}

HRESULT RuntimeMemory::release(RuntimeAllocation& allocation) {
  if (allocation.m_owner != this) return E_INVALIDARG;
  D3DDDICB_DEALLOCATE request = {};
  request.hResource = allocation.m_resource;
  HRESULT hr = completed(m_callbacks.pfnDeallocateCb(m_device, &request));
  allocation.m_owner = nullptr; allocation.m_resource = nullptr;
  allocation.m_handle = 0; allocation.m_published = false;
  return hr;
}

HRESULT RuntimeMemory::upload(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch) {
  if (allocation.m_owner != this || !allocation.m_handle) return E_INVALIDARG;
  allocation.m_published = false;
  const auto& info = allocation.m_info;
  if (!pixels || rowPitch < info.pitch
      || uint64_t(info.height - 1) * rowPitch + info.pitch > std::numeric_limits<size_t>::max())
    return E_INVALIDARG;
  D3DDDICB_LOCK lock = {};
  lock.hAllocation = allocation.m_handle;
  lock.Flags.LockEntire = 1;
  lock.Flags.WriteOnly = 1;
  // Do not discard or ignore synchronization: VidSch may still consume the
  // last Present. A successful synchronized lock precedes every CPU write.
  HRESULT hr = completed(m_callbacks.pfnLockCb(m_device, &lock));
  if (FAILED(hr)) return hr;
  if (!lock.pData || lock.hAllocation != allocation.m_handle) hr = E_FAIL;
  else {
    for (UINT y = 0; y < info.height; y++) {
      std::memcpy(static_cast<char*>(lock.pData) + size_t(y) * info.pitch,
        static_cast<const char*>(pixels) + size_t(y) * rowPitch, info.pitch);
    }
  }
  D3DDDICB_UNLOCK unlock = {};
  unlock.NumAllocations = 1;
  unlock.phAllocations = &lock.hAllocation;
  const HRESULT unlocked = completed(m_callbacks.pfnUnlockCb(m_device, &unlock));
  if (FAILED(unlocked)) hr = unlocked;
  allocation.m_published = hr == S_OK;
  return hr;
}

HRESULT RuntimeMemory::ensureContext() {
  if (m_context) return S_OK;
  D3DDDICB_CREATECONTEXT request = {};
  request.EngineAffinity = 1;
  HRESULT hr = completed(m_callbacks.pfnCreateContextCb(m_device, &request));
  if (FAILED(hr)) return hr;
  if (!request.hContext) return E_FAIL;
  m_context = request.hContext;
  return S_OK;
}

HRESULT RuntimeMemory::present(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args) {
  if (!available()) return DXGI_ERROR_UNSUPPORTED;
  // Initial windowed blit path only. Primary/flip, opened shared resources,
  // stereo and explicit destinations require their own ownership support.
  if (source.m_owner != this || !source.m_handle || !source.m_published
      || args.SrcSubResourceIndex || args.DstSubResourceIndex
      || args.hDstResource || !args.pDXGIContext || args.Flags.Value != 1)
    return E_INVALIDARG;
  HRESULT hr = ensureContext();
  if (FAILED(hr)) return hr;
  DXGIDDICB_PRESENT request = {};
  request.hSrcAllocation = source.m_handle;
  request.pDXGIContext = args.pDXGIContext;
  request.hContext = m_context;
  // The opaque DXGI context carries the runtime's destination and timing.
  // Do not substitute a global scanout escape or override its sync interval.
  return completed(m_dxgi.pfnPresentCb(m_device, &request));
}

HRESULT RuntimeMemory::close() {
  if (!m_context) return S_OK;
  D3DDDICB_DESTROYCONTEXT request = {};
  request.hContext = m_context;
  HRESULT hr = completed(m_callbacks.pfnDestroyContextCb(m_device, &request));
  m_context = nullptr;
  return hr;
}
}
