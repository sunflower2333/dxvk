#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include "umd-d3d8-runtime-policy.h"
#include "umd-d3d8-runtime-guard.h"

namespace {
namespace policy = dxvk::test::runtime8;
struct Guard {
  HMODULE module = nullptr;
  bool permissionSet = false;
  ~Guard() {
    if (permissionSet) SetEnvironmentVariableW(policy::permissionName, nullptr);
    if (module) FreeLibrary(module);
  }
};
bool absent(const wchar_t* name = policy::permissionName) {
  SetLastError(ERROR_SUCCESS);
  return !GetEnvironmentVariableW(name, nullptr, 0)
    && GetLastError() == ERROR_ENVVAR_NOT_FOUND;
}
}

int d3d8RuntimeFrontGuard(const wchar_t* frontend) noexcept {
  try {
    if constexpr (sizeof(void*) != 4) return 1;
    if (!frontend || !absent() || GetModuleHandleW(L"viogpudxvk.dll")) return 1;
    for (const auto* name : policy::diagnosticNames) if (!absent(name)) return 1;
    if (!policy::ownedFrontPath(std::wstring_view(frontend))) return 1;
    Guard guard;
    guard.module = LoadLibraryExW(frontend, nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!guard.module) return 1;
    WCHAR actual[MAX_PATH];
    const DWORD pathBytes = GetModuleFileNameW(guard.module, actual, DWORD(std::size(actual)));
    if (!pathBytes || pathBytes >= std::size(actual) || _wcsicmp(actual, frontend)) return 1;
    const FARPROC symbol = GetProcAddress(guard.module, "OpenAdapter");
    using Open = HRESULT (APIENTRY*)(D3DDDIARG_OPENADAPTER*);
    Open open = nullptr; static_assert(sizeof(open) == sizeof(symbol));
    std::memcpy(&open, &symbol, sizeof(open));
    if (!open || open(nullptr) != D3DERR_NOTAVAILABLE) return 1;
    if (!SetEnvironmentVariableW(policy::permissionName, L"read-only-wrong-d3d8-core")) return 1;
    guard.permissionSet = true;
    if (open(nullptr) != D3DERR_NOTAVAILABLE) return 1;
    if (!SetEnvironmentVariableW(policy::permissionName, policy::permissionValue)) return 1;
    if (open(nullptr) != E_INVALIDARG) return 1;
    unsigned rejected = 0;
    for (UINT version : {0u, 7u, 9u, 10u, 11u, 0xffffffffu}) {
      D3DDDIARG_OPENADAPTER args{};
      args.Interface = version; args.Version = 0xcafef00d;
      args.hAdapter = reinterpret_cast<HANDLE>(uintptr_t(0x12345678));
      args.pAdapterCallbacks = reinterpret_cast<const D3DDDI_ADAPTERCALLBACKS*>(uintptr_t(1));
      args.pAdapterFuncs = reinterpret_cast<D3DDDI_ADAPTERFUNCS*>(uintptr_t(1));
      args.DriverVersion = 0x87654321;
      const auto original = args;
      if (open(&args) != D3DERR_NOTAVAILABLE || std::memcmp(&args, &original, sizeof(args))) return 1;
      ++rejected;
    }
    D3DDDIARG_OPENADAPTER malformed{}; malformed.Interface = 8;
    const auto original = malformed;
    if (open(&malformed) != E_INVALIDARG || std::memcmp(&malformed, &original, sizeof(malformed))
        || GetModuleHandleW(L"viogpudxvk.dll")) return 1;
    malformed.pAdapterCallbacks = reinterpret_cast<const D3DDDI_ADAPTERCALLBACKS*>(uintptr_t(1));
    malformed.pAdapterFuncs = reinterpret_cast<D3DDDI_ADAPTERFUNCS*>(uintptr_t(1));
    const auto nonSystem = malformed;
    if (open(&malformed) != D3DERR_NOTAVAILABLE || std::memcmp(&malformed, &nonSystem, sizeof(malformed))
        || GetModuleHandleW(L"viogpudxvk.dll")) return 1;
    if (!SetEnvironmentVariableW(policy::permissionName, nullptr) || !absent()) return 1;
    guard.permissionSet = false;
    char line[256];
    const int bytes = std::snprintf(line, sizeof(line),
      "D3D8_FRONT_GUARD PASS denied=8876086a wrong_permission=8876086a null=80070057 invalid_interfaces=%u non_system_caller=1 no_core_open=1 system_runtime_calls=0\n", rejected);
    if (bytes <= 0 || size_t(bytes) >= sizeof(line)) return 1;
    DWORD written = 0;
    return !WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, DWORD(bytes), &written, nullptr)
      || written != DWORD(bytes) ? 1 : 0;
  } catch (...) { return 1; }
}
