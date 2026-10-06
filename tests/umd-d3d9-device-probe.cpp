// Unregistered target-only harness. All callbacks use real KMT operations;
// the Microsoft D3D runtime does not supply these callbacks or load this UMD.
#include "../src/umd/umd_d3d9_adapter.h"
#include "../src/umd/umd_runtime_identity.h"
#include <d3dkmthk.h>
#include <cstdio>
#include <cstring>
#include <vector>

static void printLuid(const LUID& luid) {
  const auto bytes = reinterpret_cast<const unsigned char*>(&luid);
  for (unsigned i = 0; i < sizeof(LUID); i++) std::printf("%02x", unsigned(bytes[i]));
}

static bool parseLuid(const WCHAR* text, LUID& luid) {
  if (wcslen(text) != 2 * sizeof(LUID)) return false;
  auto bytes = reinterpret_cast<unsigned char*>(&luid);
  for (unsigned i = 0; i < 2 * sizeof(LUID); i++) {
    const WCHAR c = text[i];
    unsigned digit;
    if (c >= L'0' && c <= L'9') digit = c - L'0';
    else if (c >= L'a' && c <= L'f') digit = c - L'a' + 10;
    else if (c >= L'A' && c <= L'F') digit = c - L'A' + 10;
    else return false;
    if (!(i & 1)) bytes[i / 2] = static_cast<unsigned char>(digit << 4);
    else bytes[i / 2] |= static_cast<unsigned char>(digit);
  }
  return luid.LowPart || luid.HighPart;
}

static int listAdapters() {
  D3DKMT_ENUMADAPTERS2 request = {};
  NTSTATUS status = D3DKMTEnumAdapters2(&request);
  if (status < 0 || !request.NumAdapters) return 1;
  std::vector<D3DKMT_ADAPTERINFO> adapters(request.NumAdapters);
  request.pAdapters = adapters.data();
  status = D3DKMTEnumAdapters2(&request);
  if (status < 0) return 1;
  bool failed = request.NumAdapters > adapters.size();
  for (size_t i = 0; i < request.NumAdapters && i < adapters.size(); i++) {
    std::printf("KMT_ENUM luid="); printLuid(adapters[i].AdapterLuid);
    std::array<uint8_t, dxvk::umd::RuntimeIdentityReplySize> bytes = {};
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapters[i].hAdapter; query.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    query.pPrivateDriverData = bytes.data(); query.PrivateDriverDataSize = UINT(bytes.size());
    dxvk::umd::RuntimeIdentity identity;
    const bool viogpu = D3DKMTQueryAdapterInfo(&query) == 0
      && dxvk::umd::readRuntimeIdentity(bytes.data(), query.PrivateDriverDataSize, identity)
      && !std::memcmp(identity.luid.data(), &adapters[i].AdapterLuid, sizeof(LUID));
    std::printf(" present_sources=%u viogpu_identity=%u\n", unsigned(adapters[i].NumOfSources), unsigned(viogpu));
    D3DKMT_CLOSEADAPTER close = {}; close.hAdapter = adapters[i].hAdapter;
    if (D3DKMTCloseAdapter(&close) < 0) failed = true;
  }
  return failed ? 1 : 0;
}

class KmtRuntime9 {
public:
  KmtRuntime9() = default;
  ~KmtRuntime9() { close(); }
  KmtRuntime9(const KmtRuntime9&) = delete;
  KmtRuntime9& operator=(const KmtRuntime9&) = delete;

  HRESULT open(const LUID& luid) {
    D3DKMT_OPENADAPTERFROMLUID adapter = {}; adapter.AdapterLuid = luid;
    HRESULT hr = result(D3DKMTOpenAdapterFromLuid(&adapter));
    std::printf("KMT_OPEN hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    m_adapter = adapter.hAdapter;
    std::printf("KMT_ADAPTER luid=");
    printLuid(luid);
    std::printf("\n");
    D3DKMT_CREATEDEVICE device = {}; device.hAdapter = m_adapter;
    hr = result(D3DKMTCreateDevice(&device));
    std::printf("KMT_CREATE_DEVICE hr=%08lx\n", static_cast<unsigned long>(hr));
    if (SUCCEEDED(hr)) m_device = device.hDevice;
    return hr;
  }
  HRESULT create() {
    D3DDDI_ADAPTERCALLBACKS callbacks = {}; callbacks.pfnQueryAdapterInfoCb = query;
    D3DDDIARG_OPENADAPTER adapter = {};
    adapter.hAdapter = &m_adapterOwner; adapter.Interface = 9;
    adapter.pAdapterCallbacks = &callbacks; adapter.pAdapterFuncs = &m_adapterFuncs;
    HRESULT hr = VioGpuDxvkOpenAdapter9ForTest(&adapter);
    std::printf("D3D9_OPEN hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    m_driverAdapter = adapter.hAdapter;
    D3DDDI_DEVICECALLBACKS kernel = {};
    kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
    kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
    kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
    kernel.pfnEscapeCb = escape; kernel.pfnRenderCb = render;
    D3DDDIARG_CREATEDEVICE device = {};
    device.hDevice = &m_deviceOwner; device.Interface = 9;
    device.pCallbacks = &kernel; device.pDeviceFuncs = &m_deviceFuncs;
    hr = m_adapterFuncs.pfnCreateDevice(m_driverAdapter, &device);
    std::printf("D3D9_CREATE hr=%08lx\n", static_cast<unsigned long>(hr));
    if (SUCCEEDED(hr)) m_driverDevice = device.hDevice;
    return hr;
  }
  HRESULT verify() {
    if (!m_driverDevice || !m_deviceFuncs.pfnFlush || !m_deviceFuncs.pfnDestroyDevice) return E_FAIL;
    HRESULT hr = m_deviceFuncs.pfnFlush(m_driverDevice);
    std::printf("D3D9_FLUSH hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    hr = m_deviceFuncs.pfnDestroyDevice(m_driverDevice);
    std::printf("D3D9_DESTROY hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    const HANDLE stale = m_driverDevice; m_driverDevice = nullptr;
    if (m_deviceFuncs.pfnFlush(stale) != E_INVALIDARG
        || m_deviceFuncs.pfnDestroyDevice(stale) != E_INVALIDARG) return E_FAIL;
    std::printf("D3D9_CALLBACKS query=%u context=%u/%u allocation=%u/%u lock=%u/%u render=%u escape=%u wrong_thread=%u\n",
      queries, contexts, contextCloses, allocations, deallocations, locks, unlocks, renders, escapes, wrongThreads);
    return contexts == 1 && contextCloses == 1 && allocations && allocations == deallocations
      && locks == unlocks && renders && !m_context && !wrongThreads ? S_OK : E_FAIL;
  }
  HRESULT close() {
    HRESULT hr = S_OK;
    if (m_driverDevice) {
      hr = m_deviceFuncs.pfnDestroyDevice(m_driverDevice);
      if (FAILED(hr)) return hr;
      m_driverDevice = nullptr;
    }
    if (m_driverAdapter) {
      hr = m_adapterFuncs.pfnCloseAdapter(m_driverAdapter);
      m_driverAdapter = nullptr;
    }
    if (m_context) {
      D3DKMT_DESTROYCONTEXT request = {}; request.hContext = m_context;
      const HRESULT cleanup = result(D3DKMTDestroyContext(&request));
      if (FAILED(cleanup)) hr = cleanup;
      m_context = 0;
    }
    if (m_device) {
      D3DKMT_DESTROYDEVICE request = {}; request.hDevice = m_device;
      const HRESULT cleanup = result(D3DKMTDestroyDevice(&request));
      if (FAILED(cleanup)) hr = cleanup;
      m_device = 0;
    }
    if (m_adapter) {
      D3DKMT_CLOSEADAPTER request = {}; request.hAdapter = m_adapter;
      const HRESULT cleanup = result(D3DKMTCloseAdapter(&request));
      if (FAILED(cleanup)) hr = cleanup;
      m_adapter = 0;
    }
    return hr;
  }

private:
  struct Owner { KmtRuntime9* runtime; };
  static KmtRuntime9* self(HANDLE handle) {
    if (!handle) return nullptr;
    auto runtime = static_cast<Owner*>(handle)->runtime;
    if (GetCurrentThreadId() != runtime->m_thread) { ++runtime->wrongThreads; return nullptr; }
    return runtime;
  }
  static HRESULT result(NTSTATUS status) { return status < 0 ? HRESULT_FROM_NT(status) : S_OK; }
  static HRESULT APIENTRY query(HANDLE handle, const D3DDDICB_QUERYADAPTERINFO* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_adapterOwner || !args) return E_INVALIDARG;
    D3DKMT_QUERYADAPTERINFO request = {};
    request.hAdapter = s->m_adapter; request.Type = KMTQAITYPE_UMDRIVERPRIVATE;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    ++s->queries; return result(D3DKMTQueryAdapterInfo(&request));
  }
  static HRESULT APIENTRY createContext(HANDLE handle, D3DDDICB_CREATECONTEXT* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || s->m_context) return E_INVALIDARG;
    D3DKMT_CREATECONTEXT request = {};
    request.hDevice = s->m_device; request.NodeOrdinal = args->NodeOrdinal;
    request.EngineAffinity = args->EngineAffinity; request.Flags = args->Flags;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    const HRESULT hr = result(D3DKMTCreateContext(&request));
    if (SUCCEEDED(hr)) {
      s->m_context = request.hContext; args->hContext = &s->m_contextOwner; ++s->contexts;
      args->pCommandBuffer = request.pCommandBuffer; args->CommandBufferSize = request.CommandBufferSize;
      args->pAllocationList = request.pAllocationList; args->AllocationListSize = request.AllocationListSize;
      args->pPatchLocationList = request.pPatchLocationList; args->PatchLocationListSize = request.PatchLocationListSize;
    }
    return hr;
  }
  static HRESULT APIENTRY destroyContext(HANDLE handle, const D3DDDICB_DESTROYCONTEXT* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || args->hContext != &s->m_contextOwner || !s->m_context) return E_INVALIDARG;
    D3DKMT_DESTROYCONTEXT request = {}; request.hContext = s->m_context;
    const HRESULT hr = result(D3DKMTDestroyContext(&request));
    if (SUCCEEDED(hr)) { s->m_context = 0; ++s->contextCloses; }
    return hr;
  }
  static HRESULT APIENTRY allocate(HANDLE handle, D3DDDICB_ALLOCATE* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || args->hResource || args->hKMResource || !args->NumAllocations) return E_INVALIDARG;
    D3DKMT_CREATEALLOCATION request = {};
    request.hDevice = s->m_device; request.NumAllocations = args->NumAllocations; request.pAllocationInfo = args->pAllocationInfo;
    const HRESULT hr = result(D3DKMTCreateAllocation(&request));
    if (SUCCEEDED(hr)) { args->hKMResource = request.hResource; s->allocations += args->NumAllocations; }
    return hr;
  }
  static HRESULT APIENTRY deallocate(HANDLE handle, const D3DDDICB_DEALLOCATE* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || args->hResource || !args->NumAllocations) return E_INVALIDARG;
    D3DKMT_DESTROYALLOCATION request = {};
    request.hDevice = s->m_device; request.AllocationCount = args->NumAllocations; request.phAllocationList = args->HandleList;
    const HRESULT hr = result(D3DKMTDestroyAllocation(&request));
    if (SUCCEEDED(hr)) s->deallocations += args->NumAllocations;
    return hr;
  }
  static HRESULT APIENTRY lock(HANDLE handle, D3DDDICB_LOCK* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args) return E_INVALIDARG;
    D3DKMT_LOCK request = {}; request.hDevice = s->m_device; request.hAllocation = args->hAllocation;
    request.PrivateDriverData = args->PrivateDriverData; request.NumPages = args->NumPages;
    request.pPages = args->pPages; request.Flags = args->Flags;
    const HRESULT hr = result(D3DKMTLock(&request));
    if (SUCCEEDED(hr)) { args->hAllocation = request.hAllocation; args->pData = request.pData; ++s->locks; }
    return hr;
  }
  static HRESULT APIENTRY unlock(HANDLE handle, const D3DDDICB_UNLOCK* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args) return E_INVALIDARG;
    D3DKMT_UNLOCK request = {}; request.hDevice = s->m_device;
    request.NumAllocations = args->NumAllocations; request.phAllocations = args->phAllocations;
    const HRESULT hr = result(D3DKMTUnlock(&request)); if (SUCCEEDED(hr)) ++s->unlocks; return hr;
  }
  static HRESULT APIENTRY escape(HANDLE handle, const D3DDDICB_ESCAPE* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_adapterOwner || !args || args->hDevice != &s->m_deviceOwner
        || args->hContext != &s->m_contextOwner || !s->m_context) return E_INVALIDARG;
    D3DKMT_ESCAPE request = {};
    request.hAdapter = s->m_adapter; request.hDevice = s->m_device; request.hContext = s->m_context;
    request.Type = D3DKMT_ESCAPE_DRIVERPRIVATE; request.Flags = args->Flags;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    ++s->escapes; return result(D3DKMTEscape(&request));
  }
  static HRESULT APIENTRY render(HANDLE handle, D3DDDICB_RENDER* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || args->hContext != &s->m_contextOwner
        || !s->m_context || args->Flags.Value || args->BroadcastContextCount) return E_INVALIDARG;
    D3DKMT_RENDER request = {}; request.hContext = s->m_context;
    request.CommandLength = args->CommandLength; request.CommandOffset = args->CommandOffset;
    request.AllocationCount = args->NumAllocations; request.PatchLocationCount = args->NumPatchLocations;
    request.NewCommandBufferSize = args->NewCommandBufferSize;
    request.NewAllocationListSize = args->NewAllocationListSize; request.NewPatchLocationListSize = args->NewPatchLocationListSize;
    const HRESULT hr = result(D3DKMTRender(&request));
    args->pNewCommandBuffer = request.pNewCommandBuffer; args->NewCommandBufferSize = request.NewCommandBufferSize;
    args->pNewAllocationList = request.pNewAllocationList; args->NewAllocationListSize = request.NewAllocationListSize;
    args->pNewPatchLocationList = request.pNewPatchLocationList; args->NewPatchLocationListSize = request.NewPatchLocationListSize;
    args->QueuedBufferCount = request.QueuedBufferCount;
    if (SUCCEEDED(hr)) ++s->renders;
    return hr;
  }
  Owner m_adapterOwner{this}, m_deviceOwner{this}, m_contextOwner{this};
  DWORD m_thread = GetCurrentThreadId();
  D3DKMT_HANDLE m_adapter = 0, m_device = 0, m_context = 0;
  HANDLE m_driverAdapter = nullptr, m_driverDevice = nullptr;
  D3DDDI_ADAPTERFUNCS m_adapterFuncs = {};
  D3DDDI_DEVICEFUNCS m_deviceFuncs = {};
  unsigned queries = 0, contexts = 0, contextCloses = 0, allocations = 0, deallocations = 0;
  unsigned locks = 0, unlocks = 0, renders = 0, escapes = 0, wrongThreads = 0;
};

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc == 2 && !wcscmp(argv[1], L"--list-adapters")) {
    try { return listAdapters(); }
    catch (...) { return 1; }
  }
  LUID luid = {};
  if (argc != 2 || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d9-device-probe <16 hex LUID bytes>|--list-adapters\n");
    return 2;
  }
  KmtRuntime9 runtime;
  HRESULT hr = runtime.open(luid);
  if (SUCCEEDED(hr)) hr = runtime.create();
  if (SUCCEEDED(hr)) hr = runtime.verify();
  const HRESULT closed = runtime.close();
  if (FAILED(closed)) hr = closed;
  std::printf("D3D9_KMT_DEVICE %s hr=%08lx; offscreen lifecycle only, no pixel rendering or ordinary runtime admission\n",
    SUCCEEDED(hr) ? "PASS" : "FAIL", static_cast<unsigned long>(hr));
  return FAILED(hr) ? 1 : 0;
}
