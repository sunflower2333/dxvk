// Read-only diagnostic frontend for Microsoft's D3D9 runtime. This DLL is
// separate from the production UMD. Its original mode blocks CreateDevice;
// a separately permitted lifecycle mode forwards to one exact CI candidate.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <intrin.h>
#include <climits>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <unordered_map>
#include "umd-d3d9-runtime-callbacks.h"

namespace {
constexpr WCHAR permission[] = L"read-only-legacy-fog-478eca2";
constexpr WCHAR candidate[] =
  L"C:\\Users\\Public\\DxvkD3D9CapsCandidate-478eca2\\viogpudxvk.dll";
constexpr WCHAR lifecyclePermission[] = L"device-lifecycle-affe7d4";
constexpr WCHAR lifecycleCandidate[] =
  L"C:\\Users\\Public\\DxvkD3D9DiscardCandidate-affe7d4\\viogpudxvk.dll";
constexpr size_t lifecycleFunctionBytes =
  offsetof(D3DDDI_DEVICEFUNCS, pfnRename) + sizeof(PFND3DDDI_RENAME);
static_assert(lifecycleFunctionBytes == 99 * sizeof(void*));
struct Adapter { D3DDDI_ADAPTERFUNCS functions; bool lifecycle; HANDLE runtime; };
struct Device {
  D3DDDI_DEVICEFUNCS functions;
  dxvk::test::RuntimeCallbacks9::Pin callbacks;
};
std::mutex adaptersMutex;
std::unordered_map<HANDLE, Adapter> adapters;
std::unordered_map<HANDLE, Device> devices;

void trace(const char* format, ...) {
  // This DLL and the probe each link a static CRT. Its buffered stdout can
  // otherwise split a callback record around the probe's API-result line.
  // Emit the complete record in one synchronous write before returning.
  char line[2048];
  const DWORD lastError = GetLastError();
  va_list arguments;
  va_start(arguments, format);
  const int length = std::vsnprintf(line, sizeof(line), format, arguments);
  va_end(arguments);
  if (length <= 0 || size_t(length) >= sizeof(line)) { SetLastError(lastError); return; }
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, DWORD(length), &written, nullptr);
  SetLastError(lastError);
}

bool permitted(bool& lifecycle) {
  WCHAR value[64] = {};
  const DWORD size = GetEnvironmentVariableW(
    L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC", value, DWORD(_countof(value)));
  lifecycle = size && size < _countof(value) && !std::wcscmp(value, lifecyclePermission);
  return lifecycle || (size && size < _countof(value) && !std::wcscmp(value, permission));
}

bool retain(HANDLE handle, D3DDDI_ADAPTERFUNCS& functions, bool* lifecycle = nullptr,
            HANDLE* runtime = nullptr) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle);
  if (entry == adapters.end()) return false;
  functions = entry->second.functions;
  if (lifecycle) *lifecycle = entry->second.lifecycle;
  if (runtime) *runtime = entry->second.runtime;
  return true;
}

bool retainDevice(HANDLE handle, D3DDDI_DEVICEFUNCS& functions) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = devices.find(handle);
  if (entry == devices.end()) return false;
  functions = entry->second.functions;
  return true;
}

HRESULT APIENTRY lifecycleCreateResource(HANDLE handle, D3DDDIARG_CREATERESOURCE* args) {
  D3DDDI_DEVICEFUNCS original = {};
  if (!args || !retainDevice(handle, original)) return E_INVALIDARG;
  const auto input = *args;
  trace("SYSTEM_D3D9_RESOURCE_BEGIN device=%p runtime=%p flags=%08x format=%u pool=%u surfaces=%u mips=%u multisample=%u quality=%u fvf=%08x\n",
    handle, input.hResource, input.Flags.Value, unsigned(input.Format), unsigned(input.Pool),
    input.SurfCount, input.MipLevels, unsigned(input.MultisampleType), input.MultisampleQuality, input.Fvf);
  if (input.pSurfList && input.SurfCount <= 32) {
    for (UINT i = 0; i < input.SurfCount; ++i) {
      const auto& surface = input.pSurfList[i];
      trace("SYSTEM_D3D9_RESOURCE_SURFACE index=%u width=%u height=%u depth=%u system_memory=%u pitch=%u slice_pitch=%u\n",
        i, surface.Width, surface.Height, surface.Depth, unsigned(surface.pSysMem != nullptr),
        surface.SysMemPitch, surface.SysMemSlicePitch);
    }
  }
  const HRESULT hr = original.pfnCreateResource(handle, args);
  trace("SYSTEM_D3D9_RESOURCE_END device=%p runtime=%p driver=%p hr=%08lx\n",
    handle, input.hResource, args->hResource, static_cast<unsigned long>(hr));
  return hr;
}

HRESULT APIENTRY lifecycleDestroyResource(HANDLE handle, HANDLE resource) {
  D3DDDI_DEVICEFUNCS original = {};
  if (!retainDevice(handle, original)) return E_INVALIDARG;
  const HRESULT hr = original.pfnDestroyResource(handle, resource);
  trace("SYSTEM_D3D9_RESOURCE_DESTROY device=%p driver=%p hr=%08lx\n",
    handle, resource, static_cast<unsigned long>(hr));
  return hr;
}

HRESULT APIENTRY lifecycleRenderState(HANDLE handle, const D3DDDIARG_RENDERSTATE* args) {
  D3DDDI_DEVICEFUNCS original = {};
  if (!args || !retainDevice(handle, original)) return E_INVALIDARG;
  const auto input = *args;
  const HRESULT hr = original.pfnSetRenderState(handle, args);
  trace("SYSTEM_D3D9_STATE device=%p state=%u value=%08x hr=%08lx\n",
    handle, unsigned(input.State), input.Value, static_cast<unsigned long>(hr));
  return hr;
}

HRESULT APIENTRY lifecycleDestroyDevice(HANDLE handle) {
  D3DDDI_DEVICEFUNCS original = {};
  if (!retainDevice(handle, original)) return E_INVALIDARG;
  const HRESULT hr = original.pfnDestroyDevice(handle);
  size_t remaining;
  dxvk::test::RuntimeCallbacks9::Pin callbacks;
  {
    std::lock_guard<std::mutex> lock(adaptersMutex);
    const auto entry = devices.find(handle);
    if (entry != devices.end()) callbacks = entry->second.callbacks;
    devices.erase(handle);
    remaining = devices.size();
  }
  // Cleanup callbacks have completed. In-flight callback wrappers hold their
  // own owner pins and never hold the registry mutex while calling runtime.
  dxvk::test::RuntimeCallbacks9::remove(callbacks);
  trace("SYSTEM_D3D9_DEVICE_DESTROY driver=%p hr=%08lx remaining=%zu\n",
    handle, static_cast<unsigned long>(hr), remaining);
  return hr;
}

HRESULT APIENTRY getCaps(HANDLE handle, const D3DDDIARG_GETCAPS* args) {
  D3DDDI_ADAPTERFUNCS original = {};
  bool lifecycle = false;
  if (!args || !retain(handle, original, &lifecycle)) return E_INVALIDARG;
  const auto input = *args;
  trace("SYSTEM_D3D9_CAPS_BEGIN type=%u bytes=%u info=%u adapter=%p\n",
    unsigned(input.Type), input.DataSize, unsigned(input.pInfo != nullptr), handle);
  const HRESULT hr = original.pfnGetCaps(handle, args);
  // Preserve the prior dynamic-texture diagnostic and isolate the next
  // validator requirement: the legacy DDI fog/specular-alpha bit (0x2000).
  // The actual runtime maps it to public FOGANDSPECULARALPHA (0x10000),
  // retains the legacy bit for HAL validation, then removes it from public
  // GetDeviceCaps. Do not add an undocumented bit to production caps here.
  // This is not a production capability. The original diagnostic blocks
  // creation; the exact separately permitted lifecycle candidate can forward.
  // Restrict it to the exact old profile so an unrelated candidate cannot
  // silently gain this declaration. This caps callback creates no GPU device.
  if (hr == S_OK && input.Type == D3DDDICAPS_GETD3D9CAPS) {
    if (!input.pData || input.DataSize != sizeof(D3DCAPS9)) return E_FAIL;
    D3DCAPS9 caps = {};
    std::memcpy(&caps, input.pData, sizeof(caps));
    if (caps.Caps2 != (lifecycle ? D3DCAPS2_DYNAMICTEXTURES : 0u)
        || caps.PrimitiveMiscCaps != 0x00028ef0u
        || caps.DevCaps2 != D3DDEVCAPS2_STREAMOFFSET
        || caps.VertexShaderVersion != D3DVS_VERSION(2, 0)
        || caps.PixelShaderVersion != D3DPS_VERSION(2, 0)) return E_FAIL;
    const UINT oldCaps2 = caps.Caps2;
    caps.Caps2 = D3DCAPS2_DYNAMICTEXTURES;
    caps.PrimitiveMiscCaps |= 0x00002000u;
    std::memcpy(input.pData, &caps, sizeof(caps));
    trace("SYSTEM_D3D9_CAPS_DIAGNOSTIC field=Caps2 before=%08x after=%08x devcaps2=%08x create_device_blocked=%u production_caps_changed=0\n",
      oldCaps2, caps.Caps2, caps.DevCaps2, unsigned(!lifecycle));
    trace("SYSTEM_D3D9_CAPS_DIAGNOSTIC field=PrimitiveMiscCaps_DDI before=00028ef0 after=%08x legacy_fog_specular_alpha=00002000 public_fog_specular_alpha=00010000 create_device_blocked=%u production_caps_changed=0\n",
      caps.PrimitiveMiscCaps, unsigned(!lifecycle));
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
  D3DDDI_ADAPTERFUNCS original = {};
  bool lifecycle = false;
  HANDLE runtimeAdapter = nullptr;
  if (!retain(handle, original, &lifecycle, &runtimeAdapter)) return E_INVALIDARG;
  // Record the runtime's inputs before either blocking or forwarding creation.
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
  if (lifecycle) {
    if (!args || !args->pDeviceFuncs || !args->pCallbacks) return E_INVALIDARG;
    const HANDLE runtime = args->hDevice;
    dxvk::test::RuntimeCallbacks9::Pin callbacks;
    try {
      callbacks = dxvk::test::RuntimeCallbacks9::install(runtime, runtimeAdapter, args->pCallbacks, trace);
    } catch (...) { return E_OUTOFMEMORY; }
    if (!callbacks) return E_INVALIDARG;
    struct CallbackGuard {
      dxvk::test::RuntimeCallbacks9::Pin owner;
      bool published = false;
      ~CallbackGuard() { if (!published) dxvk::test::RuntimeCallbacks9::remove(owner); }
    } callbackGuard{callbacks};
    const auto originalCallbacks = args->pCallbacks;
    trace("SYSTEM_D3D9_CREATE_FORWARD adapter=%p runtime=%p interface=%u version=%u flags=%08x\n",
      handle, runtime, args->Interface, args->Version, args->Flags.Value);
    trace("SYSTEM_D3D9_CALLBACK_TABLE runtime=%p adapter_runtime=%p original=%p wrapped=%p bytes=%zu owned_snapshot=1 borrowed_table_reread=0\n",
      runtime, runtimeAdapter, originalCallbacks, &callbacks->wrapped, dxvk::test::RuntimeCallbacks9::callbackBytes);
    // Keep all original CreateDevice arguments/output addresses; substitute
    // only the callback table for this call and restore its input identity.
    const HRESULT hr = [&] {
      struct Restore {
        D3DDDIARG_CREATEDEVICE* args;
        const D3DDDI_DEVICECALLBACKS* callbacks;
        ~Restore() { args->pCallbacks = callbacks; }
      } restore{args, originalCallbacks};
      args->pCallbacks = &callbacks->wrapped;
      return original.pfnCreateDevice(handle, args);
    }();
    trace("SYSTEM_D3D9_CREATE_RETURN runtime=%p driver=%p hr=%08lx\n",
      runtime, args->hDevice, static_cast<unsigned long>(hr));
    if (hr != S_OK) return hr;
    D3DDDI_DEVICEFUNCS functions = {};
    std::memcpy(&functions, args->pDeviceFuncs, lifecycleFunctionBytes);
    trace("SYSTEM_D3D9_DEVICE_FUNCTIONS interface=%u bytes=%u resource=%u state=%u validate=%u update_w=%u stream_frequency=%u palette=%u color_fill=%u display_mode=%u\n",
      D3D_UMD_INTERFACE_VERSION_VISTA, unsigned(lifecycleFunctionBytes),
      unsigned(functions.pfnCreateResource != nullptr), unsigned(functions.pfnSetRenderState != nullptr),
      unsigned(functions.pfnValidateDevice != nullptr), unsigned(functions.pfnUpdateWInfo != nullptr),
      unsigned(functions.pfnSetStreamSourceFreq != nullptr), unsigned(functions.pfnSetPalette != nullptr),
      unsigned(functions.pfnColorFill != nullptr), unsigned(functions.pfnSetDisplayMode != nullptr));
    if (!functions.pfnCreateResource || !functions.pfnDestroyResource
        || !functions.pfnSetRenderState || !functions.pfnDestroyDevice) {
      if (functions.pfnDestroyDevice) functions.pfnDestroyDevice(args->hDevice);
      return E_FAIL;
    }
    bool inserted = false;
    try {
      std::lock_guard<std::mutex> lock(adaptersMutex);
      inserted = devices.emplace(args->hDevice, Device{functions, callbacks}).second;
    } catch (...) {
      functions.pfnDestroyDevice(args->hDevice);
      return E_OUTOFMEMORY;
    }
    if (!inserted) {
      functions.pfnDestroyDevice(args->hDevice);
      return E_FAIL;
    }
    args->pDeviceFuncs->pfnCreateResource = lifecycleCreateResource;
    args->pDeviceFuncs->pfnDestroyResource = lifecycleDestroyResource;
    args->pDeviceFuncs->pfnSetRenderState = lifecycleRenderState;
    args->pDeviceFuncs->pfnDestroyDevice = lifecycleDestroyDevice;
    callbackGuard.published = true;
    return S_OK;
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
    original = entry->second.functions;
    adapters.erase(entry);
  }
  const HRESULT hr = original.pfnCloseAdapter(handle);
  trace("SYSTEM_D3D9_CLOSE hr=%08lx adapter=%p\n",
    static_cast<unsigned long>(hr), handle);
  return hr;
}
}

extern "C" HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* args) {
  bool lifecycle = false;
  if (!permitted(lifecycle)) return D3DERR_NOTAVAILABLE;
  if (!args || !args->pAdapterFuncs) return E_INVALIDARG;
  HMODULE caller = nullptr;
  WCHAR callerPath[MAX_PATH] = {};
  const void* address = _ReturnAddress();
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(address), &caller))
    GetModuleFileNameW(caller, callerPath, DWORD(_countof(callerPath)));
  trace("SYSTEM_D3D9_OPEN_BEGIN interface=%u version=%u runtime=%p caller=%ls readonly=%u\n",
    args->Interface, args->Version, args->hAdapter, callerPath, unsigned(!lifecycle));
  // Keep this module loaded for the diagnostic process lifetime, including
  // any adapter callbacks after Windows unloads another frontend instance.
  HMODULE core;
  if (lifecycle) {
    static HMODULE module = LoadLibraryExW(lifecycleCandidate, nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    core = module;
  } else {
    static HMODULE module = LoadLibraryExW(candidate, nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    core = module;
  }
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
    static_cast<unsigned long>(hr), local.DriverVersion, local.hAdapter,
    lifecycle ? lifecycleCandidate : candidate);
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!original.pfnGetCaps || !original.pfnCreateDevice || !original.pfnCloseAdapter)
    return E_FAIL;
  if (lifecycle && local.DriverVersion != D3D_UMD_INTERFACE_VERSION_VISTA) {
    original.pfnCloseAdapter(local.hAdapter);
    trace("SYSTEM_D3D9_VERSION_REJECT observed=%u expected=%u\n",
      local.DriverVersion, D3D_UMD_INTERFACE_VERSION_VISTA);
    return D3DERR_NOTAVAILABLE;
  }
  try {
    bool inserted;
    {
      std::lock_guard<std::mutex> lock(adaptersMutex);
      inserted = adapters.emplace(local.hAdapter, Adapter{original, lifecycle, args->hAdapter}).second;
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
