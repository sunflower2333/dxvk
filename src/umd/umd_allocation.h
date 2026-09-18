#pragma once

#include "umd_ddi.h"
#include "umd_adapter.h"
#include "umd_runtime_service.h"
#include <cstddef>
#include <cstdint>

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
  if (format == 3) return DXGI_FORMAT_R8G8B8A8_UNORM;
  return DXGI_FORMAT_UNKNOWN;
}

class RuntimeMemory;
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
  // An adopted allocation belongs to the process that created it. This device
  // holds a view for as long as its opened resource lives and must never
  // deallocate it; see RuntimeMemory::adopt.
  bool opened() const { return m_opened; }
private:
  friend class RuntimeMemory;
  RuntimeMemory* m_owner = nullptr;
  HANDLE m_resource = nullptr;
  D3DKMT_HANDLE m_handle = 0;
  D3DKMT_HANDLE m_kernelResource = 0;
  uint64_t m_generation = 0;
  AllocationInfo m_info;
  bool m_published = false;
  bool m_opened = false;
};

// Only the callback fields used by this D3D10.0 bridge are copied. Copying a
// whole current-SDK callback table could read past an older runtime's table.
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
  bool available() const;
  HRESULT allocate(RuntimeAllocation& out, HANDLE resource, UINT width,
    UINT height, DXGI_FORMAT format) {
    return call([&] { return allocateImpl(out, resource, width, height, format); });
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
  HRESULT present(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args) {
    return call([&] { return presentImpl(source, args); });
  }
  HRESULT close() { return !m_context ? S_OK : call([&] { return closeImpl(); }); }
private:
  template<typename Function> HRESULT call(Function&& function) {
    return m_service ? m_service->invoke(std::forward<Function>(function)) : function();
  }
  HRESULT allocateImpl(RuntimeAllocation& out, HANDLE resource, UINT width,
    UINT height, DXGI_FORMAT format);
  HRESULT adoptImpl(RuntimeAllocation& out, D3DKMT_HANDLE allocation,
    D3DKMT_HANDLE kernelResource, const AllocationInfo& info);
  HRESULT releaseImpl(RuntimeAllocation& allocation);
  HRESULT uploadImpl(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch);
  HRESULT downloadImpl(RuntimeAllocation& allocation, void* pixels, UINT rowPitch);
  HRESULT transferImpl(RuntimeAllocation& allocation, void* pixels, UINT rowPitch,
    bool publish);
  HRESULT presentImpl(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args);
  HRESULT closeImpl();
  HRESULT ensureContext();
  HRESULT checkIdentity();
  HANDLE m_device = nullptr;
  D3DDDI_DEVICECALLBACKS m_callbacks = {};
  DXGI_DDI_BASE_CALLBACKS m_dxgi = {};
  HANDLE m_context = nullptr;
  std::shared_ptr<const AdapterIdentity> m_identity;
  std::shared_ptr<RuntimeService> m_service;
  bool m_removed = false;
  bool m_querying = false;
};

}
