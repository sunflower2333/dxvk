// Genuine Windows D3D8 runtime probe. No DXVK d3d8.dll or D3D9 delegation.
#include "umd-d3d8-api.h"
#include "umd-d3d8-runtime-policy.h"
#include "umd-d3d8-runtime-guard.h"
#include "umd-d3d8-runtime-hardware.h"
#include "../src/umd/umd_runtime_imports.h"
#include <d3dkmthk.h>
#include <psapi.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
namespace policy = dxvk::test::runtime8;
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
uint16_t moduleMachine(HMODULE module);
struct ProcessApi {
  FARPROC address = nullptr;
  DWORD error = ERROR_SUCCESS;
  HMODULE owner = nullptr;
  DWORD ownerError = ERROR_SUCCESS;
};
ProcessApi lookupProcessApi(HMODULE provider, const char* symbol) {
  SetLastError(ERROR_SUCCESS);
  const FARPROC address = provider ? GetProcAddress(provider, symbol) : nullptr;
  const DWORD error = provider ? GetLastError() : ERROR_MOD_NOT_FOUND;
  return {address, error};
}
void traceProcessApi(ProcessApi& api, const wchar_t* name, const char* symbol) {
  if (api.address) {
    SetLastError(ERROR_SUCCESS);
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(api.address), &api.owner)) api.ownerError = GetLastError();
  }
  const auto ownerPath = api.owner ? path(api.owner) : std::wstring();
  trace("D3D8_PROCESS_API provider=%ls symbol=%s present=%u error=%lu address=%p owner_path=%ls owner_machine=%04x owner_error=%lu",
    name, symbol, unsigned(api.address != nullptr), api.error, reinterpret_cast<void*>(api.address),
    ownerPath.c_str(), unsigned(api.owner ? moduleMachine(api.owner) : 0), api.ownerError);
  SetLastError(api.error);
}
std::wstring systemDirectory() {
  // The legacy boolean is FALSE for an I386 process emulated on ARM64.
  // Resolve the documented signatures without raising the fixture's Vista
  // header target; these diagnostics run on the existing Windows 11 guest.
  using Machines = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);
  using Directory = UINT (WINAPI*)(LPWSTR, UINT, WORD);
  const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  require(kernel != nullptr, "system-kernel32");
  auto machineLookup = lookupProcessApi(kernel, "IsWow64Process2");
  auto directoryLookup = lookupProcessApi(kernel, "GetSystemWow64Directory2W");
  // Capture both original lookups/errors before tracing or owner queries.
  traceProcessApi(machineLookup, L"kernel32.dll", "IsWow64Process2");
  traceProcessApi(directoryLookup, L"kernel32.dll", "GetSystemWow64Directory2W");
  HMODULE directoryProvider = kernel;
  const bool alternate = !directoryLookup.address && directoryLookup.error == ERROR_PROC_NOT_FOUND;
  if (alternate) {
    // The retained target I386 Kernel32 omits this named export; its API-set
    // host, already loaded KernelBase, exports the exact documented API.
    directoryProvider = GetModuleHandleW(L"kernelbase.dll");
    directoryLookup = lookupProcessApi(directoryProvider, "GetSystemWow64Directory2W");
    traceProcessApi(directoryLookup, L"kernelbase.dll", "GetSystemWow64Directory2W");
  }
  const FARPROC machineAddress = machineLookup.address, directoryAddress = directoryLookup.address;
  Machines machines = nullptr; Directory wowDirectory = nullptr;
  static_assert(sizeof(machines) == sizeof(machineAddress) && sizeof(wowDirectory) == sizeof(directoryAddress));
  std::memcpy(&machines, &machineAddress, sizeof(machines));
  std::memcpy(&wowDirectory, &directoryAddress, sizeof(wowDirectory));
  SetLastError(machineLookup.error);
  require(machines != nullptr, "IsWow64Process2-export");
  SetLastError(directoryLookup.error);
  require(wowDirectory != nullptr, "GetSystemWow64Directory2W-export");
  USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN, nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
  require(machines(GetCurrentProcess(), &processMachine, &nativeMachine) != FALSE, "IsWow64Process2");
  const USHORT effectiveMachine = processMachine == IMAGE_FILE_MACHINE_UNKNOWN ? nativeMachine : processMachine;
  BOOL legacyWow = FALSE;
  const BOOL legacyStatus = IsWow64Process(GetCurrentProcess(), &legacyWow);
  trace("D3D8_PROCESS_MACHINE process=%04x native=%04x effective=%04x pointer_bytes=%zu legacy_status=%u legacy_wow=%u",
    unsigned(processMachine), unsigned(nativeMachine), unsigned(effectiveMachine), sizeof(void*),
    unsigned(legacyStatus != FALSE), unsigned(legacyWow != FALSE));
  require(sizeof(void*) == 4 && effectiveMachine == IMAGE_FILE_MACHINE_I386, "actual-I386-process-machine");
  wchar_t directory[MAX_PATH]{};
  const UINT length = processMachine == IMAGE_FILE_MACHINE_UNKNOWN
    ? GetSystemDirectoryW(directory, MAX_PATH)
    : wowDirectory(directory, MAX_PATH, IMAGE_FILE_MACHINE_I386);
  require(length && length < MAX_PATH, "system-directory");
  const std::wstring canonical(directory, length);
  const auto actualKernel = path(kernel);
  require(!_wcsicmp(actualKernel.c_str(), (canonical + L"\\kernel32.dll").c_str())
    && moduleMachine(kernel) == IMAGE_FILE_MACHINE_I386, "canonical-I386-Kernel32-provider");
  const auto validOwner = [&canonical](const ProcessApi& api) {
    if (!api.owner || api.ownerError || moduleMachine(api.owner) != IMAGE_FILE_MACHINE_I386) return false;
    const auto actual = path(api.owner);
    return !_wcsicmp(actual.c_str(), (canonical + L"\\kernelbase.dll").c_str())
      || !_wcsicmp(actual.c_str(), (canonical + L"\\kernel32.dll").c_str());
  };
  require(validOwner(machineLookup) && validOwner(directoryLookup), "canonical-I386-process-API-owners");
  if (alternate) {
    require(directoryProvider && directoryLookup.owner == directoryProvider
      && !_wcsicmp(path(directoryProvider).c_str(), (canonical + L"\\kernelbase.dll").c_str()),
      "canonical-I386-KernelBase-Directory2W-provider");
  }
  trace("D3D8_SYSTEM_DIRECTORY machine=%04x api=%s path=%ls", unsigned(effectiveMachine),
    processMachine == IMAGE_FILE_MACHINE_UNKNOWN ? "GetSystemDirectoryW" : "GetSystemWow64Directory2W", directory);
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
void processApiDiagnostics() {
  // Observe already loaded providers only. A missing API is diagnostic data;
  // this mode never chooses a fallback or admits a runtime/KMT invocation.
  const auto executable = GetModuleHandleW(nullptr);
  trace("D3D8_PROCESS_IMAGE path=%ls machine=%04x pointer_bytes=%zu",
    path(executable).c_str(), unsigned(moduleMachine(executable)), sizeof(void*));
  ProcessApi machineLookup, directoryLookup;
  const wchar_t* providers[] = {L"kernel32.dll", L"kernelbase.dll",
    L"api-ms-win-core-wow64-l1-1-0.dll", L"api-ms-win-core-wow64-l1-1-1.dll",
    L"api-ms-win-core-wow64-l1-1-3.dll"};
  for (size_t i = 0; i < std::size(providers); ++i) {
    SetLastError(ERROR_SUCCESS);
    const HMODULE provider = GetModuleHandleW(providers[i]);
    const DWORD error = GetLastError();
    const auto providerPath = provider ? path(provider) : std::wstring();
    trace("D3D8_PROCESS_PROVIDER name=%ls present=%u error=%lu path=%ls machine=%04x pointer_bytes=%zu",
      providers[i], unsigned(provider != nullptr), error, providerPath.c_str(),
      unsigned(provider ? moduleMachine(provider) : 0), sizeof(void*));
    auto process = lookupProcessApi(provider, "IsWow64Process2");
    auto directory = lookupProcessApi(provider, "GetSystemWow64Directory2W");
    traceProcessApi(process, providers[i], "IsWow64Process2");
    traceProcessApi(directory, providers[i], "GetSystemWow64Directory2W");
    if (!i) { machineLookup = process; directoryLookup = directory; }
  }
  using Machines = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);
  using Directory = UINT (WINAPI*)(LPWSTR, UINT, WORD);
  Machines machines = nullptr; Directory wowDirectory = nullptr;
  static_assert(sizeof(machines) == sizeof(machineLookup.address)
    && sizeof(wowDirectory) == sizeof(directoryLookup.address));
  std::memcpy(&machines, &machineLookup.address, sizeof(machines));
  std::memcpy(&wowDirectory, &directoryLookup.address, sizeof(wowDirectory));
  if (machines) {
    USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN, nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    SetLastError(ERROR_SUCCESS);
    const BOOL status = machines(GetCurrentProcess(), &processMachine, &nativeMachine);
    const DWORD error = GetLastError();
    trace("D3D8_PROCESS_API_RESULT symbol=IsWow64Process2 status=%u error=%lu process=%04x native=%04x",
      unsigned(status != FALSE), error, unsigned(processMachine), unsigned(nativeMachine));
  }
  if (wowDirectory) {
    wchar_t directory[MAX_PATH]{};
    SetLastError(ERROR_SUCCESS);
    const UINT count = wowDirectory(directory, MAX_PATH, IMAGE_FILE_MACHINE_I386);
    const DWORD error = GetLastError();
    trace("D3D8_PROCESS_API_RESULT symbol=GetSystemWow64Directory2W count=%u error=%lu requested_machine=014c path=%ls",
      count, error, count && count < MAX_PATH ? directory : L"");
  }
  wchar_t directory[MAX_PATH]{};
  SetLastError(ERROR_SUCCESS);
  const UINT count = GetSystemDirectoryW(directory, MAX_PATH);
  const DWORD error = GetLastError();
  trace("D3D8_PROCESS_API_RESULT symbol=GetSystemDirectoryW count=%u error=%lu path=%ls",
    count, error, count && count < MAX_PATH ? directory : L"");
  const auto canonical = systemDirectory();
  trace("D3D8_PROCESS_API_CANONICAL_DIRECTORY path=%ls admission=0", canonical.c_str());
  trace("D3D8_PROCESS_API_DIAGNOSTICS_COMPLETE observed_providers=5 observed_lookup_rows=10 runtime_calls=0 KMT_calls=0 core_loads=0 admission=0");
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
  ~Permission() { restore(); }
  size_t active = 0;
  std::array<std::wstring, 4> values;
  static bool absent() {
    for (const auto* name : policy::diagnosticNames) {
      SetLastError(ERROR_SUCCESS);
      if (GetEnvironmentVariableW(name, nullptr, 0) || GetLastError() != ERROR_ENVVAR_NOT_FOUND) return false;
    }
    return true;
  }
  void enable(const wchar_t* corePath, const wchar_t* coreSha256, const wchar_t* coreCommit, bool device = false) {
    require(absent(), "diagnostic-pins-originally-absent");
    values = {device ? policy::devicePermissionValue : policy::permissionValue, corePath, coreSha256, coreCommit};
    for (size_t i = 0; i < policy::diagnosticNames.size(); ++i) {
      require(SetEnvironmentVariableW(policy::diagnosticNames[i], values[i].c_str()) != FALSE,
              "set-process-local-permission-pin");
      ++active;
    }
  }
  bool restore() noexcept {
    bool restored = true;
    while (active) {
      --active;
      restored = SetEnvironmentVariableW(policy::diagnosticNames[active], nullptr) != FALSE && restored;
    }
    return restored && absent();
  }
};

// The original SysWOW64 D3D8 runtime imports this KMT function directly from
// GDI32. Preserve every real query/status, changing only the exact DX9 filename
// supplied by the caller's independently verified installed package receipt.
class Selector {
  using Query = decltype(&D3DKMTQueryAdapterInfo);
  static_assert(uint32_t(KMTQAITYPE_UMDRIVERNAME) == policy::umdNameQuery);
  static_assert(uint32_t(KMTUMDVERSION_DX9) == policy::dx9DriverNameVersion);
  struct State {
    Query original = nullptr;
    const std::wstring expected, replacement;
    std::mutex publicationMutex;
    bool active = true;
    std::atomic<unsigned> substitutions{0}, queries{0};
    State(const wchar_t* installed, const wchar_t* frontend)
      : expected(installed), replacement(frontend) { }
  };
  static std::mutex currentMutex;
  static std::shared_ptr<State> current;
  static std::atomic<Query> originalFallback;
  std::shared_ptr<State> owner;
  void** slot = nullptr;
  DWORD originalProtection = 0;
  static NTSTATUS APIENTRY query(const D3DKMT_QUERYADAPTERINFO* input) {
    std::shared_ptr<State> self;
    { std::lock_guard<std::mutex> lock(currentMutex); self = current; }
    // A callback already dispatched before restoration can arrive afterward.
    // Preserve the real query/status even when its publication owner is gone.
    if (!self) {
      const Query original = originalFallback.load();
      return original ? original(input) : NTSTATUS(0xc0000001u);
    }
    const auto request = input ? *input : D3DKMT_QUERYADAPTERINFO{};
    const NTSTATUS status = self->original(input);
    const unsigned queryCount = ++self->queries;
    std::lock_guard<std::mutex> lock(self->publicationMutex);
    bool selected = false;
    if (self->active && input && request.pPrivateDriverData
        && request.Type == KMTQAITYPE_UMDRIVERNAME
        && request.PrivateDriverDataSize == sizeof(D3DKMT_UMDFILENAMEINFO)) {
      auto* info = static_cast<D3DKMT_UMDFILENAMEINFO*>(request.pPrivateDriverData);
      if (status == 0)
        selected = policy::selectDriverName(int32_t(status), uint32_t(request.Type), request.PrivateDriverDataSize,
          sizeof(*info), uint32_t(info->Version), info->UmdFileName,
          std::wstring_view(self->expected), std::wstring_view(self->replacement));
    }
    if (selected) ++self->substitutions;
    if (queryCount <= 256)
      trace("D3D8_SELECTOR_QUERY index=%u type=%u bytes=%u kernel_adapter=%u original_status=%08lx selected=%u substitutions=%u",
        queryCount, unsigned(request.Type), request.PrivateDriverDataSize, request.hAdapter,
        static_cast<unsigned long>(status), unsigned(selected), self->substitutions.load());
    return status;
  }
  static void* address(Query function) {
    void* value = nullptr; static_assert(sizeof(value) == sizeof(function));
    std::memcpy(&value, &function, sizeof(value)); return value;
  }
public:
  ~Selector() { if (slot) restore(); }
  void install(HMODULE module, const wchar_t* front, const wchar_t* installed) {
    require(sizeof(void*) == 4 && moduleMachine(module) == IMAGE_FILE_MACHINE_I386,
            "genuine-I386-system8-selector");
    owner = std::make_shared<State>(installed, front);
    require(policy::ownedFrontPath(std::wstring_view(owner->replacement))
        && owner->expected.size() > 3 && owner->expected.size() < 260 && owner->expected[1] == L':',
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
    std::memcpy(&owner->original, &old, sizeof(owner->original));
    originalFallback.store(owner->original);
    {
      std::lock_guard<std::mutex> lock(currentMutex);
      require(!current, "single-owned-selector"); current = owner;
    }
    DWORD protection = 0, ignored = 0;
    if (!VirtualProtect(target, sizeof(void*), PAGE_READWRITE, &protection)) {
      std::lock_guard<std::mutex> lock(currentMutex);
      current.reset(); require(false, "selector-write-protection");
    }
    originalProtection = protection;
    const bool replaced = InterlockedCompareExchangePointer(target, address(&query), old) == old;
    const bool protectedAgain = VirtualProtect(target, sizeof(void*), protection, &ignored) != FALSE;
    if (replaced) slot = target;
    else { std::lock_guard<std::mutex> lock(currentMutex); current.reset(); }
    require(replaced && protectedAgain, "selector-install-and-protection");
    trace("D3D8_SELECTOR installed=1 machine=%04x pointer_bytes=%zu slot_rva=%zx registry_writes=0",
          unsigned(imports[0].machine), sizeof(void*), imports[0].offset);
  }
  bool restore() noexcept {
    try {
      if (!slot) return true;
      { std::lock_guard<std::mutex> lock(owner->publicationMutex); owner->active = false; }
      DWORD protection = 0, ignored = 0;
      if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
      void* previous = InterlockedCompareExchangePointer(slot, address(owner->original), address(&query));
      const bool restored = previous == address(&query) || previous == address(owner->original);
      const bool protectedAgain = VirtualProtect(slot, sizeof(void*), originalProtection, &ignored) != FALSE;
      if (restored && protectedAgain) slot = nullptr;
      { std::lock_guard<std::mutex> lock(currentMutex); if (current == owner) current.reset(); }
      trace("D3D8_SELECTOR restored=%u protection_restored=%u substitutions=%u queries=%u",
        unsigned(restored), unsigned(protectedAgain), owner->substitutions.load(), owner->queries.load());
      return restored && protectedAgain;
    } catch (...) { return false; }
  }
  unsigned count() const { return owner ? owner->substitutions.load() : 0; }
};
std::mutex Selector::currentMutex;
std::shared_ptr<Selector::State> Selector::current;
std::atomic<Selector::Query> Selector::originalFallback{nullptr};

bool parseLuid(const wchar_t* text, LUID& luid) {
  if (!policy::hexIdentity(std::wstring_view(text), 16)) return false;
  std::array<unsigned char, sizeof(LUID)> bytes{};
  for (size_t i = 0; i < 16; ++i) {
    const wchar_t c = policy::asciiFold(text[i]);
    const unsigned digit = c >= L'0' && c <= L'9' ? unsigned(c - L'0') : unsigned(c - L'a' + 10);
    if (i % 2) bytes[i / 2] |= static_cast<unsigned char>(digit);
    else bytes[i / 2] = static_cast<unsigned char>(digit << 4);
  }
  std::memcpy(&luid, bytes.data(), sizeof(luid)); return luid.LowPart || luid.HighPart;
}
bool sourceId(const wchar_t* text, UINT& value) {
  value = 0;
  if (!*text) return false;
  for (; *text; ++text) {
    if (*text < L'0' || *text > L'9' || value > (UINT(-1) - unsigned(*text - L'0')) / 10) return false;
    value = value * 10 + unsigned(*text - L'0');
  }
  return true;
}
void userGate() {
  DWORD session = 0;
  require(ProcessIdToSessionId(GetCurrentProcessId(), &session) && session == 1, "original-interactive-session1");
  struct Token { HANDLE handle = nullptr; ~Token() { if (handle) CloseHandle(handle); } } token;
  require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.handle) != FALSE, "OpenProcessToken");
  TOKEN_ELEVATION elevation{}; TOKEN_ELEVATION_TYPE type{}; DWORD returned = 0;
  require(GetTokenInformation(token.handle, TokenElevation, &elevation, sizeof(elevation), &returned)
      && returned == sizeof(elevation) && !elevation.TokenIsElevated, "original-non-elevated-token");
  require(GetTokenInformation(token.handle, TokenElevationType, &type, sizeof(type), &returned)
      && returned == sizeof(type) && type == TokenElevationTypeLimited, "original-limited-token");
  returned = 0; GetTokenInformation(token.handle, TokenUser, nullptr, 0, &returned);
  require(returned >= sizeof(TOKEN_USER) && returned <= 65536, "token-user-size");
  std::vector<unsigned char> user(returned);
  require(GetTokenInformation(token.handle, TokenUser, user.data(), DWORD(user.size()), &returned), "token-user");
  PSID expected = nullptr;
  require(ConvertStringSidToSidW(policy::originalUserSid, &expected) != FALSE, "expected-USER-SID");
  const bool matched = EqualSid(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, expected) != FALSE;
  LocalFree(expected); require(matched, "original-USER-SID");
  returned = 0; GetTokenInformation(token.handle, TokenIntegrityLevel, nullptr, 0, &returned);
  require(returned >= sizeof(TOKEN_MANDATORY_LABEL) && returned <= 65536, "token-integrity-size");
  std::vector<unsigned char> integrity(returned);
  require(GetTokenInformation(token.handle, TokenIntegrityLevel, integrity.data(), DWORD(integrity.size()), &returned), "token-integrity");
  const auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(integrity.data())->Label.Sid;
  require(IsValidSid(sid) && *GetSidSubAuthorityCount(sid), "integrity-SID");
  const DWORD rid = *GetSidSubAuthority(sid, DWORD(*GetSidSubAuthorityCount(sid) - 1));
  require(rid == SECURITY_MANDATORY_MEDIUM_RID, "original-medium-integrity");
  trace("D3D8_USER_GATE session=%lu elevation=0 elevation_type=%u integrity_rid=%lu sid=%ls",
    session, unsigned(type), rid, policy::originalUserSid);
}
HMONITOR matchAdapter(IDirect3D8* api, UINT index, const LUID& expected, UINT expectedSource, bool names = false) {
  const HMONITOR monitor = api ? api->GetAdapterMonitor(index)
    : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  require(monitor != nullptr, "API8-selected-monitor");
  MONITORINFOEXW info{}; info.cbSize = sizeof(info);
  require(GetMonitorInfoW(monitor, &info) != FALSE, "selected-monitor-info");
  struct Dc { HDC value = nullptr; ~Dc() { if (value) DeleteDC(value); } } dc;
  dc.value = CreateDCW(L"DISPLAY", info.szDevice, nullptr, nullptr);
  require(dc.value != nullptr, "selected-monitor-DC");
  const HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
  const auto expectedGdi = systemDirectory() + L"\\gdi32.dll";
  const auto actualGdi = gdi ? path(gdi) : std::wstring(L"<absent>");
  const uint16_t gdiMachine = gdi ? moduleMachine(gdi) : 0;
  trace("D3D8_SYSTEM_GDI32 actual=%ls expected=%ls machine=%04x pointer_bytes=%zu",
    actualGdi.c_str(), expectedGdi.c_str(), unsigned(gdiMachine), sizeof(void*));
  require(gdi && !_wcsicmp(actualGdi.c_str(), expectedGdi.c_str())
    && gdiMachine == IMAGE_FILE_MACHINE_I386, "system-GDI32");
  const auto function = [gdi](const char* name, auto& output) {
    const FARPROC pointer = GetProcAddress(gdi, name);
    static_assert(sizeof(output) == sizeof(pointer)); std::memcpy(&output, &pointer, sizeof(output));
    require(output != nullptr, name);
  };
  decltype(&D3DKMTOpenAdapterFromHdc) open = nullptr; decltype(&D3DKMTCloseAdapter) close = nullptr;
  decltype(&D3DKMTQueryAdapterInfo) query = nullptr;
  function("D3DKMTOpenAdapterFromHdc", open); function("D3DKMTCloseAdapter", close); function("D3DKMTQueryAdapterInfo", query);
  struct Adapter {
    decltype(&D3DKMTCloseAdapter) close = nullptr; D3DKMT_HANDLE handle = 0;
    ~Adapter() { if (handle) { D3DKMT_CLOSEADAPTER args{}; args.hAdapter = handle; close(&args); } }
  } adapter{close};
  D3DKMT_OPENADAPTERFROMHDC request{}; request.hDc = dc.value;
  const NTSTATUS opened = open(&request); adapter.handle = request.hAdapter;
  require(opened == 0 && adapter.handle, "KMT-open-selected-monitor");
  require(!std::memcmp(&request.AdapterLuid, &expected, sizeof(LUID)) && request.VidPnSourceId == expectedSource,
    "actual-current-LUID-source");
  D3DKMT_ADAPTERTYPE type{}; D3DKMT_QUERYADAPTERINFO properties{};
  properties.hAdapter = adapter.handle; properties.Type = KMTQAITYPE_ADAPTERTYPE;
  properties.pPrivateDriverData = &type; properties.PrivateDriverDataSize = sizeof(type);
  require(query(&properties) == 0 && !type.SoftwareDevice && type.RenderSupported, "actual-KMT-hardware-adapter");
  const auto* raw = reinterpret_cast<const unsigned char*>(&request.AdapterLuid);
  trace("D3D8_KMT_MATCH adapter=%u source=%u luid=%02x%02x%02x%02x%02x%02x%02x%02x software=0 render=1 no_device=1",
    adapter.handle, request.VidPnSourceId, unsigned(raw[0]), unsigned(raw[1]), unsigned(raw[2]), unsigned(raw[3]),
    unsigned(raw[4]), unsigned(raw[5]), unsigned(raw[6]), unsigned(raw[7]));
  if (names) {
    D3DKMT_UMDFILENAMEINFO name{}; name.Version = KMTUMDVERSION_DX9;
    properties.Type = KMTQAITYPE_UMDRIVERNAME;
    properties.pPrivateDriverData = &name; properties.PrivateDriverDataSize = sizeof(name);
    const NTSTATUS status = query(&properties);
    const bool terminated = std::find(std::begin(name.UmdFileName), std::end(name.UmdFileName), wchar_t(0)) != std::end(name.UmdFileName);
    trace("D3D8_KMT_NAME version=%u status=%08lx terminated=%u name=%ls pointer_bytes=%zu raw_bytes=%zu",
      unsigned(name.Version), static_cast<unsigned long>(status), unsigned(terminated),
      terminated ? name.UmdFileName : L"<unterminated>", sizeof(void*), sizeof(name));
    for (unsigned i = 0; i < std::size(name.UmdFileName); ++i)
      trace("D3D8_KMT_NAME_WORD index=%u value=%04x", i, unsigned(name.UmdFileName[i]));
    require(status == 0 && terminated, "actual-I386-legacy-KMT-name");
  }
  D3DKMT_CLOSEADAPTER release{}; release.hAdapter = adapter.handle;
  const NTSTATUS closed = close(&release); if (!closed) adapter.handle = 0;
  require(closed == 0, "KMT-close-selected-monitor");
  trace("D3D8_KMT_CLOSED status=00000000");
  require(DeleteDC(dc.value) != FALSE, "selected-monitor-DC-release"); dc.value = nullptr;
  return monitor;
}

struct Window {
  HWND value = nullptr;
  ~Window() { if (value) DestroyWindow(value); }
  void create(HMONITOR monitor = nullptr, bool visible = false) {
    int x = 0, y = 0;
    if (monitor) {
      MONITORINFO info{}; info.cbSize = sizeof(info);
      require(GetMonitorInfoW(monitor, &info) != FALSE, "owned-window-monitor");
      x = info.rcWork.left + 32; y = info.rcWork.top + 32;
    }
    value = CreateWindowExW(visible ? WS_EX_TOPMOST | WS_EX_TOOLWINDOW : 0, L"STATIC", L"VioGPU genuine DX8 offscreen", WS_POPUP | (visible ? WS_VISIBLE : 0),
                           x, y, 8, 8, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
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
void screen(IDirect3DDevice8* device, HWND window) {
  constexpr uint32_t color = 0xff193e72;
  call(device->Clear(0, nullptr, D3DCLEAR_TARGET, color, 1, 0), "Clear-before-Present");
  call(device->Present(nullptr, nullptr, window, nullptr), "Present-owned-window");
  RECT client{}; POINT origin{};
  require(GetClientRect(window, &client) && client.right == 8 && client.bottom == 8
      && ClientToScreen(window, &origin), "exact-present-client");
  struct Capture {
    HDC desktop = nullptr, memory = nullptr; HBITMAP bitmap = nullptr; HGDIOBJ old = nullptr;
    ~Capture() {
      if (memory && old) SelectObject(memory, old);
      if (bitmap) DeleteObject(bitmap);
      if (memory) DeleteDC(memory);
      if (desktop) ReleaseDC(nullptr, desktop);
    }
  } capture;
  capture.desktop = GetDC(nullptr); capture.memory = CreateCompatibleDC(capture.desktop);
  require(capture.desktop && capture.memory, "owned-screen-DC");
  BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 8; info.bmiHeader.biHeight = -8;
  info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  capture.bitmap = CreateDIBSection(capture.desktop, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
  require(capture.bitmap && pixels, "owned-screen-DIB");
  capture.old = SelectObject(capture.memory, capture.bitmap); require(capture.old && capture.old != HGDI_ERROR, "screen-select-DIB");
  std::array<uint32_t, 64> actual{}; bool exact = false; unsigned polls = 0;
  const ULONGLONG until = GetTickCount64() + 2500;
  do {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
    require(BitBlt(capture.memory, 0, 0, 8, 8, capture.desktop, origin.x, origin.y, SRCCOPY | CAPTUREBLT)
        && GdiFlush(), "actual-screen-capture");
    std::memcpy(actual.data(), pixels, sizeof(actual)); ++polls;
    exact = std::all_of(actual.begin(), actual.end(), [expectedRgb = color & 0xffffffu](uint32_t pixel) {
      return (pixel & 0xffffffu) == expectedRgb;
    });
    if (!exact) Sleep(16);
  } while (!exact && GetTickCount64() < until);
  for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x)
    trace("D3D8_SCREEN_PIXEL x=%u y=%u rgb=%06x", x, y, actual[y * 8 + x] & 0xffffff);
  require(exact, "independent-Present-screen-oracle");
  trace("D3D8_PRESENT PASS calls=1 pixels=64 rgb=193e72 polls=%u source=actual-screen", polls);
}
void offscreen(IDirect3D8* api, UINT adapter, HWND window, bool selected = false,
               bool present = false, const policy::D3d8HardwarePins* pins = nullptr) {
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
  if (selected) require(pins && pins->loaded(), "actual-matched-private-I386-payloads");
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
  if (selected) {
    const DWORD shader14[] = {D3DPS_VERSION(1,4), D3DSIO_MOV, 0x80000000u | D3DSP_WRITEMASK_ALL,
      0x80000000u | DWORD(D3DSPR_INPUT) | D3DSP_NOSWIZZLE, D3DSIO_END};
    DWORD handle14 = 0;
    call(device->CreatePixelShader(shader14, &handle14), "CreatePixelShader-1.4");
    call(device->SetPixelShader(handle14), "SetPixelShader-1.4");
    fvf(device.ptr, 0xffa362d1); capture(device.ptr, target.ptr, 7, 0xffa362d1);
    call(device->SetPixelShader(0), "UnbindPixelShader-1.4");
    call(device->DeletePixelShader(handle14), "DeletePixelShader-1.4");
  }
  call(device->SetRenderTarget(original.ptr, nullptr), "RestoreRenderTarget-final");
  target.reset();
  if (present) screen(device.ptr, window);
  if (selected) require(pins && pins->loaded(), "private-payloads-through-teardown");
  original.reset(); device.reset();
  if (selected) trace("D3D8_SELECTED_OFFSCREEN PASS stages=7 pixels=448 shader=VS1.1/PS1.1+PS1.4 dynamic_texture=1 resets=1 presents=%u", unsigned(present));
  else trace("D3D8_OFFSCREEN PASS stages=6 pixels=384 shader=VS1.1/PS1.1 dynamic_texture=1 resets=1 presents=0");
}
}

int wmain(int argc, wchar_t** argv) {
  const bool guard = argc == 3 && !std::wcscmp(argv[1], L"--front-guard");
  const bool enumerate = argc == 2 && !std::wcscmp(argv[1], L"--enumerate");
  const bool installedOffscreen = argc == 2 && !std::wcscmp(argv[1], L"--offscreen");
  const bool selectedEnumerate = argc == 7 && !std::wcscmp(argv[1], L"--front-enumerate");
  const bool selectedOffscreen = argc == 9 && !std::wcscmp(argv[1], L"--front-offscreen");
  const bool selectedPresent = argc == 9 && !std::wcscmp(argv[1], L"--front-present");
  const bool selectedHardware = selectedOffscreen || selectedPresent;
  const bool kmtNames = argc == 4 && !std::wcscmp(argv[1], L"--kmt-names");
  const bool processDiagnostics = argc == 2 && !std::wcscmp(argv[1], L"--process-api-diagnostics");
  const bool selected = selectedEnumerate || selectedHardware;
  const bool hardware = installedOffscreen || selectedHardware;
  LUID expectedLuid{}; UINT expectedSource = 0;
  if (selected && (!policy::ownedFrontPath(std::wstring_view(argv[2]))
      || !policy::ownedCorePath(std::wstring_view(argv[4]), std::wstring_view(argv[6]))
      || !policy::hexIdentity(std::wstring_view(argv[5]), 64))) return 64;
  if (selectedHardware && (!parseLuid(argv[7], expectedLuid) || !sourceId(argv[8], expectedSource))) return 64;
  if (kmtNames && (!parseLuid(argv[2], expectedLuid) || !sourceId(argv[3], expectedSource))) return 64;
  if (!enumerate && !hardware && !selected && !guard && !kmtNames && !processDiagnostics) return 64;
  if (guard) return d3d8RuntimeFrontGuard(argv[2]);
  if constexpr (sizeof(void*) != 4) {
    trace("D3D8_UNAVAILABLE required_machine=014c pointer_bytes=%zu exit=77", sizeof(void*)); return 77;
  }
  try {
    require(Permission::absent(), "diagnostic-pins-originally-absent");
    if (processDiagnostics) {
      require(!GetModuleHandleW(L"d3d8.dll") && !GetModuleHandleW(L"viogpudxvk.dll"), "process-diagnostics-no-graphics-factory");
      processApiDiagnostics();
      require(!GetModuleHandleW(L"d3d8.dll") && !GetModuleHandleW(L"viogpudxvk.dll"), "process-diagnostics-no-graphics-factory-after");
      return traceFailed ? 1 : 0;
    }
    if (kmtNames) {
      userGate();
      require(!GetModuleHandleW(L"d3d8.dll") && !GetModuleHandleW(L"viogpudxvk.dll"), "KMT-name-no-graphics-factory");
      matchAdapter(nullptr, 0, expectedLuid, expectedSource, true);
      trace("D3D8_KMT_NAMES_COMPLETE system_runtime_calls=0 create_device=0 core_loads=0 registry_writes=0");
      return traceFailed ? 1 : 0;
    }
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
    require(moduleMachine(runtime.value) == IMAGE_FILE_MACHINE_I386, "genuine-I386-system-d3d8");
    Permission permission; Selector selector; policy::D3d8HardwarePins pins(trace);
    if (selectedHardware) {
      userGate();
      require(pins.open(argv[4], argv[5], argv[6]), "exact-original-I386-hardware-inputs");
    }
    if (selected) {
      permission.enable(argv[4], argv[5], argv[6], selectedHardware);
      selector.install(runtime.value, argv[2], argv[3]);
    }
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
      HMONITOR monitor = selectedHardware ? matchAdapter(api.ptr, virtio, expectedLuid, expectedSource) : nullptr;
      Window window; window.create(monitor, selectedPresent);
      offscreen(api.ptr, virtio, window.value, selectedHardware, selectedPresent, &pins);
    }
    api.reset(); auditModules(directory);
    if (selected) {
      const unsigned substitutions = selector.count(); require(selector.restore(), "selector-restoration");
      require(substitutions > 0, "actual-system8-exact-name-selection");
      require(permission.restore(), "diagnostic-pin-restoration");
    }
    require(pins.restore(), "private-driver-environment-restoration");
    trace("D3D8_COMPLETE mode=%s adapters=%u create_device=%u presents=%u registry_writes=0",
      selectedPresent ? "front-present" : selectedOffscreen ? "front-offscreen" : installedOffscreen ? "offscreen"
        : selected ? "front-enumerate" : "enumerate", count, unsigned(hardware), unsigned(selectedPresent));
    return traceFailed ? 1 : 0;
  } catch (const std::exception& error) { trace("D3D8_FAILED reason=%s", error.what()); return 1; }
}
