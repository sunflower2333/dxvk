// Unregistered target-only harness. All callbacks use real KMT operations;
// the Microsoft D3D runtime does not supply these callbacks or load this UMD.
#include "../src/umd/umd_d3d9_adapter.h"
#include "../src/umd/umd_runtime_identity.h"
#include <d3dkmthk.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
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
    if (FAILED(hr)) return hr;
    m_device = device.hDevice;
    UINT version = 0;
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = m_adapter; query.Type = KMTQAITYPE_DRIVERVERSION;
    query.pPrivateDriverData = &version; query.PrivateDriverDataSize = sizeof(version);
    hr = result(D3DKMTQueryAdapterInfo(&query));
    if (FAILED(hr) || version < KMT_DRIVERVERSION_WDDM_2_0) return FAILED(hr) ? hr : E_FAIL;
    // This harness is the runtime owner of its raw KMT device. The real
    // Microsoft runtime supplies the corresponding residency services.
    D3DKMT_CREATEPAGINGQUEUE paging = {};
    paging.hDevice = m_device; paging.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
    hr = result(D3DKMTCreatePagingQueue(&paging));
    m_pagingQueue = paging.hPagingQueue; m_pagingSync = paging.hSyncObject;
    std::printf("KMT_PAGING_QUEUE hr=%08lx version=%u\n", static_cast<unsigned long>(hr), unsigned(version));
    return FAILED(hr) ? hr : m_pagingQueue && m_pagingSync ? S_OK : E_FAIL;
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
    // This lifecycle creates no surface and records no clear or draw. DXVK
    // and Turnip may suppress its empty submission; render callbacks are a
    // rendering-workload oracle, not evidence required for balanced lifetime.
    return contexts == 1 && contextCloses == 1 && allocations && allocations == deallocations
      && locks == unlocks && !m_context && !wrongThreads ? S_OK : E_FAIL;
  }
  HRESULT verifyRendering(bool drawing = false) {
    const auto& api = m_deviceFuncs;
    if (!api.pfnCreateResource || !api.pfnDestroyResource || !api.pfnSetRenderTarget
        || !api.pfnClear || !api.pfnBlt || !api.pfnLock || !api.pfnUnlock) return E_FAIL;
    char targetOwner, systemOwner;
    D3DDDI_SURFACEINFO targetInfo[2] = {{8,8,1,nullptr,0,0},{4,4,1,nullptr,0,0}};
    D3DDDIARG_CREATERESOURCE target = {};
    target.hResource = &targetOwner;
    target.Format = static_cast<D3DDDIFORMAT>(D3DFMT_A8R8G8B8);
    target.Pool = D3DDDIPOOL_LOCALVIDMEM;
    target.Flags.RenderTarget = target.Flags.NotLockable = 1;
    target.pSurfList = targetInfo; target.SurfCount = 2;
    HRESULT hr = api.pfnCreateResource(m_driverDevice, &target);
    std::printf("D3D9_TARGET_RESOURCE hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    std::array<uint8_t, 384> backing;
    backing.fill(0xcd);
    constexpr UINT pitch = 44;
    D3DDDI_SURFACEINFO systemInfo = {8,8,1,backing.data() + 16,pitch,0};
    D3DDDIARG_CREATERESOURCE system = {};
    system.hResource = &systemOwner;
    system.Format = target.Format; system.Pool = D3DDDIPOOL_SYSTEMMEM;
    system.pSurfList = &systemInfo; system.SurfCount = 1;
    hr = api.pfnCreateResource(m_driverDevice, &system);
    std::printf("D3D9_SYSTEM_RESOURCE hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    D3DDDIARG_SETRENDERTARGET bind = {0,target.hResource,0};
    hr = api.pfnSetRenderTarget(m_driverDevice, &bind);
    std::printf("D3D9_TARGET_BIND hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    const RECT full = {0,0,8,8};
    D3DDDIARG_CLEAR fill = {};
    fill.Flags = D3DCLEAR_TARGET; fill.FillColor = 0xff123456;
    hr = api.pfnClear(m_driverDevice, &fill, 1, &full);
    std::printf("D3D9_CLEAR stage=1 mode=preclipped hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    // No preclipped rectangles means no-op. A public API Clear here would
    // incorrectly overwrite the baseline and fail the byte oracle below.
    fill.FillColor = 0xffaa55cc;
    hr = api.pfnClear(m_driverDevice, &fill, 0, reinterpret_cast<const RECT*>(UINT_PTR(1)));
    std::printf("D3D9_CLEAR stage=1 mode=empty-noop hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    D3DDDIARG_BLT copy = {};
    copy.hSrcResource = target.hResource; copy.hDstResource = system.hResource;
    copy.SrcRect = copy.DstRect = full;
    UINT checked = 0, checksum = 2166136261u;
    auto readback = [&](unsigned stage) -> HRESULT {
      std::printf("D3D9_READBACK begin stage=%u\n", stage);
      const HRESULT copied = api.pfnBlt(m_driverDevice, &copy);
      std::printf("D3D9_READBACK stage=%u hr=%08lx\n", stage, static_cast<unsigned long>(copied));
      if (FAILED(copied)) return copied;
      D3DDDIARG_LOCK mapping = {}; mapping.hResource = system.hResource;
      mapping.Flags.ReadOnly = mapping.Flags.NotifyOnly = 1;
      HRESULT status = api.pfnLock(m_driverDevice, &mapping);
      if (FAILED(status)) return status;
      if (mapping.pSurfData != backing.data() + 16 || mapping.Pitch != pitch) status = E_FAIL;
      for (UINT y = 0; y < 8 && SUCCEEDED(status); ++y) {
        for (UINT x = 0; x < 8; ++x) {
          UINT actual;
          std::memcpy(&actual, backing.data() + 16 + size_t(y) * pitch + x * 4, 4);
          UINT expected = stage == 1 ? 0xff123456 :
            x >= 2 && x < 6 && y >= 2 && y < 6 ? 0xffd03070 : 0xff2468ac;
          if (stage == 3 && x >= 4 && y >= 4) expected = 0xff80c020;
          if (stage == 4) expected = 0xff3c72b9;
          if (stage >= 5) {
            expected = x >= 2 && x < 6 && y >= 1 && y < 7 ? 0xffe08020 : 0xff173149;
            if (stage == 6 && x >= 1 && x < 7 && y >= 2 && y < 6)
              expected = (expected & 0xff00ff00) | (0xff901fe3 & 0x00ff00ff);
          }
          if (actual != expected) {
            std::printf("D3D9_PIXEL_MISMATCH stage=%u x=%u y=%u actual=%08x expected=%08x\n",
              stage, x, y, actual, expected);
            status = E_FAIL; break;
          }
          ++checked;
          checksum = (checksum ^ actual) * 16777619u;
        }
        for (UINT pad = 32; pad < pitch; ++pad)
          if (backing[16 + size_t(y) * pitch + pad] != 0xcd) status = E_FAIL;
      }
      for (UINT i = 0; i < 16; ++i)
        if (backing[i] != 0xcd || backing[368 + i] != 0xcd) status = E_FAIL;
      D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = system.hResource; unmap.Flags.NotifyOnly = 1;
      const HRESULT unlocked = api.pfnUnlock(m_driverDevice, &unmap);
      return FAILED(status) ? status : unlocked;
    };
    hr = readback(1);
    if (FAILED(hr)) return hr;
    fill.Flags = D3DCLEAR_TARGET | 8; fill.FillColor = 0xff2468ac;
    hr = api.pfnClear(m_driverDevice, &fill, 0, reinterpret_cast<const RECT*>(UINT_PTR(1)));
    std::printf("D3D9_CLEAR stage=2 mode=computed hr=%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr)) return hr;
    fill.Flags = D3DCLEAR_TARGET; fill.FillColor = 0xffd03070;
    const RECT center = {2,2,6,6};
    hr = api.pfnClear(m_driverDevice, &fill, 1, &center);
    if (FAILED(hr)) return hr;
    hr = readback(2);
    if (FAILED(hr)) return hr;
    bind.SubResourceIndex = 1;
    hr = api.pfnSetRenderTarget(m_driverDevice, &bind);
    if (FAILED(hr)) return hr;
    const RECT smallArea = {0,0,4,4};
    fill.FillColor = 0xff80c020;
    hr = api.pfnClear(m_driverDevice, &fill, 1, &smallArea);
    if (FAILED(hr)) return hr;
    copy.SrcSubResourceIndex = 1; copy.SrcRect = smallArea; copy.DstRect = {4,4,8,8};
    hr = readback(3);
    if (FAILED(hr)) return hr;
    std::printf("D3D9_CLEAR_READBACK PASS pixels=%u checksum=%08x padding=retained surfaces=2\n", checked, checksum);
    if (drawing) {
      if (!api.pfnCreateVertexShaderDecl || !api.pfnSetVertexShaderDecl || !api.pfnDeleteVertexShaderDecl
          || !api.pfnSetRenderState || !api.pfnSetViewport || !api.pfnSetZRange
          || !api.pfnSetScissorRect || !api.pfnSetStreamSourceUm || !api.pfnDrawPrimitive) return E_FAIL;
      bind.SubResourceIndex = 0;
      hr = api.pfnSetRenderTarget(m_driverDevice,&bind);
      if (FAILED(hr)) return hr;
      copy.SrcSubResourceIndex = 0; copy.SrcRect = copy.DstRect = full;
      D3DDDIVERTEXELEMENT elements[] = {{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITIONT,0},
        {0,16,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
      D3DDDIARG_CREATEVERTEXSHADERDECL declaration = {2,nullptr};
      hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&declaration,elements);
      std::printf("D3D9_DRAW_DECLARATION hr=%08lx\n", static_cast<unsigned long>(hr));
      if (FAILED(hr)) return hr;
      hr = api.pfnSetVertexShaderDecl(m_driverDevice,declaration.ShaderHandle);
      if (FAILED(hr)) return hr;
      auto state = [&](D3DDDIRENDERSTATETYPE id, UINT value) {
        D3DDDIARG_RENDERSTATE args = {id,value};
        const HRESULT status = api.pfnSetRenderState(m_driverDevice,&args);
        std::printf("D3D9_DRAW_STATE id=%u value=%u hr=%08lx\n", UINT(id),value,static_cast<unsigned long>(status));
        return status;
      };
      for (const auto item : {D3DDDIARG_RENDERSTATE{D3DDDIRS_ZENABLE,0}, {D3DDDIRS_LIGHTING,0},
          {D3DDDIRS_CULLMODE,D3DCULL_NONE},{D3DDDIRS_ALPHABLENDENABLE,0},{D3DDDIRS_DITHERENABLE,0},
          {D3DDDIRS_FOGENABLE,0},{D3DDDIRS_COLORWRITEENABLE,15},{D3DDDIRS_SCISSORTESTENABLE,0}}) {
        hr = state(item.State,item.Value); if (FAILED(hr)) return hr;
      }
      const D3DDDIARG_VIEWPORTINFO viewport = {0,0,8,8};
      hr = api.pfnSetViewport(m_driverDevice,&viewport);
      if (FAILED(hr)) return hr;
      const D3DDDIARG_ZRANGE range = {0.2f,0.8f};
      hr = api.pfnSetZRange(m_driverDevice,&range);
      if (FAILED(hr)) return hr;
      struct Vertex { float x,y,z,w; D3DCOLOR color; };
      static_assert(sizeof(Vertex) == 20);
      Vertex vertices[] = {{1000,1000,0.5f,1,0xff000000}, {-0.5f,-0.5f,0.5f,1,0xff3c72b9},
        {7.5f,-0.5f,0.5f,1,0xff3c72b9}, {-0.5f,7.5f,0.5f,1,0xff3c72b9}, {7.5f,7.5f,0.5f,1,0xff3c72b9}};
      const D3DDDIARG_SETSTREAMSOURCEUM stream = {0,sizeof(Vertex)};
      hr = api.pfnSetStreamSourceUm(m_driverDevice,&stream,vertices);
      if (FAILED(hr)) return hr;
      const D3DDDIARG_DRAWPRIMITIVE primitive = {D3DPT_TRIANGLESTRIP,1,2};
      auto draw = [&](unsigned stage) {
        HRESULT status = state(D3DDDIRS_SCENECAPTURE,1);
        if (FAILED(status)) return status;
        status = api.pfnDrawPrimitive(m_driverDevice,&primitive,nullptr);
        std::printf("D3D9_DRAW stage=%u primitives=2 vertex_start=1 hr=%08lx\n", stage,static_cast<unsigned long>(status));
        const HRESULT ended = state(D3DDDIRS_SCENECAPTURE,0);
        if (FAILED(status)) return status;
        if (FAILED(ended)) return ended;
        return readback(stage);
      };
      checked = 0; checksum = 2166136261u;
      hr = draw(4); if (FAILED(hr)) return hr;
      fill.Flags = D3DCLEAR_TARGET; fill.FillColor = 0xff173149;
      hr = api.pfnClear(m_driverDevice,&fill,1,&full);
      if (FAILED(hr)) return hr;
      const RECT centerScissor = {2,1,6,7};
      hr = api.pfnSetScissorRect(m_driverDevice,&centerScissor);
      if (FAILED(hr)) return hr;
      hr = state(D3DDDIRS_SCISSORTESTENABLE,1); if (FAILED(hr)) return hr;
      for (auto& vertex : vertices) vertex.color = 0xffe08020;
      hr = draw(5); if (FAILED(hr)) return hr;
      const RECT maskScissor = {1,2,7,6};
      hr = api.pfnSetScissorRect(m_driverDevice,&maskScissor);
      if (FAILED(hr)) return hr;
      hr = state(D3DDDIRS_COLORWRITEENABLE,5); if (FAILED(hr)) return hr;
      for (auto& vertex : vertices) vertex.color = 0xff901fe3;
      hr = draw(6); if (FAILED(hr)) return hr;
      hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,declaration.ShaderHandle);
      if (FAILED(hr)) return hr;
      if (api.pfnSetVertexShaderDecl(m_driverDevice,declaration.ShaderHandle) != E_INVALIDARG) return E_FAIL;
      std::printf("D3D9_DRAW_READBACK PASS pixels=%u checksum=%08x padding=retained vertex_start=1 stages=3\n", checked,checksum);
    }
    hr = api.pfnDestroyResource(m_driverDevice, target.hResource);
    if (FAILED(hr)) return hr;
    hr = api.pfnDestroyResource(m_driverDevice, system.hResource);
    if (FAILED(hr)) return hr;
    if (api.pfnDestroyResource(m_driverDevice, target.hResource) != E_INVALIDARG) return E_FAIL;
    hr = verify();
    // This workload records real clears and image-to-buffer transfers. Empty
    // submit acceptance is insufficient for its rendering oracle.
    return FAILED(hr) ? hr : renders ? S_OK : E_FAIL;
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
      if (m_pagingQueue) {
        D3DDDI_DESTROYPAGINGQUEUE paging = {}; paging.hPagingQueue = m_pagingQueue;
        const HRESULT cleanup = result(D3DKMTDestroyPagingQueue(&paging));
        if (FAILED(cleanup)) return cleanup;
        m_pagingQueue = m_pagingSync = 0;
      }
      std::printf("KMT_RESIDENCY references=%u evictions=%u remaining=%zu\n",
        residencyReferences, residencyEvictions, m_resident.size());
      if (!m_resident.empty() || residencyReferences != residencyEvictions) hr = E_FAIL;
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
  HRESULT resident(D3DDDICB_RENDER& args) {
    if (!m_pagingQueue || !m_pagingSync || !args.pNewAllocationList
        || args.NumAllocations > args.NewAllocationListSize) return E_INVALIDARG;
    // Reserve ownership storage before any successful kernel residency call.
    try { m_resident.reserve(m_resident.size() + args.NumAllocations); }
    catch (...) { return E_OUTOFMEMORY; }
    for (UINT i = 0; i < args.NumAllocations; ++i) {
      const D3DKMT_HANDLE allocation = args.pNewAllocationList[i].hAllocation;
      if (std::find(m_resident.begin(), m_resident.end(), allocation) != m_resident.end()) continue;
      D3DDDI_MAKERESIDENT request = {};
      request.hPagingQueue = m_pagingQueue; request.NumAllocations = 1; request.AllocationList = &allocation;
      // This bounded probe keeps all live references; none can be trimmed.
      request.Flags.CantTrimFurther = 1;
      const NTSTATUS status = D3DKMTMakeResident(&request);
      std::printf("KMT_MAKE_RESIDENT allocation=%u status=%08lx fence=%llu count=%u\n",
        allocation, static_cast<unsigned long>(status), request.PagingFenceValue, request.NumAllocations);
      if (status != 0 && status != 0x103) return status < 0 ? result(status) : E_FAIL;
      m_resident.push_back(allocation); ++residencyReferences;
      if (request.NumAllocations != 1 || (status == 0x103 && !request.PagingFenceValue)) return E_FAIL;
      if (status == 0x103) m_pendingPaging = (std::max)(m_pendingPaging, request.PagingFenceValue);
    }
    if (!m_pendingPaging) return S_OK;
    const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) return HRESULT_FROM_WIN32(GetLastError());
    D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU wait = {};
    wait.hDevice = m_device; wait.ObjectCount = 1;
    wait.ObjectHandleArray = &m_pagingSync; wait.FenceValueArray = &m_pendingPaging; wait.hAsyncEvent = event;
    const NTSTATUS status = D3DKMTWaitForSynchronizationObjectFromCpu(&wait);
    const DWORD completed = status == 0 ? WaitForSingleObject(event, 5000) : WAIT_FAILED;
    CloseHandle(event);
    std::printf("KMT_PAGING_WAIT status=%08lx fence=%llu completed=%u\n",
      static_cast<unsigned long>(status), m_pendingPaging, completed);
    if (status != 0) return status < 0 ? result(status) : E_FAIL;
    if (completed != WAIT_OBJECT_0) return E_FAIL;
    m_pendingPaging = 0;
    return S_OK;
  }
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
    if (!s || handle != &s->m_deviceOwner || !args || args->hResource
        || !args->NumAllocations || !args->HandleList) return E_INVALIDARG;
    for (UINT i = 0; i < args->NumAllocations; ++i) {
      const auto found = std::find(s->m_resident.begin(), s->m_resident.end(), args->HandleList[i]);
      if (found == s->m_resident.end()) continue;
      D3DKMT_EVICT evict = {}; evict.hDevice = s->m_device;
      evict.NumAllocations = 1; evict.AllocationList = &args->HandleList[i];
      const NTSTATUS status = D3DKMTEvict(&evict);
      std::printf("KMT_EVICT allocation=%u status=%08lx\n", args->HandleList[i], static_cast<unsigned long>(status));
      if (status != 0) return status < 0 ? result(status) : E_FAIL;
      s->m_resident.erase(found); ++s->residencyEvictions;
    }
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
        || !s->m_context || args->Flags.Value || args->BroadcastContextCount) {
      std::printf("D3D9_KMT_RENDER_CB rejected hr=80070057\n");
      return E_INVALIDARG;
    }
    const HRESULT residency = s->resident(*args);
    if (FAILED(residency)) return residency;
    D3DKMT_RENDER request = {}; request.hContext = s->m_context;
    request.CommandLength = args->CommandLength; request.CommandOffset = args->CommandOffset;
    request.AllocationCount = args->NumAllocations; request.PatchLocationCount = args->NumPatchLocations;
    request.NewCommandBufferSize = args->NewCommandBufferSize;
    request.NewAllocationListSize = args->NewAllocationListSize; request.NewPatchLocationListSize = args->NewPatchLocationListSize;
    // Keep the current callback storage if the KMT thunk leaves outputs
    // untouched on failure, just as the direct Mesa transport does.
    request.pNewCommandBuffer = args->pNewCommandBuffer;
    request.pNewAllocationList = args->pNewAllocationList;
    request.pNewPatchLocationList = args->pNewPatchLocationList;
    std::printf("D3D9_KMT_RENDER_CB begin bytes=%u allocations=%u patches=%u\n",
      args->CommandLength, args->NumAllocations, args->NumPatchLocations);
    const NTSTATUS status = D3DKMTRender(&request);
    const HRESULT hr = result(status);
    std::printf("D3D9_KMT_RENDER_CB end status=%08lx hr=%08lx capacity=%u/%u/%u\n",
      static_cast<unsigned long>(status), static_cast<unsigned long>(hr),
      request.NewCommandBufferSize, request.NewAllocationListSize, request.NewPatchLocationListSize);
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
  D3DKMT_HANDLE m_pagingQueue = 0, m_pagingSync = 0;
  UINT64 m_pendingPaging = 0;
  std::vector<D3DKMT_HANDLE> m_resident;
  HANDLE m_driverAdapter = nullptr, m_driverDevice = nullptr;
  D3DDDI_ADAPTERFUNCS m_adapterFuncs = {};
  D3DDDI_DEVICEFUNCS m_deviceFuncs = {};
  unsigned queries = 0, contexts = 0, contextCloses = 0, allocations = 0, deallocations = 0;
  unsigned locks = 0, unlocks = 0, renders = 0, escapes = 0, wrongThreads = 0;
  unsigned residencyReferences = 0, residencyEvictions = 0;
};

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc == 2 && !wcscmp(argv[1], L"--list-adapters")) {
    try { return listAdapters(); }
    catch (...) { return 1; }
  }
  LUID luid = {};
  const bool drawing = argc == 3 && !wcscmp(argv[2], L"--draw");
  const bool rendering = drawing || (argc == 3 && !wcscmp(argv[2], L"--render"));
  if ((argc != 2 && !rendering) || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d9-device-probe <16 hex LUID bytes> [--render|--draw]|--list-adapters\n");
    return 2;
  }
  KmtRuntime9 runtime;
  HRESULT hr = runtime.open(luid);
  if (SUCCEEDED(hr)) hr = runtime.create();
  if (SUCCEEDED(hr)) hr = rendering ? runtime.verifyRendering(drawing) : runtime.verify();
  const HRESULT closed = runtime.close();
  if (FAILED(closed)) hr = closed;
  std::printf("D3D9_KMT_%s %s hr=%08lx; %s, no ordinary runtime admission\n",
    drawing ? "DRAW" : rendering ? "RENDER" : "DEVICE", SUCCEEDED(hr) ? "PASS" : "FAIL", static_cast<unsigned long>(hr),
    drawing ? "typed offscreen draw/readback pixels" : rendering ? "typed offscreen clear/readback pixels" : "offscreen lifecycle only, no pixel rendering");
  return FAILED(hr) ? 1 : 0;
}
