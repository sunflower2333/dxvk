// SPDX-License-Identifier: MIT
// Transparent modern validation entry; the scoped core owns negotiation.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#pragma warning(push)
#pragma warning(disable: 4201)
#include <d3d10umddi.h>
#pragma warning(pop)
#include <cstring>
#include <cwchar>
#include "umd-d3d11-system-entry.h"

namespace {
#if defined(_M_ARM64) && !defined(_M_ARM64EC)
constexpr WCHAR CoreName[] = L"viogpudxvk.dll";
#elif defined(_M_X64) || defined(_M_ARM64EC)
constexpr WCHAR CoreName[] = L"viogpudxvk_x64.dll";
#elif defined(_M_IX86)
constexpr WCHAR CoreName[] = L"viogpudxvk_x86.dll";
#else
#error Unsupported validation frontend architecture
#endif
INIT_ONCE coreOnce = INIT_ONCE_STATIC_INIT;
SRWLOCK lock = SRWLOCK_INIT;
PFND3D10DDI_OPENADAPTER coreEntry = nullptr;
HMODULE retainedCore = nullptr;
HMODULE retainedFrontend = nullptr;
VioGpuD11EntryInfo info{sizeof(VioGpuD11EntryInfo), 1, 0, 0, 0, {}, {}};
struct AdapterRecord {
  bool reserved = false;
  D3D10DDI_HADAPTER handle{};
  D3D10_2DDI_ADAPTERFUNCS functions{};
};
AdapterRecord adapters[32];
UINT64 address(const void* pointer) { return UINT64(reinterpret_cast<UINT_PTR>(pointer)); }
UINT begin(VioGpuD11Event event) {
  AcquireSRWLockExclusive(&lock);
  const UINT index = info.eventCount;
  if (index < VioGpuD11EventCapacity) {
    event.sequence = index + 1; info.events[index] = event; ++info.eventCount;
  } else info.overflow = 1;
  ReleaseSRWLockExclusive(&lock);
  return index;
}
void finish(UINT index, VioGpuD11Event event) {
  AcquireSRWLockExclusive(&lock);
  if (index < VioGpuD11EventCapacity) {
    event.sequence = index + 1; event.completed = 1; info.events[index] = event;
  }
  ReleaseSRWLockExclusive(&lock);
}
bool original(D3D10DDI_HADAPTER handle, D3D10_2DDI_ADAPTERFUNCS& functions) {
  AcquireSRWLockShared(&lock);
  bool found = false;
  for (const auto& record : adapters) {
    if (record.reserved && record.handle.pDrvPrivate == handle.pDrvPrivate && handle.pDrvPrivate) {
      functions = record.functions; found = true; break;
    }
  }
  ReleaseSRWLockShared(&lock);
  return found;
}
AdapterRecord* reserve() {
  AcquireSRWLockExclusive(&lock);
  AdapterRecord* result = nullptr;
  for (auto& record : adapters) if (!record.reserved) {
    record = {}; record.reserved = true; result = &record; break;
  }
  ReleaseSRWLockExclusive(&lock);
  return result;
}
void cancel(AdapterRecord* record) {
  AcquireSRWLockExclusive(&lock); *record = {}; ReleaseSRWLockExclusive(&lock);
}
HRESULT loadResult() {
  const DWORD error = GetLastError();
  return HRESULT_FROM_WIN32(error ? error : ERROR_MOD_NOT_FOUND);
}
struct Paths { WCHAR expected[32768], actual[32768]; };
struct ScopedPaths {
  Paths* value = static_cast<Paths*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Paths)));
  ~ScopedPaths() {
    const DWORD error = GetLastError();
    if (value) HeapFree(GetProcessHeap(), 0, value);
    SetLastError(error);
  }
};
BOOL CALLBACK loadCore(PINIT_ONCE, PVOID, PVOID*) {
  ScopedPaths storage;
  Paths* paths = storage.value;
  if (!paths) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
  HMODULE self = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&coreOnce), &self)) return FALSE;
  DWORD size = GetModuleFileNameW(self, paths->expected, 32768);
  if (!size || size >= 32768) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
  WCHAR* slash = std::wcsrchr(paths->expected, L'\\');
  if (!slash) { SetLastError(ERROR_BAD_PATHNAME); return FALSE; }
  slash[1] = 0;
  if (wcscat_s(paths->expected, CoreName)) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
  const auto matches = [&](HMODULE module) {
    size = GetModuleFileNameW(module, paths->actual, 32768);
    return size && size < 32768 && !_wcsicmp(paths->expected, paths->actual);
  };
  const HMODULE existing = GetModuleHandleW(CoreName);
  if (existing && !matches(existing)) { SetLastError(ERROR_INVALID_DLL); return FALSE; }
  const HMODULE module = LoadLibraryExW(paths->expected, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!module) return FALSE;
  if (!matches(module)) { FreeLibrary(module); SetLastError(ERROR_INVALID_DLL); return FALSE; }
  const FARPROC symbol = GetProcAddress(module, "VioGpuDxvkOpenAdapter11Fl10_0ForValidation");
  if (!symbol) { FreeLibrary(module); SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
  // A failed SYSTEM CreateDevice can immediately release its adapter/DLL.
  // Keep this test-only trace readable until the bounded probe process exits.
  HMODULE heldSelf = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
      reinterpret_cast<LPCWSTR>(&coreOnce), &heldSelf)) {
    const DWORD error = GetLastError(); FreeLibrary(module); SetLastError(error); return FALSE;
  }
  static_assert(sizeof(coreEntry) == sizeof(symbol));
  std::memcpy(&coreEntry, &symbol, sizeof(coreEntry));
  retainedCore = module; // Published core device tables require process-lifetime retention.
  retainedFrontend = heldSelf;
  AcquireSRWLockExclusive(&lock);
  std::memcpy(info.corePath, paths->actual, sizeof(info.corePath));
  ReleaseSRWLockExclusive(&lock);
  return TRUE;
}

SIZE_T APIENTRY privateSize(D3D10DDI_HADAPTER handle, const D3D10DDIARG_CALCPRIVATEDEVICESIZE* args) {
  VioGpuD11Event event{}; event.call = VioGpuD11Call::PrivateSize;
  event.adapter = address(handle.pDrvPrivate); event.argument = address(args);
  if (args) { event.interfaceVersion = args->Interface; event.version = args->Version; event.flags = args->Flags; }
  const UINT index = begin(event);
  D3D10_2DDI_ADAPTERFUNCS functions{};
  const SIZE_T result = original(handle, functions) ? functions.pfnCalcPrivateDeviceSize(handle, args) : 0;
  event.returnSize = result; finish(index, event); return result;
}
HRESULT APIENTRY create(D3D10DDI_HADAPTER handle, D3D10DDIARG_CREATEDEVICE* args) {
  VioGpuD11Event event{}; event.call = VioGpuD11Call::Create;
  event.adapter = address(handle.pDrvPrivate); event.argument = address(args);
  if (args) {
    event.interfaceVersion = args->Interface; event.version = args->Version; event.flags = args->Flags;
    event.kernelCallbacks = address(args->pKTCallbacks);
    if (args->Interface == D3D11_0_DDI_INTERFACE_VERSION) event.coreCallbacks = address(args->p11UMCallbacks);
    else if (args->Interface == D3D10_0_DDI_INTERFACE_VERSION || args->Interface == D3D10_1_DDI_INTERFACE_VERSION)
      event.coreCallbacks = address(args->pUMCallbacks);
  }
  const UINT index = begin(event);
  D3D10_2DDI_ADAPTERFUNCS functions{};
  event.result = original(handle, functions) ? functions.pfnCreateDevice(handle, args) : E_INVALIDARG;
  finish(index, event); return event.result;
}
HRESULT APIENTRY close(D3D10DDI_HADAPTER handle) {
  VioGpuD11Event event{}; event.call = VioGpuD11Call::Close; event.adapter = address(handle.pDrvPrivate);
  const UINT index = begin(event);
  D3D10_2DDI_ADAPTERFUNCS functions{};
  event.result = original(handle, functions) ? functions.pfnCloseAdapter(handle) : E_INVALIDARG;
  // No lock crosses a core callback. In-flight callers have their own original
  // function snapshot and the original opaque handle; the core owns retirement.
  if (event.result == S_OK) {
    AcquireSRWLockExclusive(&lock);
    for (auto& record : adapters) if (record.reserved && record.handle.pDrvPrivate == handle.pDrvPrivate) {
      record = {}; --info.liveAdapters; break;
    }
    ReleaseSRWLockExclusive(&lock);
  }
  finish(index, event); return event.result;
}
HRESULT APIENTRY versions(D3D10DDI_HADAPTER handle, UINT32* count, UINT64* output) {
  VioGpuD11Event event{}; event.call = VioGpuD11Call::Versions;
  event.adapter = address(handle.pDrvPrivate); event.argument = address(output);
  if (count && output) event.capacity = *count;
  const UINT index = begin(event);
  D3D10_2DDI_ADAPTERFUNCS functions{};
  event.result = original(handle, functions) ? functions.pfnGetSupportedVersions(handle, count, output) : E_INVALIDARG;
  if (count) event.count = *count;
  if (event.result == S_OK && output) {
    event.capturedVersions = event.count < event.capacity ? event.count : event.capacity;
    if (event.capturedVersions > 8) event.capturedVersions = 8;
    std::memcpy(event.versions, output, event.capturedVersions * sizeof(*output));
  }
  finish(index, event); return event.result;
}
HRESULT APIENTRY caps(D3D10DDI_HADAPTER handle, const D3D10_2DDIARG_GETCAPS* args) {
  VioGpuD11Event event{}; event.call = VioGpuD11Call::Caps;
  event.adapter = address(handle.pDrvPrivate); event.argument = address(args);
  // Retain the original output address across a reentrant runtime callback;
  // telemetry never writes it or substitutes a capability response.
  const void* output = args ? args->pData : nullptr;
  if (args) { event.type = args->Type; event.dataSize = args->DataSize; }
  const UINT index = begin(event);
  D3D10_2DDI_ADAPTERFUNCS functions{};
  event.result = original(handle, functions) ? functions.pfnGetCaps(handle, args) : E_INVALIDARG;
  if (event.result == S_OK && output && event.dataSize == sizeof(UINT)) std::memcpy(&event.caps, output, sizeof(UINT));
  finish(index, event); return event.result;
}
}

extern "C" HRESULT APIENTRY OpenAdapter10_2(D3D10DDIARG_OPENADAPTER* args) {
  if (!args) return E_INVALIDARG;
  VioGpuD11Event event{}; event.call = VioGpuD11Call::Open; event.argument = address(args);
  event.interfaceVersion = args->Interface; event.version = args->Version;
  event.runtimeAdapter = address(args->hRTAdapter.handle);
  const UINT index = begin(event);
  auto* const output = args->pAdapterFuncs_2;
  // These malformed controls cannot load a core or invoke an adapter callback.
  if (!args->pAdapterFuncs_2 || !args->pAdapterCallbacks || !args->pAdapterCallbacks->pfnQueryAdapterInfoCb) {
    args->hAdapter = {}; if (args->pAdapterFuncs_2) *args->pAdapterFuncs_2 = {};
    event.result = E_INVALIDARG; finish(index, event); return event.result;
  }
  AdapterRecord* record = reserve();
  if (!record) { event.result = E_OUTOFMEMORY; finish(index, event); return event.result; }
  if (!InitOnceExecuteOnce(&coreOnce, loadCore, nullptr, nullptr)) {
    event.result = loadResult(); cancel(record); finish(index, event); return event.result;
  }
  // Initial Interface/Version are deliberately not interpreted by a modern
  // entry. Forward original input, handles, callbacks and exact core HRESULT.
  event.result = coreEntry(args); event.adapter = address(args->hAdapter.pDrvPrivate);
  if (event.result == S_OK) {
    AcquireSRWLockExclusive(&lock);
    record->handle = args->hAdapter; record->functions = *output; ++info.liveAdapters;
    ReleaseSRWLockExclusive(&lock);
    output->pfnCalcPrivateDeviceSize = privateSize;
    output->pfnCreateDevice = create;
    output->pfnCloseAdapter = close;
    output->pfnGetSupportedVersions = versions;
    output->pfnGetCaps = caps;
  } else cancel(record);
  finish(index, event); return event.result;
}
extern "C" HRESULT APIENTRY VioGpuDxvkD11ValidationInfo(VioGpuD11EntryInfo* output) {
  if (!output || output->size != sizeof(*output)) return E_INVALIDARG;
  AcquireSRWLockShared(&lock); *output = info; ReleaseSRWLockShared(&lock); return S_OK;
}
