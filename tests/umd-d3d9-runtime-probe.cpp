// Genuine Microsoft runtime diagnostics. Enumeration creates no device; the
// separate device-contract mode requires the frontend that blocks CreateDevice.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <d3dkmthk.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace {
using QueryAdapter = NTSTATUS (APIENTRY*)(D3DKMT_QUERYADAPTERINFO*);
QueryAdapter originalQuery = nullptr;
WCHAR selectedFront[MAX_PATH] = {};
LONG nameRedirects = 0;
constexpr WCHAR installedFront[] =
  L"C:\\WINDOWS\\System32\\DriverStore\\FileRepository\\viogpuwddm.inf_arm64_46d4547d492b1e80\\viogpud3dx.dll";

NTSTATUS APIENTRY readOnlyNameQuery(D3DKMT_QUERYADAPTERINFO* args) {
  const NTSTATUS status = originalQuery(args);
  if (status || !args || args->Type != KMTQAITYPE_UMDRIVERNAME
      || args->PrivateDriverDataSize != sizeof(D3DKMT_UMDFILENAMEINFO)
      || !args->pPrivateDriverData) return status;
  auto& info = *static_cast<D3DKMT_UMDFILENAMEINFO*>(args->pPrivateDriverData);
  // The original successful query must identify this exact installed package.
  // Other adapters, DDI versions and all private/caps queries remain original.
  if (info.Version != KMTUMDVERSION_DX9
      || !std::wmemchr(info.UmdFileName, L'\0', _countof(info.UmdFileName))
      || _wcsicmp(info.UmdFileName, installedFront)) return status;
  if (wcscpy_s(info.UmdFileName, selectedFront)) return status;
  const LONG count = InterlockedIncrement(&nameRedirects);
  if (count <= 64)
    std::printf("SYSTEM_D3D9_NAME_REDIRECT count=%ld version=0 kernel_adapter=%u path=%ls process_local=1 readonly=1\n",
      count, args->hAdapter, info.UmdFileName);
  return status;
}

struct ReadOnlyNameSelector {
  PVOID volatile* slot = nullptr;
  PVOID saved = nullptr;
  DWORD originalProtection = 0;
  bool restore() {
    if (!slot) return true;
    DWORD protect = 0;
    if (!VirtualProtect(const_cast<PVOID*>(slot), sizeof(PVOID), PAGE_READWRITE, &protect)) return false;
    InterlockedExchangePointer(slot, saved);
    DWORD ignored = 0;
    const bool restored = VirtualProtect(const_cast<PVOID*>(slot), sizeof(PVOID), originalProtection, &ignored) != FALSE;
    if (restored) slot = nullptr;
    return restored;
  }
  ~ReadOnlyNameSelector() { restore(); }
  bool install(const WCHAR* front) {
    static_assert(sizeof(PVOID) == sizeof(ULONGLONG));
    constexpr WCHAR ownedPrefix[] = L"C:\\Users\\Public\\DxvkD3D9RuntimeReadOnly-";
    const DWORD length = GetFullPathNameW(front, DWORD(_countof(selectedFront)), selectedFront, nullptr);
    if (!length || length >= _countof(selectedFront)
        || _wcsnicmp(selectedFront, ownedPrefix, _countof(ownedPrefix) - 1)) return false;
    const WCHAR* leaf = std::wcsrchr(selectedFront, L'\\');
    if (!leaf || std::wcscmp(leaf + 1, L"viogpu-d3d9-runtime-front.dll")) return false;
    const HMODULE module = GetModuleHandleW(L"d3d9.dll");
    if (!module) return false;
    WCHAR system[MAX_PATH] = {}, loaded[MAX_PATH] = {};
    const UINT systemSize = GetSystemDirectoryW(system, UINT(_countof(system)));
    const DWORD loadedSize = GetModuleFileNameW(module, loaded, DWORD(_countof(loaded)));
    if (!systemSize || systemSize >= _countof(system)
        || !loadedSize || loadedSize >= _countof(loaded)) return false;
    const std::wstring expected = std::wstring(system) + L"\\d3d9.dll";
    if (_wcsicmp(expected.c_str(), loaded)) return false;
    auto* base = reinterpret_cast<BYTE*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    const DWORD imageBytes = nt->OptionalHeader.SizeOfImage;
    const auto imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress || imports.VirtualAddress >= imageBytes
        || imports.Size > imageBytes - imports.VirtualAddress) return false;
    const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
    const UINT count = imports.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
    for (UINT i = 0; i < count && descriptors[i].Name; ++i) {
      const auto& descriptor = descriptors[i];
      if (descriptor.Name >= imageBytes || descriptor.OriginalFirstThunk >= imageBytes
          || descriptor.FirstThunk >= imageBytes || !descriptor.OriginalFirstThunk) return false;
      if (_stricmp(reinterpret_cast<const char*>(base + descriptor.Name), "GDI32.dll")) continue;
      const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + descriptor.OriginalFirstThunk);
      auto* pointers = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor.FirstThunk);
      const SIZE_T maxNames = (imageBytes - descriptor.OriginalFirstThunk) / sizeof(IMAGE_THUNK_DATA64);
      const SIZE_T maxPointers = (imageBytes - descriptor.FirstThunk) / sizeof(IMAGE_THUNK_DATA64);
      for (SIZE_T n = 0; n < maxNames && n < maxPointers && names[n].u1.AddressOfData; ++n) {
        if (IMAGE_SNAP_BY_ORDINAL64(names[n].u1.Ordinal)) continue;
        if (names[n].u1.AddressOfData >= imageBytes) return false;
        const auto* symbol = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[n].u1.AddressOfData);
        if (std::strcmp(reinterpret_cast<const char*>(symbol->Name), "D3DKMTQueryAdapterInfo")) continue;
        std::memcpy(&saved, &pointers[n].u1.Function, sizeof(saved));
        static_assert(sizeof(originalQuery) == sizeof(saved));
        std::memcpy(&originalQuery, &saved, sizeof(originalQuery));
        if (!originalQuery) return false;
        QueryAdapter replacement = readOnlyNameQuery;
        PVOID target = nullptr;
        static_assert(sizeof(target) == sizeof(replacement));
        std::memcpy(&target, &replacement, sizeof(target));
        auto* address = reinterpret_cast<PVOID volatile*>(&pointers[n].u1.Function);
        DWORD protect = 0;
        if (!VirtualProtect(const_cast<PVOID*>(address), sizeof(PVOID), PAGE_READWRITE, &protect)) return false;
        InterlockedExchangePointer(address, target);
        slot = address;
        originalProtection = protect;
        DWORD ignored = 0;
        if (!VirtualProtect(const_cast<PVOID*>(address), sizeof(PVOID), protect, &ignored)) return false;
        std::printf("SYSTEM_D3D9_NAME_SELECTOR installed=1 process_local=1 version=0 device_creation=blocked\n");
        return true;
      }
    }
    return false;
  }
};
}

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
  if (!SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", L"read-only-478eca2")) return 1;
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

int frontEnumerate(const WCHAR* front) {
  ReadOnlyNameSelector selector;
  if (!selector.install(front)) return 1;
  if (!SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", L"read-only-478eca2")) return 1;
  const int result = enumerate();
  SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", nullptr);
  const bool restored = selector.restore();
  std::printf("SYSTEM_D3D9_FRONT_ENUM result=%d redirects=%ld selector_restored=%u readonly=1 ordinary_runtime_admission=0\n",
    result, nameRedirects, unsigned(restored));
  return result || !restored || !nameRedirects ? 1 : 0;
}

int deviceContract() {
  // Verify the genuine system module and record caps before any device attempt.
  if (enumerate()) return 1;
  struct Apis {
    IDirect3D9* normal = nullptr;
    IDirect3D9Ex* extended = nullptr;
    ~Apis() {
      if (extended) extended->Release();
      if (normal) normal->Release();
    }
  } api;
  api.normal = Direct3DCreate9(D3D_SDK_VERSION);
  if (!api.normal || FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &api.extended))
      || !api.extended) return 1;
  const auto supported = [](IDirect3D9* runtime, const char* name) {
    D3DADAPTER_IDENTIFIER9 identifier = {};
    D3DCAPS9 caps = {};
    const UINT adapters = runtime->GetAdapterCount();
    const HRESULT identity = adapters == 1
      ? runtime->GetAdapterIdentifier(0, 0, &identifier) : E_FAIL;
    const HRESULT capsHr = identity == S_OK
      ? runtime->GetDeviceCaps(0, D3DDEVTYPE_HAL, &caps) : E_FAIL;
    const bool accepted = adapters == 1 && identity == S_OK && capsHr == S_OK
      && identifier.VendorId == 0x1af4 && identifier.DeviceId == 0x1050
      && caps.VertexShaderVersion == D3DVS_VERSION(2, 0)
      && caps.PixelShaderVersion == D3DPS_VERSION(2, 0);
    std::printf("SYSTEM_D3D9_DEVICE_PREFLIGHT api=%s adapters=%u identity=%08lx caps=%08lx vendor=%08x device=%08x accepted=%u\n",
      name, adapters, static_cast<unsigned long>(identity),
      static_cast<unsigned long>(capsHr), identifier.VendorId, identifier.DeviceId,
      unsigned(accepted));
    return accepted;
  };
  if (!supported(api.normal, "9") || !supported(api.extended, "9Ex")) return 1;
  struct Window {
    HWND handle = nullptr;
    ~Window() { if (handle) DestroyWindow(handle); }
  } window;
  window.handle = CreateWindowExW(0, L"STATIC", L"VioGpu runtime device contract",
    WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (!window.handle || IsWindowVisible(window.handle)) return 1;
  D3DPRESENT_PARAMETERS parameters = {};
  parameters.BackBufferWidth = parameters.BackBufferHeight = 16;
  parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
  parameters.BackBufferCount = 1;
  parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
  parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
  parameters.hDeviceWindow = window.handle;
  parameters.Windowed = TRUE;
  parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
  IDirect3DDevice9* normal = nullptr;
  auto normalParameters = parameters;
  std::printf("SYSTEM_D3D9_DEVICE_ATTEMPT api=9 ordinal=0 hal=1 hardware_vertex_processing=1 window_visible=0 core_creation=blocked\n");
  const HRESULT normalHr = api.normal->CreateDevice(0, D3DDEVTYPE_HAL, window.handle,
    D3DCREATE_HARDWARE_VERTEXPROCESSING, &normalParameters, &normal);
  std::printf("SYSTEM_D3D9_DEVICE_RESULT api=9 hr=%08lx object=%u explicit_create_device_calls=1\n",
    static_cast<unsigned long>(normalHr), unsigned(normal != nullptr));
  if (normal) normal->Release();
  if (SUCCEEDED(normalHr) || normal) return 1;
  IDirect3DDevice9Ex* extended = nullptr;
  auto extendedParameters = parameters;
  std::printf("SYSTEM_D3D9_DEVICE_ATTEMPT api=9Ex ordinal=0 hal=1 hardware_vertex_processing=1 window_visible=0 core_creation=blocked\n");
  const HRESULT extendedHr = api.extended->CreateDeviceEx(0, D3DDEVTYPE_HAL, window.handle,
    D3DCREATE_HARDWARE_VERTEXPROCESSING, &extendedParameters, nullptr, &extended);
  std::printf("SYSTEM_D3D9_DEVICE_RESULT api=9Ex hr=%08lx object=%u explicit_create_device_calls=2\n",
    static_cast<unsigned long>(extendedHr), unsigned(extended != nullptr));
  if (extended) extended->Release();
  if (SUCCEEDED(extendedHr) || extended) return 1;
  std::printf("SYSTEM_D3D9_DEVICE_CONTRACT DONE explicit_create_device_calls=2 core_create_device_calls=0 rendering=0 ordinary_runtime_admission=0\n");
  return 0;
}

int frontDeviceContract(const WCHAR* front) {
  ReadOnlyNameSelector selector;
  if (!selector.install(front)) return 1;
  if (!SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", L"read-only-478eca2")) return 1;
  const int result = deviceContract();
  SetEnvironmentVariableW(L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", nullptr);
  const bool restored = selector.restore();
  std::printf("SYSTEM_D3D9_FRONT_DEVICE_CONTRACT result=%d redirects=%ld selector_restored=%u core_create_device_calls=0 rendering=0 ordinary_runtime_admission=0\n",
    result, nameRedirects, unsigned(restored));
  return result || !restored || !nameRedirects ? 1 : 0;
}

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  try {
    if (argc == 2 && !std::wcscmp(argv[1], L"--enumerate")) return enumerate();
    if (argc == 3 && !std::wcscmp(argv[1], L"--front-guard")) return frontGuard(argv[2]);
    if (argc == 3 && !std::wcscmp(argv[1], L"--front-enumerate")) return frontEnumerate(argv[2]);
    if (argc == 3 && !std::wcscmp(argv[1], L"--front-device-contract")) return frontDeviceContract(argv[2]);
  } catch (...) { return 1; }
  std::fprintf(stderr, "Usage: --enumerate | --front-guard <owned-frontend-path> | --front-enumerate <owned-frontend-path> | --front-device-contract <owned-frontend-path>\n");
  return 64;
}
