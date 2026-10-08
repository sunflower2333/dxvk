// SPDX-License-Identifier: MIT
// UNREGISTERED exact D10.0 or D10.1 development entry. No production admission changes.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d10umddi.h>
#include <cstring>
#include <cwchar>
#include <memory>
#include <new>
#include "umd-system-validation-entry.h"

namespace {
#if defined(VIOGPU_SYSTEM_VALIDATION_D10_1)
constexpr UINT ValidationInterface = D3D10_1_DDI_INTERFACE_VERSION;
#else
constexpr UINT ValidationInterface = D3D10_0_DDI_INTERFACE_VERSION;
#endif
#if defined(_M_ARM64) && !defined(_M_ARM64EC)
constexpr WCHAR CoreName[] = L"viogpudxvk.dll";
#elif defined(_M_X64) || defined(_M_ARM64EC)
constexpr WCHAR CoreName[] = L"viogpudxvk_x64.dll";
#elif defined(_M_IX86)
constexpr WCHAR CoreName[] = L"viogpudxvk_x86.dll";
#else
#error Supported validation views are ARM64, ARM64EC/x64 and x86
#endif
INIT_ONCE coreOnce = INIT_ONCE_STATIC_INIT;
HMODULE coreModule;
PFND3D10DDI_OPENADAPTER coreEntry;
SRWLOCK infoLock = SRWLOCK_INIT;
VioGpuSystemValidationEntryInfo info{sizeof(info), ValidationInterface, 0, 0, E_FAIL, 0, 0, {}};
struct Paths { WCHAR expected[32768]; WCHAR actual[32768]; };

HRESULT win32Result() {
  const DWORD error = GetLastError();
  return HRESULT_FROM_WIN32(error ? error : ERROR_MOD_NOT_FOUND);
}
BOOL CALLBACK loadCore(PINIT_ONCE, PVOID, PVOID*) {
  std::unique_ptr<Paths> paths(new (std::nothrow) Paths{});
  if (!paths) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
  HMODULE self = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&coreOnce), &self)) return FALSE;
  DWORD length = GetModuleFileNameW(self, paths->expected, 32768);
  if (!length || length >= 32768) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
  WCHAR* slash = std::wcsrchr(paths->expected, L'\\');
  if (!slash) { SetLastError(ERROR_BAD_PATHNAME); return FALSE; }
  slash[1] = 0;
  if (wcscat_s(paths->expected, CoreName)) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
  HMODULE existing = GetModuleHandleW(CoreName);
  if (existing) {
    length = GetModuleFileNameW(existing, paths->actual, 32768);
    if (!length || length >= 32768 || _wcsicmp(paths->actual, paths->expected)) {
      SetLastError(ERROR_INVALID_DLL); return FALSE;
    }
  }
  HMODULE loaded = LoadLibraryExW(paths->expected, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!loaded) return FALSE;
  length = GetModuleFileNameW(loaded, paths->actual, 32768);
  if (!length || length >= 32768 || _wcsicmp(paths->actual, paths->expected)) {
    FreeLibrary(loaded); SetLastError(ERROR_INVALID_DLL); return FALSE;
  }
  FARPROC symbol = GetProcAddress(loaded, "VioGpuDxvkOpenAdapterForTest");
  if (!symbol) { FreeLibrary(loaded); SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
  static_assert(sizeof(coreEntry) == sizeof(symbol));
  std::memcpy(&coreEntry, &symbol, sizeof(coreEntry));
  // Retain the resolved module for the process lifetime: tables/children keep
  // calling the core after OpenAdapter returns. No loader work in DllMain.
  coreModule = loaded;
  AcquireSRWLockExclusive(&infoLock);
  std::memcpy(info.corePath, paths->actual, sizeof(info.corePath));
  ReleaseSRWLockExclusive(&infoLock);
  return TRUE;
}
HRESULT record(UINT interfaceVersion, UINT version, HRESULT result) {
  AcquireSRWLockExclusive(&infoLock);
  ++info.calls; if (result == S_OK) ++info.successfulCalls;
  info.lastInterface = interfaceVersion; info.lastVersion = version; info.lastResult = result;
  ReleaseSRWLockExclusive(&infoLock);
  return result;
}
}

extern "C" HRESULT APIENTRY OpenAdapter10(D3D10DDIARG_OPENADAPTER* args) {
  if (!args) return E_INVALIDARG;
  const UINT interfaceVersion = args->Interface, version = args->Version;
  if (interfaceVersion != ValidationInterface)
    return record(interfaceVersion, version, DXGI_ERROR_UNSUPPORTED);
  if (!InitOnceExecuteOnce(&coreOnce, loadCore, nullptr, nullptr))
    return record(interfaceVersion, version, win32Result());
  // Forward the original pointer, callback tables, handles and exact HRESULT.
  // The private core entry still validates runtime version and real identity.
  return record(interfaceVersion, version, coreEntry(args));
}

extern "C" HRESULT APIENTRY VioGpuDxvkValidationEntryInfo(VioGpuSystemValidationEntryInfo* output) {
  if (!output || output->size != sizeof(*output)) return E_INVALIDARG;
  AcquireSRWLockShared(&infoLock);
  *output = info;
  ReleaseSRWLockShared(&infoLock);
  return S_OK;
}
