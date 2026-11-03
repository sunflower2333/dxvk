#include "umd_runtime_gpu.h"
#include <unordered_map>
#include "umd_allocation.h"
#include <atomic>
#include <cstring>
#include <limits>
#include <utility>

namespace dxvk::umd {
namespace {
// First backend failures remain inspectable in DWM without changing its
// environment or relying on a writable log directory. Publish each slot once.
struct RuntimeGpuFailureRecord {
  std::atomic<uint32_t> complete{0};
  uint32_t result = 0, callbackResult = 0, thread = 0;
  uint64_t stage = 0, owner = 0, ticks = 0, generation = 0;
  uint32_t context = 0, queue = 0, callback = 0, reserved = 0;
  uint32_t pagingQueue = 0, pagingSync = 0;
  uint64_t pagingTarget = 0, pagingCpuAddress = 0, pagingObserved = 0;
  uint32_t pagingObservedValid = 0, pagingWaitStatus = WAIT_FAILED;
};
struct RuntimeGpuFailureTrace {
  uint32_t size = sizeof(RuntimeGpuFailureTrace), version = 2;
  std::atomic<uint32_t> count{0};
  uint32_t capacity = 32;
  RuntimeGpuFailureRecord records[32];
} runtimeGpuFailureTrace;
static_assert(sizeof(RuntimeGpuFailureRecord) == 104);
// Exact VIOGPU v0 wire types. Runtime opaque handles never enter these bytes.
#pragma pack(push, 4)
struct Header {
  uint32_t magic = 0x504d5644, version = 0, size, reserved = 0;
  bool valid(uint32_t expected) const {
    return magic == 0x504d5644 && !version && size == expected && !reserved;
  }
};
struct Create { Header header{0x504d5644, 0, 32, 0}; uint64_t generation; uint32_t flags = 0, reserved = 0; };
struct Context {
  Header header{0x504d5644, 0, 64, 0}; uint32_t opcode = 1, flags = 0;
  uint64_t expected = 0, start = 0, size = 0, generation = 0;
  uint32_t context = 0, queue = 0;
};
struct Fence {
  Header header{0x504d5644, 0, 56, 0}; uint32_t opcode = 2, flags = 0;
  uint64_t expected = 0, completed = 0, generation = 0;
  uint32_t context = 0, reserved = 0;
};
struct Render {
  Header header{0x504d5644, 0, 0, 0}; uint32_t opcode = 1, flags = 0;
  uint64_t generation = 0;
  uint32_t references = 64, count = 0, stream = 0, size = 0, reserved[4] = {};
};
struct Reference { uint32_t index, flags; uint64_t offset, length; uint32_t patch, reserved; };
struct NativeShare {
  Header header{0x504d5644, 0, 88, 0}; uint32_t opcode = 0, flags = 0;
  uint64_t generation = 0, key = 0, address = 0, size = 0;
  uint32_t context = 0, reserved = 0;
  uint64_t reserved2[3] = {};
};
struct ImportedReference {
  uint64_t key, address, size, generation;
  uint32_t access, allocationIndexPlusOne;
};
#pragma pack(pop)
static_assert(sizeof(Create) == 32 && sizeof(Context) == 64 && sizeof(Fence) == 56);
static_assert(sizeof(Render) == 64 && sizeof(Reference) == 32);
static_assert(sizeof(NativeShare) == 88 && sizeof(ImportedReference) == 40);
HRESULT exact(HRESULT hr) { return hr == S_OK || FAILED(hr) ? hr : E_FAIL; }
struct Pending {
  bool& flag;
  explicit Pending(bool& value) : flag(value) { flag = true; }
  ~Pending() { flag = false; }
};
template<typename Function, Function function> struct RuntimeEntry;
template<typename... Args, int32_t (MWD_CALL *function)(void*, Args...)>
struct RuntimeEntry<int32_t (MWD_CALL *)(void*, Args...), function> {
  static int32_t MWD_CALL call(void* owner, Args... args) {
    return static_cast<RuntimeGpu*>(owner)->serviceCall([&] { return function(owner, args...); });
  }
};
template<auto function>
constexpr auto runtimeEntry = &RuntimeEntry<decltype(function), function>::call;
}

const mwd_callbacks RuntimeGpu::s_callbacks = {
  MWD_RUNTIME_MAGIC, MWD_RUNTIME_ABI_VERSION, sizeof(mwd_callbacks), 0,
  runtimeEntry<getContext>, runtimeEntry<allocate>, runtimeEntry<retain>,
  runtimeEntry<static_cast<int32_t (MWD_CALL *)(void*, void*)>(release)>,
  runtimeEntry<map>, runtimeEntry<unmap>, runtimeEntry<submit>,
  runtimeEntry<completed>, runtimeEntry<status>
};

std::shared_ptr<RuntimeGpu> RuntimeGpu::create(HANDLE device,
    const D3DDDI_DEVICECALLBACKS& cb, std::shared_ptr<const AdapterIdentity> identity,
    std::shared_ptr<RuntimeService> service) {
  auto result = std::shared_ptr<RuntimeGpu>(new RuntimeGpu);
  char diagnosticFlag[2] = {};
  const DWORD diagnosticFlagBytes = GetEnvironmentVariableA("TU_WDDM_DIAGNOSTICS",
    diagnosticFlag, sizeof(diagnosticFlag));
  char queryTrace[2] = {};
  const DWORD queryTraceBytes = GetEnvironmentVariableA("DXVK_UMD_ADAPTER_QUERY_TRACE",
    queryTrace, sizeof(queryTrace));
  // The owned adapter-query diagnostic also needs the first context failure:
  // a failed backend can unload its private ICD before the module census.
  result->m_diagnostics = RuntimeGpuDiagnostics(RuntimeGpuDiagnostics::enabledFlag(
    diagnosticFlagBytes == 1 ? diagnosticFlag : nullptr)
    || RuntimeGpuDiagnostics::enabledFlag(queryTraceBytes == 1 ? queryTrace : nullptr));
  result->m_device = device;
  result->m_identity = std::move(identity);
  result->m_service = std::move(service);
  // Copy only callbacks in the common D3D9/D3D10 table. Never retain a pointer
  // to runtime-owned tables or read a newer whole-WDK structure from old input.
  auto& copy = result->m_callbacks;
  copy.pfnAllocateCb = cb.pfnAllocateCb; copy.pfnDeallocateCb = cb.pfnDeallocateCb;
  copy.pfnLockCb = cb.pfnLockCb; copy.pfnUnlockCb = cb.pfnUnlockCb;
  copy.pfnCreateContextCb = cb.pfnCreateContextCb; copy.pfnDestroyContextCb = cb.pfnDestroyContextCb;
  copy.pfnEscapeCb = cb.pfnEscapeCb; copy.pfnRenderCb = cb.pfnRenderCb;
  if (result->m_identity) copyResidencyCallbacks(*result->m_identity, cb, copy);
  return result;
}
RuntimeGpu::~RuntimeGpu() { close(); }
HRESULT RuntimeGpu::makeResident(Allocation& allocation) {
  return makeResident(allocation.handle, allocation.resident);
}
HRESULT RuntimeGpu::makeResident(uint32_t handle, bool& resident, const bool* active) {
  if (!m_identity->explicitResidency() || resident) return S_OK;
  const auto& cb = m_callbacks;
  if (!cb.pfnMakeResidentCb || !cb.pfnEvictCb || !cb.pfnCreatePagingQueueCb
      || !cb.pfnDestroyPagingQueueCb
      || (!cb.pfnWaitForSynchronizationObjectFromGpuCb && !cb.pfnWaitForSynchronizationObjectFromCpuCb))
    return traceFailure("residency-callbacks", DXGI_ERROR_UNSUPPORTED);
  if (!m_pagingQueue) {
    D3DDDICB_CREATEPAGINGQUEUE create = {};
    create.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
    const HRESULT hr = exact(cb.pfnCreatePagingQueueCb(m_device, &create));
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    // Even a malformed/failed create may return ownership to release at close.
    m_pagingQueue = create.hPagingQueue; m_pagingSync = create.hSyncObject;
    m_pagingCpuFence = create.FenceValueCPUVirtualAddress;
    if (active && !*active) return DXGI_ERROR_DEVICE_REMOVED;
    if (hr != S_OK || !m_pagingQueue || !m_pagingSync)
      return traceFailure("create-paging-queue", FAILED(hr) ? hr : E_FAIL);
  }
  if (!m_pagingSync) return E_FAIL;
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    if (active && !*active) return DXGI_ERROR_DEVICE_REMOVED;
    D3DDDI_MAKERESIDENT request = {};
    request.hPagingQueue = m_pagingQueue; request.NumAllocations = 1;
    request.AllocationList = &handle;
    // Every live native BO may be referenced by the next raw-IOVA packet.
    // There is no idle BO to trim. Retry once without the soft budget limit,
    // never set MustSucceed and never retry a non-budget failure.
    request.Flags.CantTrimFurther = attempt != 0;
    const HRESULT hr = cb.pfnMakeResidentCb(m_device, &request);
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    if (hr == S_OK || hr == E_PENDING) {
      if (!active || *active) resident = true;
      if (request.NumAllocations != 1 || (hr == E_PENDING && !request.PagingFenceValue))
        return traceFailure("make-resident-reply", E_FAIL);
      // A cancelled open cannot reuse its handle, but paging work accepted by
      // this queue still contributes to the context's next GPU dependency.
      if (hr == E_PENDING && request.PagingFenceValue > m_pagingFence)
        m_pagingFence = request.PagingFenceValue;
      if (active && !*active) return DXGI_ERROR_DEVICE_REMOVED;
      return S_OK;
    }
    if (active && !*active) return DXGI_ERROR_DEVICE_REMOVED;
    if (hr != E_OUTOFMEMORY || attempt)
      return traceFailure("make-resident", exact(hr), true, hr);
  }
  return E_OUTOFMEMORY;
}
HRESULT RuntimeGpu::evict(Allocation& allocation) {
  return evict(allocation.handle, allocation.resident);
}
HRESULT RuntimeGpu::evict(uint32_t handle, bool& resident) {
  if (!resident) return S_OK;
  D3DDDICB_EVICT request = {};
  request.NumAllocations = 1; request.AllocationList = &handle;
  const HRESULT hr = exact(m_callbacks.pfnEvictCb(m_device, &request));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK) resident = false;
  return traceFailure("evict", hr);
}
HRESULT RuntimeGpu::waitPaging() {
  if (!m_pagingFence) return S_OK;
  const uint64_t fence = m_pagingFence;
  const D3DKMT_HANDLE sync = m_pagingSync;
  if (m_callbacks.pfnWaitForSynchronizationObjectFromGpuCb) {
    // Queue the residency dependency on the same context as the following
    // RenderCb. VidMm may legitimately finish paging after our bounded CPU
    // fallback wait; the scheduler must order GPU access after completion.
    // S_OK means the wait was queued, not that the CPU fence has advanced.
    D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMGPU request = {};
    request.hContext = m_context; request.ObjectCount = 1;
    request.ObjectHandleArray = &sync; request.MonitoredFenceValueArray = &fence;
    m_pagingWaitStatus = WAIT_FAILED;
    const HRESULT callbackHr = m_callbacks.pfnWaitForSynchronizationObjectFromGpuCb(m_device, &request);
    const HRESULT hr = m_live ? exact(callbackHr) : DXGI_ERROR_DEVICE_REMOVED;
    if (hr == S_OK && m_pagingFence == fence) m_pagingFence = 0;
    return traceFailure("wait-paging-gpu", hr, true, callbackHr);
  }
  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!event) return HRESULT_FROM_WIN32(GetLastError());
  D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMCPU request = {};
  request.ObjectCount = 1; request.ObjectHandleArray = &sync; request.FenceValueArray = &fence;
  request.hAsyncEvent = event;
  m_pagingWaitStatus = WAIT_FAILED;
  const HRESULT callbackHr = m_callbacks.pfnWaitForSynchronizationObjectFromCpuCb(m_device, &request);
  HRESULT hr = exact(callbackHr);
  if (!m_live) hr = DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK) {
    const DWORD status = WaitForSingleObject(event, 5000);
    m_pagingWaitStatus = status;
    if (status != WAIT_OBJECT_0)
      hr = HRESULT_FROM_WIN32(status == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT);
  }
  CloseHandle(event);
  if (hr == S_OK && m_pagingFence == fence) m_pagingFence = 0;
  return traceFailure("wait-paging", hr, true, callbackHr);
}
RuntimeBackend RuntimeGpu::backend() {
  return {{MWD_STYPE_DEVICE, nullptr, &s_callbacks, this}, shared_from_this()};
}
HRESULT RuntimeGpu::importNativeSurface(uint32_t handle, uint64_t key, uint64_t size,
    std::shared_ptr<void> allocationOwner, mwd_allocation* out, const std::atomic<bool>* revoked) {
  if (!out) return E_POINTER;
  *out = {};
  if (!handle || !key || key > UINT32_MAX || !size || (size & 4095)
      || size > (UINT32_MAX & ~uint64_t(4095)) || !allocationOwner) return E_INVALIDARG;
  return serviceCall([&]() -> HRESULT {
    try {
      Call call(this);
      if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
      // Creating resources can be cancelled in a callback before a binding
      // exists. Once registered below, binding.active protects later callbacks.
      if (revoked && revoked->load()) return DXGI_ERROR_DEVICE_REMOVED;
      HRESULT hr = context();
      if (hr != S_OK) return hr;
      if (revoked && revoked->load()) return DXGI_ERROR_DEVICE_REMOVED;
      for (const auto& entry : m_allocations) {
        const auto& existing = *entry.second;
        if (existing.nativeKey == key) continue;
        if (existing.handle == handle) return E_INVALIDARG;
        for (const auto& binding : existing.nativeOwners)
          if (binding.active && (binding.handle == handle || binding.owner == allocationOwner)) return E_INVALIDARG;
      }
      for (const auto& entry : m_allocations) {
        auto& existing = *entry.second;
        if (existing.nativeKey == key) {
          if (existing.pending || existing.nativeUncertain) return DXGI_ERROR_WAS_STILL_DRAWING;
          if (existing.size != size || existing.users == UINT32_MAX) return E_INVALIDARG;
          existing.nativeOwners.remove_if([](const NativeOwner& value) { return !value.active; });
          bool resident = false;
          for (const auto& binding : existing.nativeOwners) {
            if (binding.owner == allocationOwner) {
              if (!binding.active || binding.handle != handle) return E_INVALIDARG;
              ++existing.users; publish(existing, out); return S_OK;
            }
            resident |= binding.active && binding.handle == handle && binding.resident;
          }
          // Keep the registration stable across reentrant runtime callbacks.
          existing.nativeOwners.push_back({std::move(allocationOwner), handle});
          auto& binding = existing.nativeOwners.back();
          Pending pending(existing.pending);
          hr = resident ? S_OK : makeResident(handle, binding.resident, &binding.active);
          if (!binding.active) { binding.resident = false; hr = DXGI_ERROR_DEVICE_REMOVED; }
          if (hr == S_OK) hr = identity();
          if (hr == S_OK && !binding.active) hr = DXGI_ERROR_DEVICE_REMOVED;
          if (hr == S_OK) {
            existing.selectNativeHandle(); ++existing.users; publish(existing, out);
          } else {
            // A successful MakeResident is kept until the runtime tears down
            // this failed open; never touch its handle after returning failure.
            revokeNativeOwner(existing, binding);
          }
          return hr;
        }
      }
      Allocation* allocation = nullptr;
      hr = reserve(size, 4096, 0, 4u | MWD_ALLOCATION_NATIVE_SURFACE, allocation);
      if (hr != S_OK) return hr;
      allocation->pending = true;
      struct Finish {
        RuntimeGpu& owner; Allocation* value;
        ~Finish() {
          value->pending = false;
          if (!value->handle && !value->nativeKey) owner.m_allocations.erase(value->address);
        }
      } finish{*this, allocation};
      allocation->nativeOwners.push_back({std::move(allocationOwner), handle});
      allocation->handle = handle;
      auto& binding = allocation->nativeOwners.front();
      hr = makeResident(handle, binding.resident, &binding.active);
      if (!binding.active) { binding.resident = false; hr = DXGI_ERROR_DEVICE_REMOVED; }
      if (hr == S_OK) {
        // A failed escape may have reached the host. Keep this range and
        // backing until RELEASE_NATIVE or context teardown confirms retirement.
        allocation->nativeKey = key; allocation->nativeUncertain = true;
        hr = nativeShare(*allocation, 6);
        if (hr == S_OK) allocation->nativeUncertain = false;
      }
      if (hr == S_OK) hr = identity();
      if (hr == S_OK && !binding.active) hr = DXGI_ERROR_DEVICE_REMOVED;
      if (hr == S_OK) publish(*allocation, out);
      else if (m_live) release(*allocation);
      return hr;
    } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
      catch (...) { return E_FAIL; }
  });
}
void RuntimeGpu::revokeNativeOwner(Allocation& allocation, NativeOwner& binding) {
  binding.active = false;
  allocation.selectNativeHandle();
  if (binding.resident) {
    // Two runtime opens may use the same handle. Transfer our single residency
    // reference to a surviving registration rather than evicting its storage.
    for (auto& other : allocation.nativeOwners) {
      if (other.active && other.handle == binding.handle) {
        other.resident = true; binding.resident = false; break;
      }
    }
    // Submission drain is not GPU completion. The runtime's actual allocation
    // destruction retires this residency reference, including in-flight work.
    // Never Evict here or retry later through a potentially recycled handle.
    binding.resident = false;
  }
  binding.owner.reset();
}
HRESULT RuntimeGpu::revokeNativeSurface(const std::shared_ptr<void>& allocationOwner) {
  if (!allocationOwner) return E_INVALIDARG;
  return serviceCall([&]() -> HRESULT {
    Call call(this);
    for (const auto& entry : m_allocations) {
      auto& allocation = *entry.second;
      for (auto& binding : allocation.nativeOwners) {
        if (binding.owner != allocationOwner) continue;
        revokeNativeOwner(allocation, binding);
        return S_OK;
      }
    }
    return S_OK;
  });
}
HRESULT RuntimeGpu::nativeShare(Allocation& allocation, uint32_t opcode) {
  NativeShare info;
  info.opcode = opcode; info.generation = m_info.generation;
  info.key = allocation.nativeKey; info.address = allocation.address; info.size = allocation.size;
  const NativeShare expected = info;
  D3DDDICB_ESCAPE request = {};
  request.hDevice = m_device; request.hContext = m_context;
  request.pPrivateDriverData = &info; request.PrivateDriverDataSize = sizeof(info);
  const HRESULT hr = exact(m_callbacks.pfnEscapeCb(m_identity->runtime, &request));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) return traceFailure("native-share-callback", hr);
  // The only output is the validated context id. Host surfaces have no owner
  // context to alias; changed addresses/sizes cannot be accepted as success.
  const uint32_t contextId = info.context;
  info.context = 0;
  if (contextId != m_info.context_id || std::memcmp(&info, &expected, sizeof(info)))
    return traceFailure("native-share-reply", E_FAIL);
  return S_OK;
}
RuntimeGpu::Call::Call(void* ptr)
: value(static_cast<RuntimeGpu*>(ptr)), owner(value->weak_from_this().lock()), lock(value->m_mutex) { ++value->m_active; }
RuntimeGpu::Call::~Call() { --value->m_active; }
bool RuntimeGpu::Call::live(bool cleanup) const {
  return value->m_live && (cleanup || !value->m_closing) && value->m_device && value->m_identity;
}

RuntimeGpuDiagnosticInfo RuntimeGpu::traceInfo() const {
  RuntimeGpuDiagnosticInfo info;
  info.generation = m_info.generation ? m_info.generation : m_identity ? m_identity->generation : 0;
  info.context = m_info.context_id; info.queue = m_info.queue_id;
  return info;
}
RuntimeGpuDiagnosticInfo RuntimeGpu::traceCloseInfo() const {
  auto info = traceInfo();
  if (!m_diagnostics.enabled()) return info;
  info.activeCalls = m_active;
  for (const auto& entry : m_allocations) {
    if (entry.second->handle) ++info.allocations;
    if (entry.second->locked) ++info.lockedAllocations;
  }
  return info;
}
HRESULT RuntimeGpu::traceFailure(const char* stage, HRESULT hr,
    const RuntimeGpuDiagnosticInfo& info, bool callback, HRESULT callbackHr) {
  if (FAILED(hr)) {
    const DWORD savedError = GetLastError();
    uint32_t index = runtimeGpuFailureTrace.count.load(std::memory_order_relaxed);
    while (index < runtimeGpuFailureTrace.capacity
        && !runtimeGpuFailureTrace.count.compare_exchange_weak(index, index + 1,
          std::memory_order_relaxed)) { }
    if (index < runtimeGpuFailureTrace.capacity) {
      auto& record = runtimeGpuFailureTrace.records[index];
      record.result = uint32_t(hr); record.callbackResult = uint32_t(callbackHr);
      record.thread = GetCurrentThreadId(); record.stage = uint64_t(uintptr_t(stage));
      record.owner = uint64_t(uintptr_t(this)); record.ticks = GetTickCount64();
      record.generation = info.generation; record.context = info.context;
      record.queue = info.queue; record.callback = callback;
      record.pagingQueue = m_pagingQueue; record.pagingSync = m_pagingSync;
      record.pagingTarget = m_pagingFence;
      record.pagingCpuAddress = uint64_t(uintptr_t(m_pagingCpuFence));
      record.pagingWaitStatus = m_pagingWaitStatus;
      // Failure-only observation of the runtime's read-only monitored fence.
      // A protected/retired mapping must not replace the original error with
      // an access violation. This never changes a fence or satisfies a wait.
      SIZE_T read = 0;
      record.pagingObservedValid = m_live && m_pagingCpuFence
        && ReadProcessMemory(GetCurrentProcess(), m_pagingCpuFence,
          &record.pagingObserved, sizeof(record.pagingObserved), &read)
        && read == sizeof(record.pagingObserved);
      record.complete.store(1, std::memory_order_release);
    }
    SetLastError(savedError);
    m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::Failure,
      stage, hr, info, callback, callbackHr);
  }
  return hr;
}

HRESULT RuntimeGpu::identity() {
  if (!m_live || m_removed || !m_device || !m_identity || !m_identity->available()) return DXGI_ERROR_DEVICE_REMOVED;
  if (m_querying) return DXGI_ERROR_WAS_STILL_DRAWING;
  Pending pending(m_querying);
  RuntimeIdentity current;
  HRESULT hr = m_identity->queryCurrent(current);
  if (!m_live || !m_identity->available()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK && (std::memcmp(current.luid.data(), &m_identity->luid, sizeof(LUID))
      || current.generation != m_identity->generation || current.capabilities != m_identity->capabilities))
    hr = DXGI_ERROR_DEVICE_REMOVED;
  if (hr == DXGI_ERROR_DEVICE_REMOVED) m_removed = true;
  return exact(hr);
}
HRESULT RuntimeGpu::context() {
  if (m_creating) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (m_info.context_id) return identity();
  const auto& cb = m_callbacks;
  if (!cb.pfnAllocateCb || !cb.pfnDeallocateCb || !cb.pfnLockCb || !cb.pfnUnlockCb
      || !cb.pfnCreateContextCb || !cb.pfnDestroyContextCb || !cb.pfnEscapeCb || !cb.pfnRenderCb)
    return DXGI_ERROR_UNSUPPORTED;
  HRESULT hr = identity();
  if (FAILED(hr)) return hr;
  Pending pending(m_creating);
  if (m_context) {
    D3DDDICB_DESTROYCONTEXT destroy = {}; destroy.hContext = m_context;
    hr = exact(cb.pfnDestroyContextCb(m_device, &destroy));
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    if (FAILED(hr)) return hr;
    m_context = nullptr;
  }
  Create info; info.generation = m_identity->generation;
  D3DDDICB_CREATECONTEXT create = {};
  create.EngineAffinity = 1; create.pPrivateDriverData = &info; create.PrivateDriverDataSize = sizeof(info);
  const HRESULT callbackHr = cb.pfnCreateContextCb(m_device, &create);
  hr = exact(callbackHr);
  traceFailure("create-context-callback", hr, true, callbackHr);
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  m_context = create.hContext;
  if (hr == S_OK && !m_context) hr = E_FAIL;
  if (hr == S_OK && !m_identity->available()) hr = DXGI_ERROR_DEVICE_REMOVED;
  Context reply;
  reply.expected = m_identity->generation;
  if (hr == S_OK) {
    D3DDDICB_ESCAPE escape = {};
    escape.hDevice = m_device; escape.hContext = m_context;
    escape.pPrivateDriverData = &reply; escape.PrivateDriverDataSize = sizeof(reply);
    const HRESULT escapeHr = cb.pfnEscapeCb(m_identity->runtime, &escape);
    hr = exact(escapeHr);
    traceFailure("context-escape-callback", hr, true, escapeHr);
    if (!m_live || !m_identity->available()) hr = DXGI_ERROR_DEVICE_REMOVED;
  }
  if (hr == S_OK && (!reply.header.valid(sizeof(reply)) || reply.opcode != 1 || reply.flags
      || reply.expected != m_identity->generation || reply.generation != reply.expected
      || !reply.context || !reply.queue || !reply.start || !reply.size
      || ((reply.start | reply.size) & 4095) || reply.size > UINT64_MAX - reply.start
      || !create.pCommandBuffer || create.CommandBufferSize < sizeof(Render) || create.CommandBufferSize > 65536
      || !create.pAllocationList || !create.AllocationListSize || create.AllocationListSize > 1024
      || !create.pPatchLocationList || !create.PatchLocationListSize || create.PatchLocationListSize > 1024)) hr = E_FAIL;
  if (hr == S_OK) hr = identity();
  // Preserve the validation failure before balancing teardown can return a
  // different HRESULT or retire this owner from inside DestroyContextCb.
  traceFailure("context-validation", hr);
  if (hr == S_OK) {
    std::memcpy(m_info.luid, &m_identity->luid, sizeof(LUID));
    m_info.generation = reply.generation; m_info.va_start = reply.start; m_info.va_size = reply.size;
    m_info.context_id = reply.context; m_info.queue_id = reply.queue;
    m_commands = create.pCommandBuffer; m_commandSize = create.CommandBufferSize;
    m_allocationList = create.pAllocationList; m_allocationCount = create.AllocationListSize;
    m_patchList = create.pPatchLocationList; m_patchCount = create.PatchLocationListSize;
    m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::ContextReady,
      "context", S_OK, traceInfo());
  } else if (m_live && m_context) {
    D3DDDICB_DESTROYCONTEXT destroy = {}; destroy.hContext = m_context;
    const HRESULT cleanup = exact(cb.pfnDestroyContextCb(m_device, &destroy));
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    if (cleanup == S_OK) m_context = nullptr;
    else hr = cleanup; // Retain ownership for close/retry when destroy fails.
  }
  return traceFailure("context-validation", hr);
}

RuntimeGpu::Allocation* RuntimeGpu::find(void* token) {
  for (const auto& entry : m_allocations) if (entry.second->token == token) return entry.second.get();
  return nullptr;
}
void RuntimeGpu::publish(Allocation& a, mwd_allocation* out) {
  *out = {a.token, a.address, a.size, m_info.generation, a.handle, a.flags};
}
HRESULT RuntimeGpu::reserve(uint64_t size, uint64_t alignment, uint64_t requested,
    uint32_t flags, Allocation*& allocation) {
  uint64_t address = m_info.va_start;
  const uint64_t end = address + m_info.va_size;
  auto position = m_allocations.begin();
  while (true) {
    if (address > UINT64_MAX - (alignment - 1)) return E_OUTOFMEMORY;
    address = (address + alignment - 1) & ~(alignment - 1);
    const uint64_t limit = position == m_allocations.end() ? end : position->first;
    if (requested >= address && requested <= limit) address = requested;
    if ((!requested || address == requested) && address <= limit && size <= limit - address) break;
    if ((requested && requested < limit) || position == m_allocations.end()) return E_OUTOFMEMORY;
    address = position->second->address + position->second->size;
    ++position;
  }
  if (!m_nextToken) return E_OUTOFMEMORY;
  auto value = std::make_unique<Allocation>();
  value->token = reinterpret_cast<void*>(m_nextToken++);
  value->address = address; value->size = size; value->flags = flags;
  allocation = value.get();
  m_allocations.emplace_hint(position, address, std::move(value));
  return S_OK;
}
int32_t MWD_CALL RuntimeGpu::getContext(void* ptr, mwd_context_info* out) {
  if (!out) return E_POINTER;
  *out = {};
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return self.traceFailure("context-owner", DXGI_ERROR_DEVICE_REMOVED);
  const HRESULT hr = self.context();
  if (hr == S_OK) *out = self.m_info;
  return self.traceFailure("context", hr);
}
int32_t MWD_CALL RuntimeGpu::allocate(void* ptr, uint64_t size, uint64_t alignment,
    uint64_t requested, uint32_t flags, mwd_allocation* out) {
  if (!out) return E_POINTER;
  *out = {};
  try {
    Call call(ptr); auto& self = *call.value;
    if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
    if (!size || size > (UINT32_MAX & ~uint64_t(4095)) || alignment < 4096
        || (alignment & (alignment - 1)) || (requested & (alignment - 1))
        || !(flags & 4) || (flags & ~14u)) return E_INVALIDARG;
    HRESULT hr = self.context();
    if (FAILED(hr)) return hr;
    size = (size + 4095) & ~uint64_t(4095);
    Allocation* allocation = nullptr;
    hr = self.reserve(size, alignment, requested, flags, allocation);
    if (hr != S_OK) return hr;
    allocation->pending = true;
    struct Finish {
      RuntimeGpu& owner; Allocation* value;
      ~Finish() {
        value->pending = false;
        if (!value->handle) owner.m_allocations.erase(value->address);
      }
    } finish{self, allocation};
    AllocationInfo info;
    info.size = size; info.requestedIova = allocation->address; info.resetGeneration = self.m_info.generation;
    info.flags = flags; info.contextId = self.m_info.context_id;
    D3DDDI_ALLOCATIONINFO output = {};
    output.pPrivateDriverData = &info; output.PrivateDriverDataSize = sizeof(info);
    D3DDDICB_ALLOCATE request = {};
    request.NumAllocations = 1; request.pAllocationInfo = &output;
    hr = exact(self.m_callbacks.pfnAllocateCb(self.m_device, &request));
    allocation->handle = output.hAllocation;
    if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
    if (hr == S_OK && (!allocation->handle || request.hKMResource)) hr = E_FAIL;
    if (hr == S_OK) hr = self.makeResident(*allocation);
    if (hr == S_OK) hr = self.identity();
    if (hr == S_OK) self.publish(*allocation, out);
    // Failed creates retain unpublished ownership until close if cleanup fails.
    // Keep this record alive until Finish clears its reservation.
    else if (allocation->handle && self.m_live) {
      self.evict(*allocation);
      if (!self.m_live) return DXGI_ERROR_DEVICE_REMOVED;
      D3DDDICB_DEALLOCATE cleanup = {};
      cleanup.NumAllocations = 1; cleanup.HandleList = &allocation->handle;
      const HRESULT released = exact(self.m_callbacks.pfnDeallocateCb(self.m_device, &cleanup));
      if (!self.m_live) return DXGI_ERROR_DEVICE_REMOVED;
      if (released == S_OK) { allocation->handle = 0; allocation->resident = false; }
      else hr = released;
    }
    return hr;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
int32_t MWD_CALL RuntimeGpu::retain(void* ptr, void* token, mwd_allocation* out) {
  if (!out) return E_POINTER;
  *out = {};
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  auto* a = self.find(token);
  if (!a || a->pending || a->nativeUncertain || !a->handle || a->users == UINT32_MAX
      || (a->native() && !a->nativeKey)) return E_INVALIDARG;
  Pending pending(a->pending);
  HRESULT hr = self.identity();
  if (hr == S_OK && a->native()) {
    a->selectNativeHandle();
    if (!a->handle || !a->nativeKey) hr = DXGI_ERROR_DEVICE_REMOVED;
  }
  if (hr == S_OK) { ++a->users; self.publish(*a, out); }
  return hr;
}
HRESULT RuntimeGpu::unlock(Allocation& a) {
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (!a.locked) return S_OK;
  D3DDDICB_UNLOCK request = {}; request.NumAllocations = 1; request.phAllocations = &a.handle;
  const HRESULT hr = exact(m_callbacks.pfnUnlockCb(m_device, &request));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK) { a.locked = false; a.mapValid = false; a.mapping = nullptr; a.maps = 0; }
  return hr;
}
HRESULT RuntimeGpu::release(Allocation& a) {
  if (a.native()) {
    if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
    if (a.nativeKey) {
      const HRESULT hr = nativeShare(a, 7);
      if (hr != S_OK) return hr;
      a.nativeKey = 0; a.nativeUncertain = false;
    }
    for (auto& binding : a.nativeOwners) {
      if (!binding.active) continue;
      // Remove an in-progress eviction from the transferable residency ledger.
      // Its callback may revoke this owner and leave another open of the same
      // handle alive; success must not evict their one reference twice.
      bool resident = std::exchange(binding.resident, false);
      const HRESULT hr = evict(binding.handle, resident);
      if (resident) {
        for (auto& survivor : a.nativeOwners) {
          if (survivor.active && survivor.handle == binding.handle) {
            survivor.resident = true;
            break;
          }
        }
      }
      if (hr != S_OK) return hr;
    }
    // It belongs to the surface owner, never this GPU allocation ledger.
    a.handle = 0; a.nativeOwners.clear();
    return S_OK;
  }
  HRESULT hr = unlock(a);
  if (FAILED(hr) || !a.handle) return hr;
  // A successful destroy removes any residency reference even if Evict fails.
  evict(a);
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  D3DDDICB_DEALLOCATE request = {}; request.NumAllocations = 1; request.HandleList = &a.handle;
  hr = exact(m_callbacks.pfnDeallocateCb(m_device, &request));
  if (!m_live) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK) { a.handle = 0; a.resident = false; }
  return hr;
}
int32_t MWD_CALL RuntimeGpu::release(void* ptr, void* token) {
  Call call(ptr); auto& self = *call.value;
  auto* a = self.find(token);
  if (!a || a->pending || !a->users) return E_INVALIDARG;
  if (self.m_closing && self.m_live) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (a->users > 1) { --a->users; return S_OK; }
  HRESULT hr = S_OK;
  { Pending pending(a->pending); if (call.live(true)) hr = self.release(*a); }
  if (hr == S_OK) self.m_allocations.erase(a->address);
  return hr;
}
int32_t MWD_CALL RuntimeGpu::map(void* ptr, void* token, void** out, uint32_t* handle) {
  if (!out || !handle) return E_POINTER;
  *out = nullptr; *handle = 0;
  Call call(ptr); auto& self = *call.value;
  if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
  auto* a = self.find(token);
  if (!a || a->pending || !a->handle || !(a->flags & 2) || a->maps == UINT32_MAX) return E_INVALIDARG;
  Pending pending(a->pending);
  HRESULT hr = self.identity();
  if (FAILED(hr)) return hr;
  // A malformed successful Lock whose balancing Unlock failed is still owned,
  // but its returned pointer must never become a successful nested mapping.
  if (a->locked && !a->mapValid) return E_FAIL;
  if (!a->locked) {
    D3DDDICB_LOCK request = {}; request.hAllocation = a->handle; request.Flags.LockEntire = 1;
    const HRESULT locked = self.m_callbacks.pfnLockCb(self.m_device, &request);
    if (!call.live()) return DXGI_ERROR_DEVICE_REMOVED;
    if (FAILED(locked)) return locked;
    a->locked = true;
    // A renamed allocation is the same owner, not a new allocation/token.
    if (request.hAllocation) a->handle = request.hAllocation;
    a->mapping = request.pData;
    hr = exact(locked);
    if (hr == S_OK && (!request.hAllocation || !request.pData)) hr = E_FAIL;
    if (hr == S_OK) hr = self.identity();
    if (FAILED(hr)) {
      const HRESULT cleanup = self.unlock(*a);
      return FAILED(cleanup) ? cleanup : hr;
    }
    a->mapValid = true;
  }
  ++a->maps; *out = a->mapping; *handle = a->handle;
  return S_OK;
}
int32_t MWD_CALL RuntimeGpu::unmap(void* ptr, void* token) {
  Call call(ptr); auto& self = *call.value;
  auto* a = self.find(token);
  if (!a || a->pending) return E_INVALIDARG;
  if (self.m_closing && self.m_live) return DXGI_ERROR_WAS_STILL_DRAWING;
  if (!call.live(true)) return S_OK;
  if (!a->maps) return E_INVALIDARG;
  if (a->maps > 1) { --a->maps; return S_OK; }
  Pending pending(a->pending);
  return self.unlock(*a);
}
int32_t MWD_CALL RuntimeGpu::submit(void* ptr, const void* stream, uint32_t size,
    const mwd_reference* refs, uint32_t count) {
  Call call(ptr); auto& self = *call.value;
  auto diagnostic = self.traceInfo(); diagnostic.references = count; diagnostic.streamBytes = size;
  self.m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::SubmitEntered,
    "submit", S_OK, diagnostic);
  auto failure = [&](const char* stage, HRESULT hr, uint32_t index = UINT32_MAX,
                     bool callback = false, HRESULT callbackHr = S_OK) {
    diagnostic.index = index;
    return self.traceFailure(stage, hr, diagnostic, callback, callbackHr);
  };
  if (!call.live()) return failure("submit-owner", DXGI_ERROR_DEVICE_REMOVED);
  if (!stream || size < 8 || size > 65536 || !refs || !count || count > 1024 || self.m_submitting)
    return failure("submit-arguments", E_INVALIDARG);
  uint32_t ownedCount = 0, importedCount = 0, ownedIndices[1024] = {};
  // find() is a linear scan over every registration, and Turnip lists every
  // live BO in every submit: resolving each reference (and each alias pair)
  // through it cost DWM ~3-4 ms of CPU per present. Index tokens once per
  // submit, and again after callbacks that may retire an open.
  std::unordered_map<void*, Allocation*> byToken;
  auto index = [&] {
    byToken.clear(); byToken.reserve(self.m_allocations.size());
    for (const auto& entry : self.m_allocations) byToken.emplace(entry.second->token, entry.second.get());
  };
  auto lookup = [&](void* token) -> Allocation* {
    const auto it = byToken.find(token);
    return it == byToken.end() ? nullptr : it->second;
  };
  index();
  // Validate all owners/ranges before modifying runtime-owned command buffers.
  for (uint32_t i = 0; i < count; ++i) {
    auto* a = lookup(refs[i].token);
    if (!a || a->pending || a->nativeUncertain || !a->handle || !refs[i].flags || (refs[i].flags & ~3u)
        || ((a->flags & 8u) && (refs[i].flags & 2u)) || refs[i].offset > UINT32_MAX
        || refs[i].offset > a->size || !refs[i].length || refs[i].length > a->size - refs[i].offset)
      return failure("submit-reference", E_INVALIDARG, i);
    if (a->native()) {
      if (!a->nativeKey || refs[i].offset || refs[i].length != a->size
          || refs[i].patch_offset != MWD_NATIVE_SURFACE_REFERENCE || ++importedCount > 256)
        return failure("submit-native-reference", E_INVALIDARG, i);
    } else {
      if (refs[i].patch_offset % 4 || refs[i].patch_offset > size - 8)
        return failure("submit-reference-patch", E_INVALIDARG, i);
      ownedIndices[ownedCount++] = i;
    }
    if (self.m_diagnostics.enabled() && a->locked) ++diagnostic.lockedReferences;
    for (uint32_t j = 0; j < i; ++j) {
      const uint64_t x = refs[i].patch_offset, y = refs[j].patch_offset;
      if (refs[i].token == refs[j].token || a->handle == lookup(refs[j].token)->handle
          || (x != MWD_NATIVE_SURFACE_REFERENCE && y != MWD_NATIVE_SURFACE_REFERENCE
              && x < y + 8 && y < x + 8))
        return failure("submit-alias", E_INVALIDARG, i);
    }
  }
  const uint64_t importOffset = sizeof(Render) + uint64_t(ownedCount) * sizeof(Reference);
  const uint64_t offset = importOffset + uint64_t(importedCount) * sizeof(ImportedReference);
  const uint64_t bytes = offset + size;
  if (!ownedCount || bytes > self.m_commandSize || count > self.m_allocationCount
      || ownedCount > self.m_patchCount || !self.m_commands || !self.m_allocationList || !self.m_patchList)
    return failure("submit-buffers", E_INVALIDARG);
  struct References {
    Allocation* values[1024] = {}; uint32_t count = 0;
    ~References() { for (uint32_t i = 0; i < count; ++i) values[i]->pending = false; }
  } owners;
  for (uint32_t i = 0; i < count; ++i) {
    auto* a = lookup(refs[i].token); a->pending = true; owners.values[owners.count++] = a;
  }
  Pending pending(self.m_submitting);
  HRESULT hr = self.identity();
  if (FAILED(hr)) return failure("submit-identity", hr);
  if (self.m_pagingFence) {
    hr = self.waitPaging();
    if (FAILED(hr)) return failure("submit-paging", hr);
    hr = self.identity();
    if (FAILED(hr)) return failure("submit-after-paging", hr);
  }
  // Identity and paging callbacks can synchronously destroy an open. Select
  // the remaining registration only after those callbacks have returned.
  index();
  for (uint32_t i = 0; i < count; ++i) {
    auto* a = lookup(refs[i].token);
    if (a->native()) a->selectNativeHandle();
    if (!a->handle) return failure("submit-retired-handle", DXGI_ERROR_DEVICE_REMOVED, i);
    for (uint32_t j = 0; j < i; ++j)
      if (a->handle == lookup(refs[j].token)->handle)
        return failure("submit-retired-alias", E_INVALIDARG, i);
  }
  Render header;
  header.header.size = uint32_t(bytes); header.generation = self.m_info.generation;
  header.count = ownedCount; header.stream = uint32_t(offset); header.size = size;
  if (importedCount) {
    header.flags = 1; header.reserved[0] = uint32_t(importOffset);
    header.reserved[1] = importedCount; header.reserved[2] = 2;
  }
  auto* packet = static_cast<uint8_t*>(self.m_commands);
  std::memcpy(packet, &header, sizeof(header)); std::memcpy(packet + offset, stream, size);
  uint32_t ownedIndex = 0, importedIndex = 0;
  for (uint32_t i = 0; i < count; ++i) {
    auto* a = lookup(refs[i].token);
    const uint32_t index = a->native() ? ownedCount + importedIndex : ownedIndex;
    auto& allocation = self.m_allocationList[index]; allocation = {};
    allocation.hAllocation = a->handle; allocation.WriteOperation = (refs[i].flags & 2) != 0;
    if (a->native()) {
      const ImportedReference wire = {a->nativeKey, a->address, a->size, self.m_info.generation,
        refs[i].flags, index + 1};
      std::memcpy(packet + importOffset + importedIndex++ * sizeof(wire), &wire, sizeof(wire));
    } else {
      const Reference wire = {index, refs[i].flags, refs[i].offset, refs[i].length, refs[i].patch_offset, 0};
      std::memcpy(packet + sizeof(Render) + ownedIndex++ * sizeof(wire), &wire, sizeof(wire));
      auto& patch = self.m_patchList[index]; patch = {};
      patch.AllocationIndex = index; patch.AllocationOffset = UINT(refs[i].offset); patch.PatchOffset = UINT(offset + refs[i].patch_offset);
    }
  }
  D3DDDICB_RENDER request = {}; request.hContext = self.m_context; request.CommandLength = UINT(bytes);
  request.NumAllocations = count; request.NumPatchLocations = ownedCount;
  request.pNewCommandBuffer = self.m_commands; request.NewCommandBufferSize = self.m_commandSize;
  request.pNewAllocationList = self.m_allocationList; request.NewAllocationListSize = self.m_allocationCount;
  request.pNewPatchLocationList = self.m_patchList; request.NewPatchLocationListSize = self.m_patchCount;
  bool renderSnapshot = false;
  if (self.m_diagnostics.wantsRenderSnapshot()) {
    RuntimeGpuRenderReferenceDiagnostic values[RuntimeGpuDiagnostics::RenderReferenceLimit] = {};
    // Signed native MSM layout: request36, BO16, Presumed offset8. Identify only
    // that bounded layout for the BO field; a supplied patch slot is separate.
    uint32_t native[9] = {};
    if (size >= sizeof(native)) std::memcpy(native, packet + offset, sizeof(native));
    const bool nativeLayout = size >= sizeof(native) && native[0] == 7 && native[1] == size
      && native[6] == ownedCount && sizeof(native) + uint64_t(ownedCount) * 16 + uint64_t(native[7]) * 32 == size;
    const uint32_t captured = ownedCount < RuntimeGpuDiagnostics::RenderReferenceLimit
      ? ownedCount : RuntimeGpuDiagnostics::RenderReferenceLimit;
    for (uint32_t i = 0; i < captured; ++i) {
      auto& value = values[i];
      const auto& allocation = self.m_allocationList[i];
      const auto& patch = self.m_patchList[i];
      value.allocationHandle = allocation.hAllocation; value.writeOperation = allocation.WriteOperation;
      value.allocationIndex = patch.AllocationIndex; value.allocationOffset = patch.AllocationOffset;
      value.patchOffset = patch.PatchOffset; value.relativePatchOffset = refs[ownedIndices[i]].patch_offset;
      std::memcpy(&value.slot, packet + value.patchOffset, sizeof(value.slot));
      const uint32_t presumedOffset = 44 + i * 16;
      if (nativeLayout && presumedOffset <= size - sizeof(value.boPresumed)) {
        value.boPresumedAvailable = true; value.boPresumedOffset = presumedOffset;
        std::memcpy(&value.boPresumed, packet + offset + presumedOffset, sizeof(value.boPresumed));
      }
    }
    renderSnapshot = self.m_diagnostics.recordRenderBefore(stderr, diagnostic, values, ownedCount);
  }
  const HRESULT callbackHr = self.m_callbacks.pfnRenderCb(self.m_device, &request);
  hr = exact(callbackHr);
  // Old command/list buffers may no longer be valid. Only the original callback
  // result enters this after record; all metadata above is pre-callback input.
  if (renderSnapshot) self.m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::RenderAfter,
    "render-callback", hr, diagnostic, true, callbackHr);
  // Record the original callback result before cleanup or replacement-buffer
  // validation can replace the final failure. Positive non-S_OK stays visible.
  failure("render-callback", hr, UINT32_MAX, true, callbackHr);
  if (!call.live()) return failure("submit-after-callback-owner", DXGI_ERROR_DEVICE_REMOVED);
  // Callback replacement buffers become authoritative on failure too.
  self.m_commands = request.pNewCommandBuffer; self.m_commandSize = request.NewCommandBufferSize;
  self.m_allocationList = request.pNewAllocationList; self.m_allocationCount = request.NewAllocationListSize;
  self.m_patchList = request.pNewPatchLocationList; self.m_patchCount = request.NewPatchLocationListSize;
  if (!self.m_commands || self.m_commandSize < sizeof(Render) || self.m_commandSize > 65536
      || !self.m_allocationList || !self.m_allocationCount || self.m_allocationCount > 1024
      || !self.m_patchList || !self.m_patchCount || self.m_patchCount > 1024) {
    self.m_removed = true;
    return failure("submit-replacements", E_FAIL, UINT32_MAX, true, callbackHr);
  }
  if (FAILED(hr)) return hr;
  hr = self.identity();
  if (FAILED(hr)) return failure("submit-after-callback-identity", hr);
  if (hr == S_OK) self.m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::SubmitSucceeded,
    "submit", hr, diagnostic, true, callbackHr);
  if (hr == S_OK && self.m_service) self.m_service->requestAmortizedProcessing();
  return hr;
}
int32_t MWD_CALL RuntimeGpu::completed(void* ptr, uint32_t* out) {
  if (!out) return E_POINTER;
  *out = 0;
  Call call(ptr); auto& self = *call.value;
  if (!call.live(true) || !self.m_info.context_id || !self.m_identity->available()) return DXGI_ERROR_DEVICE_REMOVED;
  Fence info; info.expected = self.m_info.generation;
  D3DDDICB_ESCAPE request = {}; request.hDevice = self.m_device; request.hContext = self.m_context;
  request.pPrivateDriverData = &info; request.PrivateDriverDataSize = sizeof(info);
  HRESULT hr = exact(self.m_callbacks.pfnEscapeCb(self.m_identity->runtime, &request));
  if (!call.live(true) || !self.m_identity->available()) return DXGI_ERROR_DEVICE_REMOVED;
  if (hr == S_OK && (!info.header.valid(sizeof(info)) || info.opcode != 2 || info.flags || info.reserved
      || info.expected != self.m_info.generation || info.generation != info.expected
      || info.context != self.m_info.context_id || info.completed > UINT32_MAX)) hr = E_FAIL;
  if (hr == S_OK) hr = self.identity();
  if (hr == S_OK) *out = uint32_t(info.completed);
  return hr;
}
int32_t MWD_CALL RuntimeGpu::status(void* ptr) {
  Call call(ptr);
  const HRESULT hr = call.live(true) ? call.value->identity() : DXGI_ERROR_DEVICE_REMOVED;
  return call.value->traceFailure("status", hr);
}

HRESULT RuntimeGpu::close() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  if (!m_live || m_closing) return S_OK;
  m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::CloseEntered,
    "close", S_OK, traceCloseInfo());
  m_closing = true;
  HRESULT result = S_OK;
  // A runtime callback can retire the device synchronously. In that case final
  // kernel teardown owns residual allocations; do not invoke a second callback
  // or free mapped backing underneath the interrupted backend operation.
  if (!m_active) {
    for (auto& entry : m_allocations) {
      Pending pending(entry.second->pending);
      const HRESULT hr = release(*entry.second);
      if (FAILED(hr) && SUCCEEDED(result)) result = hr;
    }
    m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::AllocationCleanupFinished,
      "close-allocations", result, traceCloseInfo());
    if (m_context) {
      D3DDDICB_DESTROYCONTEXT request = {}; request.hContext = m_context;
      const auto diagnostic = traceCloseInfo();
      m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::DestroyContextEntered,
        "close-context", S_OK, diagnostic);
      const HRESULT callbackHr = m_callbacks.pfnDestroyContextCb(m_device, &request);
      const HRESULT hr = exact(callbackHr);
      m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::DestroyContextFinished,
        "close-context", hr, diagnostic, true, callbackHr);
      if (FAILED(hr) && SUCCEEDED(result)) result = hr;
      if (hr == S_OK) {
        m_context = nullptr;
        // Confirmed context destruction retires even an uncertain import or
        // a RELEASE refused while commands were in flight. Drop only our
        // residency reference; the surface owner still owns its VidMm handle.
        for (auto& entry : m_allocations) {
          auto& a = *entry.second;
          if (!a.native()) continue;
          Pending pending(a.pending);
          a.nativeKey = 0; a.nativeUncertain = false;
          const HRESULT cleanup = release(a);
          if (FAILED(cleanup) && SUCCEEDED(result)) result = cleanup;
        }
      }
    }
    if (m_pagingQueue) {
      D3DDDI_DESTROYPAGINGQUEUE request = {}; request.hPagingQueue = m_pagingQueue;
      const HRESULT hr = exact(m_callbacks.pfnDestroyPagingQueueCb(m_device, &request));
      if (FAILED(hr) && SUCCEEDED(result)) result = hr;
    }
  }
  const auto diagnostic = traceCloseInfo();
  m_live = false; m_context = nullptr; m_device = nullptr; m_callbacks = {};
  m_info = {}; m_commands = nullptr; m_allocationList = nullptr; m_patchList = nullptr;
  m_commandSize = m_allocationCount = m_patchCount = 0;
  m_pagingQueue = m_pagingSync = 0; m_pagingFence = 0;
  m_pagingCpuFence = nullptr; m_pagingWaitStatus = WAIT_FAILED;
  m_diagnostics.record(stderr, RuntimeGpuDiagnostics::Event::CloseFinished,
    "close", result, diagnostic);
  return result;
}
}
