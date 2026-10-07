// Unregistered target-only harness. All callbacks use real KMT operations;
// the Microsoft D3D runtime does not supply these callbacks or load this UMD.
#include "../src/umd/umd_d3d9_adapter.h"
#include "../src/umd/umd_runtime_identity.h"
#include "../src/umd/umd_allocation.h"
#include <d3dkmthk.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>

class PresentWindow {
public:
  ~PresentWindow() {
    if (m_window) DestroyWindow(m_window);
    if (m_class) UnregisterClassW(L"DxvkTypedPresentProbe", GetModuleHandleW(nullptr));
  }
  HRESULT create() {
    SetProcessDPIAware();
    WNDCLASSW wc = {};
    wc.lpfnWndProc = procedure; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"DxvkTypedPresentProbe";
    m_class = RegisterClassW(&wc);
    if (!m_class) return HRESULT_FROM_WIN32(GetLastError());
    m_window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      wc.lpszClassName, L"DXVK typed presentation probe", WS_POPUP,
      64,64,64,64,nullptr,nullptr,wc.hInstance,nullptr);
    if (!m_window) return HRESULT_FROM_WIN32(GetLastError());
    ShowWindow(m_window,SW_SHOWNOACTIVATE);
    if (!UpdateWindow(m_window)) return HRESULT_FROM_WIN32(GetLastError());
    RECT client = {};
    if (!GetClientRect(m_window,&client) || client.right != 64 || client.bottom != 64) return E_FAIL;
    return S_OK;
  }
  HWND handle() const { return m_window; }
  void pump() const {
    MSG msg;
    while (PeekMessageW(&msg,m_window,0,0,PM_REMOVE)) {
      TranslateMessage(&msg); DispatchMessageW(&msg);
    }
  }
  HRESULT capture(std::array<UINT,4096>& pixels) const {
    POINT origin = {};
    if (!m_window || !ClientToScreen(m_window,&origin)) return E_FAIL;
    HDC screen = GetDC(nullptr);
    if (!screen) return E_FAIL;
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 64; info.bmiHeader.biHeight = -64;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* data = nullptr;
    HBITMAP bitmap = memory ? CreateDIBSection(screen,&info,DIB_RGB_COLORS,&data,nullptr,0) : nullptr;
    HGDIOBJ previous = bitmap ? SelectObject(memory,bitmap) : nullptr;
    HRESULT hr = E_FAIL;
    if (previous && previous != HGDI_ERROR && data
        && BitBlt(memory,0,0,64,64,screen,origin.x,origin.y,SRCCOPY | CAPTUREBLT)
        && GdiFlush()) {
      std::memcpy(pixels.data(),data,sizeof(pixels)); hr = S_OK;
    }
    if (previous && previous != HGDI_ERROR) SelectObject(memory,previous);
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    ReleaseDC(nullptr,screen);
    return hr;
  }
private:
  static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PAINT) {
      PAINTSTRUCT paint;
      const HDC dc = BeginPaint(window,&paint);
      // This fallback paint deliberately has no frame-oracle colors.
      if (dc) FillRect(dc,&paint.rcPaint,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
      EndPaint(window,&paint); return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
  }
  HWND m_window = nullptr;
  ATOM m_class = 0;
};

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
    m_luid = luid;
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
    kernel.pfnPresentCb = present;
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
      && locks == unlocks && !m_context && !m_presentContext && m_resources.empty()
      && presentContexts == presentContextCloses && !m_sourceOwned && !wrongThreads ? S_OK : E_FAIL;
  }
  HRESULT verifyRendering(bool drawing = false, bool shaders = false, bool textures = false,
                          bool buffers = false, bool depthStencil = false, bool fixedFunction = false,
                          bool bufferTransfer = false, bool clipPlanes = false, bool gpuQueries = false,
                          bool presentation = false) {
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
      // Retain every clip-stage pixel even if its first comparison fails.
      // The independent host oracle can then inspect the complete pattern.
      if (stage >= 67 && stage <= 76 && SUCCEEDED(status)) {
        for (UINT y = 0; y < 8; ++y) for (UINT x = 0; x < 8; ++x) {
          UINT actual;
          std::memcpy(&actual,backing.data() + 16 + size_t(y) * pitch + x * 4,4);
          std::printf("D3D9_CLIP_PIXEL stage=%u x=%u y=%u value=%08x\n",stage,x,y,actual);
        }
      }
      if (stage >= 77 && stage <= 79 && SUCCEEDED(status)) {
        for (UINT y = 0; y < 8; ++y) for (UINT x = 0; x < 8; ++x) {
          UINT actual;
          std::memcpy(&actual,backing.data() + 16 + size_t(y) * pitch + x * 4,4);
          std::printf("D3D9_QUERY_PIXEL stage=%u x=%u y=%u value=%08x\n",stage,x,y,actual);
        }
      }
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
          if (stage == 7) expected = 0xff204060;
          if (stage == 8) expected = 0xff80c060;
          if (stage == 9) expected = x >= 4 ? 0xffe03090 : 0xff17293b;
          if (stage >= 10 && stage <= 17) {
            const UINT textureColors[] = {0xff204060,0xffe02020,0xff3080d0,0xffe02020,
              0xff808080,0xff647a98,0xff204060,0xff4060e0};
            expected = textureColors[stage - 10];
          }
          if (stage >= 18 && stage <= 23)
            expected = stage <= 20 ? 0xffa03658 : stage == 21 ? 0xff2785b3 : 0xffe19c47;
          if (stage >= 24 && stage <= 37) {
            const UINT depthColors[] = {0xff39a57b,0xffc0472a,0xff356bd1,0xff438bc2,
              0xff7bb85f,0xffdd9235,0xffdd9235,0xff7bb85f,0xff8b57c5,0xffbd6382,
              0xff4eac77,0xff649dd8,0xffceaf45,0xff2fafbf};
            expected = depthColors[stage - 24];
            if (stage == 29 || stage == 30 || stage == 36 || stage == 37) {
              const UINT right = stage == 30 || stage == 37 ? 4u : 6u;
              if (!(x >= 3 && x < right && y >= 2 && y < 6)) expected = 0xff07131f;
            }
          }
          if (stage >= 38 && stage <= 43) {
            const RECT fixedBounds[] = {{2,2,6,6},{4,3,8,7},{3,3,5,5},{5,3,7,5},{1,3,3,5},{4,2,6,4}};
            const UINT fixedColors[] = {0xffb03d87,0xff63ba49,0xffd18c37,0xff75a5d1,0xff9f68cb,0xff43b49c};
            const auto& bounds = fixedBounds[stage-38];
            expected = LONG(x) >= bounds.left && LONG(x) < bounds.right
              && LONG(y) >= bounds.top && LONG(y) < bounds.bottom ? fixedColors[stage-38] : 0xff0b1723;
          }
          if (stage >= 44 && stage <= 58) {
            const UINT lightColors[] = {0xff0000ff,0xff000000,0xffff0000,0xff00ff00,0xff0000ff,
              0xff00ff00,0xff000000,0xff00ff00,0xff00ffff,0xff0000ff,0xffff00ff,
              0xff0000ff,0xffff00ff,0xff0000ff,0xff00ffff};
            expected = lightColors[stage-44];
          }
          if (stage >= 59 && stage <= 66)
            expected = stage == 59 ? 0xffa05c71 : stage <= 63 ? 0xffb08746 : 0xff65b82f;
          if (stage >= 67 && stage <= 76) {
            const UINT clipColors[] = {0xff739a4c,0xffc85d8a,0xff49a1d2,0xff9a73c4,0xff4da57e,
              0xffce9341,0xfff0a236,0xff3dae96,0xffba567d,0xff64bdc9};
            const bool visible = stage == 68 ? x >= 4 : stage == 69 ? x < 4
              : stage == 70 ? y < 4 : stage == 71 ? x < 4 && y < 4
              : stage != 73 && stage != 74;
            expected = visible ? clipColors[stage-67] : 0xff091725;
          }
          if (stage >= 77 && stage <= 79) {
            const UINT queryColors[] = {0xffbd5c83,0xff43a6c2,0xff73b248};
            const bool visible = stage == 77 || (stage == 78 && x >= 1 && x < 5 && y >= 2 && y < 6);
            expected = visible ? queryColors[stage-77] : 0xff091725;
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
      if (shaders) {
        if (!api.pfnCreateVertexShaderFunc || !api.pfnSetVertexShaderFunc || !api.pfnDeleteVertexShaderFunc
          || !api.pfnCreatePixelShader || !api.pfnSetPixelShader || !api.pfnDeletePixelShader
          || !api.pfnSetVertexShaderConst || !api.pfnSetPixelShaderConst
          || !api.pfnSetVertexShaderConstI || !api.pfnSetPixelShaderConstI
          || !api.pfnSetVertexShaderConstB || !api.pfnSetPixelShaderConstB) return E_FAIL;
        hr = state(D3DDDIRS_SCISSORTESTENABLE,0); if (FAILED(hr)) return hr;
        hr = state(D3DDDIRS_COLORWRITEENABLE,15); if (FAILED(hr)) return hr;
        const D3DDDIVERTEXELEMENT position = {0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0};
        declaration = {1,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&declaration,&position);
        if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,declaration.ShaderHandle);
        if (FAILED(hr)) return hr;
        struct Position { float x,y,z,w; };
        const Position positions[] = {{1000,1000,0.5f,1},{-1,1,0.5f,1},{1,1,0.5f,1},
          {-1,-1,0.5f,1},{1,-1,0.5f,1}};
        const D3DDDIARG_SETSTREAMSOURCEUM shaderStream = {0,sizeof(Position)};
        hr = api.pfnSetStreamSourceUm(m_driverDevice,&shaderStream,positions);
        if (FAILED(hr)) return hr;
        // Actual assembled SM1.1, SM2.0 and SM3.0 programs. SM3 uses float,
        // integer and boolean constants in both stages to select the output.
        // D3D9 requires the semantic declaration even for vs_1_1. Without it,
        // v0 has no vertex input binding and the quad degenerates to one point.
        const UINT vs1[] = {0xfffe0101,0x0000001f,0x80000000,0x900f0000,
          0x00000002,0xc00f0000,0x90e40000,0xa0e40000,0x0000ffff};
        const UINT ps1[] = {0xffff0101,0x00000001,0x800f0000,0xa0e40000,0x0000ffff};
        const UINT vs2[] = {0xfffe0200,0x0200001f,0x80000000,0x900f0000,
          0x03000002,0xc00f0000,0x90e40000,0xa0e40000,0x0000ffff};
        const UINT ps2[] = {0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
        const UINT vs3[] = {0xfffe0300,0x0200001f,0x80000000,0x900f0000,
          0x0200001f,0x80000000,0xe00f0000,0x01000026,0xf0e40000,0x01000028,0xe0e40800,
          0x03000002,0xe00f0000,0x90e40000,0xa0e40000,0x0000002a,
          0x02000001,0xe00f0000,0x90e40000,0x0000002b,0x00000027,0x0000ffff};
        const UINT ps3[] = {0xffff0300,0x01000026,0xf0e40000,0x01000028,0xe0e40800,
          0x02000001,0x800f0800,0xa0e40000,0x0000002a,
          0x02000001,0x800f0800,0xa0e40001,0x0000002b,0x00000027,0x0000ffff};
        struct Program { const UINT* vertex; UINT vertexBytes; const UINT* pixel; UINT pixelBytes; };
        const Program programs[] = {{vs1,sizeof(vs1),ps1,sizeof(ps1)},
          {vs2,sizeof(vs2),ps2,sizeof(ps2)},{vs3,sizeof(vs3),ps3,sizeof(ps3)}};
        checked = 0; checksum = 2166136261u;
        for (UINT model = 1; model <= 3; ++model) {
          const auto& program = programs[model - 1];
          D3DDDIARG_CREATEVERTEXSHADERFUNC vertexShader = {program.vertexBytes,nullptr};
          D3DDDIARG_CREATEPIXELSHADER pixelShader = {program.pixelBytes,nullptr};
          hr = api.pfnCreateVertexShaderFunc(m_driverDevice,&vertexShader,program.vertex);
          std::printf("D3D9_SHADER_CREATE model=%u stage=vertex bytes=%u hr=%08lx\n",model,program.vertexBytes,static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          hr = api.pfnCreatePixelShader(m_driverDevice,&pixelShader,program.pixel);
          std::printf("D3D9_SHADER_CREATE model=%u stage=pixel bytes=%u hr=%08lx\n",model,program.pixelBytes,static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          hr = api.pfnSetVertexShaderFunc(m_driverDevice,vertexShader.ShaderHandle);
          if (FAILED(hr)) return hr;
          hr = api.pfnSetPixelShader(m_driverDevice,pixelShader.ShaderHandle);
          if (FAILED(hr)) return hr;
          D3DDDIARG_SETVERTEXSHADERCONST vertexConstants = {0,1};
          const float offset[4] = {model == 3 ? 1.0f : 0.0f,0,0,0};
          hr = api.pfnSetVertexShaderConst(m_driverDevice,&vertexConstants,offset);
          if (FAILED(hr)) return hr;
          D3DDDIARG_SETPIXELSHADERCONST pixelConstants = {0,2};
          const float color[8] = {model == 1 ? 32.0f/255 : 128.0f/255,
            model == 1 ? 64.0f/255 : 192.0f/255,96.0f/255,1.0f,
            224.0f/255,48.0f/255,144.0f/255,1.0f};
          hr = api.pfnSetPixelShaderConst(m_driverDevice,&pixelConstants,color);
          if (FAILED(hr)) return hr;
          if (model == 3) {
            const INT repeat[4] = {1,0,0,0}; const BOOL yes = TRUE, no = FALSE;
            const D3DDDIARG_SETVERTEXSHADERCONSTI vertexInt = {0,1};
            const D3DDDIARG_SETPIXELSHADERCONSTI pixelInt = {0,1};
            const D3DDDIARG_SETVERTEXSHADERCONSTB vertexBool = {0,1};
            const D3DDDIARG_SETPIXELSHADERCONSTB pixelBool = {0,1};
            hr = api.pfnSetVertexShaderConstI(m_driverDevice,&vertexInt,repeat);
            if (FAILED(hr)) return hr;
            hr = api.pfnSetPixelShaderConstI(m_driverDevice,&pixelInt,repeat);
            if (FAILED(hr)) return hr;
            hr = api.pfnSetVertexShaderConstB(m_driverDevice,&vertexBool,&yes);
            if (FAILED(hr)) return hr;
            hr = api.pfnSetPixelShaderConstB(m_driverDevice,&pixelBool,&no);
            if (FAILED(hr)) return hr;
            std::printf("D3D9_SHADER_CONSTANTS model=3 vertex=float4/int4/bool pixel=float4/int4/bool\n");
          }
          fill.FillColor = 0xff17293b;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full);
          if (FAILED(hr)) return hr;
          hr = draw(6 + model);
          if (FAILED(hr)) return hr;
          hr = api.pfnDeleteVertexShaderFunc(m_driverDevice,vertexShader.ShaderHandle);
          if (FAILED(hr)) return hr;
          hr = api.pfnDeletePixelShader(m_driverDevice,pixelShader.ShaderHandle);
          if (FAILED(hr)) return hr;
          if (api.pfnSetVertexShaderFunc(m_driverDevice,vertexShader.ShaderHandle) != E_INVALIDARG
            || api.pfnSetPixelShader(m_driverDevice,pixelShader.ShaderHandle) != E_INVALIDARG) return E_FAIL;
        }
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,declaration.ShaderHandle);
        if (FAILED(hr)) return hr;
        std::printf("D3D9_SHADER_READBACK PASS pixels=%u checksum=%08x padding=retained models=1/2/3 constants=float4/int4/bool\n",checked,checksum);
      }
      if (textures) {
        if (!api.pfnSetTexture || !api.pfnSetTextureStageState || !api.pfnTexBlt) return E_FAIL;
        char textureOwners[3];
        std::array<std::vector<uint8_t>,3> textureData;
        D3DDDI_SURFACEINFO levels[3] = {};
        const UINT topColors[] = {0xff204060,0xff80c020,0xffe03090,0xff4060e0};
        const UINT middleColors[] = {0xff202020,0xffe02020,0xff20e0e0,0xffe0e0e0};
        for (UINT i = 0; i < 3; ++i) {
          const UINT width = 4u >> i, texturePitch = width * 4 + 12;
          textureData[i].resize(32 + size_t(texturePitch) * width,0xcd);
          for (UINT y = 0; y < width; ++y) for (UINT x = 0; x < width; ++x) {
            const UINT color = i == 0 ? topColors[(y / 2) * 2 + x / 2]
              : i == 1 ? middleColors[y * 2 + x] : 0xff3080d0;
            std::memcpy(textureData[i].data() + 16 + size_t(y) * texturePitch + x * 4,&color,4);
          }
          levels[i] = {width,width,UINT_MAX,textureData[i].data() + 16,texturePitch,UINT_MAX};
        }
        const auto originalTextureData = textureData;
        D3DDDIARG_CREATERESOURCE textureSource = {};
        textureSource.hResource = &textureOwners[0]; textureSource.Format = target.Format;
        textureSource.Pool = D3DDDIPOOL_SYSTEMMEM; textureSource.Flags.Texture = 1;
        textureSource.pSurfList = levels; textureSource.SurfCount = textureSource.MipLevels = 3;
        textureSource.MultisampleType = static_cast<D3DDDIMULTISAMPLE_TYPE>(UINT_MAX);
        textureSource.MultisampleQuality = textureSource.Fvf = UINT_MAX;
        hr = api.pfnCreateResource(m_driverDevice,&textureSource);
        std::printf("D3D9_TEXTURE_CREATE kind=system levels=3 hr=%08lx\n",static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        D3DDDI_SURFACEINFO videoLevels[3] = {{4,4,UINT_MAX,nullptr,UINT_MAX,UINT_MAX},
          {2,2,UINT_MAX,nullptr,UINT_MAX,UINT_MAX},{1,1,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}};
        D3DDDIARG_CREATERESOURCE textureFull = textureSource;
        textureFull.hResource = &textureOwners[1]; textureFull.Pool = D3DDDIPOOL_VIDEOMEMORY;
        textureFull.pSurfList = videoLevels;
        hr = api.pfnCreateResource(m_driverDevice,&textureFull);
        std::printf("D3D9_TEXTURE_CREATE kind=video levels=3 hr=%08lx\n",static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        D3DDDIARG_CREATERESOURCE textureSmall = textureFull;
        textureSmall.hResource = &textureOwners[2]; textureSmall.pSurfList = videoLevels + 1;
        textureSmall.SurfCount = textureSmall.MipLevels = 2;
        hr = api.pfnCreateResource(m_driverDevice,&textureSmall);
        std::printf("D3D9_TEXTURE_CREATE kind=small levels=2 hr=%08lx\n",static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        D3DDDIARG_TEXBLT upload = {textureFull.hResource,textureSource.hResource,UINT_MAX,{0,0},{0,0,4,4}};
        hr = api.pfnTexBlt(m_driverDevice,&upload);
        std::printf("D3D9_TEXTURE_UPLOAD source_levels=3 destination_levels=3 hr=%08lx\n",static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        upload.hDstResource = textureSmall.hResource;
        hr = api.pfnTexBlt(m_driverDevice,&upload);
        std::printf("D3D9_TEXTURE_UPLOAD source_levels=3 destination_levels=2 hr=%08lx\n",static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        const D3DDDIVERTEXELEMENT textureElements[] = {{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0},
          {0,16,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0}};
        declaration = {2,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&declaration,textureElements);
        if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,declaration.ShaderHandle);
        if (FAILED(hr)) return hr;
        const UINT textureVs[] = {0xfffe0200,
          0x0200001f,0x80000000,0x900f0000,0x0200001f,0x80000005,0x90030001,
          0x02000001,0xc00f0000,0x90e40000,0x02000001,0xe0030000,0x90e40001,0x0000ffff};
        const UINT texturePs[] = {0xffff0200,
          0x0200001f,0x80000000,0xb0030000,0x0200001f,0x90000000,0xa00f0800,
          0x03000042,0x800f0000,0xb0e40000,0xa0e40800,0x02000001,0x800f0800,0x80e40000,0x0000ffff};
        D3DDDIARG_CREATEVERTEXSHADERFUNC textureVertex = {sizeof(textureVs),nullptr};
        D3DDDIARG_CREATEPIXELSHADER texturePixel = {sizeof(texturePs),nullptr};
        hr = api.pfnCreateVertexShaderFunc(m_driverDevice,&textureVertex,textureVs);
        std::printf("D3D9_TEXTURE_SHADER stage=vertex bytes=%u hr=%08lx\n",UINT(sizeof(textureVs)),static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        hr = api.pfnCreatePixelShader(m_driverDevice,&texturePixel,texturePs);
        std::printf("D3D9_TEXTURE_SHADER stage=pixel bytes=%u hr=%08lx\n",UINT(sizeof(texturePs)),static_cast<unsigned long>(hr));
        if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderFunc(m_driverDevice,textureVertex.ShaderHandle);
        if (FAILED(hr)) return hr;
        hr = api.pfnSetPixelShader(m_driverDevice,texturePixel.ShaderHandle);
        if (FAILED(hr)) return hr;
        auto sampler = [&](D3DDDITEXTURESTAGESTATETYPE id, UINT value) {
          D3DDDIARG_TEXTURESTAGESTATE args = {0,id,value};
          const HRESULT status = api.pfnSetTextureStageState(m_driverDevice,&args);
          std::printf("D3D9_TEXTURE_STATE id=%u value=%u hr=%08lx\n",UINT(id),value,static_cast<unsigned long>(status));
          return status;
        };
        for (const auto item : {D3DDDIARG_TEXTURESTAGESTATE{0,D3DDDITSS_MINFILTER,D3DTEXF_POINT},
          {0,D3DDDITSS_MAGFILTER,D3DTEXF_POINT},{0,D3DDDITSS_MIPFILTER,D3DTEXF_POINT},
          {0,D3DDDITSS_MIPMAPLODBIAS,0},{0,D3DDDITSS_SRGBTEXTURE,0},{0,D3DDDITSS_BORDERCOLOR,0xff647a98}}) {
          hr = sampler(item.State,item.Value); if (FAILED(hr)) return hr;
        }
        struct TextureVertex { float x,y,z,w,u,v; };
        TextureVertex verticesWithUv[] = {{1000,1000,0.5f,1,0,0},{-1,1,0.5f,1,0,0},
          {1,1,0.5f,1,0,0},{-1,-1,0.5f,1,0,0},{1,-1,0.5f,1,0,0}};
        const D3DDDIARG_SETSTREAMSOURCEUM textureStream = {0,sizeof(TextureVertex)};
        hr = api.pfnSetStreamSourceUm(m_driverDevice,&textureStream,verticesWithUv);
        if (FAILED(hr)) return hr;
        checked = 0; checksum = 2166136261u;
        for (UINT stage = 10; stage <= 17; ++stage) {
          const UINT mip = stage == 11 ? 1 : stage == 12 ? 2 : 0;
          const bool smallDestination = stage == 13 || stage == 14;
          const UINT address = stage == 15 ? D3DTADDRESS_BORDER : stage == 17 ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
          const float uv = stage >= 15 ? -0.375f : stage == 14 ? 0.5f : (stage == 11 || stage == 13) ? 0.75f : 0.375f;
          for (auto& vertex : verticesWithUv) { vertex.u = uv; vertex.v = (stage == 11 || stage == 13) ? 0.25f : uv; }
          hr = api.pfnSetTexture(m_driverDevice,0,smallDestination ? textureSmall.hResource : textureFull.hResource);
          if (FAILED(hr)) return hr;
          hr = sampler(D3DDDITSS_MAXMIPLEVEL,mip); if (FAILED(hr)) return hr;
          hr = sampler(D3DDDITSS_ADDRESSU,address); if (FAILED(hr)) return hr;
          hr = sampler(D3DDDITSS_ADDRESSV,address); if (FAILED(hr)) return hr;
          hr = sampler(D3DDDITSS_MINFILTER,stage == 14 ? D3DTEXF_LINEAR : D3DTEXF_POINT); if (FAILED(hr)) return hr;
          hr = sampler(D3DDDITSS_MAGFILTER,stage == 14 ? D3DTEXF_LINEAR : D3DTEXF_POINT); if (FAILED(hr)) return hr;
          fill.FillColor = 0xff17293b;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
          std::printf("D3D9_TEXTURE_DRAW stage=%u chain=%s mip=%u address=%u filter=%s\n",
            stage,smallDestination ? "small" : "full",mip,address,stage == 14 ? "linear" : "point");
          hr = draw(stage); if (FAILED(hr)) return hr;
        }
        if (textureData != originalTextureData) return E_FAIL;
        for (HANDLE resource : {textureSmall.hResource,textureFull.hResource,textureSource.hResource}) {
          hr = api.pfnDestroyResource(m_driverDevice,resource); if (FAILED(hr)) return hr;
          if (api.pfnSetTexture(m_driverDevice,0,resource) != E_INVALIDARG) return E_FAIL;
        }
        hr = api.pfnDeleteVertexShaderFunc(m_driverDevice,textureVertex.ShaderHandle); if (FAILED(hr)) return hr;
        hr = api.pfnDeletePixelShader(m_driverDevice,texturePixel.ShaderHandle); if (FAILED(hr)) return hr;
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,declaration.ShaderHandle); if (FAILED(hr)) return hr;
        std::printf("D3D9_TEXTURE_READBACK PASS pixels=%u checksum=%08x padding=retained levels=3/2 filters=point/linear address=border/clamp/wrap\n",checked,checksum);
      }
      if (buffers) {
        if (!api.pfnSetStreamSource || !api.pfnSetIndices || !api.pfnDrawIndexedPrimitive) return E_FAIL;
        char owners[4];
        // Nonzero binding/declaration offsets, padded strides, and no unused
        // trailing stride bytes make every address component observable.
        std::array<uint8_t,124> positionBytes; positionBytes.fill(0xcd);
        std::array<uint8_t,44> colorBytes; colorBytes.fill(0xcd);
        const float positions[][4] = {{1000,1000,0.5f,1},{-1,1,0.5f,1},{1,1,0.5f,1},
          {-1,-1,0.5f,1},{1,-1,0.5f,1}};
        for (UINT i = 0; i < 5; ++i) {
          std::memcpy(positionBytes.data() + 12 + i * 24,positions[i],16);
          const UINT color = 0xffa03658;
          std::memcpy(colorBytes.data() + 8 + i * 8,&color,4);
        }
        const UINT16 indices16[] = {UINT16_MAX,UINT16_MAX,3,4,5,5,4,6};
        const UINT32 indices32[] = {UINT32_MAX,UINT32_MAX,UINT32_MAX,3,4,5,5,4,6};
        HANDLE resources[4] = {};
        const void* initial[] = {positionBytes.data(),colorBytes.data(),indices16,indices32};
        const UINT sizes[] = {UINT(positionBytes.size()),UINT(colorBytes.size()),sizeof(indices16),sizeof(indices32)};
        for (UINT i = 0; i < 4; ++i) {
          D3DDDI_SURFACEINFO info = {sizes[i],UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
          D3DDDIARG_CREATERESOURCE resource = {};
          resource.hResource = &owners[i]; resource.pSurfList = &info; resource.SurfCount = 1;
          resource.Pool = D3DDDIPOOL_LOCALVIDMEM;
          resource.Format = static_cast<D3DDDIFORMAT>(i < 2 ? D3DFMT_VERTEXDATA : i == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32);
          resource.Flags.VertexBuffer = i < 2; resource.Flags.IndexBuffer = i >= 2;
          resource.Flags.Dynamic = resource.Flags.WriteOnly = i == 1;
          hr = api.pfnCreateResource(m_driverDevice,&resource);
          std::printf("D3D9_BUFFER_CREATE slot=%u bytes=%u dynamic=%u hr=%08lx\n",i,sizes[i],UINT(i == 1),static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          resources[i] = resource.hResource;
          D3DDDIARG_LOCK mapping = {}; mapping.hResource = resources[i]; mapping.Flags.WriteOnly = 1;
          if (i == 1) mapping.Flags.Discard = 1;
          hr = api.pfnLock(m_driverDevice,&mapping); if (FAILED(hr)) return hr;
          if (!mapping.pSurfData || mapping.Pitch || mapping.SlicePitch) return E_FAIL;
          std::memcpy(mapping.pSurfData,initial[i],sizes[i]);
          D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = resources[i];
          hr = api.pfnUnlock(m_driverDevice,&unmap); if (FAILED(hr)) return hr;
        }
        const D3DDDIVERTEXELEMENT bufferElements[] = {{3,4,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0},
          {7,4,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
        declaration = {2,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&declaration,bufferElements); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,declaration.ShaderHandle); if (FAILED(hr)) return hr;
        const UINT vs[] = {0xfffe0200,
          0x0200001f,0x80000000,0x900f0000,0x0200001f,0x8000000a,0x900f0001,
          0x02000001,0xc00f0000,0x90e40000,0x02000001,0xd00f0000,0x90e40001,0x0000ffff};
        const UINT ps[] = {0xffff0200,0x0200001f,0x80000000,0x900f0000,
          0x02000001,0x800f0800,0x90e40000,0x0000ffff};
        D3DDDIARG_CREATEVERTEXSHADERFUNC vertexShader = {sizeof(vs),nullptr};
        D3DDDIARG_CREATEPIXELSHADER pixelShader = {sizeof(ps),nullptr};
        hr = api.pfnCreateVertexShaderFunc(m_driverDevice,&vertexShader,vs); if (FAILED(hr)) return hr;
        hr = api.pfnCreatePixelShader(m_driverDevice,&pixelShader,ps); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderFunc(m_driverDevice,vertexShader.ShaderHandle); if (FAILED(hr)) return hr;
        hr = api.pfnSetPixelShader(m_driverDevice,pixelShader.ShaderHandle); if (FAILED(hr)) return hr;
        D3DDDIARG_SETSTREAMSOURCEUM noUser = {0,UINT_MAX};
        hr = api.pfnSetStreamSourceUm(m_driverDevice,&noUser,nullptr); if (FAILED(hr)) return hr;
        D3DDDIARG_SETSTREAMSOURCE bufferStream = {3,resources[0],8,24};
        hr = api.pfnSetStreamSource(m_driverDevice,&bufferStream); if (FAILED(hr)) return hr;
        bufferStream = {7,resources[1],4,8};
        hr = api.pfnSetStreamSource(m_driverDevice,&bufferStream); if (FAILED(hr)) return hr;
        checked = 0; checksum = 2166136261u;
        for (UINT stage = 18; stage <= 23; ++stage) {
          if (stage == 21 || stage == 22) {
            D3DDDIARG_LOCK mapping = {}; mapping.hResource = resources[1]; mapping.Flags.WriteOnly = 1;
            if (stage == 21) { mapping.Flags.NoOverwrite = mapping.Flags.RangeValid = 1; mapping.Range = {16,28}; }
            else mapping.Flags.Discard = 1;
            hr = api.pfnLock(m_driverDevice,&mapping); if (FAILED(hr)) return hr;
            if (!mapping.pSurfData || mapping.Pitch || mapping.SlicePitch) return E_FAIL;
            const UINT color = stage == 21 ? 0xff2785b3 : 0xffe19c47;
            if (stage == 22) std::memset(mapping.pSurfData,0xcd,colorBytes.size());
            for (UINT i = 1; i < 5; ++i)
              std::memcpy(static_cast<uint8_t*>(mapping.pSurfData) + (stage == 21 ? (i-1)*8 : 8+i*8),&color,4);
            D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = resources[1];
            hr = api.pfnUnlock(m_driverDevice,&unmap); if (FAILED(hr)) return hr;
            std::printf("D3D9_BUFFER_UPDATE stage=%u mode=%s offset=%u bytes=%u\n",stage,
              stage == 21 ? "nooverwrite" : "discard",stage == 21 ? 16u : 0u,stage == 21 ? 28u : 44u);
          }
          if (stage == 23) {
            D3DDDIARG_LOCK mapping = {}; mapping.hResource = resources[2];
            mapping.Flags.RangeValid = mapping.Flags.WriteOnly = 1; mapping.Range = {4,12};
            hr = api.pfnLock(m_driverDevice,&mapping); if (FAILED(hr)) return hr;
            const UINT16 replacement[] = {0,1,2,2,1,3};
            std::memcpy(mapping.pSurfData,replacement,sizeof(replacement));
            D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = resources[2];
            hr = api.pfnUnlock(m_driverDevice,&unmap); if (FAILED(hr)) return hr;
          }
          fill.FillColor = 0xff0a152d;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_SCENECAPTURE,1); if (FAILED(hr)) return hr;
          if (stage == 18) {
            const D3DDDIARG_DRAWPRIMITIVE bufferPrimitive = {D3DPT_TRIANGLESTRIP,1,2};
            hr = api.pfnDrawPrimitive(m_driverDevice,&bufferPrimitive,nullptr);
          } else {
            const UINT slot = stage == 19 || stage == 23 ? 2 : 3;
            D3DDDIARG_SETINDICES indices = {resources[slot],slot == 2 ? 2u : 4u};
            hr = api.pfnSetIndices(m_driverDevice,&indices);
            if (SUCCEEDED(hr)) {
              const D3DDDIARG_DRAWINDEXEDPRIMITIVE bufferIndexed = {D3DPT_TRIANGLELIST,
                stage == 23 ? 1 : -2,stage == 23 ? 0u : 3u,4,slot == 2 ? 2u : 3u,2};
              hr = api.pfnDrawIndexedPrimitive(m_driverDevice,&bufferIndexed);
            }
          }
          std::printf("D3D9_BUFFER_DRAW stage=%u mode=%s base=%d min=%u start=%u hr=%08lx\n",stage,
            stage == 18 ? "vertex" : stage == 19 || stage == 23 ? "index16" : "index32",
            stage == 18 ? 0 : stage == 23 ? 1 : -2,stage == 18 || stage == 23 ? 0u : 3u,
            stage == 18 ? 1u : stage == 19 || stage == 23 ? 2u : 3u,static_cast<unsigned long>(hr));
          const HRESULT ended = state(D3DDDIRS_SCENECAPTURE,0);
          if (FAILED(hr)) return hr;
          if (FAILED(ended)) return ended;
          hr = readback(stage); if (FAILED(hr)) return hr;
        }
        D3DDDIARG_LOCK mapping = {}; mapping.hResource = resources[0]; mapping.Flags.ReadOnly = 1;
        hr = api.pfnLock(m_driverDevice,&mapping); if (FAILED(hr)) return hr;
        const bool retained = mapping.pSurfData && !std::memcmp(mapping.pSurfData,positionBytes.data(),positionBytes.size());
        D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = resources[0];
        hr = api.pfnUnlock(m_driverDevice,&unmap); if (FAILED(hr)) return hr;
        if (!retained) return E_FAIL;
        for (HANDLE resource : resources) {
          hr = api.pfnDestroyResource(m_driverDevice,resource); if (FAILED(hr)) return hr;
          if (api.pfnDestroyResource(m_driverDevice,resource) != E_INVALIDARG) return E_FAIL;
        }
        hr = api.pfnDeleteVertexShaderFunc(m_driverDevice,vertexShader.ShaderHandle); if (FAILED(hr)) return hr;
        hr = api.pfnDeletePixelShader(m_driverDevice,pixelShader.ShaderHandle); if (FAILED(hr)) return hr;
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,declaration.ShaderHandle); if (FAILED(hr)) return hr;
        std::printf("D3D9_BUFFER_READBACK PASS pixels=%u checksum=%08x padding=retained streams=3/7 indices=16/32 base=negative/positive dynamic=nooverwrite/discard\n",checked,checksum);
      }
      if (depthStencil) {
        if (!api.pfnSetDepthStencil) return E_FAIL;
        char depthOwners[2];
        HANDLE depthSurfaces[2] = {};
        D3DDDI_SURFACEINFO depthInfo = {8,8,1,nullptr,0,0};
        for (UINT depthSlot = 0; depthSlot < 2; ++depthSlot) {
          D3DDDIARG_CREATERESOURCE depthResource = {};
          depthResource.hResource = &depthOwners[depthSlot];
          depthResource.Format = static_cast<D3DDDIFORMAT>(depthSlot ? D3DFMT_D24S8 : D3DFMT_D16);
          depthResource.Pool = D3DDDIPOOL_LOCALVIDMEM;
          depthResource.Flags.ZBuffer = depthResource.Flags.NotLockable = 1;
          depthResource.pSurfList = &depthInfo; depthResource.SurfCount = 1;
          hr = api.pfnCreateResource(m_driverDevice,&depthResource);
          std::printf("D3D9_DEPTH_CREATE slot=%u format=%u hr=%08lx\n",depthSlot,
            UINT(depthResource.Format),static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          depthSurfaces[depthSlot] = depthResource.hResource;
        }
        D3DDDIVERTEXELEMENT depthElements[] = {{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITIONT,0},
          {0,16,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
        D3DDDIARG_CREATEVERTEXSHADERDECL depthDeclaration = {2,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&depthDeclaration,depthElements); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,depthDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        for (const auto depthTextureState : {D3DDDIARG_TEXTURESTAGESTATE{0,D3DDDITSS_COLOROP,D3DTOP_SELECTARG1},
          {0,D3DDDITSS_COLORARG1,D3DTA_DIFFUSE},{0,D3DDDITSS_ALPHAOP,D3DTOP_SELECTARG1},
          {0,D3DDDITSS_ALPHAARG1,D3DTA_DIFFUSE},{1,D3DDDITSS_COLOROP,D3DTOP_DISABLE}}) {
          hr = api.pfnSetTextureStageState(m_driverDevice,&depthTextureState); if (FAILED(hr)) return hr;
        }
        for (const auto depthState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_LIGHTING,0},
          {D3DDDIRS_CULLMODE,D3DCULL_NONE},{D3DDDIRS_ALPHABLENDENABLE,0},{D3DDDIRS_ALPHATESTENABLE,0},
          {D3DDDIRS_FOGENABLE,0},{D3DDDIRS_DITHERENABLE,0},{D3DDDIRS_COLORWRITEENABLE,15},
          {D3DDDIRS_SRGBWRITEENABLE,0}}) {
          hr = state(depthState.State,depthState.Value); if (FAILED(hr)) return hr;
        }
        const D3DDDIARG_ZRANGE depthRange = {0.0f,1.0f};
        hr = api.pfnSetZRange(m_driverDevice,&depthRange); if (FAILED(hr)) return hr;
        auto depthArea = [&](const D3DDDIARG_VIEWPORTINFO& depthViewport, const RECT& depthScissor, bool enable) {
          HRESULT depthStatus = api.pfnSetViewport(m_driverDevice,&depthViewport);
          if (SUCCEEDED(depthStatus)) depthStatus = api.pfnSetScissorRect(m_driverDevice,&depthScissor);
          if (SUCCEEDED(depthStatus)) depthStatus = state(D3DDDIRS_SCISSORTESTENABLE,enable ? 1 : 0);
          return depthStatus;
        };
        const D3DDDIARG_VIEWPORTINFO depthFullViewport = {0,0,8,8}, depthClipViewport = {2,1,4,5};
        const RECT depthClipScissor = {3,2,7,7}, depthOutsideScissor = {7,7,8,8}, depthExplicit = {0,0,4,8};
        auto depthClear = [&](UINT depthStage, UINT depthFlags, float depthValue, UINT stencilValue,
                              UINT depthCount, const RECT* depthRects) {
          D3DDDIARG_CLEAR depthClearArgs = {};
          depthClearArgs.Flags = depthFlags; depthClearArgs.FillDepth = depthValue;
          depthClearArgs.FillStencil = stencilValue; depthClearArgs.FillColor = 0xffb062c4;
          const HRESULT depthStatus = api.pfnClear(m_driverDevice,&depthClearArgs,depthCount,depthRects);
          std::printf("D3D9_DEPTH_CLEAR stage=%u flags=%u depth=%.3f stencil=%u rects=%u hr=%08lx\n",
            depthStage,depthFlags,depthValue,stencilValue,depthCount,static_cast<unsigned long>(depthStatus));
          return depthStatus;
        };
        auto depthDraw = [&](UINT depthStage, float depthValue, D3DCOLOR depthColor) {
          for (UINT depthVertex = 1; depthVertex < 5; ++depthVertex) {
            vertices[depthVertex].z = depthValue; vertices[depthVertex].color = depthColor;
          }
          HRESULT depthStatus = api.pfnSetStreamSourceUm(m_driverDevice,&stream,vertices);
          if (SUCCEEDED(depthStatus)) depthStatus = state(D3DDDIRS_SCENECAPTURE,1);
          if (FAILED(depthStatus)) return depthStatus;
          depthStatus = api.pfnDrawPrimitive(m_driverDevice,&primitive,nullptr);
          std::printf("D3D9_DEPTH_DRAW stage=%u depth=%.3f color=%08x hr=%08lx\n",
            depthStage,depthValue,depthColor,static_cast<unsigned long>(depthStatus));
          const HRESULT depthEnded = state(D3DDDIRS_SCENECAPTURE,0);
          return FAILED(depthStatus) ? depthStatus : depthEnded;
        };
        checked = 0; checksum = 2166136261u;
        for (UINT depthStage = 24; depthStage <= 37; ++depthStage) {
          D3DDDIARG_SETDEPTHSTENCIL depthBind = {depthSurfaces[depthStage <= 31 ? 0 : 1]};
          hr = api.pfnSetDepthStencil(m_driverDevice,&depthBind); if (FAILED(hr)) return hr;
          hr = depthArea(depthFullViewport,full,false); if (FAILED(hr)) return hr;
          for (const auto depthState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_ZENABLE,1},
            {D3DDDIRS_ZWRITEENABLE,1},{D3DDDIRS_ZFUNC,D3DCMP_LESS},{D3DDDIRS_STENCILENABLE,depthStage >= 32 ? 1u : 0u},
            {D3DDDIRS_STENCILFUNC,D3DCMP_EQUAL},{D3DDDIRS_STENCILREF,7},{D3DDDIRS_STENCILMASK,255},
            {D3DDDIRS_STENCILWRITEMASK,255},{D3DDDIRS_STENCILFAIL,D3DSTENCILOP_KEEP},
            {D3DDDIRS_STENCILZFAIL,D3DSTENCILOP_KEEP},{D3DDDIRS_STENCILPASS,D3DSTENCILOP_KEEP},
            {D3DDDIRS_TWOSIDEDSTENCILMODE,0}}) {
            hr = state(depthState.State,depthState.Value); if (FAILED(hr)) return hr;
          }
          fill.Flags = D3DCLEAR_TARGET; fill.FillColor = 0xff07131f;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
          const float initialDepth = depthStage == 27 || depthStage == 29 || depthStage == 30
            || depthStage == 32 || depthStage == 37 ? 0.0f : depthStage == 26 ? 0.5f : 0.75f;
          hr = depthClear(depthStage,D3DCLEAR_ZBUFFER | (depthStage >= 32 ? D3DCLEAR_STENCIL : 0),
            initialDepth,depthStage == 32 ? 7u : depthStage == 34 ? 160u : 0u,1,&full);
          if (FAILED(hr)) return hr;
          if (depthStage == 24 || depthStage == 25 || depthStage == 26) {
            hr = state(D3DDDIRS_ZWRITEENABLE,depthStage == 25 ? 0 : 1); if (FAILED(hr)) return hr;
            hr = state(D3DDDIRS_ZFUNC,depthStage == 26 ? D3DCMP_GREATEREQUAL : D3DCMP_LESS); if (FAILED(hr)) return hr;
            hr = depthDraw(depthStage,depthStage == 26 ? 0.75f : 0.25f,
              depthStage == 26 ? 0xff356bd1 : 0xff39a57b); if (FAILED(hr)) return hr;
            hr = depthDraw(depthStage,depthStage == 26 ? 0.25f : 0.625f,0xffc0472a); if (FAILED(hr)) return hr;
          } else if (depthStage <= 31) {
            if (depthStage != 28) {
              hr = depthArea(depthClipViewport,depthStage == 31 ? depthOutsideScissor : depthClipScissor,true);
              if (FAILED(hr)) return hr;
            }
            hr = depthClear(depthStage,D3DCLEAR_ZBUFFER | (depthStage >= 29 ? 8 : 0),
              depthStage == 28 || depthStage == 31 ? 0.0f : 1.0f,0,
              depthStage == 27 || depthStage == 30 ? 1u : 0u,
              depthStage == 27 ? &full : depthStage == 30 ? &depthExplicit : reinterpret_cast<const RECT*>(UINT_PTR(1)));
            if (FAILED(hr)) return hr;
            hr = depthArea(depthFullViewport,full,false); if (FAILED(hr)) return hr;
            hr = depthDraw(depthStage,0.5f,depthStage == 27 ? 0xff438bc2
              : depthStage == 28 || depthStage == 31 ? 0xff7bb85f : 0xffdd9235); if (FAILED(hr)) return hr;
          } else if (depthStage == 32 || depthStage == 33) {
            hr = depthClear(depthStage,depthStage == 32 ? D3DCLEAR_ZBUFFER : D3DCLEAR_STENCIL,
              1.0f,7,1,&full); if (FAILED(hr)) return hr;
            hr = depthDraw(depthStage,0.5f,depthStage == 32 ? 0xff8b57c5 : 0xffbd6382); if (FAILED(hr)) return hr;
          } else if (depthStage == 34) {
            for (const auto depthState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_ZENABLE,0},
              {D3DDDIRS_STENCILFUNC,D3DCMP_ALWAYS},{D3DDDIRS_STENCILREF,181},
              {D3DDDIRS_STENCILWRITEMASK,15},{D3DDDIRS_STENCILPASS,D3DSTENCILOP_REPLACE}}) {
              hr = state(depthState.State,depthState.Value); if (FAILED(hr)) return hr;
            }
            hr = depthDraw(depthStage,0.5f,0xffc0472a); if (FAILED(hr)) return hr;
            hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
            for (const auto depthState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_STENCILFUNC,D3DCMP_EQUAL},
              {D3DDDIRS_STENCILREF,165},{D3DDDIRS_STENCILWRITEMASK,0},{D3DDDIRS_STENCILPASS,D3DSTENCILOP_KEEP}}) {
              hr = state(depthState.State,depthState.Value); if (FAILED(hr)) return hr;
            }
            hr = depthDraw(depthStage,0.5f,0xff4eac77); if (FAILED(hr)) return hr;
          } else {
            hr = depthArea(depthClipViewport,depthClipScissor,true); if (FAILED(hr)) return hr;
            hr = depthClear(depthStage,depthStage == 37 ? 15u : D3DCLEAR_STENCIL | (depthStage == 36 ? 8u : 0u),
              1.0f,7,depthStage == 36 ? 0u : 1u,
              depthStage == 35 ? &full : depthStage == 36 ? reinterpret_cast<const RECT*>(UINT_PTR(1)) : &depthExplicit);
            if (FAILED(hr)) return hr;
            if (depthStage == 35) {
              hr = depthClear(depthStage,D3DCLEAR_STENCIL,0.0f,0,0,reinterpret_cast<const RECT*>(UINT_PTR(1)));
              if (FAILED(hr)) return hr;
            }
            hr = depthArea(depthFullViewport,full,false); if (FAILED(hr)) return hr;
            hr = state(D3DDDIRS_ZENABLE,depthStage == 37 ? 1 : 0); if (FAILED(hr)) return hr;
            hr = depthDraw(depthStage,0.5f,depthStage == 35 ? 0xff649dd8 : depthStage == 36 ? 0xffceaf45 : 0xff2fafbf);
            if (FAILED(hr)) return hr;
          }
          hr = readback(depthStage); if (FAILED(hr)) return hr;
        }
        for (HANDLE depthResource : depthSurfaces) {
          hr = api.pfnDestroyResource(m_driverDevice,depthResource); if (FAILED(hr)) return hr;
          const D3DDDIARG_SETDEPTHSTENCIL staleDepth = {depthResource};
          if (api.pfnSetDepthStencil(m_driverDevice,&staleDepth) != E_INVALIDARG) return E_FAIL;
        }
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,depthDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        hr = state(D3DDDIRS_ZENABLE,0); if (FAILED(hr)) return hr;
        hr = state(D3DDDIRS_STENCILENABLE,0); if (FAILED(hr)) return hr;
        std::printf("D3D9_DEPTH_READBACK PASS pixels=%u checksum=%08x padding=retained formats=D16/D24S8 depth=less/gequal/write stencil=preserved/masked clear=preclipped/computed/empty\n",checked,checksum);
      }
      if (fixedFunction) {
        if (!api.pfnSetTransform || !api.pfnMultiplyTransform || !api.pfnSetMaterial
            || !api.pfnCreateLight || !api.pfnSetLight || !api.pfnDestroyLight) return E_FAIL;
        struct FixedVertex { float x,y,z,nx,ny,nz; D3DCOLOR color; };
        static_assert(sizeof(FixedVertex) == 28);
        const D3DDDIVERTEXELEMENT fixedElements[] = {{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
          {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
        D3DDDIARG_CREATEVERTEXSHADERDECL fixedDeclaration = {3,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&fixedDeclaration,fixedElements); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,fixedDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        const D3DDDIARG_SETSTREAMSOURCEUM fixedStream = {0,sizeof(FixedVertex)};
        const D3DDDIARG_VIEWPORTINFO fixedViewport = {0,0,8,8};
        hr = api.pfnSetViewport(m_driverDevice,&fixedViewport); if (FAILED(hr)) return hr;
        for (const auto fixedState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_ZENABLE,0},
          {D3DDDIRS_STENCILENABLE,0},{D3DDDIRS_SCISSORTESTENABLE,0},{D3DDDIRS_AMBIENT,0},
          {D3DDDIRS_SPECULARENABLE,0},{D3DDDIRS_NORMALIZENORMALS,1},{D3DDDIRS_LOCALVIEWER,0},
          {D3DDDIRS_DIFFUSEMATERIALSOURCE,D3DMCS_MATERIAL},{D3DDDIRS_AMBIENTMATERIALSOURCE,D3DMCS_MATERIAL},
          {D3DDDIRS_EMISSIVEMATERIALSOURCE,D3DMCS_MATERIAL},{D3DDDIRS_SPECULARMATERIALSOURCE,D3DMCS_MATERIAL}}) {
          hr = state(fixedState.State,fixedState.Value); if (FAILED(hr)) return hr;
        }
        const D3DMATRIX identity = [] {
          D3DMATRIX matrix = {}; matrix._11 = matrix._22 = matrix._33 = matrix._44 = 1.0f; return matrix;
        }();
        auto fixedMatrix = [&](UINT fixedStage, D3DTRANSFORMSTATETYPE type, const D3DMATRIX& matrix, bool multiply) {
          HRESULT status;
          if (multiply) {
            const D3DDDIARG_MULTIPLYTRANSFORM args = {type,matrix};
            status = api.pfnMultiplyTransform(m_driverDevice,&args);
          } else {
            const D3DDDIARG_SETTRANSFORM args = {type,matrix};
            status = api.pfnSetTransform(m_driverDevice,&args);
          }
          std::printf("D3D9_FIXED_TRANSFORM stage=%u type=%u multiply=%u hr=%08lx\n",
            fixedStage,UINT(type),UINT(multiply),static_cast<unsigned long>(status));
          return status;
        };
        auto fixedData = [&](UINT fixedStage, UINT index, const D3DDDI_LIGHT& light) {
          const D3DDDIARG_SETLIGHT args = {index,D3DDDI_SETLIGHT_DATA};
          const HRESULT status = api.pfnSetLight(m_driverDevice,&args,&light);
          std::printf("D3D9_FIXED_LIGHT_DATA stage=%u index=%u type=%u hr=%08lx\n",
            fixedStage,index,UINT(light.Type),static_cast<unsigned long>(status));
          return status;
        };
        auto fixedEnable = [&](UINT fixedStage, UINT index, bool enable) {
          const D3DDDIARG_SETLIGHT args = {index,enable ? D3DDDI_SETLIGHT_ENABLE : D3DDDI_SETLIGHT_DISABLE};
          const HRESULT status = api.pfnSetLight(m_driverDevice,&args,reinterpret_cast<const D3DDDI_LIGHT*>(UINT_PTR(1)));
          std::printf("D3D9_FIXED_LIGHT_ENABLE stage=%u index=%u enable=%u hr=%08lx\n",
            fixedStage,index,UINT(enable),static_cast<unsigned long>(status));
          return status;
        };
        D3DDDI_LIGHT primary = {}; primary.Type = D3DLIGHT_DIRECTIONAL;
        primary.Diffuse = {1,0,0,0}; primary.Direction.z = 1;
        constexpr UINT firstLight = UINT_MAX, secondLight = 7, reusedLight = 0x80000001u;
        checked = 0; checksum = 2166136261u;
        for (UINT fixedStage = 38; fixedStage <= 58; ++fixedStage) {
          for (const auto type : {D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION}) {
            hr = fixedMatrix(fixedStage,type,identity,false); if (FAILED(hr)) return hr;
          }
          hr = state(D3DDDIRS_LIGHTING,fixedStage >= 44 ? 1 : 0); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_COLORVERTEX,fixedStage >= 44 ? 0 : 1); if (FAILED(hr)) return hr;
          D3DDDIARG_SETMATERIAL material = {};
          material.Diffuse = material.Ambient = {1,1,1,1};
          if (fixedStage == 44 || fixedStage == 48) material.Emissive.b = 1;
          hr = api.pfnSetMaterial(m_driverDevice,&material); if (FAILED(hr)) return hr;
          FixedVertex fixedVertices[5] = {};
          fixedVertices[0] = {1000,1000,.5f,0,0,-1,0xff000000};
          const UINT transformColors[] = {0xffb03d87,0xff63ba49,0xffd18c37,0xff75a5d1,0xff9f68cb,0xff43b49c};
          for (UINT v = 1; v < 5; ++v) {
            fixedVertices[v] = {fixedStage < 44 ? (v == 1 || v == 3 ? -.625f : .375f) : (v == 1 || v == 3 ? -2.0f : 2.0f),
              fixedStage < 44 ? (v <= 2 ? .625f : -.375f) : (v <= 2 ? 2.0f : -2.0f),
              .5f,0,0,fixedStage >= 51 ? 1.0f : -1.0f,fixedStage < 44 ? transformColors[fixedStage-38] : 0xffa040d0};
          }
          if (fixedStage == 39 || fixedStage == 41 || fixedStage == 42 || fixedStage == 43) {
            D3DMATRIX translation = identity;
            translation._41 = fixedStage == 42 ? -.5f : fixedStage == 43 ? .25f : .5f;
            translation._42 = fixedStage == 39 ? -.25f : fixedStage == 43 ? .25f : 0.0f;
            const auto type = fixedStage == 42 ? D3DTS_VIEW : fixedStage == 43 ? D3DTS_PROJECTION : D3DTS_WORLD;
            hr = fixedMatrix(fixedStage,type,translation,false); if (FAILED(hr)) return hr;
            if (fixedStage != 39) {
              D3DMATRIX scaling = identity; scaling._11 = scaling._22 = .5f;
              hr = fixedMatrix(fixedStage,type,scaling,true); if (FAILED(hr)) return hr;
            }
          } else if (fixedStage == 40) {
            D3DMATRIX translation = identity; translation._41 = -.25f; translation._42 = .25f;
            hr = fixedMatrix(fixedStage,D3DTS_VIEW,translation,false); if (FAILED(hr)) return hr;
            D3DMATRIX scaling = identity; scaling._11 = scaling._22 = .5f;
            hr = fixedMatrix(fixedStage,D3DTS_PROJECTION,scaling,false); if (FAILED(hr)) return hr;
          }
          if (fixedStage == 45 || fixedStage == 52 || fixedStage == 54) {
            const D3DDDIARG_CREATELIGHT args = {fixedStage == 45 ? firstLight : fixedStage == 52 ? secondLight : reusedLight};
            hr = api.pfnCreateLight(m_driverDevice,&args);
            std::printf("D3D9_FIXED_LIGHT_CREATE stage=%u index=%u hr=%08lx\n",
              fixedStage,args.Index,static_cast<unsigned long>(hr));
            if (FAILED(hr)) return hr;
          }
          if (fixedStage == 45 || fixedStage == 47 || fixedStage == 50) {
            if (fixedStage == 47) primary.Diffuse = {0,1,0,0};
            if (fixedStage == 50) primary.Direction.z = -1;
            hr = fixedData(fixedStage,firstLight,primary); if (FAILED(hr)) return hr;
          }
          if (fixedStage == 46 || fixedStage == 48 || fixedStage == 49) {
            hr = fixedEnable(fixedStage,firstLight,fixedStage != 48); if (FAILED(hr)) return hr;
          }
          if (fixedStage == 52) {
            D3DDDI_LIGHT secondary = {}; secondary.Type = D3DLIGHT_DIRECTIONAL;
            secondary.Diffuse = {0,0,1,0}; secondary.Direction.z = -1;
            hr = fixedData(fixedStage,secondLight,secondary); if (FAILED(hr)) return hr;
            hr = fixedEnable(fixedStage,secondLight,true); if (FAILED(hr)) return hr;
          }
          if (fixedStage == 53) {
            const D3DDDIARG_DESTROYLIGHT args = {firstLight};
            hr = api.pfnDestroyLight(m_driverDevice,&args);
            std::printf("D3D9_FIXED_LIGHT_DESTROY stage=%u index=%u hr=%08lx\n",
              fixedStage,firstLight,static_cast<unsigned long>(hr));
            if (FAILED(hr)) return hr;
            const D3DDDIARG_SETLIGHT stale = {firstLight,D3DDDI_SETLIGHT_ENABLE};
            if (api.pfnSetLight(m_driverDevice,&stale,nullptr) != E_INVALIDARG) return E_FAIL;
          }
          if (fixedStage >= 54) {
            if (fixedStage == 54) {
              primary = {}; primary.Type = D3DLIGHT_DIRECTIONAL;
              primary.Diffuse = {1,0,0,0}; primary.Direction.z = -1;
            } else if (fixedStage == 55) {
              primary = {}; primary.Type = D3DLIGHT_POINT;
              primary.Ambient = {1,0,0,0}; primary.Position.z = -4;
              primary.Range = primary.Attenuation0 = 1;
            } else if (fixedStage == 56) primary.Range = 1000;
            else if (fixedStage == 57) {
              primary = {}; primary.Type = D3DLIGHT_SPOT;
              primary.Ambient = {0,1,0,0}; primary.Position.z = -4; primary.Direction.z = 1;
              primary.Range = 1000; primary.Attenuation0 = primary.Falloff = 1;
              primary.Theta = primary.Phi = .25f;
            } else primary.Theta = primary.Phi = 3.14159265358979323846f;
            hr = fixedData(fixedStage,reusedLight,primary); if (FAILED(hr)) return hr;
            if (fixedStage == 54) {
              hr = fixedEnable(fixedStage,reusedLight,true); if (FAILED(hr)) return hr;
            }
          }
          hr = api.pfnSetStreamSourceUm(m_driverDevice,&fixedStream,fixedVertices); if (FAILED(hr)) return hr;
          fill.FillColor = 0xff0b1723;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_SCENECAPTURE,1); if (FAILED(hr)) return hr;
          hr = api.pfnDrawPrimitive(m_driverDevice,&primitive,nullptr);
          std::printf("D3D9_FIXED_DRAW stage=%u lighting=%u normal_z=%d hr=%08lx\n",
            fixedStage,UINT(fixedStage >= 44),fixedStage >= 51 ? 1 : -1,static_cast<unsigned long>(hr));
          const HRESULT ended = state(D3DDDIRS_SCENECAPTURE,0);
          if (FAILED(hr)) return hr;
          if (FAILED(ended)) return ended;
          hr = readback(fixedStage); if (FAILED(hr)) return hr;
        }
        const D3DDDIARG_DESTROYLIGHT retire = {secondLight};
        hr = api.pfnDestroyLight(m_driverDevice,&retire); if (FAILED(hr)) return hr;
        // Leave one owned enabled light for DestroyDevice's worker cleanup.
        std::printf("D3D9_FIXED_LIGHT_CLOSE index=%u enabled=1\n",reusedLight);
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,fixedDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        std::printf("D3D9_FIXED_READBACK PASS pixels=%u checksum=%08x padding=retained transforms=world/view/projection/multiply lights=directional/point/spot lifetime=sparse/enable/disable/destroy/reuse\n",checked,checksum);
      }
      if (bufferTransfer) {
        if (!api.pfnBufBlt || !api.pfnSetStreamSource || !api.pfnSetIndices
            || !api.pfnDrawIndexedPrimitive) return E_FAIL;
        constexpr UINT vertexBytes = 160, payloadBytes = 140, stride = 28;
        using ByteImage = std::array<uint8_t,vertexBytes>;
        using VertexPayload = std::array<uint8_t,payloadBytes>;
        std::array<ByteImage,9> expected;
        for (auto& image : expected) image.fill(0xcd);
        expected[4].fill(0xa9); expected[5].fill(0xa6);
        auto vertexPayload = [&](UINT color) {
          VertexPayload bytes; bytes.fill(0xcd);
          for (UINT v = 0; v < 5; ++v) {
            const float position[] = {v == 0 ? 1000.0f : v == 1 || v == 3 ? -2.0f : 10.0f,
              v == 0 ? 1000.0f : v <= 2 ? -2.0f : 10.0f,.5f,1.0f};
            const UINT diffuse = v == 0 ? 0xff000000 : color;
            std::memcpy(bytes.data() + v * stride + 3,position,sizeof(position));
            std::memcpy(bytes.data() + v * stride + 19,&diffuse,sizeof(diffuse));
          }
          return bytes;
        };
        const auto firstPayload = vertexPayload(0xffa05c71);
        const auto secondPayload = vertexPayload(0xffb08746);
        const auto thirdPayload = vertexPayload(0xff65b82f);
        std::copy(firstPayload.begin(),firstPayload.end(),expected[0].begin() + 5);
        std::copy(secondPayload.begin(),secondPayload.end(),expected[2].begin() + 9);
        std::array<uint8_t,192> borrowedInput, borrowedOutput;
        std::array<uint8_t,64> borrowedIndices;
        borrowedInput.fill(0xcd); borrowedOutput.fill(0xcd); borrowedIndices.fill(0xcd);
        std::copy(expected[0].begin(),expected[0].end(),borrowedInput.begin() + 16);
        std::copy(expected[4].begin(),expected[4].end(),borrowedOutput.begin() + 16);
        std::copy_n(expected[5].begin(),32,borrowedIndices.begin() + 16);
        void* external[] = {borrowedInput.data()+16,nullptr,nullptr,nullptr,
          borrowedOutput.data()+16,borrowedIndices.data()+16,nullptr,nullptr,nullptr};
        const UINT sizes[] = {160,160,160,160,160,32,32,64,160};
        char transferOwners[9]; HANDLE transferResources[9] = {};
        for (UINT slot = 0; slot < 9; ++slot) {
          const bool systemPool = slot == 0 || slot == 2 || slot == 4 || slot == 5 || slot == 7 || slot == 8;
          const bool dynamic = slot == 2 || slot == 3 || slot == 4 || slot == 7;
          D3DDDI_SURFACEINFO info = {sizes[slot],UINT_MAX,UINT_MAX,external[slot],UINT_MAX,UINT_MAX};
          D3DDDIARG_CREATERESOURCE resource = {};
          resource.hResource = &transferOwners[slot]; resource.pSurfList = &info; resource.SurfCount = 1;
          resource.Pool = systemPool ? D3DDDIPOOL_SYSTEMMEM : D3DDDIPOOL_LOCALVIDMEM;
          resource.Format = static_cast<D3DDDIFORMAT>(slot == 5 || slot == 6 ? D3DFMT_INDEX16 : slot == 7 ? D3DFMT_INDEX32 : D3DFMT_VERTEXDATA);
          resource.Flags.VertexBuffer = slot < 5 || slot == 8;
          resource.Flags.IndexBuffer = slot >= 5 && slot <= 7;
          resource.Flags.Dynamic = dynamic;
          resource.Flags.WriteOnly = slot == 1 || slot == 3 || slot == 6;
          hr = api.pfnCreateResource(m_driverDevice,&resource);
          std::printf("D3D9_TRANSFER_CREATE slot=%u bytes=%u pool=%s borrowed=%u dynamic=%u hr=%08lx\n",
            slot,sizes[slot],systemPool ? "system" : "default",UINT(external[slot] != nullptr),UINT(dynamic),static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          transferResources[slot] = resource.hResource;
          if (!external[slot]) {
            D3DDDIARG_LOCK mapping = {}; mapping.hResource = resource.hResource; mapping.Flags.WriteOnly = 1;
            hr = api.pfnLock(m_driverDevice,&mapping); if (FAILED(hr)) return hr;
            if (!mapping.pSurfData || mapping.Pitch || mapping.SlicePitch) return E_FAIL;
            std::memcpy(mapping.pSurfData,expected[slot].data(),sizes[slot]);
            D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = resource.hResource;
            hr = api.pfnUnlock(m_driverDevice,&unmap); if (FAILED(hr)) return hr;
          }
        }
        // Put the byte-range prefix in the stream binding. Dynamic SYSTEMMEM
        // uploads pack the declared vertex bytes into a fresh aligned buffer.
        const D3DDDIVERTEXELEMENT transferElements[] = {{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITIONT,0},
          {0,16,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
        D3DDDIARG_CREATEVERTEXSHADERDECL transferDeclaration = {2,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&transferDeclaration,transferElements); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,transferDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        const D3DDDIARG_SETSTREAMSOURCEUM noUser = {0,UINT_MAX};
        hr = api.pfnSetStreamSourceUm(m_driverDevice,&noUser,nullptr); if (FAILED(hr)) return hr;
        const D3DDDIARG_VIEWPORTINFO transferViewport = {0,0,8,8};
        hr = api.pfnSetViewport(m_driverDevice,&transferViewport); if (FAILED(hr)) return hr;
        for (const auto transferState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_LIGHTING,0},
          {D3DDDIRS_COLORVERTEX,1},{D3DDDIRS_ZENABLE,0},{D3DDDIRS_STENCILENABLE,0},
          {D3DDDIRS_SCISSORTESTENABLE,0},{D3DDDIRS_COLORWRITEENABLE,15}}) {
          hr = state(transferState.State,transferState.Value); if (FAILED(hr)) return hr;
        }
        auto transfer = [&](UINT stage,UINT source,UINT srcOffset,UINT destination,UINT dstOffset,UINT count) {
          D3DDDIARG_BUFFERBLT args = {};
          args.hSrcResource = transferResources[source]; args.SrcRange = {srcOffset,count};
          args.hDstResource = transferResources[destination]; args.Offset = dstOffset;
          const HRESULT status = api.pfnBufBlt(m_driverDevice,&args);
          std::printf("D3D9_TRANSFER_COPY stage=%u src=%u src_offset=%u dst=%u dst_offset=%u bytes=%u hr=%08lx\n",
            stage,source,srcOffset,destination,dstOffset,count,static_cast<unsigned long>(status));
          if (SUCCEEDED(status)) {
            const auto snapshot = expected[source];
            std::copy_n(snapshot.begin()+srcOffset,count,expected[destination].begin()+dstOffset);
          }
          return status;
        };
        UINT bytesChecked = 0, byteChecksum = 2166136261u;
        auto verifyBytes = [&](UINT stage,UINT slot) -> HRESULT {
          HRESULT status = transfer(stage,slot,0,8,0,sizes[slot]); if (FAILED(status)) return status;
          D3DDDIARG_LOCK mapping = {}; mapping.hResource = transferResources[8];
          mapping.Flags.ReadOnly = mapping.Flags.RangeValid = 1; mapping.Range = {0,sizes[slot]};
          status = api.pfnLock(m_driverDevice,&mapping); if (FAILED(status)) return status;
          if (!mapping.pSurfData || mapping.Pitch || mapping.SlicePitch) status = E_FAIL;
          UINT stageChecksum = 2166136261u;
          for (UINT i = 0; i < sizes[slot] && SUCCEEDED(status); ++i) {
            const uint8_t actual = static_cast<const uint8_t*>(mapping.pSurfData)[i];
            if (actual != expected[slot][i]) {
              std::printf("D3D9_TRANSFER_BYTE_MISMATCH stage=%u slot=%u offset=%u actual=%02x expected=%02x\n",
                stage,slot,i,unsigned(actual),unsigned(expected[slot][i]));
              status = E_FAIL; break;
            }
            ++bytesChecked; byteChecksum = (byteChecksum ^ actual) * 16777619u;
            stageChecksum = (stageChecksum ^ actual) * 16777619u;
          }
          D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = transferResources[8];
          const HRESULT unlocked = api.pfnUnlock(m_driverDevice,&unmap);
          if (FAILED(status)) return status;
          if (FAILED(unlocked)) return unlocked;
          std::printf("D3D9_TRANSFER_BYTE_READBACK stage=%u slot=%u bytes=%u checksum=%08x\n",stage,slot,sizes[slot],stageChecksum);
          return S_OK;
        };
        auto writeRange = [&](UINT stage,UINT slot,UINT offset,const void* data,UINT count) -> HRESULT {
          D3DDDIARG_LOCK mapping = {}; mapping.hResource = transferResources[slot];
          mapping.Flags.WriteOnly = mapping.Flags.RangeValid = 1;
          mapping.Flags.NotifyOnly = external[slot] != nullptr; mapping.Range = {offset,count};
          HRESULT status = api.pfnLock(m_driverDevice,&mapping);
          std::printf("D3D9_TRANSFER_LOCK stage=%u slot=%u offset=%u bytes=%u notify=%u hr=%08lx\n",
            stage,slot,offset,count,UINT(mapping.Flags.NotifyOnly),static_cast<unsigned long>(status));
          if (FAILED(status)) return status;
          if (!mapping.pSurfData || mapping.Pitch || mapping.SlicePitch
              || (external[slot] && mapping.pSurfData != static_cast<uint8_t*>(external[slot])+offset)) return E_FAIL;
          std::memcpy(mapping.pSurfData,data,count);
          D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = transferResources[slot];
          unmap.Flags.NotifyOnly = mapping.Flags.NotifyOnly;
          status = api.pfnUnlock(m_driverDevice,&unmap);
          if (SUCCEEDED(status)) std::memcpy(expected[slot].data()+offset,data,count);
          return status;
        };
        auto verifyExternal = [&](UINT slot) {
          const auto pointer = static_cast<const uint8_t*>(external[slot]);
          if (std::memcmp(pointer,expected[slot].data(),sizes[slot])) return false;
          for (UINT guard = 0; guard < 16; ++guard)
            if (pointer[int(guard)-16] != 0xcd || pointer[sizes[slot]+guard] != 0xcd) return false;
          return true;
        };
        auto transferDraw = [&](UINT stage,UINT vertexSlot,UINT offset,UINT indexSlot) -> HRESULT {
          const D3DDDIARG_SETSTREAMSOURCE stream = {0,transferResources[vertexSlot],offset,stride};
          HRESULT status = api.pfnSetStreamSource(m_driverDevice,&stream); if (FAILED(status)) return status;
          if (indexSlot) {
            const D3DDDIARG_SETINDICES indices = {transferResources[indexSlot],indexSlot == 6 ? 2u : 4u};
            status = api.pfnSetIndices(m_driverDevice,&indices); if (FAILED(status)) return status;
          }
          fill.FillColor = 0xff091725;
          status = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(status)) return status;
          status = state(D3DDDIRS_SCENECAPTURE,1); if (FAILED(status)) return status;
          if (indexSlot) {
            const D3DDDIARG_DRAWINDEXEDPRIMITIVE indexed = {D3DPT_TRIANGLELIST,0,1,4,3,2};
            status = api.pfnDrawIndexedPrimitive(m_driverDevice,&indexed);
          } else {
            const D3DDDIARG_DRAWPRIMITIVE strip = {D3DPT_TRIANGLESTRIP,1,2};
            status = api.pfnDrawPrimitive(m_driverDevice,&strip,nullptr);
          }
          std::printf("D3D9_TRANSFER_DRAW stage=%u vertex=%u offset=%u stride=28 mode=%s index_start=%u hr=%08lx\n",
            stage,vertexSlot,offset,indexSlot == 6 ? "index16" : indexSlot == 7 ? "index32" : "vertex",
            indexSlot ? 3u : 0u,static_cast<unsigned long>(status));
          const HRESULT ended = state(D3DDDIRS_SCENECAPTURE,0);
          if (FAILED(status)) return status;
          if (FAILED(ended)) return ended;
          return readback(stage);
        };
        checked = 0; checksum = 2166136261u;
        hr = transfer(59,0,5,1,13,payloadBytes); if (FAILED(hr)) return hr;
        hr = transferDraw(59,1,16,0); if (FAILED(hr)) return hr;
        hr = verifyBytes(59,1); if (FAILED(hr)) return hr;
        hr = transfer(60,2,9,3,13,payloadBytes); if (FAILED(hr)) return hr;
        hr = transferDraw(60,3,16,0); if (FAILED(hr)) return hr;
        hr = verifyBytes(60,3); if (FAILED(hr)) return hr;
        hr = transfer(61,3,13,3,17,payloadBytes); if (FAILED(hr)) return hr;
        hr = transferDraw(61,3,20,0); if (FAILED(hr)) return hr;
        hr = verifyBytes(61,3); if (FAILED(hr)) return hr;
        hr = transfer(62,3,17,3,13,payloadBytes); if (FAILED(hr)) return hr;
        hr = transferDraw(62,3,16,0); if (FAILED(hr)) return hr;
        hr = verifyBytes(62,3); if (FAILED(hr)) return hr;
        hr = transfer(63,3,13,4,5,payloadBytes); if (FAILED(hr)) return hr;
        if (!verifyExternal(4)) return E_FAIL;
        hr = transferDraw(63,4,8,0); if (FAILED(hr)) return hr;
        hr = verifyBytes(63,4); if (FAILED(hr)) return hr;
        hr = writeRange(64,0,5,thirdPayload.data(),payloadBytes); if (FAILED(hr)) return hr;
        if (!verifyExternal(0)) return E_FAIL;
        hr = transfer(64,0,5,2,9,payloadBytes); if (FAILED(hr)) return hr;
        hr = transferDraw(64,2,12,0); if (FAILED(hr)) return hr;
        hr = verifyBytes(64,2); if (FAILED(hr)) return hr;
        const UINT16 indices16[] = {1,2,3,3,2,4};
        hr = writeRange(65,5,1,indices16,sizeof(indices16)); if (FAILED(hr)) return hr;
        if (!verifyExternal(5)) return E_FAIL;
        hr = transfer(65,5,1,6,6,sizeof(indices16)); if (FAILED(hr)) return hr;
        hr = transferDraw(65,2,12,6); if (FAILED(hr)) return hr;
        hr = verifyBytes(65,6); if (FAILED(hr)) return hr;
        const UINT indices32[] = {1,2,3,3,2,4};
        hr = writeRange(66,1,1,indices32,sizeof(indices32)); if (FAILED(hr)) return hr;
        hr = transfer(66,1,1,7,12,sizeof(indices32)); if (FAILED(hr)) return hr;
        hr = transferDraw(66,2,12,7); if (FAILED(hr)) return hr;
        hr = verifyBytes(66,7); if (FAILED(hr)) return hr;
        if (!verifyExternal(0) || !verifyExternal(4) || !verifyExternal(5)) return E_FAIL;
        for (HANDLE resource : transferResources) {
          hr = api.pfnDestroyResource(m_driverDevice,resource); if (FAILED(hr)) return hr;
          if (api.pfnDestroyResource(m_driverDevice,resource) != E_INVALIDARG) return E_FAIL;
        }
        D3DDDIARG_BUFFERBLT stale = {};
        stale.hSrcResource = transferResources[0]; stale.hDstResource = transferResources[1]; stale.SrcRange = {5,payloadBytes};
        if (api.pfnBufBlt(m_driverDevice,&stale) != E_INVALIDARG) return E_FAIL;
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,transferDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        std::printf("D3D9_TRANSFER_READBACK PASS pixels=%u checksum=%08x bytes=%u byte_checksum=%08x guards=retained pools=system/default copies=range/overlap/readback indices=16/32\n",
          checked,checksum,bytesChecked,byteChecksum);
      }
      if (clipPlanes) {
        if (!api.pfnSetClipPlane || !api.pfnSetTransform) return E_FAIL;
        struct ClipVertex { float x,y,z; D3DCOLOR color; };
        static_assert(sizeof(ClipVertex) == 16);
        const D3DDDIVERTEXELEMENT clipElements[] = {{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
          {0,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
        D3DDDIARG_CREATEVERTEXSHADERDECL clipDeclaration = {2,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&clipDeclaration,clipElements); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,clipDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        const D3DDDIARG_SETSTREAMSOURCEUM clipStream = {0,sizeof(ClipVertex)};
        const D3DDDIARG_VIEWPORTINFO clipViewport = {0,0,8,8};
        hr = api.pfnSetViewport(m_driverDevice,&clipViewport); if (FAILED(hr)) return hr;
        for (const auto clipState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_ZENABLE,0},
          {D3DDDIRS_STENCILENABLE,0},{D3DDDIRS_SCISSORTESTENABLE,0},
          {D3DDDIRS_LIGHTING,0},{D3DDDIRS_COLORVERTEX,1},{D3DDDIRS_CLIPPLANEENABLE,0}}) {
          hr = state(clipState.State,clipState.Value); if (FAILED(hr)) return hr;
        }
        D3DMATRIX identity = {}; identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
        for (const auto type : {D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION}) {
          const D3DDDIARG_SETTRANSFORM transform = {type,identity};
          hr = api.pfnSetTransform(m_driverDevice,&transform);
          std::printf("D3D9_CLIP_TRANSFORM type=%u hr=%08lx\n",UINT(type),static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
        }
        auto plane = [&](UINT stage,UINT index,float a,float b,float c,float d) -> HRESULT {
          const D3DDDIARG_SETCLIPPLANE args = {index,{a,b,c,d}};
          const HRESULT status = api.pfnSetClipPlane(m_driverDevice,&args);
          std::printf("D3D9_CLIP_PLANE stage=%u index=%u a=%.3f b=%.3f c=%.3f d=%.3f hr=%08lx\n",
            stage,index,double(a),double(b),double(c),double(d),static_cast<unsigned long>(status));
          return status;
        };
        for (UINT index = 0; index < 6; ++index) {
          hr = plane(0,index,0,0,0,0); if (FAILED(hr)) return hr;
        }
        const UINT clipColors[] = {0xff739a4c,0xffc85d8a,0xff49a1d2,0xff9a73c4,0xff4da57e,
          0xffce9341,0xfff0a236,0xff3dae96,0xffba567d,0xff64bdc9};
        checked = 0; checksum = 2166136261u;
        for (UINT stage = 67; stage <= 76; ++stage) {
          UINT mask = 0;
          // Place x/y boundaries between D3D9 integer pixel centers. This
          // avoids a clipping edge passing through a tested sample.
          if (stage == 67) hr = plane(stage,0,1,0,0,.125f);
          else if (stage == 69) hr = plane(stage,0,-1,0,0,-.125f);
          else if (stage == 70) hr = plane(stage,5,0,1,0,-.125f);
          else if (stage == 72) hr = plane(stage,3,0,0,1,-.25f);
          else if (stage == 73) hr = plane(stage,3,0,0,-1,.25f);
          else if (stage == 74 || stage == 76) hr = plane(stage,5,0,0,0,-1);
          else if (stage == 75) hr = plane(stage,5,0,0,0,1);
          if (FAILED(hr)) return hr;
          if (stage == 68 || stage == 69) mask = 1;
          else if (stage == 70 || stage == 74 || stage == 75) mask = 32;
          else if (stage == 71) mask = 33;
          else if (stage == 72 || stage == 73) mask = 8;
          hr = state(D3DDDIRS_CLIPPLANEENABLE,mask);
          std::printf("D3D9_CLIP_ENABLE stage=%u mask=%u hr=%08lx\n",stage,mask,static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          ClipVertex clipVertices[5] = {{1000,1000,.5f,0xff000000},
            {-2,2,.5f,clipColors[stage-67]},{2,2,.5f,clipColors[stage-67]},
            {-2,-2,.5f,clipColors[stage-67]},{2,-2,.5f,clipColors[stage-67]}};
          hr = api.pfnSetStreamSourceUm(m_driverDevice,&clipStream,clipVertices); if (FAILED(hr)) return hr;
          fill.FillColor = 0xff091725;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_SCENECAPTURE,1); if (FAILED(hr)) return hr;
          hr = api.pfnDrawPrimitive(m_driverDevice,&primitive,nullptr);
          std::printf("D3D9_CLIP_DRAW stage=%u hr=%08lx\n",stage,static_cast<unsigned long>(hr));
          const HRESULT ended = state(D3DDDIRS_SCENECAPTURE,0);
          if (FAILED(hr)) return hr;
          if (FAILED(ended)) return ended;
          hr = readback(stage); if (FAILED(hr)) return hr;
        }
        D3DDDIARG_SETCLIPPLANE invalid = {6,{91,92,93,94}};
        if (api.pfnSetClipPlane(m_driverDevice,&invalid) != E_INVALIDARG) return E_FAIL;
        invalid.Index = UINT_MAX;
        if (api.pfnSetClipPlane(m_driverDevice,&invalid) != E_INVALIDARG) return E_FAIL;
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,clipDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        std::printf("D3D9_CLIP_READBACK PASS pixels=%u checksum=%08x padding=retained coefficients=xyzw lifetime=snapshot indices=0/3/5 enable=disable/update/sparse/intersection\n",checked,checksum);
      }
      if (gpuQueries) {
        if (!api.pfnCreateQuery || !api.pfnIssueQuery || !api.pfnGetQueryData || !api.pfnDestroyQuery) return E_FAIL;
        const D3DDDIQUERYTYPE queryTypes[] = {D3DDDIQUERYTYPE_EVENT,D3DDDIQUERYTYPE_OCCLUSION,
          D3DDDIQUERYTYPE_TIMESTAMP,D3DDDIQUERYTYPE_TIMESTAMP,D3DDDIQUERYTYPE_TIMESTAMPDISJOINT,
          D3DDDIQUERYTYPE_TIMESTAMPFREQ};
        const UINT querySizes[] = {sizeof(BOOL),sizeof(UINT),sizeof(UINT64),sizeof(UINT64),sizeof(BOOL),sizeof(UINT64)};
        std::array<HANDLE,6> queryHandles = {};
        char unsupportedOwner;
        D3DDDIARG_CREATEQUERY unsupported = {D3DDDIQUERYTYPE_VCACHE,&unsupportedOwner};
        const HRESULT unsupportedResult = api.pfnCreateQuery(m_driverDevice,&unsupported);
        std::printf("D3D9_QUERY_UNSUPPORTED type=%u hr=%08lx output=retained\n",UINT(unsupported.QueryType),static_cast<unsigned long>(unsupportedResult));
        // The matched Adreno backend has no NVIDIA-specific VCACHE hints.
        if (unsupportedResult != D3DERR_NOTAVAILABLE || unsupported.hQuery != &unsupportedOwner) return E_FAIL;
        for (UINT id = 0; id < queryHandles.size(); ++id) {
          D3DDDIARG_CREATEQUERY query = {queryTypes[id],nullptr};
          hr = api.pfnCreateQuery(m_driverDevice,&query);
          std::printf("D3D9_QUERY_CREATE id=%u type=%u bytes=%u hr=%08lx\n",id,UINT(queryTypes[id]),querySizes[id],static_cast<unsigned long>(hr));
          if (hr != S_OK || !query.hQuery) return FAILED(hr) ? hr : E_FAIL;
          queryHandles[id] = query.hQuery;
          std::array<uint8_t,32> output; output.fill(0xc1);
          const auto before = output;
          const D3DDDIARG_GETQUERYDATA get = {query.hQuery,output.data() + 5};
          const HRESULT pending = api.pfnGetQueryData(m_driverDevice,&get);
          std::printf("D3D9_QUERY_UNISSUED id=%u hr=%08lx guards=retained\n",id,static_cast<unsigned long>(pending));
          if (pending != S_FALSE || output != before) return E_FAIL;
        }
        auto issue = [&](UINT id,UINT flags) -> HRESULT {
          D3DDDIARG_ISSUEQUERY args = {}; args.hQuery = queryHandles[id]; args.Flags.Value = flags;
          const HRESULT status = api.pfnIssueQuery(m_driverDevice,&args);
          std::printf("D3D9_QUERY_ISSUE id=%u flags=%u hr=%08lx\n",id,flags,static_cast<unsigned long>(status));
          return status == S_OK ? S_OK : FAILED(status) ? status : E_FAIL;
        };
        if (issue(0,1) != E_INVALIDARG) return E_FAIL;
        const ULONGLONG queryDeadline = GetTickCount64() + 8000;
        UINT completions = 0;
        auto complete = [&](UINT id,UINT caseId,UINT64& value) -> HRESULT {
          std::array<uint8_t,32> output; output.fill(0xc1);
          const auto before = output;
          const D3DDDIARG_GETQUERYDATA get = {queryHandles[id],output.data() + 5};
          UINT polls = 0;
          HRESULT status;
          do {
            ++polls;
            status = api.pfnGetQueryData(m_driverDevice,&get);
            if (status == S_FALSE) {
              if (output != before || GetTickCount64() >= queryDeadline) return E_FAIL;
              Sleep(1);
            }
          } while (status == S_FALSE);
          if (status != S_OK) return FAILED(status) ? status : E_FAIL;
          for (UINT i = 0; i < output.size(); ++i)
            if ((i < 5 || i >= 5 + querySizes[id]) && output[i] != 0xc1) return E_FAIL;
          value = 0;
          std::memcpy(&value,output.data() + 5,querySizes[id]);
          const auto completed = output;
          output.fill(0xc1);
          if (api.pfnGetQueryData(m_driverDevice,&get) != S_OK || output != completed) return E_FAIL;
          const D3DDDIARG_GETQUERYDATA poll = {queryHandles[id],nullptr};
          if (api.pfnGetQueryData(m_driverDevice,&poll) != S_OK) return E_FAIL;
          ++completions;
          std::printf("D3D9_QUERY_RESULT case=%u id=%u type=%u bytes=%u polls=%u value=%llu data=",caseId,id,UINT(queryTypes[id]),querySizes[id],polls,static_cast<unsigned long long>(value));
          for (UINT i = 0; i < querySizes[id]; ++i) std::printf("%02x",UINT(completed[5 + i]));
          std::printf(" guards=retained cached=exact poll=complete\n");
          return S_OK;
        };
        struct QueryVertex { float x,y,z; D3DCOLOR color; };
        static_assert(sizeof(QueryVertex) == 16);
        const D3DDDIVERTEXELEMENT queryElements[] = {{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
          {0,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}};
        D3DDDIARG_CREATEVERTEXSHADERDECL queryDeclaration = {2,nullptr};
        hr = api.pfnCreateVertexShaderDecl(m_driverDevice,&queryDeclaration,queryElements); if (FAILED(hr)) return hr;
        hr = api.pfnSetVertexShaderDecl(m_driverDevice,queryDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        const D3DDDIARG_SETSTREAMSOURCEUM queryStream = {0,sizeof(QueryVertex)};
        const D3DDDIARG_VIEWPORTINFO queryViewport = {0,0,8,8};
        hr = api.pfnSetViewport(m_driverDevice,&queryViewport); if (FAILED(hr)) return hr;
        D3DMATRIX identity = {}; identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
        for (const auto type : {D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION}) {
          const D3DDDIARG_SETTRANSFORM transform = {type,identity};
          hr = api.pfnSetTransform(m_driverDevice,&transform); if (FAILED(hr)) return hr;
        }
        for (const auto queryState : {D3DDDIARG_RENDERSTATE{D3DDDIRS_ZENABLE,0},
          {D3DDDIRS_STENCILENABLE,0},{D3DDDIRS_LIGHTING,0},{D3DDDIRS_COLORVERTEX,1}}) {
          hr = state(queryState.State,queryState.Value); if (FAILED(hr)) return hr;
        }
        const D3DDDIARG_SETCLIPPLANE reject = {5,{0,0,0,-1}};
        hr = api.pfnSetClipPlane(m_driverDevice,&reject); if (FAILED(hr)) return hr;
        hr = issue(4,1); if (FAILED(hr)) return hr;
        hr = issue(2,2); if (FAILED(hr)) return hr;
        hr = issue(0,2); if (FAILED(hr)) return hr;
        const UINT queryColors[] = {0xffbd5c83,0xff43a6c2,0xff73b248};
        const UINT queryCounts[] = {64,16,0};
        checked = 0; checksum = 2166136261u;
        for (UINT stage = 77; stage <= 79; ++stage) {
          hr = state(D3DDDIRS_CLIPPLANEENABLE,stage == 79 ? 32 : 0); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_SCISSORTESTENABLE,stage == 78 ? 1 : 0); if (FAILED(hr)) return hr;
          const RECT scissor = {1,2,5,6};
          hr = api.pfnSetScissorRect(m_driverDevice,&scissor); if (FAILED(hr)) return hr;
          QueryVertex queryVertices[5] = {{1000,1000,.5f,0xff000000},
            {-2,2,.5f,queryColors[stage-77]},{2,2,.5f,queryColors[stage-77]},
            {-2,-2,.5f,queryColors[stage-77]},{2,-2,.5f,queryColors[stage-77]}};
          hr = api.pfnSetStreamSourceUm(m_driverDevice,&queryStream,queryVertices); if (FAILED(hr)) return hr;
          fill.FillColor = 0xff091725;
          hr = api.pfnClear(m_driverDevice,&fill,1,&full); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_SCENECAPTURE,1); if (FAILED(hr)) return hr;
          hr = issue(1,1); if (FAILED(hr)) return hr;
          std::array<uint8_t,32> pendingBytes; pendingBytes.fill(0xc1);
          const auto before = pendingBytes;
          const D3DDDIARG_GETQUERYDATA begun = {queryHandles[1],pendingBytes.data() + 5};
          const HRESULT pending = api.pfnGetQueryData(m_driverDevice,&begun);
          std::printf("D3D9_QUERY_BEGUN stage=%u hr=%08lx guards=retained\n",stage,static_cast<unsigned long>(pending));
          if (pending != S_FALSE || pendingBytes != before) return E_FAIL;
          hr = api.pfnDrawPrimitive(m_driverDevice,&primitive,nullptr);
          std::printf("D3D9_QUERY_DRAW stage=%u hr=%08lx\n",stage,static_cast<unsigned long>(hr));
          if (FAILED(hr)) return hr;
          hr = issue(1,2); if (FAILED(hr)) return hr;
          hr = state(D3DDDIRS_SCENECAPTURE,0); if (FAILED(hr)) return hr;
          hr = api.pfnFlush(m_driverDevice); if (FAILED(hr)) return hr;
          UINT64 count = 0;
          hr = complete(1,stage,count); if (FAILED(hr)) return hr;
          if (count != queryCounts[stage-77]) return E_FAIL;
          hr = readback(stage); if (FAILED(hr)) return hr;
        }
        hr = issue(3,2); if (FAILED(hr)) return hr;
        hr = issue(4,2); if (FAILED(hr)) return hr;
        hr = issue(5,2); if (FAILED(hr)) return hr;
        hr = issue(0,2); if (FAILED(hr)) return hr;
        hr = issue(0,2); if (FAILED(hr)) return hr;
        hr = api.pfnFlush(m_driverDevice); if (FAILED(hr)) return hr;
        UINT64 event = 0,first = 0,last = 0,disjoint = 0,frequency = 0;
        hr = complete(0,80,event); if (FAILED(hr) || event != TRUE) return FAILED(hr) ? hr : E_FAIL;
        hr = complete(2,81,first); if (FAILED(hr)) return hr;
        hr = complete(3,82,last); if (FAILED(hr) || last < first) return FAILED(hr) ? hr : E_FAIL;
        hr = complete(4,83,disjoint); if (FAILED(hr) || disjoint != FALSE) return FAILED(hr) ? hr : E_FAIL;
        hr = complete(5,84,frequency); if (FAILED(hr) || !frequency) return FAILED(hr) ? hr : E_FAIL;
        hr = issue(0,2); if (FAILED(hr)) return hr;
        hr = api.pfnFlush(m_driverDevice); if (FAILED(hr)) return hr;
        hr = complete(0,85,event); if (FAILED(hr) || event != TRUE) return FAILED(hr) ? hr : E_FAIL;
        for (UINT id = 0; id < queryHandles.size(); ++id) {
          hr = api.pfnDestroyQuery(m_driverDevice,queryHandles[id]);
          std::printf("D3D9_QUERY_DESTROY id=%u hr=%08lx\n",id,static_cast<unsigned long>(hr));
          if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
          const D3DDDIARG_GETQUERYDATA stale = {queryHandles[id],nullptr};
          if (api.pfnDestroyQuery(m_driverDevice,queryHandles[id]) != E_INVALIDARG
              || api.pfnGetQueryData(m_driverDevice,&stale) != E_INVALIDARG) return E_FAIL;
        }
        hr = api.pfnDeleteVertexShaderDecl(m_driverDevice,queryDeclaration.ShaderHandle); if (FAILED(hr)) return hr;
        std::printf("D3D9_QUERY_READBACK PASS pixels=%u checksum=%08x queries=6 completions=%u occlusion=64/16/0 event=full-BOOL timestamps=ordered frequency=positive disjoint=false guards=retained cached=exact lifetime=owned\n",checked,checksum,completions);
      }
    }
    if (presentation) {
      hr = verifyPresentation();
      if (FAILED(hr)) return hr;
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
  HRESULT verifyPresentation() {
    const auto& api = m_deviceFuncs;
    if (!api.pfnPresent || !api.pfnSetViewport) return E_FAIL;
    HRESULT hr = m_window.create();
    std::printf("D3D9_PRESENT_WINDOW hr=%08lx client=64x64\n",static_cast<unsigned long>(hr));
    if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
    hr = acquirePresentationSource();
    if (hr != S_OK) return hr;
    const UINT colors[4][4] = {
      {0xff2748ad,0xffd1376b,0xff42b87c,0xffeab325},
      {0xff6f32c5,0xff198bd4,0xffbaed43,0xffe56139},
      {0xff82d719,0xffbc46a8,0xff357fe2,0xffef9031},
      {0xffd6ac35,0xff4ae179,0xff973ce8,0xff205db7}
    };
    UINT checked = 0, checksum = 2166136261u;
    for (unsigned format = 0; format < 2; ++format) {
      char runtimeOwner;
      D3DDDI_SURFACEINFO info = {64,64,1,nullptr,0,0};
      D3DDDIARG_CREATERESOURCE target = {};
      target.hResource = &runtimeOwner; target.pSurfList = &info; target.SurfCount = 1;
      target.Pool = D3DDDIPOOL_LOCALVIDMEM;
      target.Format = static_cast<D3DDDIFORMAT>(format ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8);
      target.Flags.RenderTarget = target.Flags.NotLockable = 1;
      hr = api.pfnCreateResource(m_driverDevice,&target);
      std::printf("D3D9_PRESENT_RESOURCE format=%u hr=%08lx\n",format,static_cast<unsigned long>(hr));
      if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
      const D3DDDIARG_SETRENDERTARGET bind = {0,target.hResource,0};
      hr = api.pfnSetRenderTarget(m_driverDevice,&bind); if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
      const D3DDDIARG_VIEWPORTINFO viewport = {0,0,64,64};
      hr = api.pfnSetViewport(m_driverDevice,&viewport); if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
      for (unsigned frame = 0; frame < 2; ++frame) {
        const unsigned stage = format * 2 + frame + 1;
        for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
          const LONG x = LONG(quadrant % 2) * 32, y = LONG(quadrant / 2) * 32;
          const RECT rect = {x,y,x+32,y+32};
          D3DDDIARG_CLEAR fill = {};
          fill.Flags = D3DCLEAR_TARGET; fill.FillColor = colors[stage - 1][quadrant];
          hr = api.pfnClear(m_driverDevice,&fill,1,&rect);
          if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
        }
        m_window.pump();
        D3DDDIARG_PRESENT args = {};
        args.hSrcResource = target.hResource; args.Flags.Blt = 1;
        args.DstSubResourceIndex = UINT_MAX;
        args.FlipInterval = static_cast<D3DDDI_FLIPINTERVAL_TYPE>(UINT_MAX);
        const auto before = args;
        hr = api.pfnPresent(m_driverDevice,&args);
        std::printf("D3D9_PRESENT_SUBMIT stage=%u format=%u hr=%08lx\n",stage,format,static_cast<unsigned long>(hr));
        if (hr != S_OK || std::memcmp(&before,&args,sizeof(args))) return FAILED(hr) ? hr : E_FAIL;
        std::array<UINT,4096> capture = {};
        const ULONGLONG deadline = GetTickCount64() + 2500;
        unsigned consecutive = 0, polls = 0;
        while (GetTickCount64() < deadline && consecutive < 2) {
          m_window.pump();
          hr = m_window.capture(capture); ++polls;
          if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
          bool equal = true;
          for (UINT y = 0; y < 64; ++y) for (UINT x = 0; x < 64; ++x) {
            const UINT expected = colors[stage - 1][(y / 32) * 2 + x / 32] & 0xffffff;
            if ((capture[y * 64 + x] & 0xffffff) != expected) equal = false;
          }
          consecutive = equal ? consecutive + 1 : 0;
          if (consecutive < 2) Sleep(20);
        }
        // Preserve every actual screen pixel even if presentation never matches.
        for (UINT y = 0; y < 64; ++y) for (UINT x = 0; x < 64; ++x) {
          const UINT value = capture[y * 64 + x];
          std::printf("D3D9_PRESENT_SCREEN_PIXEL stage=%u x=%u y=%u value=%08x\n",stage,x,y,value);
          checksum = (checksum ^ (value & 0xffffff)) * 16777619u; ++checked;
        }
        std::printf("D3D9_PRESENT_SCREEN stage=%u polls=%u consecutive=%u pixels=4096 hr=%08lx\n",
          stage,polls,consecutive,static_cast<unsigned long>(consecutive == 2 ? S_OK : E_FAIL));
        if (consecutive != 2) return E_FAIL;
      }
      hr = api.pfnDestroyResource(m_driverDevice,target.hResource);
      if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
    }
    if (presents != 4 || presentAllocations != 2 || presentDeallocations != 2 || presentContexts != 1) return E_FAIL;
    std::printf("D3D9_PRESENT_READBACK PASS pixels=%u checksum=%08x submissions=4 allocations=2/2 context=1 formats=A8/X8 frames=updated/reused source=owned capture=screen-rgb\n",checked,checksum);
    return releasePresentationSource();
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
    if (m_presentContext) {
      D3DKMT_DESTROYCONTEXT request = {}; request.hContext = m_presentContext;
      const HRESULT cleanup = result(D3DKMTDestroyContext(&request));
      if (FAILED(cleanup)) hr = cleanup;
      m_presentContext = 0;
    }
    if (m_device) {
      const HRESULT released = releasePresentationSource();
      if (FAILED(released)) hr = released;
      if (m_pagingQueue) {
        D3DDDI_DESTROYPAGINGQUEUE paging = {}; paging.hPagingQueue = m_pagingQueue;
        const HRESULT cleanup = result(D3DKMTDestroyPagingQueue(&paging));
        if (FAILED(cleanup)) return cleanup;
        m_pagingQueue = m_pagingSync = 0;
      }
      std::printf("KMT_RESIDENCY references=%u evictions=%u remaining=%zu\n",
        residencyReferences, residencyEvictions, m_resident.size());
      if (!m_resident.empty() || residencyReferences != residencyEvictions) hr = E_FAIL;
      if (!m_resources.empty()) hr = E_FAIL;
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
  HRESULT acquirePresentationSource() {
    if (!m_device || !m_window.handle() || m_sourceOwned) return E_INVALIDARG;
    const HDC dc = GetDC(m_window.handle());
    if (!dc) return E_FAIL;
    D3DKMT_OPENADAPTERFROMHDC adapter = {}; adapter.hDc = dc;
    const NTSTATUS opened = D3DKMTOpenAdapterFromHdc(&adapter);
    ReleaseDC(m_window.handle(),dc);
    if (opened != 0) return opened < 0 ? result(opened) : E_FAIL;
    if (!adapter.hAdapter) return E_FAIL;
    D3DKMT_CLOSEADAPTER close = {}; close.hAdapter = adapter.hAdapter;
    const NTSTATUS closed = D3DKMTCloseAdapter(&close);
    std::printf("D3D9_PRESENT_SOURCE_OPEN status=%08lx close_status=%08lx source=%u luid=",
      static_cast<unsigned long>(opened),static_cast<unsigned long>(closed),adapter.VidPnSourceId);
    printLuid(adapter.AdapterLuid); std::printf("\n");
    if (closed != 0) return closed < 0 ? result(closed) : E_FAIL;
    if (std::memcmp(&adapter.AdapterLuid,&m_luid,sizeof(m_luid))) return E_FAIL;
    // Emulated ownership has no real primary ownership. This diagnostic must
    // leave the desktop's display owner intact and never change its mode.
    const D3DKMT_VIDPNSOURCEOWNER_TYPE type = D3DKMT_VIDPNSOURCEOWNER_EMULATED;
    D3DKMT_SETVIDPNSOURCEOWNER owner = {};
    owner.hDevice = m_device; owner.pType = &type;
    owner.pVidPnSourceId = &adapter.VidPnSourceId; owner.VidPnSourceCount = 1;
    const NTSTATUS status = D3DKMTSetVidPnSourceOwner(&owner);
    std::printf("D3D9_PRESENT_SOURCE_ACQUIRE status=%08lx source=%u type=emulated\n",
      static_cast<unsigned long>(status),adapter.VidPnSourceId);
    if (status != 0) return status < 0 ? result(status) : E_FAIL;
    m_sourceOwned = true;
    return S_OK;
  }
  HRESULT releasePresentationSource() {
    if (!m_sourceOwned) return S_OK;
    if (!m_device) return E_FAIL;
    // A zero-count request releases only this owned device's source handles.
    D3DKMT_SETVIDPNSOURCEOWNER owner = {}; owner.hDevice = m_device;
    const NTSTATUS status = D3DKMTSetVidPnSourceOwner(&owner);
    std::printf("D3D9_PRESENT_SOURCE_RELEASE status=%08lx\n",static_cast<unsigned long>(status));
    if (status != 0) return status < 0 ? result(status) : E_FAIL;
    m_sourceOwned = false;
    return S_OK;
  }
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
    if (!s || handle != &s->m_deviceOwner || !args) return E_INVALIDARG;
    const bool standard = !args->pPrivateDriverData && !args->PrivateDriverDataSize;
    auto& context = standard ? s->m_presentContext : s->m_context;
    if (context) return E_INVALIDARG;
    D3DKMT_CREATECONTEXT request = {};
    request.hDevice = s->m_device; request.NodeOrdinal = args->NodeOrdinal;
    request.EngineAffinity = args->EngineAffinity; request.Flags = args->Flags;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    const HRESULT hr = result(D3DKMTCreateContext(&request));
    if (standard) std::printf("D3D9_PRESENT_CONTEXT_CREATE hr=%08lx context=%u\n",
      static_cast<unsigned long>(hr),request.hContext);
    if (SUCCEEDED(hr)) {
      context = request.hContext;
      args->hContext = standard ? &s->m_presentContextOwner : &s->m_contextOwner;
      if (standard) ++s->presentContexts; else ++s->contexts;
      args->pCommandBuffer = request.pCommandBuffer; args->CommandBufferSize = request.CommandBufferSize;
      args->pAllocationList = request.pAllocationList; args->AllocationListSize = request.AllocationListSize;
      args->pPatchLocationList = request.pPatchLocationList; args->PatchLocationListSize = request.PatchLocationListSize;
    }
    return hr;
  }
  static HRESULT APIENTRY destroyContext(HANDLE handle, const D3DDDICB_DESTROYCONTEXT* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args) return E_INVALIDARG;
    const bool standard = args->hContext == &s->m_presentContextOwner;
    if (!standard && args->hContext != &s->m_contextOwner) return E_INVALIDARG;
    auto& context = standard ? s->m_presentContext : s->m_context;
    if (!context) return E_INVALIDARG;
    D3DKMT_DESTROYCONTEXT request = {}; request.hContext = context;
    const HRESULT hr = result(D3DKMTDestroyContext(&request));
    if (standard) std::printf("D3D9_PRESENT_CONTEXT_DESTROY hr=%08lx\n",static_cast<unsigned long>(hr));
    if (SUCCEEDED(hr)) {
      context = 0;
      if (standard) ++s->presentContextCloses; else ++s->contextCloses;
    }
    return hr;
  }
  static HRESULT APIENTRY allocate(HANDLE handle, D3DDDICB_ALLOCATE* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || args->hKMResource
        || !args->NumAllocations || !args->pAllocationInfo) return E_INVALIDARG;
    ResourceAllocation* resource = nullptr;
    if (args->hResource) {
      if (args->NumAllocations != 1 || s->m_resources.count(args->hResource)) return E_INVALIDARG;
      // Reserve ownership storage before the kernel can create any allocation.
      auto entry = s->m_resources.emplace(args->hResource,ResourceAllocation{}).first;
      try { entry->second.allocations.resize(args->NumAllocations); }
      catch (...) { s->m_resources.erase(entry); return E_OUTOFMEMORY; }
      resource = &entry->second;
    }
    D3DKMT_CREATEALLOCATION request = {};
    request.hDevice = s->m_device; request.NumAllocations = args->NumAllocations; request.pAllocationInfo = args->pAllocationInfo;
    request.pPrivateDriverData = args->pPrivateDriverData; request.PrivateDriverDataSize = args->PrivateDriverDataSize;
    if (resource) {
      request.Flags.CreateResource = 1;
      request.hPrivateRuntimeResourceHandle = args->hResource;
    }
    const HRESULT hr = result(D3DKMTCreateAllocation(&request));
    if (SUCCEEDED(hr)) {
      args->hKMResource = request.hResource; s->allocations += args->NumAllocations;
      if (resource) {
        resource->kernel = request.hResource;
        for (UINT i = 0; i < args->NumAllocations; ++i) resource->allocations[i] = args->pAllocationInfo[i].hAllocation;
        ++s->presentAllocations;
      }
    } else if (resource) s->m_resources.erase(args->hResource);
    if (args->hResource) std::printf("D3D9_PRESENT_ALLOCATION hr=%08lx resource=%u allocation=%u\n",
      static_cast<unsigned long>(hr),args->hKMResource,args->pAllocationInfo->hAllocation);
    return hr;
  }
  static HRESULT APIENTRY deallocate(HANDLE handle, const D3DDDICB_DEALLOCATE* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args) return E_INVALIDARG;
    ResourceAllocation* resource = nullptr;
    const D3DKMT_HANDLE* handles = args->HandleList;
    UINT count = args->NumAllocations;
    if (args->hResource) {
      const auto entry = s->m_resources.find(args->hResource);
      if (entry == s->m_resources.end() || count || handles) return E_INVALIDARG;
      resource = &entry->second; handles = resource->allocations.data(); count = UINT(resource->allocations.size());
    }
    if (!count || !handles) return E_INVALIDARG;
    for (UINT i = 0; i < count; ++i) {
      const auto found = std::find(s->m_resident.begin(), s->m_resident.end(), handles[i]);
      if (found == s->m_resident.end()) continue;
      D3DKMT_EVICT evict = {}; evict.hDevice = s->m_device;
      evict.NumAllocations = 1; evict.AllocationList = &handles[i];
      const NTSTATUS status = D3DKMTEvict(&evict);
      std::printf("KMT_EVICT allocation=%u status=%08lx\n",handles[i],static_cast<unsigned long>(status));
      if (status != 0) return status < 0 ? result(status) : E_FAIL;
      s->m_resident.erase(found); ++s->residencyEvictions;
    }
    D3DKMT_DESTROYALLOCATION request = {};
    request.hDevice = s->m_device;
    if (resource && resource->kernel) request.hResource = resource->kernel;
    else { request.AllocationCount = count; request.phAllocationList = handles; }
    const HRESULT hr = result(D3DKMTDestroyAllocation(&request));
    if (SUCCEEDED(hr)) {
      s->deallocations += count;
      if (resource) { ++s->presentDeallocations; s->m_resources.erase(args->hResource); }
    }
    if (args->hResource) std::printf("D3D9_PRESENT_DEALLOCATION hr=%08lx\n",static_cast<unsigned long>(hr));
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
  static HRESULT APIENTRY present(HANDLE handle, D3DDDICB_PRESENT* args) {
    auto s = self(handle);
    if (!s || handle != &s->m_deviceOwner || !args || !s->m_window.handle()
        || args->hContext != &s->m_presentContextOwner || !s->m_presentContext
        || !args->hSrcAllocation || args->hDstAllocation || args->BroadcastContextCount
        || args->PrivateDriverDataSize || args->pPrivateDriverData || args->SyncIntervalOverrideValid) return E_INVALIDARG;
    bool owned = false;
    for (const auto& resource : s->m_resources)
      if (resource.second.allocations.size() == 1 && resource.second.allocations[0] == args->hSrcAllocation) owned = true;
    if (!owned) return E_INVALIDARG;
    D3DDDI_ALLOCATIONLIST reference = {}; reference.hAllocation = args->hSrcAllocation;
    D3DDDICB_RENDER residency = {};
    residency.NumAllocations = residency.NewAllocationListSize = 1; residency.pNewAllocationList = &reference;
    const HRESULT resident = s->resident(residency);
    if (FAILED(resident)) return resident;
    const RECT rect = {0,0,64,64};
    D3DKMT_PRESENT request = {};
    request.hContext = s->m_presentContext; request.hWindow = s->m_window.handle();
    request.hSource = args->hSrcAllocation; request.SrcRect = request.DstRect = rect;
    request.Flags.Blt = request.Flags.SrcRectValid = request.Flags.DstRectValid = 1;
    request.SubRectCnt = 1; request.pSrcSubRects = &rect;
    request.PresentCount = s->presents + 1;
    const NTSTATUS status = D3DKMTPresent(&request);
    const HRESULT hr = status == 0 ? S_OK : status < 0 ? result(status) : E_FAIL;
    std::printf("D3D9_KMT_PRESENT stage=%u status=%08lx hr=%08lx context=%u source=%u composition=%u\n",
      s->presents + 1,static_cast<unsigned long>(status),static_cast<unsigned long>(hr),
      s->m_presentContext,args->hSrcAllocation,unsigned(request.bOptimizeForComposition));
    if (hr == S_OK) ++s->presents;
    return hr;
  }
  struct ResourceAllocation { D3DKMT_HANDLE kernel = 0; std::vector<D3DKMT_HANDLE> allocations; };
  Owner m_adapterOwner{this}, m_deviceOwner{this}, m_contextOwner{this}, m_presentContextOwner{this};
  DWORD m_thread = GetCurrentThreadId();
  LUID m_luid = {};
  bool m_sourceOwned = false;
  D3DKMT_HANDLE m_adapter = 0, m_device = 0, m_context = 0, m_presentContext = 0;
  D3DKMT_HANDLE m_pagingQueue = 0, m_pagingSync = 0;
  UINT64 m_pendingPaging = 0;
  std::vector<D3DKMT_HANDLE> m_resident;
  std::map<HANDLE,ResourceAllocation> m_resources;
  PresentWindow m_window;
  HANDLE m_driverAdapter = nullptr, m_driverDevice = nullptr;
  D3DDDI_ADAPTERFUNCS m_adapterFuncs = {};
  D3DDDI_DEVICEFUNCS m_deviceFuncs = {};
  unsigned queries = 0, contexts = 0, contextCloses = 0, allocations = 0, deallocations = 0;
  unsigned locks = 0, unlocks = 0, renders = 0, escapes = 0, wrongThreads = 0;
  unsigned residencyReferences = 0, residencyEvictions = 0;
  unsigned presents = 0, presentContexts = 0, presentContextCloses = 0;
  unsigned presentAllocations = 0, presentDeallocations = 0;
};

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc == 2 && !wcscmp(argv[1], L"--list-adapters")) {
    try { return listAdapters(); }
    catch (...) { return 1; }
  }
  LUID luid = {};
  const bool presentation = argc == 3 && !wcscmp(argv[2], L"--present");
  const bool gpuQueries = presentation || (argc == 3 && !wcscmp(argv[2], L"--queries"));
  const bool clipPlanes = gpuQueries || (argc == 3 && !wcscmp(argv[2], L"--clip-planes"));
  const bool bufferTransfer = clipPlanes || (argc == 3 && !wcscmp(argv[2], L"--buffer-transfer"));
  const bool fixedFunction = bufferTransfer || (argc == 3 && !wcscmp(argv[2], L"--fixed-function"));
  const bool depthStencil = fixedFunction || (argc == 3 && !wcscmp(argv[2], L"--depth"));
  const bool buffers = depthStencil || (argc == 3 && !wcscmp(argv[2], L"--buffer"));
  const bool textures = buffers || (argc == 3 && !wcscmp(argv[2], L"--texture"));
  const bool shaders = textures || (argc == 3 && !wcscmp(argv[2], L"--shader"));
  const bool drawing = shaders || (argc == 3 && !wcscmp(argv[2], L"--draw"));
  const bool rendering = drawing || (argc == 3 && !wcscmp(argv[2], L"--render"));
  if ((argc != 2 && !rendering) || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d9-device-probe <16 hex LUID bytes> [--render|--draw|--shader|--texture|--buffer|--depth|--fixed-function|--buffer-transfer|--clip-planes|--queries|--present]|--list-adapters\n");
    return 2;
  }
  KmtRuntime9 runtime;
  HRESULT hr = runtime.open(luid);
  if (SUCCEEDED(hr)) hr = runtime.create();
  if (SUCCEEDED(hr)) hr = rendering ? runtime.verifyRendering(drawing,shaders,textures,buffers,depthStencil,fixedFunction,bufferTransfer,clipPlanes,gpuQueries,presentation) : runtime.verify();
  const HRESULT closed = runtime.close();
  if (FAILED(closed)) hr = closed;
  std::printf("D3D9_KMT_%s %s hr=%08lx; %s, no ordinary runtime admission\n",
    presentation ? "PRESENT" : gpuQueries ? "QUERY" : clipPlanes ? "CLIP" : bufferTransfer ? "BUFFER_TRANSFER" : fixedFunction ? "FIXED" : depthStencil ? "DEPTH" : buffers ? "BUFFER" : textures ? "TEXTURE" : shaders ? "SHADER" : drawing ? "DRAW" : rendering ? "RENDER" : "DEVICE", SUCCEEDED(hr) ? "PASS" : "FAIL", static_cast<unsigned long>(hr),
    presentation ? "typed kernel blit and actual screen pixels" : gpuQueries ? "typed GPU query completion/occlusion/timestamp/readback" : clipPlanes ? "typed homogeneous clip-plane draw/readback pixels" : bufferTransfer ? "typed system-memory/buffer-transfer draw/readback pixels" : fixedFunction ? "typed fixed-function transform/light draw/readback pixels" : depthStencil ? "typed depth/stencil clear/draw/readback pixels" : buffers ? "typed vertex/index/range-lock draw/readback pixels" : textures ? "typed texture/mip/sampler draw/readback pixels" : shaders ? "typed SM1-3 shader draw/readback pixels" : drawing ? "typed offscreen draw/readback pixels" : rendering ? "typed offscreen clear/readback pixels" : "offscreen lifecycle only, no pixel rendering");
  return FAILED(hr) ? 1 : 0;
}
