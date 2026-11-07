#pragma once

#include "umd_adapter_identity.h"
#include "umd_runtime_bridge.h"
#include "umd_runtime_service.h"
#include "umd_runtime_diagnostics.h"
#include <map>
#include <list>
#include <mutex>
#include <atomic>

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
  // Import a real VidMm Native AHB wrapper into this device's GPU context.
  // The caller retains the wrapper through allocationOwner; this bridge never
  // deallocates its borrowed handle or exposes a guest CPU mapping.
  HRESULT importNativeSurface(uint32_t handle, uint64_t key, uint64_t size,
    std::shared_ptr<void> allocationOwner, mwd_allocation* out,
    const std::atomic<bool>* revoked = nullptr);
  // End one runtime open without retiring another open of the same image.
  // Must run before its borrowed handle becomes invalid. A pending RenderCb
  // already owns its allocation-list references; later packets may not use it.
  HRESULT revokeNativeSurface(const std::shared_ptr<void>& allocationOwner);
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
  struct NativeOwner {
    std::shared_ptr<void> owner;
    uint32_t handle = 0;
    bool active = true, resident = false;
  };
  struct Allocation {
    void* token = nullptr;
    uint64_t address = 0, size = 0;
    uint32_t handle = 0, flags = 0, users = 1, maps = 0;
    void* mapping = nullptr;
    bool pending = false, locked = false, mapValid = false, resident = false;
    uint64_t nativeKey = 0;
    bool nativeUncertain = false;
    std::list<NativeOwner> nativeOwners;
    bool native() const { return (flags & MWD_ALLOCATION_NATIVE_SURFACE) != 0; }
    void selectNativeHandle() {
      handle = 0;
      for (const auto& entry : nativeOwners) if (entry.active) { handle = entry.handle; break; }
    }
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
  HRESULT makeResident(Allocation& allocation);
  HRESULT makeResident(uint32_t handle, bool& resident, const bool* active = nullptr);
  HRESULT evict(Allocation& allocation);
  HRESULT evict(uint32_t handle, bool& resident);
  void revokeNativeOwner(Allocation& allocation, NativeOwner& owner);
  HRESULT waitPaging();
  RuntimeGpuDiagnosticInfo traceInfo() const;
  RuntimeGpuDiagnosticInfo traceCloseInfo() const;
  HRESULT traceFailure(const char* stage, HRESULT hr,
    const RuntimeGpuDiagnosticInfo& info, bool callback = false, HRESULT callbackHr = S_OK);
  HRESULT traceFailure(const char* stage, HRESULT hr, bool callback = false, HRESULT callbackHr = S_OK) {
    return traceFailure(stage, hr, traceInfo(), callback, callbackHr);
  }
  HRESULT unlock(Allocation& allocation);
  HRESULT release(Allocation& allocation);
  HRESULT reserve(uint64_t size, uint64_t alignment, uint64_t requested,
    uint32_t flags, Allocation*& allocation);
  HRESULT nativeShare(Allocation& allocation, uint32_t opcode);
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
  D3DKMT_HANDLE m_pagingQueue = 0, m_pagingSync = 0;
  uint64_t m_pagingFence = 0;
  void* m_pagingCpuFence = nullptr;
  DWORD m_pagingWaitStatus = WAIT_FAILED;
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
