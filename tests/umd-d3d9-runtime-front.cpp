// Read-only diagnostic frontend for Microsoft's D3D9 runtime. This DLL is
// separate from the production UMD and never forwards CreateDevice.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <intrin.h>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <unordered_map>

namespace {
constexpr WCHAR permission[] = L"read-only-legacy-fog-478eca2";
constexpr WCHAR candidate[] =
  L"C:\\Users\\Public\\DxvkD3D9CapsCandidate-478eca2\\viogpudxvk.dll";
std::mutex adaptersMutex;
std::unordered_map<HANDLE, D3DDDI_ADAPTERFUNCS> adapters;

void trace(const char* format, ...) {
  // This DLL and the probe each link a static CRT. Its buffered stdout can
  // otherwise split a callback record around the probe's API-result line.
  // Emit the complete record in one synchronous write before returning.
  char line[2048];
  va_list arguments;
  va_start(arguments, format);
  const int length = std::vsnprintf(line, sizeof(line), format, arguments);
  va_end(arguments);
  if (length <= 0 || size_t(length) >= sizeof(line)) return;
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, DWORD(length), &written, nullptr);
}

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
  trace("SYSTEM_D3D9_CAPS_BEGIN type=%u bytes=%u info=%u adapter=%p\n",
    unsigned(input.Type), input.DataSize, unsigned(input.pInfo != nullptr), handle);
  const HRESULT hr = original.pfnGetCaps(handle, args);
  // Preserve the prior dynamic-texture diagnostic and isolate the next
  // validator requirement: the legacy DDI fog/specular-alpha bit (0x2000).
  // The actual runtime maps it to public FOGANDSPECULARALPHA (0x10000),
  // retains the legacy bit for HAL validation, then removes it from public
  // GetDeviceCaps. Do not add an undocumented bit to production caps here.
  // This is not a production capability: CreateDevice always rejects below.
  // Restrict it to the exact old profile so an unrelated candidate cannot
  // silently gain this declaration. No device/backend callback is invoked.
  if (hr == S_OK && input.Type == D3DDDICAPS_GETD3D9CAPS) {
    if (!input.pData || input.DataSize != sizeof(D3DCAPS9)) return E_FAIL;
    D3DCAPS9 caps = {};
    std::memcpy(&caps, input.pData, sizeof(caps));
    if (caps.Caps2 || caps.PrimitiveMiscCaps != 0x00028ef0u
        || caps.DevCaps2 != D3DDEVCAPS2_STREAMOFFSET
        || caps.VertexShaderVersion != D3DVS_VERSION(2, 0)
        || caps.PixelShaderVersion != D3DPS_VERSION(2, 0)) return E_FAIL;
    caps.Caps2 = D3DCAPS2_DYNAMICTEXTURES;
    caps.PrimitiveMiscCaps |= 0x00002000u;
    std::memcpy(input.pData, &caps, sizeof(caps));
    trace("SYSTEM_D3D9_CAPS_DIAGNOSTIC field=Caps2 before=00000000 after=%08x devcaps2=%08x create_device_blocked=1 production_caps_changed=0\n",
      caps.Caps2, caps.DevCaps2);
    trace("SYSTEM_D3D9_CAPS_DIAGNOSTIC field=PrimitiveMiscCaps_DDI before=00028ef0 after=%08x legacy_fog_specular_alpha=00002000 public_fog_specular_alpha=00010000 create_device_blocked=1 production_caps_changed=0\n",
      caps.PrimitiveMiscCaps);
  }
  UINT count = UINT_MAX;
  if (hr == S_OK && input.pData && input.DataSize == sizeof(count)
      && (input.Type == D3DDDICAPS_GETFORMATCOUNT
       || input.Type == D3DDDICAPS_GETD3DQUERYCOUNT))
    std::memcpy(&count, input.pData, sizeof(count));
  trace("SYSTEM_D3D9_CAPS_END type=%u hr=%08lx count=%u adapter=%p\n",
    unsigned(input.Type), static_cast<unsigned long>(hr), count, handle);
  return hr;
}

HRESULT APIENTRY createDevice(HANDLE handle, D3DDDIARG_CREATEDEVICE* args) {
  // Capture the runtime's input contract without constructing a GPU device.
  if (args) {
    const auto input = *args;
    trace("SYSTEM_D3D9_DEVICE_CONTRACT interface=%u version=%u flags=%08x runtime=%p callbacks=%u functions=%u command_buffer=%u command_bytes=%u allocation_list=%u allocation_count=%u patch_list=%u patch_count=%u core_create_device_calls=0\n",
      input.Interface, input.Version, input.Flags.Value, input.hDevice,
      unsigned(input.pCallbacks != nullptr), unsigned(input.pDeviceFuncs != nullptr),
      unsigned(input.pCommandBuffer != nullptr), input.CommandBufferSize,
      unsigned(input.pAllocationList != nullptr), input.AllocationListSize,
      unsigned(input.pPatchLocationList != nullptr), input.PatchLocationListSize);
    if (input.pCallbacks) {
      const auto& cb = *input.pCallbacks;
      trace("SYSTEM_D3D9_DEVICE_CALLBACKS allocate=%u deallocate=%u lock=%u unlock=%u create_context=%u destroy_context=%u escape=%u render=%u present=%u priority=%u residency=%u core_callback_calls=0\n",
        unsigned(cb.pfnAllocateCb != nullptr), unsigned(cb.pfnDeallocateCb != nullptr),
        unsigned(cb.pfnLockCb != nullptr), unsigned(cb.pfnUnlockCb != nullptr),
        unsigned(cb.pfnCreateContextCb != nullptr), unsigned(cb.pfnDestroyContextCb != nullptr),
        unsigned(cb.pfnEscapeCb != nullptr), unsigned(cb.pfnRenderCb != nullptr),
        unsigned(cb.pfnPresentCb != nullptr), unsigned(cb.pfnSetPriorityCb != nullptr),
        unsigned(cb.pfnQueryResidencyCb != nullptr));
    }
  }
  trace("SYSTEM_D3D9_CREATE_BLOCKED adapter=%p hr=%08lx\n",
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
  trace("SYSTEM_D3D9_CLOSE hr=%08lx adapter=%p\n",
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
  trace("SYSTEM_D3D9_OPEN_BEGIN interface=%u version=%u runtime=%p caller=%ls readonly=1\n",
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
  trace("SYSTEM_D3D9_OPEN_END hr=%08lx driver_version=%u adapter=%p core=%ls\n",
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
