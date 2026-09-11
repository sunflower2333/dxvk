#pragma once

#include "umd_ddi.h"
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

class RuntimeMemory;
class RuntimeAllocation {
public:
  RuntimeAllocation() = default;
  RuntimeAllocation(const RuntimeAllocation&) = delete;
  RuntimeAllocation& operator=(const RuntimeAllocation&) = delete;
  ~RuntimeAllocation();
  HRESULT release();
  D3DKMT_HANDLE handle() const { return m_handle; }
private:
  friend class RuntimeMemory;
  RuntimeMemory* m_owner = nullptr;
  HANDLE m_resource = nullptr;
  D3DKMT_HANDLE m_handle = 0;
  AllocationInfo m_info;
  bool m_published = false;
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
    const DXGI_DDI_BASE_CALLBACKS* dxgi);
  bool available() const;
  HRESULT allocate(RuntimeAllocation& out, HANDLE resource, UINT width,
    UINT height, DXGI_FORMAT format);
  HRESULT release(RuntimeAllocation& allocation);
  HRESULT upload(RuntimeAllocation& allocation, const void* pixels, UINT rowPitch);
  HRESULT present(RuntimeAllocation& source, const DXGI_DDI_ARG_PRESENT& args);
  HRESULT close();
private:
  HRESULT ensureContext();
  HANDLE m_device = nullptr;
  D3DDDI_DEVICECALLBACKS m_callbacks = {};
  DXGI_DDI_BASE_CALLBACKS m_dxgi = {};
  HANDLE m_context = nullptr;
};

}
