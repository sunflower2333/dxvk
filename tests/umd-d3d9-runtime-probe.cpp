// Read-only enumeration through the Microsoft DLL. No CreateDevice call,
// app-local D3D runtime, drawing, presentation or registration is performed.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

int frontGuard(const WCHAR* path) {
  SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", nullptr);
  const HMODULE module = LoadLibraryExW(path, nullptr,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (!module) return 1;
  const FARPROC symbol = GetProcAddress(module, "OpenAdapter");
  using Open = HRESULT (APIENTRY*)(D3DDDIARG_OPENADAPTER*);
  static_assert(sizeof(Open) == sizeof(symbol));
  Open open = nullptr;
  std::memcpy(&open, &symbol, sizeof(open));
  if (!open) return 1;
  const HRESULT denied = open(nullptr);
  if (!SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", L"read-only-061ee8f")) return 1;
  const HRESULT invalid = open(nullptr);
  SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", nullptr);
  FreeLibrary(module);
  if (denied != D3DERR_NOTAVAILABLE || invalid != E_INVALIDARG) return 1;
  std::printf("SYSTEM_D3D9_FRONT_GUARD PASS denied=%08lx invalid=%08lx no_core_open=1\n",
    static_cast<unsigned long>(denied), static_cast<unsigned long>(invalid));
  return 0;
}

int enumerate() {
  WCHAR system[MAX_PATH] = {}, loaded[MAX_PATH] = {};
  const UINT size = GetSystemDirectoryW(system, UINT(_countof(system)));
  if (!size || size >= _countof(system)) return 1;
  const HMODULE module = GetModuleHandleW(L"d3d9.dll");
  const DWORD length = GetModuleFileNameW(module, loaded, DWORD(_countof(loaded)));
  if (!module || !length || length >= _countof(loaded)) return 1;
  const std::wstring expected = std::wstring(system) + L"\\d3d9.dll";
  if (_wcsicmp(expected.c_str(), loaded)) return 1;
  std::printf("SYSTEM_D3D9_MODULE path=%ls readonly=1\n", loaded);
  IDirect3D9* api = Direct3DCreate9(D3D_SDK_VERSION);
  if (!api) { std::printf("SYSTEM_D3D9_CREATE_API null=1\n"); return 1; }
  const UINT count = api->GetAdapterCount();
  std::printf("SYSTEM_D3D9_ADAPTER_COUNT count=%u\n", count);
  if (count > 16) { api->Release(); return 1; }
  for (UINT i = 0; i < count; ++i) {
    D3DADAPTER_IDENTIFIER9 identifier = {};
    const HRESULT hr = api->GetAdapterIdentifier(i, 0, &identifier);
    std::printf("SYSTEM_D3D9_ADAPTER ordinal=%u hr=%08lx vendor=%08x device=%08x driver=%.*s description=%.*s\n",
      i, static_cast<unsigned long>(hr), identifier.VendorId, identifier.DeviceId,
      int(sizeof(identifier.Driver)), identifier.Driver,
      int(sizeof(identifier.Description)), identifier.Description);
    D3DCAPS9 caps = {};
    const HRESULT capsHr = api->GetDeviceCaps(i, D3DDEVTYPE_HAL, &caps);
    std::printf("SYSTEM_D3D9_HAL_CAPS ordinal=%u hr=%08lx vertex_shader=%08x pixel_shader=%08x\n",
      i, static_cast<unsigned long>(capsHr), caps.VertexShaderVersion, caps.PixelShaderVersion);
  }
  api->Release();
  IDirect3D9Ex* extended = nullptr;
  const HRESULT ex = Direct3DCreate9Ex(D3D_SDK_VERSION, &extended);
  std::printf("SYSTEM_D3D9_EX hr=%08lx object=%u\n", static_cast<unsigned long>(ex), unsigned(extended != nullptr));
  if (SUCCEEDED(ex) && extended) {
    const UINT adapters = extended->GetAdapterCount();
    if (adapters > 16) { extended->Release(); return 1; }
    for (UINT i = 0; i < adapters; ++i) {
      LUID luid = {};
      const HRESULT hr = extended->GetAdapterLUID(i, &luid);
      std::printf("SYSTEM_D3D9_EX_ADAPTER ordinal=%u hr=%08lx luid=",
        i, static_cast<unsigned long>(hr));
      for (const BYTE value : reinterpret_cast<const BYTE(&)[sizeof(LUID)]>(luid))
        std::printf("%02x", unsigned(value));
      std::printf("\n");
      D3DCAPS9 caps = {};
      const HRESULT capsHr = extended->GetDeviceCaps(i, D3DDEVTYPE_HAL, &caps);
      std::printf("SYSTEM_D3D9_EX_HAL_CAPS ordinal=%u hr=%08lx vertex_shader=%08x pixel_shader=%08x\n",
        i, static_cast<unsigned long>(capsHr), caps.VertexShaderVersion, caps.PixelShaderVersion);
    }
    extended->Release();
  }
  std::printf("SYSTEM_D3D9_ENUM DONE readonly=1 explicit_create_device_calls=0 rendering=0 ordinary_runtime_admission=0\n");
  return 0;
}

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  try {
    if (argc == 2 && !std::wcscmp(argv[1], L"--enumerate")) return enumerate();
    if (argc == 3 && !std::wcscmp(argv[1], L"--front-guard")) return frontGuard(argv[2]);
  } catch (...) { return 1; }
  std::fprintf(stderr, "Usage: --enumerate | --front-guard <owned-frontend-path>\n");
  return 64;
}
