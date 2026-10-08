// SPDX-License-Identifier: MIT
// The legacy runtime has one native UMD-name slot for ARM64 and emulated x64.
// Each actual ARM64X view forwards the typed SDK call to its matching original
// core. This entry does not create runtime callbacks, alter caps or load D3D APIs.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <cstring>
#include <cwchar>

#if defined(_M_ARM64EC)
constexpr WCHAR relativeCore[] = L"x64\\viogpudxvk.dll";
#elif defined(_M_ARM64)
constexpr WCHAR relativeCore[] = L"arm64\\viogpudxvk.dll";
#else
#error Build the legacy frontend as actual ARM64 and ARM64EC views, then merge ARM64X
#endif

namespace {
INIT_ONCE once = INIT_ONCE_STATIC_INIT;
HMODULE core = nullptr;
PFND3DDDI_OPENADAPTER entry = nullptr;

HRESULT errorResult() {
  const DWORD error = GetLastError();
  return HRESULT_FROM_WIN32(error ? error : ERROR_MOD_NOT_FOUND);
}

struct Paths {
  WCHAR expected[32768];
  WCHAR actual[32768];
};
struct Storage {
  Paths* paths = static_cast<Paths*>(HeapAlloc(GetProcessHeap(), 0, sizeof(Paths)));
  ~Storage() {
    const DWORD error = GetLastError();
    if (paths) HeapFree(GetProcessHeap(), 0, paths);
    SetLastError(error);
  }
};

BOOL CALLBACK initialize(PINIT_ONCE, PVOID, PVOID*) {
  Storage storage;
  if (!storage.paths) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
  HMODULE self = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&once), &self)) return FALSE;
  auto& expected = storage.paths->expected;
  auto& actual = storage.paths->actual;
  DWORD length = GetModuleFileNameW(self, expected, DWORD(_countof(expected)));
  if (!length || length >= _countof(expected)) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
  WCHAR* slash = std::wcsrchr(expected, L'\\');
  if (!slash) { SetLastError(ERROR_BAD_PATHNAME); return FALSE; }
  slash[1] = L'\0';
  if (wcscat_s(expected, relativeCore)) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }

  // A cached core from a different package must not silently replace this one.
  const HMODULE cached = GetModuleHandleW(L"viogpudxvk.dll");
  if (cached) {
    length = GetModuleFileNameW(cached, actual, DWORD(_countof(actual)));
    if (!length || length >= _countof(actual) || _wcsicmp(expected, actual)) {
      SetLastError(ERROR_INVALID_DLL); return FALSE;
    }
  }
  HMODULE loaded = LoadLibraryExW(expected, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!loaded) return FALSE;
  length = GetModuleFileNameW(loaded, actual, DWORD(_countof(actual)));
  if (!length || length >= _countof(actual) || _wcsicmp(expected, actual)) {
    FreeLibrary(loaded); SetLastError(ERROR_INVALID_DLL); return FALSE;
  }
  FARPROC symbol = GetProcAddress(loaded, "OpenAdapter");
  if (!symbol) {
    FreeLibrary(loaded); SetLastError(ERROR_PROC_NOT_FOUND); return FALSE;
  }
  PFND3DDDI_OPENADAPTER typed = nullptr;
  static_assert(sizeof(typed) == sizeof(symbol));
  std::memcpy(&typed, &symbol, sizeof(typed));
  // Keep the core loaded for the process lifetime: its adapter/device function
  // tables remain callable after this entry returns. No DllMain loader-lock work.
  core = loaded;
  entry = typed;
  return TRUE;
}
}

extern "C" HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* args) {
  if (!args) return E_INVALIDARG;
  if (!InitOnceExecuteOnce(&once, initialize, nullptr, nullptr)) return errorResult();
  if (!core || !entry) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  return entry(args);
}
