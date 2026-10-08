#pragma once

#include "umd_ddi.h"
#include "umd_adapter.h"
#include "umd_runtime_service.h"
#include <cstddef>
#include <cstdint>
#include <functional>

namespace dxvk::umd {

// Exact v0 VIOGPU_WDDM_ALLOCATION_INFO wire layout. This is kernel allocation
// metadata, not DXVK's Wine shared-image escape or Vulkan external memory.
#pragma pack(push, 4)
struct AllocationInfo {
  uint32_t magic = 0x504d5644, version = 0, headerSize = 80, reserved = 0;
  uint64_t size = 0, alignment = 4096, requestedIova = 0, resetGeneration = 0;
  uint32_t flags = 2, format = 0, width = 0, height = 0, pitch = 0;
  uint32_t refreshNumerator = 0, refreshDenominator = 0, contextId = 0;
};
#pragma pack(pop)
static_assert(sizeof(AllocationInfo) == 80);
static_assert(offsetof(AllocationInfo, flags) == 48);
static_assert(offsetof(AllocationInfo, pitch) == 64);

// The wire format codes AllocationInfo carries. The inverse of the mapping
// RuntimeMemory::allocateImpl applies; keep the two in step, because an
// allocation this bridge created must decode to the format it asked for when
// another process opens it.
inline DXGI_FORMAT allocationFormat(uint32_t format) {
  if (format == 1) return DXGI_FORMAT_B8G8R8A8_UNORM;
  if (format == 2) return DXGI_FORMAT_B8G8R8X8_UNORM;
  if (format == 3) return DXGI_FORMAT_R8G8B8A8_UNORM;
  return DXGI_FORMAT_UNKNOWN;
}

class RuntimeMemory;
struct PrimaryAllocationTransaction;
struct PrimaryLockTransaction;
struct PrimaryStagingReleaseTransaction;
class RuntimeAllocation {
public:
  RuntimeAllocation() = default;
  RuntimeAllocation(const RuntimeAllocation&) = delete;
  RuntimeAllocation& operator=(const RuntimeAllocation&) = delete;
  RuntimeAllocation(RuntimeAllocation&& other) noexcept;
  ~RuntimeAllocation();
  HRESULT release();
  D3DKMT_HANDLE handle() const { return m_handle; }
  D3DKMT_HANDLE kernelResource() const { return m_kernelResource; }
  HANDLE runtimeResource() const { return m_resource; }
  uint64_t generation() const { return m_generation; }
  const AllocationInfo& info() const { return m_info; }
  // An adopted primary belongs to its creating process. An opened primary may
  // also own an independent internal staging allocation; only that allocation
  // can be passed to this device's DeallocateCb.
  bool opened() const { return m_opened; }
  bool primary() const { return m_info.flags == 1 && m_stagingHandle != 0; }
  // DXGI rotates kernel identities, not runtime resource handles. The latter
  // remain the keys DeallocateCb uses after the runtime performs its rotation.
  // Preflight the entire chain before making any mutation or callback.
  bool canRotateWith(const RuntimeAllocation& other) const noexcept;
  void swapIdentity(RuntimeAllocation& other) noexcept;
private:
  friend class RuntimeMemory;
  RuntimeMemory* m_owner = nullptr;
  HANDLE m_resource = nullptr;
  D3DKMT_HANDLE m_handle = 0;
  D3DKMT_HANDLE m_kernelResource = 0;
  // A created primary pair belongs to m_resource. For an opened primary this
  // is a separately owned device allocation and m_handle remains borrowed.
  D3DKMT_HANDLE m_stagingHandle = 0;
  uint64_t m_generation = 0;
  AllocationInfo m_info;
  bool m_published = false;
  bool m_opened = false;
  bool m_ownedStaging = false;
  RuntimeAllocation* m_previous = nullptr;
  RuntimeAllocation* m_next = nullptr;
  bool m_tracked = false;
  bool m_locked = false;
  bool m_releasing = false;
  std::shared_ptr<PrimaryAllocationTransaction> m_pendingAllocation;
  std::shared_ptr<PrimaryLockTransaction> m_pendingLock;
  std::shared_ptr<PrimaryStagingReleaseTransaction> m_stagingRelease;
  // Reserved before primary acquisition. A failed cleanup may outlive this
  // C++ owner; move it into stable ledger storage without allocating in Destroy.
  std::unique_ptr<RuntimeAllocation> m_cleanupOwner;
  RuntimeAllocation* m_orphanNext = nullptr;
};

// Only kernel callback fields used by the negotiated bridge are copied;
// copying a whole current-SDK table could read past an older runtime's table.
// The DXGI callback table is retained live, as required by its separate ABI.
class RuntimeMemory {
public:
  RuntimeMemory() = default;
  RuntimeMemory(const RuntimeMemory&) = delete;
  RuntimeMemory& operator=(const RuntimeMemory&) = delete;
  ~RuntimeMemory();
  void initialize(HANDLE device, const D3DDDI_DEVICECALLBACKS& kernel,
    const DXGI_DDI_BASE_CALLBACKS* dxgi,
    std::shared_ptr<const AdapterIdentity> identity = {},
    std::shared_ptr<RuntimeService> service = {});
  void initialize9(HANDLE device, const D3DDDI_DEVICECALLBACKS& kernel,
    std::shared_ptr<const AdapterIdentity> identity,
    std::shared_ptr<RuntimeService> service);
  bool available() const;
  bool available9() const { return m_callbacks.pfnPresentCb && available(); }
  HRESULT allocate(RuntimeAllocation& out, HANDLE resource, UINT width,
    UINT height, DXGI_FORMAT format) {
    return call([&] { return allocateImpl(out, resource, width, height, format); });
  }
  HRESULT allocatePrimary(RuntimeAllocation& out, HANDLE resource, UINT width,
    UINT height, DXGI_FORMAT format, UINT numerator, UINT denominator) {
    return call([&] { return allocatePrimaryImpl(out, resource, width, height, format,
      numerator, denominator); });
  }
  bool primaryAvailable() const {
    return !m_callbacks.pfnPresentCb && available() && m_callbacks.pfnRenderCb
      && m_callbacks.pfnSetDisplayModeCb;
  }
  HRESULT release(RuntimeAllocation& allocation) {
    return call([&] { return releaseImpl(allocation); });
  }
  HRESULT upload(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch) {
    return call([&] { return uploadImpl(allocation, pixels, rowPitch); });
  }
  // The mirror of upload, for a shared surface whose authoritative pixels live
  // in the kernel allocation because another process wrote them there.
  HRESULT download(RuntimeAllocation& allocation, void* pixels, UINT rowPitch) {
    return call([&] { return downloadImpl(allocation, pixels, rowPitch); });
  }
  // Take a view of an allocation another process created and this device only
  // opened. No AllocateCb ran, so no DeallocateCb may run either.
  HRESULT adopt(RuntimeAllocation& out, D3DKMT_HANDLE allocation,
    D3DKMT_HANDLE kernelResource, const AllocationInfo& info) {
    return call([&] { return adoptImpl(out, allocation, kernelResource, info); });
  }
  // Open the runtime's single standard-primary allocation, then acquire one
  // internal staging allocation using hResource=NULL. Runtime/kernel resource
  // cookies remain those of the borrowed primary, never the staging handle.
  HRESULT adoptPrimary(RuntimeAllocation& out, D3DKMT_HANDLE allocation,
    D3DKMT_HANDLE kernelResource, const AllocationInfo& info) {
    return call([&] { return adoptPrimaryImpl(out, allocation, kernelResource, info); });
  }
  // Raw allocation handles originate from pinned tracked resource owners;
  // runtime resource cookies and kernel resource handles are never interchanged.
  HRESULT queryResidency(const D3DKMT_HANDLE* allocations, UINT count,
    D3DDDI_RESIDENCYSTATUS* output, const std::function<bool()>& live = {}) {
    return call([&] { return queryResidencyImpl(allocations, count, output, live); });
  }
  HRESULT setPriority(D3DKMT_HANDLE allocation, UINT priority,
    const std::function<bool()>& live = {}) {
    return call([&] { return setPriorityImpl(allocation, priority, live); });
  }
  // The pinned allocation may outlive the resource whose runtime storage was
  // reclaimed by a callback. Revalidate that resource before submitting it.
  HRESULT present(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args,
    const std::function<bool()>& live = {}) {
    return call([&] { return presentImpl(source, args, live); });
  }
  HRESULT present9(RuntimeAllocation& source, const D3DDDIARG_PRESENT& args,
    const std::function<bool()>& live = {}) {
    return call([&] { return present9Impl(source, args, live); });
  }
  HRESULT setDisplayMode(RuntimeAllocation& primary, const std::function<bool()>& live = {}) {
    return call([&] { return setDisplayModeImpl(primary, live); });
  }
  HRESULT close() { return !m_context ? S_OK : call([&] { return closeImpl(); }); }
  // Terminal modern-device cleanup happens while DestroyDevice's original
  // callback owner is valid, including when surfaces remain pinned by an
  // interrupted operation. Ordinary context-only close and typed9 are separate.
  HRESULT closeDeviceAllocations() { return call([&] { return closeDeviceAllocationsImpl(); }); }
private:
  friend class RuntimeAllocation;
  void track(RuntimeAllocation& allocation) noexcept;
  void untrack(RuntimeAllocation& allocation) noexcept;
  void replace(RuntimeAllocation& previous, RuntimeAllocation& current) noexcept;
  HRESULT retainFailedPrimary(RuntimeAllocation& allocation);
  template<typename Function> HRESULT call(Function&& function) {
    return m_service ? m_service->invoke(std::forward<Function>(function)) : function();
  }
  HRESULT allocateImpl(RuntimeAllocation& out, HANDLE resource, UINT width,
    UINT height, DXGI_FORMAT format);
  HRESULT allocatePrimaryImpl(RuntimeAllocation&, HANDLE, UINT, UINT, DXGI_FORMAT, UINT, UINT);
  HRESULT adoptImpl(RuntimeAllocation& out, D3DKMT_HANDLE allocation,
    D3DKMT_HANDLE kernelResource, const AllocationInfo& info);
  HRESULT adoptPrimaryImpl(RuntimeAllocation&, D3DKMT_HANDLE, D3DKMT_HANDLE, const AllocationInfo&);
  HRESULT releaseStaging(RuntimeAllocation&);
  void detachOpenedPrimary(RuntimeAllocation&) noexcept;
  HRESULT releaseImpl(RuntimeAllocation& allocation);
  HRESULT uploadImpl(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch);
  HRESULT downloadImpl(RuntimeAllocation& allocation, void* pixels, UINT rowPitch);
  HRESULT transferImpl(RuntimeAllocation& allocation, void* pixels, UINT rowPitch,
    bool publish);
  HRESULT copyPrimary(RuntimeAllocation&, bool publish);
  HRESULT waitPrimary(RuntimeAllocation&);
  HRESULT unlockPrimary(RuntimeAllocation&);
  HRESULT queryResidencyImpl(const D3DKMT_HANDLE*, UINT, D3DDDI_RESIDENCYSTATUS*,
    const std::function<bool()>& live);
  HRESULT setPriorityImpl(D3DKMT_HANDLE, UINT, const std::function<bool()>& live);
  HRESULT presentImpl(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args,
    const std::function<bool()>& live);
  HRESULT present9Impl(RuntimeAllocation& source, const D3DDDIARG_PRESENT& args,
    const std::function<bool()>& live);
  HRESULT setDisplayModeImpl(RuntimeAllocation&, const std::function<bool()>& live);
  HRESULT closeImpl();
  HRESULT closeDeviceAllocationsImpl();
  HRESULT ensureContext();
  HRESULT checkIdentity();
  HANDLE m_device = nullptr;
  D3DDDI_DEVICECALLBACKS m_callbacks = {};
  // DXGI owns this table and may replace its callback addresses whenever no
  // thread is inside the UMD. Keep the table pointer, as required by the DXGI
  // DDI, instead of snapshotting pfnPresentCb at device creation.
  const DXGI_DDI_BASE_CALLBACKS* m_dxgi = nullptr;
  HANDLE m_context = nullptr;
  D3DDDICB_CREATECONTEXT m_contextBuffers = {};
  std::shared_ptr<const AdapterIdentity> m_identity;
  std::shared_ptr<RuntimeService> m_service;
  bool m_removed = false;
  bool m_querying = false;
  bool m_releasing9 = false;
  RuntimeAllocation* m_allocations = nullptr;
  RuntimeAllocation* m_orphans = nullptr;
  bool m_terminal = false;
  bool m_policyActive = false;
  bool m_primaryActive = false;
  bool m_creatingContext = false;
};

}
