#pragma once

#include "umd-kmt-compute-transport.h"

// Test-only bridge from real runtime residency callbacks to the same KMT
// device/context. No CPU paging wait and no implicit Render-time MakeResident.
class KmtRuntimePaging final {
public:
  explicit KmtRuntimePaging(KmtComputeTransport& transport) : m_transport(transport) { }
  ~KmtRuntimePaging() {
    close();
    if (s_owner == this) s_owner = nullptr;
  }
  HRESULT close() {
    if (!m_queue) return S_OK;
    // Successful DestroyDevice implicitly retires its queues. Never retry an
    // expired queue handle after the parent has closed the raw KMT device.
    if (!m_transport.m_device) { m_queue = m_sync = 0; m_cpu = nullptr; return S_OK; }
    D3DDDI_DESTROYPAGINGQUEUE request = {}; request.hPagingQueue = m_queue;
    const auto status = D3DKMTDestroyPagingQueue(&request);
    std::printf("NATIVE_PAGING_FALLBACK_DESTROY status=%08lx\n", (unsigned long)status);
    if (status == 0) { m_queue = m_sync = 0; m_cpu = nullptr; }
    // Fallback cleanup deliberately does not satisfy the balanced oracle.
    return KmtComputeTransport::result(status);
  }
  HRESULT callbacks(D3DDDI_DEVICECALLBACKS& cb) {
    if (s_owner || !m_transport.m_device) return E_FAIL;
    s_owner = this; m_transport.m_runtimeResidency = true;
    cb.pfnCreatePagingQueueCb = create;
    cb.pfnDestroyPagingQueueCb = destroy;
    cb.pfnMakeResidentCb = resident;
    cb.pfnEvictCb = evict;
    cb.pfnWaitForSynchronizationObjectFromGpuCb = waitGpu;
    cb.pfnWaitForSynchronizationObjectFromCpuCb = nullptr;
    cb.pfnEscapeCb = escape;
    return S_OK;
  }
  bool balanced() const {
    return m_creates == 1 && m_destroys == 1 && !m_queue && m_pendingImports
      && m_pendingNativeWaits && !m_transport.m_runtimeUnqueuedPaging;
  }
  void print() const {
    std::printf("NATIVE_RUNTIME_PAGING queues=%u/%u pending_imports=%u gpu_waits=%u"
      " pending_native_gpu_waits=%u cpu_waits=0 render_residency=0 unqueued=%llu balanced=%u\n",
      m_creates, m_destroys, m_pendingImports, m_waits,
      m_pendingNativeWaits, (unsigned long long)m_transport.m_runtimeUnqueuedPaging, unsigned(balanced()));
  }
private:
  inline static KmtRuntimePaging* s_owner = nullptr;
  KmtComputeTransport& m_transport;
  D3DKMT_HANDLE m_queue = 0, m_sync = 0;
  const volatile UINT64* m_cpu = nullptr;
  unsigned m_creates = 0, m_destroys = 0, m_pendingImports = 0, m_waits = 0;
  unsigned m_pendingNativeWaits = 0;
  UINT64 m_nativeTarget = 0;
  static KmtRuntimePaging* owner(HANDLE cookie, bool adapter = false) {
    auto s = s_owner;
    return s && KmtComputeTransport::owner(cookie, adapter) == &s->m_transport ? s : nullptr;
  }
  UINT64 observed() const { return m_cpu ? *m_cpu : 0; }
  static HRESULT APIENTRY create(HANDLE cookie, D3DDDICB_CREATEPAGINGQUEUE* args) {
    auto s = owner(cookie);
    if (!s || !args || s->m_queue || args->Priority != D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL
        || args->PhysicalAdapterIndex) return E_INVALIDARG;
    D3DKMT_CREATEPAGINGQUEUE request = {}; request.hDevice = s->m_transport.m_device;
    request.Priority = args->Priority;
    const NTSTATUS status = D3DKMTCreatePagingQueue(&request);
    s->m_queue = args->hPagingQueue = request.hPagingQueue;
    s->m_sync = args->hSyncObject = request.hSyncObject;
    args->FenceValueCPUVirtualAddress = request.FenceValueCPUVirtualAddress;
    s->m_cpu = static_cast<const volatile UINT64*>(request.FenceValueCPUVirtualAddress);
    std::printf("NATIVE_PAGING_CREATE status=%08lx queue=%u sync=%u cpu=%p tick=%llu\n",
      (unsigned long)status, s->m_queue, s->m_sync, request.FenceValueCPUVirtualAddress,
      (unsigned long long)GetTickCount64());
    if (status != 0) return KmtComputeTransport::result(status);
    ++s->m_creates;
    return s->m_queue && s->m_sync && s->m_cpu ? S_OK : E_FAIL;
  }
  static HRESULT APIENTRY destroy(HANDLE cookie, const D3DDDI_DESTROYPAGINGQUEUE* args) {
    auto s = owner(cookie);
    if (!s || !args || !s->m_queue || args->hPagingQueue != s->m_queue) return E_INVALIDARG;
    D3DDDI_DESTROYPAGINGQUEUE request = *args;
    const auto completed = s->observed();
    const NTSTATUS status = D3DKMTDestroyPagingQueue(&request);
    std::printf("NATIVE_PAGING_DESTROY status=%08lx observed=%llu tick=%llu\n",
      (unsigned long)status, (unsigned long long)completed, (unsigned long long)GetTickCount64());
    if (status == 0) { s->m_queue = s->m_sync = 0; s->m_cpu = nullptr; ++s->m_destroys; }
    return KmtComputeTransport::result(status);
  }
  static HRESULT APIENTRY resident(HANDLE cookie, D3DDDI_MAKERESIDENT* args) {
    auto s = owner(cookie);
    if (!s || !args || !s->m_queue || args->hPagingQueue != s->m_queue
        || args->NumAllocations != 1 || !args->AllocationList || args->PriorityList
        || args->Flags.MustSucceed) return E_INVALIDARG;
    auto& t = s->m_transport;
    const auto handle = args->AllocationList[0];
    if ((!t.allocation(handle) && !t.nativeAllocation(handle))
        || std::find(t.m_resident.begin(), t.m_resident.end(), handle) != t.m_resident.end()) return E_INVALIDARG;
    try { t.m_resident.reserve(t.m_resident.size() + 1); }
    catch (...) { return E_OUTOFMEMORY; }
    const auto before = GetTickCount64();
    const NTSTATUS status = D3DKMTMakeResident(args);
    std::printf("NATIVE_PAGING_RESIDENT status=%08lx allocation=%u fence=%llu observed=%llu"
      " tick=%llu duration_ms=%llu\n", (unsigned long)status, handle,
      (unsigned long long)args->PagingFenceValue, (unsigned long long)s->observed(),
      (unsigned long long)before, (unsigned long long)(GetTickCount64() - before));
    if (status != 0 && status != 0x103) return KmtComputeTransport::result(status);
    t.m_resident.push_back(handle); ++t.counts.residents;
    if (args->NumAllocations != 1 || (status == 0x103 && !args->PagingFenceValue)) {
      ++t.counts.malformedOutputs; return E_FAIL;
    }
    if (status == 0x103) {
      t.m_runtimeUnqueuedPaging = (std::max)(t.m_runtimeUnqueuedPaging, args->PagingFenceValue);
      if (t.nativeAllocation(handle)) s->m_nativeTarget = (std::max)(s->m_nativeTarget, args->PagingFenceValue);
    }
    return status == 0x103 ? E_PENDING : S_OK;
  }
  static HRESULT APIENTRY evict(HANDLE cookie, D3DDDICB_EVICT* args) {
    auto s = owner(cookie);
    if (!s || !args || args->NumAllocations != 1 || !args->AllocationList || args->Flags.Value) return E_INVALIDARG;
    auto& t = s->m_transport;
    const auto entry = std::find(t.m_resident.begin(), t.m_resident.end(), args->AllocationList[0]);
    if (entry == t.m_resident.end()) return E_INVALIDARG;
    D3DKMT_EVICT request = {}; request.hDevice = t.m_device;
    request.NumAllocations = args->NumAllocations; request.AllocationList = args->AllocationList;
    request.Flags = args->Flags;
    const NTSTATUS status = D3DKMTEvict(&request); args->NumBytesToTrim = request.NumBytesToTrim;
    std::printf("NATIVE_PAGING_EVICT status=%08lx allocation=%u tick=%llu\n",
      (unsigned long)status, args->AllocationList[0], (unsigned long long)GetTickCount64());
    if (status == 0) { t.m_resident.erase(entry); ++t.counts.evictions; }
    return KmtComputeTransport::result(status);
  }
  static HRESULT APIENTRY waitGpu(HANDLE cookie, const D3DDDICB_WAITFORSYNCHRONIZATIONOBJECTFROMGPU* args) {
    auto s = owner(cookie);
    if (!s || !args || args->hContext != &s->m_transport.m_contextCookie
        || !s->m_transport.m_context || args->ObjectCount != 1 || !args->ObjectHandleArray
        || args->ObjectHandleArray[0] != s->m_sync || !args->MonitoredFenceValueArray
        || args->MonitoredFenceValueArray[0] != s->m_transport.m_runtimeUnqueuedPaging) return E_INVALIDARG;
    for (unsigned i = 1; i < 8; ++i) if (args->Reserved[i]) return E_INVALIDARG;
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU request = {};
    request.hContext = s->m_transport.m_context; request.ObjectCount = args->ObjectCount;
    request.ObjectHandleArray = args->ObjectHandleArray; request.MonitoredFenceValueArray = args->MonitoredFenceValueArray;
    const auto before = GetTickCount64();
    const auto observedBefore = s->observed();
    const bool nativePending = s->m_cpu && s->m_nativeTarget && observedBefore < s->m_nativeTarget;
    const NTSTATUS status = D3DKMTWaitForSynchronizationObjectFromGpu(&request);
    std::printf("NATIVE_PAGING_GPU_WAIT status=%08lx context=%u fence=%llu observed_before=%llu observed_after=%llu"
      " native_target=%llu native_pending=%u tick=%llu duration_ms=%llu\n", (unsigned long)status, request.hContext,
      (unsigned long long)args->MonitoredFenceValueArray[0], (unsigned long long)observedBefore,
      (unsigned long long)s->observed(), (unsigned long long)s->m_nativeTarget, unsigned(nativePending),
      (unsigned long long)before, (unsigned long long)(GetTickCount64() - before));
    if (status == 0) { ++s->m_waits; s->m_pendingNativeWaits += nativePending; s->m_transport.m_runtimeUnqueuedPaging = 0; }
    return KmtComputeTransport::result(status);
  }
  static HRESULT APIENTRY escape(HANDLE cookie, const D3DDDICB_ESCAPE* args) {
    auto s = owner(cookie, true);
    if (!s || !args) return E_INVALIDARG;
    uint32_t header[5] = {};
    const bool native = args->pPrivateDriverData && args->PrivateDriverDataSize == 88;
    if (native) std::memcpy(header, args->pPrivateDriverData, sizeof(header));
    if (native && header[4] == 6) {
      const auto target = s->m_transport.m_runtimeUnqueuedPaging;
      const auto value = s->observed();
      const bool pending = s->m_cpu && target && value < target;
      s->m_pendingImports += pending;
      std::printf("NATIVE_PAGING_IMPORT target=%llu observed=%llu pending=%u tick=%llu\n",
        (unsigned long long)target, (unsigned long long)value, unsigned(pending),
        (unsigned long long)GetTickCount64());
    }
    return KmtComputeTransport::escape(cookie, args);
  }
};
