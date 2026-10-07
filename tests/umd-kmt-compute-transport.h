#pragma once

#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_runtime_identity.h"
#include <d3dkmthk.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

// Test-only owner of a real KMT device. Microsoft D3D runtime activation and
// presentation are separate gates. No raw KMT handle is used as a DDI cookie.
class KmtComputeTransport {
public:
  struct Counts {
    unsigned queries = 0, contexts = 0, contextCloses = 0;
    unsigned allocations = 0, deallocations = 0, locks = 0, unlocks = 0;
    unsigned renders = 0, escapes = 0, residents = 0, evictions = 0;
    std::atomic<unsigned> wrongThreads{0}, badCookies{0};
    unsigned coreErrors = 0, malformedOutputs = 0;
  } counts;

  KmtComputeTransport() {
    KmtComputeTransport* empty = nullptr;
    s_owner.compare_exchange_strong(empty, this);
  }
  ~KmtComputeTransport() { close(); if (s_owner == this) s_owner = nullptr; }
  KmtComputeTransport(const KmtComputeTransport&) = delete;
  KmtComputeTransport& operator=(const KmtComputeTransport&) = delete;

  HRESULT open(const LUID& luid) {
    if (s_owner != this || m_adapter || (!luid.LowPart && !luid.HighPart)) return E_INVALIDARG;
    D3DKMT_OPENADAPTERFROMLUID opened = {}; opened.AdapterLuid = luid;
    HRESULT hr = result(D3DKMTOpenAdapterFromLuid(&opened));
    m_adapter = opened.hAdapter;
    if (hr != S_OK) return hr;
    m_luid = luid;
    if (!m_adapter) return E_FAIL;
    D3DKMT_ADAPTERTYPE type = {};
    hr = adapterInfo(KMTQAITYPE_ADAPTERTYPE, &type, sizeof(type));
    if (hr != S_OK) return hr;
    if (type.SoftwareDevice || !type.RenderSupported) return DXGI_ERROR_UNSUPPORTED;
    std::array<unsigned char, dxvk::umd::RuntimeIdentityReplySize> reply{};
    hr = adapterInfo(KMTQAITYPE_UMDRIVERPRIVATE, reply.data(), UINT(reply.size()));
    if (hr != S_OK) return hr;
    if (!dxvk::umd::readRuntimeIdentity(reply.data(), reply.size(), m_identity)
        || std::memcmp(m_identity.luid.data(), &luid, sizeof(luid))) return DXGI_ERROR_UNSUPPORTED;
    UINT version = 0;
    hr = adapterInfo(KMTQAITYPE_DRIVERVERSION, &version, sizeof(version));
    if (hr != S_OK || version < KMT_DRIVERVERSION_WDDM_2_0)
      return hr != S_OK ? hr : DXGI_ERROR_UNSUPPORTED;
    D3DKMT_CREATEDEVICE created = {}; created.hAdapter = m_adapter;
    hr = result(D3DKMTCreateDevice(&created));
    m_device = created.hDevice;
    if (hr != S_OK) return hr;
    if (!m_device) return E_FAIL;
    D3DKMT_CREATEPAGINGQUEUE paging = {};
    paging.hDevice = m_device; paging.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
    hr = result(D3DKMTCreatePagingQueue(&paging));
    m_pagingQueue = paging.hPagingQueue; m_pagingSync = paging.hSyncObject;
    std::printf("DX11_KMT_SELECTED generation=%llu capabilities=%llu version=%u software=0\n",
      static_cast<unsigned long long>(m_identity.generation),
      static_cast<unsigned long long>(m_identity.capabilities), version);
    return hr != S_OK ? hr : m_pagingQueue && m_pagingSync ? S_OK : E_FAIL;
  }
  D3D10DDI_HRTADAPTER runtimeAdapter() { return {&m_adapterCookie}; }
  D3D10DDI_HRTDEVICE runtimeDevice() { return {&m_deviceCookie}; }
  D3D10DDI_HRTCORELAYER runtimeCore() { return {&m_coreCookie}; }
  void callbacks(D3DDDI_ADAPTERCALLBACKS& adapter, D3DDDI_DEVICECALLBACKS& kernel,
      D3D11DDI_CORELAYER_DEVICECALLBACKS& core) {
    fillKernelCallbacks(adapter, kernel);
    core = {}; core.pfnSetErrorCb = error;
  }
  // The legacy graphics probe owns its original live core table too. The
  // SDK defines this SetError callback independently of device table ABI.
  void callbacks(D3DDDI_ADAPTERCALLBACKS& adapter, D3DDDI_DEVICECALLBACKS& kernel,
      D3D10DDI_CORELAYER_DEVICECALLBACKS& core) {
    fillKernelCallbacks(adapter, kernel);
    core = {}; core.pfnSetErrorCb = error;
  }
  void beginDdi() { m_error = S_OK; }
  HRESULT lastError() const { return m_error; }
  void printCounts() const {
    std::printf("DX11_KMT_COUNTS queries=%u contexts=%u/%u allocations=%u/%u locks=%u/%u renders=%u escapes=%u residency=%u/%u wrong_threads=%u bad_cookies=%u core_errors=%u malformed_outputs=%u remaining_allocations=%zu remaining_residents=%zu pending_paging=%llu\n",
      counts.queries, counts.contexts, counts.contextCloses, counts.allocations, counts.deallocations,
      counts.locks, counts.unlocks, counts.renders, counts.escapes, counts.residents, counts.evictions,
      counts.wrongThreads.load(), counts.badCookies.load(), counts.coreErrors, counts.malformedOutputs,
      m_allocations.size(), m_resident.size(), static_cast<unsigned long long>(m_pendingPaging));
  }
  bool balanced() const {
    return counts.queries && counts.contexts == 1 && counts.contextCloses == counts.contexts
      && counts.allocations && counts.allocations == counts.deallocations
      && counts.locks && counts.locks == counts.unlocks && counts.renders && counts.escapes >= 2
      && counts.residents && counts.residents == counts.evictions
      && !counts.wrongThreads && !counts.badCookies && !counts.coreErrors && !counts.malformedOutputs
      && !m_context && !m_pendingPaging && m_allocations.empty() && m_resident.empty();
  }
  HRESULT close() {
    HRESULT hr = S_OK;
    auto keep = [&](HRESULT status) { if (hr == S_OK && status != S_OK) hr = status; };
    // A failed probe still releases its raw kernel device. These cleanup calls
    // do not turn residual UMD ownership into a successful balance oracle.
    if (m_context) {
      D3DKMT_DESTROYCONTEXT request = {}; request.hContext = m_context;
      const HRESULT released = result(D3DKMTDestroyContext(&request)); keep(released);
      if (released == S_OK) m_context = 0;
    }
    for (size_t i = 0; i < m_allocations.size();) {
      auto& allocation = m_allocations[i];
      if (allocation.locked) {
        D3DKMT_UNLOCK request = {}; request.hDevice = m_device;
        request.NumAllocations = 1; request.phAllocations = &allocation.handle;
        const HRESULT released = result(D3DKMTUnlock(&request)); keep(released);
        if (released == S_OK) allocation.locked = false;
      }
      keep(evict(allocation.handle));
      D3DKMT_DESTROYALLOCATION request = {}; request.hDevice = m_device;
      request.AllocationCount = 1; request.phAllocationList = &allocation.handle;
      const HRESULT released = result(D3DKMTDestroyAllocation(&request)); keep(released);
      if (released == S_OK) m_allocations.erase(m_allocations.begin() + i);
      else ++i;
    }
    if (m_pagingQueue) {
      D3DDDI_DESTROYPAGINGQUEUE request = {}; request.hPagingQueue = m_pagingQueue;
      const HRESULT released = result(D3DKMTDestroyPagingQueue(&request)); keep(released);
      if (released == S_OK) m_pagingQueue = m_pagingSync = 0;
    }
    if (m_device) {
      D3DKMT_DESTROYDEVICE request = {}; request.hDevice = m_device;
      const HRESULT released = result(D3DKMTDestroyDevice(&request)); keep(released);
      if (released == S_OK) {
        m_device = m_context = m_pagingQueue = m_pagingSync = 0;
        m_pendingPaging = 0; m_allocations.clear(); m_resident.clear();
      }
    }
    if (m_adapter && !m_device) {
      D3DKMT_CLOSEADAPTER request = {}; request.hAdapter = m_adapter;
      const HRESULT released = result(D3DKMTCloseAdapter(&request)); keep(released);
      if (released == S_OK) m_adapter = 0;
    }
    return hr;
  }

private:
  struct Allocation { D3DKMT_HANDLE handle; bool locked; };
  inline static std::atomic<KmtComputeTransport*> s_owner{nullptr};
  char m_adapterCookie = 0, m_deviceCookie = 0, m_contextCookie = 0, m_coreCookie = 0;
  DWORD m_thread = GetCurrentThreadId();
  D3DKMT_HANDLE m_adapter = 0, m_device = 0, m_context = 0;
  D3DKMT_HANDLE m_pagingQueue = 0, m_pagingSync = 0;
  UINT64 m_pendingPaging = 0;
  LUID m_luid = {};
  dxvk::umd::RuntimeIdentity m_identity;
  std::vector<Allocation> m_allocations;
  std::vector<D3DKMT_HANDLE> m_resident;
  HRESULT m_error = S_OK;

  static void fillKernelCallbacks(D3DDDI_ADAPTERCALLBACKS& adapter, D3DDDI_DEVICECALLBACKS& kernel) {
    adapter = {}; adapter.pfnQueryAdapterInfoCb = query;
    kernel = {}; kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
    kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
    kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
    kernel.pfnEscapeCb = escape; kernel.pfnRenderCb = render;
  }

  static HRESULT result(NTSTATUS status) {
    return status == 0 ? S_OK : status < 0 ? HRESULT_FROM_NT(status) : E_FAIL;
  }
  static KmtComputeTransport* owner(HANDLE cookie, bool adapter) {
    auto s = s_owner.load();
    if (!s) return nullptr;
    if (cookie != (adapter ? &s->m_adapterCookie : &s->m_deviceCookie)) {
      ++s->counts.badCookies; return nullptr;
    }
    if (GetCurrentThreadId() != s->m_thread) { ++s->counts.wrongThreads; return nullptr; }
    return s;
  }
  static void APIENTRY error(D3D10DDI_HRTCORELAYER core, HRESULT hr) {
    auto s = s_owner.load();
    if (!s) return;
    if (core.handle != &s->m_coreCookie) { ++s->counts.badCookies; return; }
    if (GetCurrentThreadId() != s->m_thread) { ++s->counts.wrongThreads; return; }
    s->m_error = hr; ++s->counts.coreErrors;
    std::printf("DX11_CORE_ERROR hr=%08lx\n", static_cast<unsigned long>(hr));
  }
  HRESULT adapterInfo(KMTQUERYADAPTERINFOTYPE type, void* data, UINT size) {
    D3DKMT_QUERYADAPTERINFO request = {}; request.hAdapter = m_adapter;
    request.Type = type; request.pPrivateDriverData = data; request.PrivateDriverDataSize = size;
    return result(D3DKMTQueryAdapterInfo(&request));
  }
  static HRESULT APIENTRY query(HANDLE cookie, const D3DDDICB_QUERYADAPTERINFO* args) {
    auto s = owner(cookie, true);
    if (!s || !args || !args->pPrivateDriverData
        || args->PrivateDriverDataSize != dxvk::umd::RuntimeIdentityReplySize) return E_INVALIDARG;
    ++s->counts.queries;
    const HRESULT hr = s->adapterInfo(KMTQAITYPE_UMDRIVERPRIVATE, args->pPrivateDriverData, args->PrivateDriverDataSize);
    if (hr != S_OK) return hr;
    dxvk::umd::RuntimeIdentity observed;
    if (!dxvk::umd::readRuntimeIdentity(args->pPrivateDriverData, args->PrivateDriverDataSize, observed)
        || observed.luid != s->m_identity.luid || observed.generation != s->m_identity.generation
        || observed.capabilities != s->m_identity.capabilities) return DXGI_ERROR_DEVICE_REMOVED;
    return S_OK;
  }
  static HRESULT APIENTRY createContext(HANDLE cookie, D3DDDICB_CREATECONTEXT* args) {
    auto s = owner(cookie, false);
    if (!s || !s->m_device || s->m_context || !args || !args->pPrivateDriverData
        || args->PrivateDriverDataSize != 32 || args->NodeOrdinal || args->EngineAffinity != 1
        || args->Flags.Value) return E_INVALIDARG;
    D3DKMT_CREATECONTEXT request = {}; request.hDevice = s->m_device;
    request.NodeOrdinal = args->NodeOrdinal; request.EngineAffinity = args->EngineAffinity;
    request.Flags = args->Flags; request.pPrivateDriverData = args->pPrivateDriverData;
    request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    const HRESULT hr = result(D3DKMTCreateContext(&request));
    std::printf("DX11_KMT_CREATE_CONTEXT hr=%08lx context=%u\n", static_cast<unsigned long>(hr), request.hContext);
    s->m_context = request.hContext;
    if (hr == S_OK) {
      if (!s->m_context) return E_FAIL;
      args->hContext = &s->m_contextCookie;
      args->pCommandBuffer = request.pCommandBuffer; args->CommandBufferSize = request.CommandBufferSize;
      args->pAllocationList = request.pAllocationList; args->AllocationListSize = request.AllocationListSize;
      args->pPatchLocationList = request.pPatchLocationList; args->PatchLocationListSize = request.PatchLocationListSize;
      ++s->counts.contexts;
    }
    return hr;
  }
  static HRESULT APIENTRY destroyContext(HANDLE cookie, const D3DDDICB_DESTROYCONTEXT* args) {
    auto s = owner(cookie, false);
    if (!s || !args || !s->m_context || args->hContext != &s->m_contextCookie) return E_INVALIDARG;
    D3DKMT_DESTROYCONTEXT request = {}; request.hContext = s->m_context;
    const HRESULT hr = result(D3DKMTDestroyContext(&request));
    std::printf("DX11_KMT_DESTROY_CONTEXT hr=%08lx\n", static_cast<unsigned long>(hr));
    if (hr == S_OK) { s->m_context = 0; ++s->counts.contextCloses; }
    return hr;
  }
  Allocation* allocation(D3DKMT_HANDLE handle) {
    for (auto& entry : m_allocations) if (entry.handle == handle) return &entry;
    return nullptr;
  }
  static HRESULT APIENTRY allocate(HANDLE cookie, D3DDDICB_ALLOCATE* args) {
    auto s = owner(cookie, false);
    if (!s || !s->m_device || !s->m_context || !args || args->hResource || args->hKMResource
        || args->NumAllocations != 1 || !args->pAllocationInfo) return E_INVALIDARG;
    const auto& input = args->pAllocationInfo[0];
    if (input.hAllocation || !input.pPrivateDriverData
        || input.PrivateDriverDataSize != sizeof(dxvk::umd::AllocationInfo)) return E_INVALIDARG;
    try { s->m_allocations.reserve(s->m_allocations.size() + 1); }
    catch (...) { return E_OUTOFMEMORY; }
    D3DKMT_CREATEALLOCATION request = {}; request.hDevice = s->m_device;
    request.NumAllocations = 1; request.pAllocationInfo = args->pAllocationInfo;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    const HRESULT hr = result(D3DKMTCreateAllocation(&request));
    const D3DKMT_HANDLE handle = args->pAllocationInfo[0].hAllocation;
    const bool duplicate = handle && s->allocation(handle);
    std::printf("DX11_KMT_ALLOCATE hr=%08lx allocation=%u\n", static_cast<unsigned long>(hr), handle);
    // Retain every returned allocation owner before validating the output.
    // RuntimeGpu also balances an unpublished handle when creation fails.
    if (handle && !duplicate) {
      s->m_allocations.push_back({handle, false}); ++s->counts.allocations;
    }
    args->hKMResource = request.hResource;
    if (hr == S_OK && (!handle || request.hResource || duplicate)) {
      ++s->counts.malformedOutputs; return E_FAIL;
    }
    return hr;
  }
  HRESULT evict(D3DKMT_HANDLE handle) {
    const auto entry = std::find(m_resident.begin(), m_resident.end(), handle);
    if (entry == m_resident.end()) return S_OK;
    D3DKMT_EVICT request = {}; request.hDevice = m_device;
    request.NumAllocations = 1; request.AllocationList = &handle;
    const HRESULT hr = result(D3DKMTEvict(&request));
    std::printf("DX11_KMT_EVICT hr=%08lx allocation=%u\n", static_cast<unsigned long>(hr), handle);
    if (hr == S_OK) { m_resident.erase(entry); ++counts.evictions; }
    return hr;
  }
  static HRESULT APIENTRY deallocate(HANDLE cookie, const D3DDDICB_DEALLOCATE* args) {
    auto s = owner(cookie, false);
    if (!s || !args || args->hResource || args->NumAllocations != 1 || !args->HandleList) return E_INVALIDARG;
    auto entry = s->allocation(*args->HandleList);
    if (!entry || entry->locked) return E_INVALIDARG;
    const HRESULT resident = s->evict(entry->handle);
    if (resident != S_OK) return resident;
    D3DKMT_DESTROYALLOCATION request = {}; request.hDevice = s->m_device;
    request.AllocationCount = 1; request.phAllocationList = args->HandleList;
    const HRESULT hr = result(D3DKMTDestroyAllocation(&request));
    std::printf("DX11_KMT_DEALLOCATE hr=%08lx allocation=%u\n", static_cast<unsigned long>(hr), entry->handle);
    if (hr == S_OK) {
      s->m_allocations.erase(s->m_allocations.begin() + (entry - s->m_allocations.data()));
      ++s->counts.deallocations;
    }
    return hr;
  }
  static HRESULT APIENTRY lock(HANDLE cookie, D3DDDICB_LOCK* args) {
    auto s = owner(cookie, false);
    if (!s || !args || !args->Flags.LockEntire || args->Flags.Discard || args->Flags.IgnoreSync) return E_INVALIDARG;
    auto entry = s->allocation(args->hAllocation);
    if (!entry || entry->locked) return E_INVALIDARG;
    D3DKMT_LOCK request = {}; request.hDevice = s->m_device; request.hAllocation = args->hAllocation;
    request.PrivateDriverData = args->PrivateDriverData; request.NumPages = args->NumPages;
    request.pPages = args->pPages; request.Flags = args->Flags;
    const HRESULT hr = result(D3DKMTLock(&request));
    std::printf("DX11_KMT_LOCK hr=%08lx allocation=%u data=%p\n", static_cast<unsigned long>(hr), request.hAllocation, request.pData);
    if (hr == S_OK) {
      // Preserve the actual returned owner for balancing even on malformed
      // output. The typed UMD decides whether the mapping can be published.
      const bool renamed = request.hAllocation && request.hAllocation != entry->handle;
      if (request.hAllocation) entry->handle = request.hAllocation;
      entry->locked = true; ++s->counts.locks;
      args->hAllocation = request.hAllocation; args->pData = request.pData;
      // This workload never requests Discard. Unexpected renaming cannot be
      // accepted as a balanced residency reference to the original handle.
      if (renamed || !request.hAllocation || !request.pData) {
        ++s->counts.malformedOutputs; return E_FAIL;
      }
    }
    return hr;
  }
  static HRESULT APIENTRY unlock(HANDLE cookie, const D3DDDICB_UNLOCK* args) {
    auto s = owner(cookie, false);
    if (!s || !args || args->NumAllocations != 1 || !args->phAllocations) return E_INVALIDARG;
    auto entry = s->allocation(*args->phAllocations);
    if (!entry || !entry->locked) return E_INVALIDARG;
    D3DKMT_UNLOCK request = {}; request.hDevice = s->m_device;
    request.NumAllocations = 1; request.phAllocations = args->phAllocations;
    const HRESULT hr = result(D3DKMTUnlock(&request));
    std::printf("DX11_KMT_UNLOCK hr=%08lx allocation=%u\n", static_cast<unsigned long>(hr), entry->handle);
    if (hr == S_OK) { entry->locked = false; ++s->counts.unlocks; }
    return hr;
  }
  HRESULT resident(const D3DDDICB_RENDER& args) {
    if (!m_pagingQueue || !m_pagingSync || !args.pNewAllocationList
        || args.NumAllocations > args.NewAllocationListSize) return E_INVALIDARG;
    try { m_resident.reserve(m_resident.size() + args.NumAllocations); }
    catch (...) { return E_OUTOFMEMORY; }
    for (UINT i = 0; i < args.NumAllocations; ++i) {
      const D3DKMT_HANDLE handle = args.pNewAllocationList[i].hAllocation;
      auto owned = allocation(handle);
      // Turnip keeps command/global BOs mapped while submitting GPU references.
      // A valid owned CPU mapping alone does not prohibit GPU residency.
      if (!owned) return E_INVALIDARG;
      if (std::find(m_resident.begin(), m_resident.end(), handle) != m_resident.end()) continue;
      D3DDDI_MAKERESIDENT request = {}; request.hPagingQueue = m_pagingQueue;
      request.NumAllocations = 1; request.AllocationList = &handle; request.Flags.CantTrimFurther = 1;
      const NTSTATUS status = D3DKMTMakeResident(&request);
      std::printf("DX11_KMT_MAKE_RESIDENT status=%08lx allocation=%u fence=%llu count=%u\n",
        static_cast<unsigned long>(status), handle, static_cast<unsigned long long>(request.PagingFenceValue), request.NumAllocations);
      if (status != 0 && status != 0x103) return result(status);
      m_resident.push_back(handle); ++counts.residents;
      if (request.NumAllocations != 1 || (status == 0x103 && !request.PagingFenceValue)) {
        ++counts.malformedOutputs; return E_FAIL;
      }
      if (status == 0x103) m_pendingPaging = (std::max)(m_pendingPaging, request.PagingFenceValue);
    }
    if (!m_pendingPaging) return S_OK;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) return HRESULT_FROM_WIN32(GetLastError());
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU wait = {}; wait.hDevice = m_device;
    wait.ObjectCount = 1; wait.ObjectHandleArray = &m_pagingSync;
    wait.FenceValueArray = &m_pendingPaging; wait.hAsyncEvent = event;
    const NTSTATUS status = D3DKMTWaitForSynchronizationObjectFromCpu(&wait);
    const DWORD completed = status == 0 ? WaitForSingleObject(event, 5000) : WAIT_FAILED;
    const BOOL eventClosed = CloseHandle(event);
    const DWORD eventCloseError = eventClosed ? ERROR_SUCCESS : GetLastError();
    std::printf("DX11_KMT_PAGING_WAIT status=%08lx fence=%llu completed=%lu\n",
      static_cast<unsigned long>(status), static_cast<unsigned long long>(m_pendingPaging), static_cast<unsigned long>(completed));
    if (!eventClosed) return eventCloseError ? HRESULT_FROM_WIN32(eventCloseError) : E_FAIL;
    if (status != 0) return result(status);
    if (completed != WAIT_OBJECT_0) return E_FAIL;
    m_pendingPaging = 0;
    return S_OK;
  }
  static HRESULT APIENTRY escape(HANDLE cookie, const D3DDDICB_ESCAPE* args) {
    auto s = owner(cookie, true);
    if (!s || !args || args->hDevice != &s->m_deviceCookie || args->hContext != &s->m_contextCookie
        || !s->m_context || !args->pPrivateDriverData
        || (args->PrivateDriverDataSize != 64 && args->PrivateDriverDataSize != 56)
        || args->Flags.Value) return E_INVALIDARG;
    D3DKMT_ESCAPE request = {}; request.hAdapter = s->m_adapter; request.hDevice = s->m_device;
    request.hContext = s->m_context; request.Type = D3DKMT_ESCAPE_DRIVERPRIVATE; request.Flags = args->Flags;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    ++s->counts.escapes;
    const HRESULT hr = result(D3DKMTEscape(&request));
    std::printf("DX11_KMT_ESCAPE hr=%08lx bytes=%u\n", static_cast<unsigned long>(hr), args->PrivateDriverDataSize);
    return hr;
  }
  static HRESULT APIENTRY render(HANDLE cookie, D3DDDICB_RENDER* args) {
    auto s = owner(cookie, false);
    if (!s || !args || !s->m_context || args->hContext != &s->m_contextCookie
        || args->Flags.Value || args->BroadcastContextCount) return E_INVALIDARG;
    const HRESULT ready = s->resident(*args);
    if (ready != S_OK) return ready;
    D3DKMT_RENDER request = {}; request.hContext = s->m_context;
    request.CommandOffset = args->CommandOffset; request.CommandLength = args->CommandLength;
    request.AllocationCount = args->NumAllocations; request.PatchLocationCount = args->NumPatchLocations;
    request.pNewCommandBuffer = args->pNewCommandBuffer; request.NewCommandBufferSize = args->NewCommandBufferSize;
    request.pNewAllocationList = args->pNewAllocationList; request.NewAllocationListSize = args->NewAllocationListSize;
    request.pNewPatchLocationList = args->pNewPatchLocationList; request.NewPatchLocationListSize = args->NewPatchLocationListSize;
    const NTSTATUS status = D3DKMTRender(&request);
    // Actual KMT replacement outputs stay authoritative after failure as well.
    args->pNewCommandBuffer = request.pNewCommandBuffer; args->NewCommandBufferSize = request.NewCommandBufferSize;
    args->pNewAllocationList = request.pNewAllocationList; args->NewAllocationListSize = request.NewAllocationListSize;
    args->pNewPatchLocationList = request.pNewPatchLocationList; args->NewPatchLocationListSize = request.NewPatchLocationListSize;
    args->QueuedBufferCount = request.QueuedBufferCount;
    if (status == 0) ++s->counts.renders;
    std::printf("DX11_KMT_RENDER status=%08lx bytes=%u allocations=%u patches=%u\n",
      static_cast<unsigned long>(status), args->CommandLength, args->NumAllocations, args->NumPatchLocations);
    return result(status);
  }
};
