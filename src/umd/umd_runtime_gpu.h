#pragma once

#include "umd_adapter_identity.h"
#include "umd_runtime_bridge.h"
#include "umd_runtime_service.h"
#include "umd_runtime_diagnostics.h"
#include <map>
#include <mutex>

namespace dxvk::umd {

// An owner outside runtime private storage. Turnip keeps this owner alive until
// vkDestroyDevice, including worker drain and failed device creation cleanup.
class RuntimeGpu final : public std::enable_shared_from_this<RuntimeGpu> {
public:
  static std::shared_ptr<RuntimeGpu> create(HANDLE device,
    const D3DDDI_DEVICECALLBACKS& callbacks,
    std::shared_ptr<const AdapterIdentity> identity,
    std::shared_ptr<RuntimeService> service = {});
  ~RuntimeGpu();
  RuntimeBackend backend();
  // Close on the DestroyDevice caller after synchronous backend drain, before
  // that DDI returns and the runtime may invalidate handles and backing.
  HRESULT close();
  template<typename Function> HRESULT serviceCall(Function&& function) {
    if (!m_diagnostics.enabled())
      return m_service ? m_service->invoke(std::forward<Function>(function)) : function();
    bool entered = false;
    auto invoke = [&] { entered = true; return function(); };
    const HRESULT hr = m_service ? m_service->invoke(invoke) : invoke();
    if (FAILED(hr)) {
      std::lock_guard<std::recursive_mutex> lock(m_mutex);
      traceFailure(entered ? "callback-result" : "callback-dispatch", hr);
    }
    return hr;
  }

private:
  struct Allocation {
    void* token = nullptr;
    uint64_t address = 0, size = 0;
    uint32_t handle = 0, flags = 0, users = 1, maps = 0;
    void* mapping = nullptr;
    bool pending = false, locked = false, mapValid = false;
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
  RuntimeGpuDiagnosticInfo traceInfo() const;
  HRESULT traceFailure(const char* stage, HRESULT hr,
    const RuntimeGpuDiagnosticInfo& info, bool callback = false, HRESULT callbackHr = S_OK);
  HRESULT traceFailure(const char* stage, HRESULT hr, bool callback = false, HRESULT callbackHr = S_OK) {
    return traceFailure(stage, hr, traceInfo(), callback, callbackHr);
  }
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
  RuntimeGpuDiagnostics m_diagnostics;
  std::shared_ptr<RuntimeService> m_service;
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
