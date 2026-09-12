#pragma once

#include "umd_adapter.h"
#include "umd_runtime_bridge.h"
#include <map>
#include <mutex>

namespace dxvk::umd {

// An owner outside runtime private storage. Turnip keeps this owner alive until
// vkDestroyDevice, including worker drain and failed device creation cleanup.
class RuntimeGpu final : public std::enable_shared_from_this<RuntimeGpu> {
public:
  static std::shared_ptr<RuntimeGpu> create(HANDLE device,
    const D3DDDI_DEVICECALLBACKS& callbacks,
    std::shared_ptr<const AdapterIdentity> identity);
  ~RuntimeGpu();
  RuntimeBackend backend();
  // Call after releasing/draining the embedded device, before DestroyDevice
  // returns to the runtime. No callback can use a retired runtime handle.
  HRESULT close();

private:
  struct Allocation {
    void* token = nullptr;
    uint64_t address = 0, size = 0;
    uint32_t handle = 0, flags = 0, users = 1, maps = 0;
    void* mapping = nullptr;
    bool pending = false, locked = false;
  };
  struct Call {
    RuntimeGpu* value;
    std::shared_ptr<RuntimeGpu> owner;
    std::unique_lock<std::recursive_mutex> lock;
    explicit Call(void* ptr);
    ~Call();
    bool live(bool cleanup = false) const;
  };
  RuntimeGpu() = default;
  HRESULT identity();
  HRESULT context();
  HRESULT unlock(Allocation& allocation);
  HRESULT release(Allocation& allocation);
  Allocation* find(void* token);
  void publish(Allocation& allocation, mwd_allocation* out);
  static int32_t MWD_CALL getContext(void*, mwd_context_info*);
  static int32_t MWD_CALL allocate(void*, uint64_t, uint64_t, uint64_t, uint32_t, mwd_allocation*);
  static int32_t MWD_CALL retain(void*, void*, mwd_allocation*);
  static int32_t MWD_CALL release(void*, void*);
  static int32_t MWD_CALL map(void*, void*, void**, uint32_t*);
  static int32_t MWD_CALL unmap(void*, void*);
  static int32_t MWD_CALL submit(void*, const void*, uint32_t, const mwd_reference*, uint32_t);
  static int32_t MWD_CALL completed(void*, uint32_t*);
  static int32_t MWD_CALL status(void*);
  static const mwd_callbacks s_callbacks;

  std::recursive_mutex m_mutex;
  HANDLE m_device = nullptr, m_context = nullptr;
  D3DDDI_DEVICECALLBACKS m_callbacks = {};
  std::shared_ptr<const AdapterIdentity> m_identity;
  mwd_context_info m_info = {};
  std::map<uint64_t, std::unique_ptr<Allocation>> m_allocations;
  void* m_commands = nullptr;
  D3DDDI_ALLOCATIONLIST* m_allocationList = nullptr;
  D3DDDI_PATCHLOCATIONLIST* m_patchList = nullptr;
  UINT m_commandSize = 0, m_allocationCount = 0, m_patchCount = 0;
  bool m_live = true, m_closing = false, m_removed = false;
  bool m_querying = false, m_creating = false, m_submitting = false;
  unsigned m_active = 0;
  uintptr_t m_nextToken = 1;
};

}
