// Read-only diagnostic frontend for Microsoft's D3D9 runtime. This DLL is
// separate from the production UMD and never forwards CreateDevice.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <intrin.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <unordered_map>

namespace {
constexpr WCHAR permission[] = L"read-only-478eca2";
constexpr WCHAR candidate[] =
  L"C:\\Users\\Public\\DxvkD3D9CapsCandidate-478eca2\\viogpudxvk.dll";
std::mutex adaptersMutex;
std::unordered_map<HANDLE, D3DDDI_ADAPTERFUNCS> adapters;

bool permitted() {
  WCHAR value[64] = {};
  const DWORD size = GetEnvironmentVariableW(
    L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", value, DWORD(_countof(value)));
  return size && size < _countof(value) && !std::wcscmp(value, permission);
}

bool retain(HANDLE handle, D3DDDI_ADAPTERFUNCS& functions) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle);
  if (entry == adapters.end()) return false;
  functions = entry->second;
  return true;
}

HRESULT APIENTRY getCaps(HANDLE handle, const D3DDDIARG_GETCAPS* args) {
  D3DDDI_ADAPTERFUNCS original = {};
  if (!args || !retain(handle, original)) return E_INVALIDARG;
  const auto input = *args;
  std::printf("SYSTEM_D3D9_CAPS_BEGIN type=%u bytes=%u info=%u adapter=%p\n",
    unsigned(input.Type), input.DataSize, unsigned(input.pInfo != nullptr), handle);
  const HRESULT hr = original.pfnGetCaps(handle, args);
  UINT count = UINT_MAX;
  if (hr == S_OK && input.pData && input.DataSize == sizeof(count)
      && (input.Type == D3DDDICAPS_GETFORMATCOUNT
       || input.Type == D3DDDICAPS_GETD3DQUERYCOUNT))
    std::memcpy(&count, input.pData, sizeof(count));
  std::printf("SYSTEM_D3D9_CAPS_END type=%u hr=%08lx count=%u adapter=%p\n",
    unsigned(input.Type), static_cast<unsigned long>(hr), count, handle);
  return hr;
}

HRESULT APIENTRY createDevice(HANDLE handle, D3DDDIARG_CREATEDEVICE*) {
  // Enumeration diagnostics must not construct even an empty GPU device.
  std::printf("SYSTEM_D3D9_CREATE_BLOCKED adapter=%p hr=%08lx\n",
    handle, static_cast<unsigned long>(D3DERR_NOTAVAILABLE));
  return D3DERR_NOTAVAILABLE;
}

HRESULT APIENTRY closeAdapter(HANDLE handle) {
  D3DDDI_ADAPTERFUNCS original = {};
  {
    std::lock_guard<std::mutex> lock(adaptersMutex);
    const auto entry = adapters.find(handle);
    if (entry == adapters.end()) return E_INVALIDARG;
    original = entry->second;
    adapters.erase(entry);
  }
  const HRESULT hr = original.pfnCloseAdapter(handle);
  std::printf("SYSTEM_D3D9_CLOSE hr=%08lx adapter=%p\n",
    static_cast<unsigned long>(hr), handle);
  return hr;
}
}

extern "C" HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* args) {
  if (!permitted()) return D3DERR_NOTAVAILABLE;
  if (!args || !args->pAdapterFuncs) return E_INVALIDARG;
  HMODULE caller = nullptr;
  WCHAR callerPath[MAX_PATH] = {};
  const void* address = _ReturnAddress();
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(address), &caller))
    GetModuleFileNameW(caller, callerPath, DWORD(_countof(callerPath)));
  std::printf("SYSTEM_D3D9_OPEN_BEGIN interface=%u version=%u runtime=%p caller=%ls readonly=1\n",
    args->Interface, args->Version, args->hAdapter, callerPath);
  // Keep this module loaded for the diagnostic process lifetime, including
  // any adapter callbacks after Windows unloads another frontend instance.
  static HMODULE core = LoadLibraryExW(candidate, nullptr,
    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (!core) return HRESULT_FROM_WIN32(GetLastError());
  const FARPROC symbol = GetProcAddress(core, "VioGpuDxvkOpenAdapter9ForTest");
  using Open = HRESULT (APIENTRY*)(D3DDDIARG_OPENADAPTER*);
  static_assert(sizeof(Open) == sizeof(symbol));
  Open open = nullptr;
  std::memcpy(&open, &symbol, sizeof(open));
  if (!open) return E_NOINTERFACE;
  D3DDDIARG_OPENADAPTER local = *args;
  D3DDDI_ADAPTERFUNCS original = {};
  local.pAdapterFuncs = &original;
  const HRESULT hr = open(&local);
  std::printf("SYSTEM_D3D9_OPEN_END hr=%08lx driver_version=%u adapter=%p core=%ls\n",
    static_cast<unsigned long>(hr), local.DriverVersion, local.hAdapter, candidate);
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!original.pfnGetCaps || !original.pfnCreateDevice || !original.pfnCloseAdapter)
    return E_FAIL;
  try {
    bool inserted;
    {
      std::lock_guard<std::mutex> lock(adaptersMutex);
      inserted = adapters.emplace(local.hAdapter, original).second;
    }
    if (!inserted) {
      original.pfnCloseAdapter(local.hAdapter);
      return E_FAIL;
    }
  } catch (...) {
    original.pfnCloseAdapter(local.hAdapter);
    return E_OUTOFMEMORY;
  }
  const D3DDDI_ADAPTERFUNCS wrapped = {getCaps, createDevice, closeAdapter};
  *args->pAdapterFuncs = wrapped;
  args->hAdapter = local.hAdapter;
  args->DriverVersion = local.DriverVersion;
  return S_OK;
}
