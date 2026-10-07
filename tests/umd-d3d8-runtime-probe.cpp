// Genuine Windows D3D8 runtime probe. No DXVK d3d8.dll or D3D9 delegation.
#include "umd-d3d8-api.h"
#include "../src/umd/umd_runtime_imports.h"
#include <d3dkmthk.h>
#include <psapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <stdexcept>
#include <string>

namespace {
bool traceFailed = false;
void trace(const char* format, ...) {
  char line[4096];
  va_list args; va_start(args, format);
  const int bytes = std::vsnprintf(line, sizeof(line) - 2, format, args); va_end(args);
  if (bytes < 0 || size_t(bytes) >= sizeof(line) - 2) { traceFailed = true; return; }
  line[bytes] = '\n';
  DWORD written = 0;
  if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, DWORD(bytes + 1), &written, nullptr)
      || written != DWORD(bytes + 1)) traceFailed = true;
}
void require(bool value, const char* what) {
  if (!value) { trace("D3D8_ERROR operation=%s win32=%lu", what, GetLastError()); throw std::runtime_error(what); }
}
void call(HRESULT value, const char* what) {
  trace("D3D8_API operation=%s hr=%08lx", what, static_cast<unsigned long>(value));
  require(value == S_OK, what);
}
template<typename T> struct Com {
  T* ptr = nullptr;
  ~Com() { reset(); }
  void reset() { if (ptr) { ptr->Release(); ptr = nullptr; } }
  T* operator->() const { return ptr; }
  Com() = default;
  Com(const Com&) = delete;
  Com& operator=(const Com&) = delete;
};
struct Module { HMODULE value = nullptr; ~Module() { if (value) FreeLibrary(value); } };
std::wstring path(HMODULE module) {
  wchar_t value[32768]; const DWORD length = GetModuleFileNameW(module, value, DWORD(std::size(value)));
  require(length && length < std::size(value), "GetModuleFileName");
  return {value, length};
}
std::wstring systemDirectory() {
  wchar_t directory[MAX_PATH]; BOOL wow = FALSE;
  require(IsWow64Process(GetCurrentProcess(), &wow) != FALSE, "IsWow64Process");
  const UINT length = wow ? GetSystemWow64DirectoryW(directory, MAX_PATH) : GetSystemDirectoryW(directory, MAX_PATH);
  require(length && length < MAX_PATH, "system-directory");
  return {directory, length};
}
dxvk::umd::diagnostic::RuntimeImage image(HMODULE module) {
  MODULEINFO info{};
  require(K32GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)) != FALSE,
          "K32GetModuleInformation");
  return {static_cast<const uint8_t*>(info.lpBaseOfDll), info.SizeOfImage};
}
uint16_t moduleMachine(HMODULE module) {
  const auto loaded = image(module); uint32_t pe = 0; uint16_t machine = 0;
  require(dxvk::umd::diagnostic::runtimeImageRead(loaded, 0x3c, pe)
      && dxvk::umd::diagnostic::runtimeImageRead(loaded, size_t(pe) + 4, machine), "module-machine");
  return machine;
}
void auditModules(const std::wstring& directory) {
  for (const auto* name : {L"d3d8.dll", L"d3d8thk.dll", L"d3d9.dll", L"dxgi.dll", L"d3d11.dll", L"d3d9on12.dll"}) {
    if (const HMODULE loaded = GetModuleHandleW(name)) {
      const auto actual = path(loaded); const auto expected = directory + L"\\" + name;
      trace("D3D8_MODULE name=%ls path=%ls machine=%04x", name, actual.c_str(), unsigned(moduleMachine(loaded)));
      require(!_wcsicmp(actual.c_str(), expected.c_str()), "system-api-module-path");
    }
  }
  require(!GetModuleHandleW(L"d3d10warp.dll"), "no-WARP-module");
}
struct Permission {
  static constexpr const wchar_t* name = L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC";
  ~Permission() { if (active) SetEnvironmentVariableW(name, nullptr); }
  bool active = false;
  void enable() {
    SetLastError(ERROR_SUCCESS);
    require(GetEnvironmentVariableW(name, nullptr, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND,
            "diagnostic-gate-originally-absent");
    require(SetEnvironmentVariableW(name, L"read-only-legacy-fog-478eca2") != FALSE, "set-read-only-gate");
    active = true;
  }
};

// The original SysWOW64 D3D8 runtime imports this KMT function directly from
// GDI32. Preserve every real query/status, changing only the exact DX9 filename
// supplied by the caller's independently verified installed package receipt.
class Selector {
  using Query = decltype(&D3DKMTQueryAdapterInfo);
  static std::atomic<Selector*> current;
  Query original = nullptr;
  void** slot = nullptr;
  std::wstring expected, replacement;
  std::atomic<unsigned> substitutions{0};
  static NTSTATUS APIENTRY query(const D3DKMT_QUERYADAPTERINFO* input) {
    auto* self = current.load();
    if (!self) return NTSTATUS(0xc0000001u);
    const auto request = input ? *input : D3DKMT_QUERYADAPTERINFO{};
    const NTSTATUS status = self->original(input);
    if (status < 0 || !input || request.Type != KMTQAITYPE_UMDRIVERNAME
        || request.PrivateDriverDataSize != sizeof(D3DKMT_UMDFILENAMEINFO) || !request.pPrivateDriverData)
      return status;
    auto* info = static_cast<D3DKMT_UMDFILENAMEINFO*>(request.pPrivateDriverData);
    if (info->Version != KMTUMDVERSION_DX9) return status;
    const auto* end = std::find(info->UmdFileName, info->UmdFileName + std::size(info->UmdFileName), wchar_t(0));
    if (end == info->UmdFileName + std::size(info->UmdFileName)
        || _wcsicmp(info->UmdFileName, self->expected.c_str())) return status;
    std::fill(std::begin(info->UmdFileName), std::end(info->UmdFileName), wchar_t(0));
    std::copy(self->replacement.begin(), self->replacement.end(), info->UmdFileName);
    const unsigned count = ++self->substitutions;
    trace("D3D8_SELECTOR_QUERY version=DX9 status=%08lx exact-name=1 substitutions=%u",
          static_cast<unsigned long>(status), count);
    return status;
  }
  static void* address(Query function) {
    void* value = nullptr; static_assert(sizeof(value) == sizeof(function));
    std::memcpy(&value, &function, sizeof(value)); return value;
  }
public:
  ~Selector() { if (slot) restore(); }
  void install(HMODULE module, const wchar_t* front, const wchar_t* installed) {
    replacement = front; expected = installed;
    const std::wstring prefix = L"C:\\Users\\Public\\DxvkD3D8Runtime-";
    const std::wstring suffix = L"\\viogpu-d3d9-runtime-front.dll";
    require(replacement.size() < 260 && replacement.size() > prefix.size() + suffix.size()
        && !_wcsnicmp(replacement.c_str(), prefix.c_str(), prefix.size())
        && !_wcsicmp(replacement.c_str() + replacement.size() - suffix.size(), suffix.c_str())
        && replacement.find(L"..") == std::wstring::npos && expected.size() && expected.size() < 260,
        "owned-frontend/exact-installed-name");
    require(GetFileAttributesW(front) != INVALID_FILE_ATTRIBUTES, "frontend-file-present");
    std::vector<dxvk::umd::diagnostic::RuntimeImport> imports;
    const auto loaded = image(module);
    require(dxvk::umd::diagnostic::findRuntimeImports(loaded, "GDI32.dll", "D3DKMTQueryAdapterInfo", imports)
        && imports.size() == 1 && imports[0].pointerBytes == sizeof(void*), "exact-system8-KMT-import");
    auto** target = reinterpret_cast<void**>(const_cast<uint8_t*>(loaded.data) + imports[0].offset);
    void* old = *target;
    const HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    const FARPROC exported = gdi ? GetProcAddress(gdi, "D3DKMTQueryAdapterInfo") : nullptr;
    void* exportedAddress = nullptr; static_assert(sizeof(exported) == sizeof(exportedAddress));
    std::memcpy(&exportedAddress, &exported, sizeof(exportedAddress));
    require(gdi && old == exportedAddress,
            "original-system-KMT-import");
    std::memcpy(&original, &old, sizeof(original));
    Selector* absent = nullptr;
    require(current.compare_exchange_strong(absent, this), "single-owned-selector");
    DWORD protection = 0, ignored = 0;
    if (!VirtualProtect(target, sizeof(void*), PAGE_READWRITE, &protection)) {
      current.store(nullptr); require(false, "selector-write-protection");
    }
    const bool replaced = InterlockedCompareExchangePointer(target, address(&query), old) == old;
    const bool protectedAgain = VirtualProtect(target, sizeof(void*), protection, &ignored) != FALSE;
    if (replaced) slot = target;
    require(replaced && protectedAgain, "selector-install-and-protection");
    trace("D3D8_SELECTOR installed=1 machine=%04x pointer_bytes=%zu slot_rva=%zx registry_writes=0",
          unsigned(imports[0].machine), sizeof(void*), imports[0].offset);
  }
  bool restore() noexcept {
    DWORD protection = 0, ignored = 0;
    if (!slot || !VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    const bool restored = InterlockedCompareExchangePointer(slot, address(original), address(&query)) == address(&query);
    const bool protectedAgain = VirtualProtect(slot, sizeof(void*), protection, &ignored) != FALSE;
    slot = nullptr; current.store(nullptr);
    trace("D3D8_SELECTOR restored=%u protection_restored=%u substitutions=%u", unsigned(restored),
          unsigned(protectedAgain), substitutions.load());
    return restored && protectedAgain;
  }
  unsigned count() const { return substitutions.load(); }
};
std::atomic<Selector*> Selector::current{nullptr};

struct Window {
  HWND value = nullptr;
  ~Window() { if (value) DestroyWindow(value); }
  void create() {
    value = CreateWindowExW(0, L"STATIC", L"VioGPU genuine DX8 offscreen", WS_POPUP,
                           0, 0, 8, 8, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    require(value != nullptr, "owned-hidden-window");
  }
};
void capture(IDirect3DDevice8* device, IDirect3DSurface8* target, unsigned stage, uint32_t expected) {
  Com<IDirect3DSurface8> readback;
  call(device->CreateImageSurface(8, 8, D3DFMT_A8R8G8B8, &readback.ptr), "CreateImageSurface");
  call(device->CopyRects(target, nullptr, 0, readback.ptr, nullptr), "CopyRects-RT-to-systemmem");
  D3DLOCKED_RECT locked{};
  call(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY), "LockRect-readback");
  bool exact = locked.pBits && locked.Pitch >= 32;
  if (exact) for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x) {
    uint32_t pixel = 0;
    std::memcpy(&pixel, static_cast<const uint8_t*>(locked.pBits) + size_t(y) * size_t(locked.Pitch) + 4 * x, 4);
    trace("D3D8_PIXEL stage=%u x=%u y=%u value=%08x", stage, x, y, pixel);
    exact = exact && pixel == expected;
  }
  call(readback->UnlockRect(), "UnlockRect-readback");
  require(exact, "independent-offscreen-pixel-oracle");
}
void state(IDirect3DDevice8* device) {
  for (const auto pair : {std::pair<D3DRENDERSTATETYPE, DWORD>{D3DRS_ZENABLE, FALSE},
      {D3DRS_ZWRITEENABLE, FALSE}, {D3DRS_CULLMODE, D3DCULL_NONE}, {D3DRS_LIGHTING, FALSE},
      {D3DRS_ALPHABLENDENABLE, FALSE}, {D3DRS_ALPHATESTENABLE, FALSE}, {D3DRS_FOGENABLE, FALSE},
      {D3DRS_DITHERENABLE, FALSE}}) call(device->SetRenderState(pair.first, pair.second), "SetRenderState");
  call(device->SetPixelShader(0), "SetPixelShader-fixed");
  call(device->SetTexture(0, nullptr), "SetTexture-null");
  call(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1), "ColorOp");
  call(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE), "ColorArg");
  call(device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1), "AlphaOp");
  call(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE), "AlphaArg");
  call(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE), "DisableStage1");
}
void fvf(IDirect3DDevice8* device, uint32_t color) {
  struct Vertex { float x, y, z, rhw; DWORD color; };
  const Vertex vertices[] = {{-.5f,-.5f,.5f,1,color},{7.5f,-.5f,.5f,1,color},
                            {-.5f,7.5f,.5f,1,color},{7.5f,7.5f,.5f,1,color}};
  call(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "SetVertexShader-FVF");
  call(device->BeginScene(), "BeginScene");
  call(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)), "DrawPrimitiveUP-FVF");
  call(device->EndScene(), "EndScene");
}
void offscreen(IDirect3D8* api, UINT adapter, HWND window) {
  for (const D3DFORMAT color : {D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8}) {
    call(api->CheckDeviceFormat(adapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
        D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, color), "CheckDeviceFormat-RT");
    call(api->CheckDeviceFormat(adapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
        D3DUSAGE_DYNAMIC, D3DRTYPE_TEXTURE, color), "CheckDeviceFormat-dynamic2D");
  }
  for (const D3DFORMAT depth : {D3DFMT_D16, D3DFMT_D24S8})
    call(api->CheckDeviceFormat(adapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depth), "CheckDeviceFormat-depth");
  D3DPRESENT_PARAMETERS parameters{};
  parameters.BackBufferWidth = parameters.BackBufferHeight = 8;
  parameters.BackBufferFormat = D3DFMT_X8R8G8B8; parameters.BackBufferCount = 1;
  parameters.SwapEffect = D3DSWAPEFFECT_DISCARD; parameters.hDeviceWindow = window; parameters.Windowed = TRUE;
  Com<IDirect3DDevice8> device;
  call(api->CreateDevice(adapter, D3DDEVTYPE_HAL, window,
       D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &parameters, &device.ptr), "CreateDevice-HAL-hardwareVP");
  require(device.ptr != nullptr, "genuine-HAL-device");
  Com<IDirect3DSurface8> original, target;
  call(device->GetRenderTarget(&original.ptr), "GetRenderTarget");
  const auto createTarget = [&] {
    call(device->CreateRenderTarget(8, 8, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, FALSE, &target.ptr), "CreateRenderTarget");
    call(device->SetRenderTarget(target.ptr, nullptr), "SetRenderTarget"); state(device.ptr);
  };
  createTarget();
  call(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff123456, 1, 0), "Clear");
  capture(device.ptr, target.ptr, 1, 0xff123456);
  fvf(device.ptr, 0xff739a4c); capture(device.ptr, target.ptr, 2, 0xff739a4c);
  const DWORD declaration[] = {D3DVSD_STREAM(0), D3DVSD_REG(0, D3DVSDT_FLOAT4),
                              D3DVSD_REG(1, D3DVSDT_D3DCOLOR), D3DVSD_END()};
  const DWORD vs[] = {D3DVS_VERSION(1,1), D3DSIO_MOV,
    0x80000000u | DWORD(D3DSPR_RASTOUT) | D3DSP_WRITEMASK_ALL,
    0x80000000u | DWORD(D3DSPR_INPUT) | D3DSP_NOSWIZZLE,
    D3DSIO_MOV, 0x80000000u | DWORD(D3DSPR_ATTROUT) | D3DSP_WRITEMASK_ALL,
    0x80000001u | DWORD(D3DSPR_INPUT) | D3DSP_NOSWIZZLE, D3DSIO_END};
  const DWORD ps[] = {D3DPS_VERSION(1,1), D3DSIO_MOV, 0x80000000u | D3DSP_WRITEMASK_ALL,
                     0x80000000u | DWORD(D3DSPR_INPUT) | D3DSP_NOSWIZZLE, D3DSIO_END};
  DWORD vertexShader = 0, pixelShader = 0;
  call(device->CreateVertexShader(declaration, vs, &vertexShader, 0), "CreateVertexShader-1.1");
  call(device->CreatePixelShader(ps, &pixelShader), "CreatePixelShader-1.1");
  call(device->SetVertexShader(vertexShader), "SetVertexShader-1.1");
  call(device->SetPixelShader(pixelShader), "SetPixelShader-1.1");
  struct ClipVertex { float x, y, z, w; DWORD color; };
  const ClipVertex vertices[] = {{-1,1,.5f,1,0xffc0568e},{1,1,.5f,1,0xffc0568e},
                                 {-1,-1,.5f,1,0xffc0568e},{1,-1,.5f,1,0xffc0568e}};
  call(device->BeginScene(), "BeginScene");
  call(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ClipVertex)), "DrawPrimitiveUP-SM1.1");
  call(device->EndScene(), "EndScene"); capture(device.ptr, target.ptr, 3, 0xffc0568e);
  call(device->SetPixelShader(0), "UnbindPixelShader");
  call(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "UnbindVertexShader");
  call(device->DeletePixelShader(pixelShader), "DeletePixelShader");
  call(device->DeleteVertexShader(vertexShader), "DeleteVertexShader");
  Com<IDirect3DTexture8> texture;
  call(device->CreateTexture(2, 2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture.ptr), "CreateTexture-dynamic");
  D3DLOCKED_RECT locked{}; call(texture->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD), "LockRect-dynamic");
  require(locked.pBits && locked.Pitch >= 8, "dynamic-texture-mapping");
  for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x) {
    const DWORD color = 0xff288cb0;
    std::memcpy(static_cast<uint8_t*>(locked.pBits) + size_t(y) * size_t(locked.Pitch) + x * 4, &color, 4);
  }
  call(texture->UnlockRect(0), "UnlockRect-dynamic");
  call(device->SetTexture(0, texture.ptr), "SetTexture-dynamic");
  call(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE), "ColorArg-texture");
  call(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE), "AlphaArg-texture");
  call(device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT), "MinFilter");
  call(device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT), "MagFilter");
  struct TexVertex { float x, y, z, rhw, u, v; };
  const TexVertex texVertices[] = {{-.5f,-.5f,.5f,1,.25f,.25f},{7.5f,-.5f,.5f,1,.75f,.25f},
                                  {-.5f,7.5f,.5f,1,.25f,.75f},{7.5f,7.5f,.5f,1,.75f,.75f}};
  call(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1), "SetVertexShader-FVF-texture");
  call(device->BeginScene(), "BeginScene");
  call(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, texVertices, sizeof(TexVertex)), "DrawPrimitiveUP-texture");
  call(device->EndScene(), "EndScene"); capture(device.ptr, target.ptr, 4, 0xff288cb0);
  call(device->SetTexture(0, nullptr), "UnbindTexture"); texture.reset();
  call(device->SetRenderTarget(original.ptr, nullptr), "RestoreRenderTarget"); target.reset(); original.reset();
  call(device->Reset(&parameters), "Reset");
  call(device->GetRenderTarget(&original.ptr), "GetRenderTarget-after-reset"); createTarget();
  call(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff623a81, 1, 0), "Clear-after-reset");
  capture(device.ptr, target.ptr, 5, 0xff623a81);
  fvf(device.ptr, 0xff91b742); capture(device.ptr, target.ptr, 6, 0xff91b742);
  call(device->SetRenderTarget(original.ptr, nullptr), "RestoreRenderTarget-final");
  target.reset(); original.reset(); device.reset();
  trace("D3D8_OFFSCREEN PASS stages=6 pixels=384 shader=VS1.1/PS1.1 dynamic_texture=1 resets=1 presents=0");
}
}

int wmain(int argc, wchar_t** argv) {
  const bool enumerate = argc == 2 && !std::wcscmp(argv[1], L"--enumerate");
  const bool hardware = argc == 2 && !std::wcscmp(argv[1], L"--offscreen");
  const bool selected = argc == 4 && !std::wcscmp(argv[1], L"--front-enumerate");
  if (!enumerate && !hardware && !selected) return 64;
  try {
    SetLastError(ERROR_SUCCESS);
    require(GetEnvironmentVariableW(Permission::name, nullptr, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND,
            "diagnostic-gate-originally-absent");
    const auto directory = systemDirectory(); const auto expected = directory + L"\\d3d8.dll";
    if (GetFileAttributesW(expected.c_str()) == INVALID_FILE_ATTRIBUTES) {
      trace("D3D8_UNAVAILABLE path=%ls pointer_bytes=%zu exit=77", expected.c_str(), sizeof(void*)); return 77;
    }
    require(!GetModuleHandleW(L"d3d8.dll"), "runtime-not-preloaded");
    Module runtime; runtime.value = LoadLibraryExW(expected.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(runtime.value && !_wcsicmp(path(runtime.value).c_str(), expected.c_str()), "genuine-system-d3d8");
    auditModules(directory);
    trace("D3D8_RUNTIME path=%ls machine=%04x pointer_bytes=%zu sdk_version=%u caps_bytes=%zu",
          expected.c_str(), unsigned(moduleMachine(runtime.value)), sizeof(void*), D3D_SDK_VERSION, sizeof(D3DCAPS8));
    Permission permission; Selector selector;
    if (selected) { permission.enable(); selector.install(runtime.value, argv[2], argv[3]); }
    using Create = IDirect3D8* (WINAPI*)(UINT);
    const FARPROC symbol = GetProcAddress(runtime.value, "Direct3DCreate8");
    require(symbol != nullptr, "Direct3DCreate8-export");
    Create create = nullptr; static_assert(sizeof(create) == sizeof(symbol)); std::memcpy(&create, &symbol, sizeof(create));
    Com<IDirect3D8> api; api.ptr = create(D3D_SDK_VERSION);
    trace("D3D8_API operation=Direct3DCreate8 object=%u", unsigned(api.ptr != nullptr));
    require(api.ptr != nullptr, "Direct3DCreate8-object");
    const UINT count = api->GetAdapterCount(); trace("D3D8_ADAPTER_COUNT count=%u", count);
    require(count <= 16, "bounded-adapter-count");
    UINT virtio = UINT(-1); bool hal = false;
    for (UINT i = 0; i < count; ++i) {
      D3DADAPTER_IDENTIFIER8 id{}; D3DCAPS8 caps{};
      const HRESULT idStatus = api->GetAdapterIdentifier(i, 0, &id);
      const HRESULT capsStatus = api->GetDeviceCaps(i, D3DDEVTYPE_HAL, &caps);
      trace("D3D8_ADAPTER index=%u identifier_hr=%08lx caps_hr=%08lx vendor=%04lx device=%04lx devcaps=%08lx vs=%08lx ps=%08lx constants=%lu",
        i, static_cast<unsigned long>(idStatus), static_cast<unsigned long>(capsStatus),
        id.VendorId, id.DeviceId, caps.DevCaps, caps.VertexShaderVersion, caps.PixelShaderVersion, caps.MaxVertexShaderConst);
      if (idStatus == S_OK && id.VendorId == 0x1af4 && id.DeviceId == 0x1050) {
        require(virtio == UINT(-1), "unique-VirtIO-adapter"); virtio = i;
        hal = capsStatus == S_OK && caps.DeviceType == D3DDEVTYPE_HAL
          && (caps.DevCaps & (D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_HWTRANSFORMANDLIGHT))
            == (D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_HWTRANSFORMANDLIGHT);
      }
    }
    if (hardware) {
      require(virtio != UINT(-1) && hal, "VirtIO-HAL-hardware-caps");
      Window window; window.create(); offscreen(api.ptr, virtio, window.value);
    }
    api.reset(); auditModules(directory);
    if (selected) {
      const unsigned substitutions = selector.count(); require(selector.restore(), "selector-restoration");
      require(substitutions > 0, "actual-system8-exact-name-selection");
    }
    trace("D3D8_COMPLETE mode=%s adapters=%u create_device=%u presents=0 registry_writes=0",
          hardware ? "offscreen" : (selected ? "front-enumerate" : "enumerate"), count, unsigned(hardware));
    return traceFailed ? 1 : 0;
  } catch (const std::exception& error) { trace("D3D8_FAILED reason=%s", error.what()); return 1; }
}
