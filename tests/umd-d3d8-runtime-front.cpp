// Process-local enumeration/device diagnostic frontend for Microsoft's genuine I386 D3D8 runtime.
// This owned harness is separate from production UMD registration/admission.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <intrin.h>
#include <psapi.h>
#include <bcrypt.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "umd-d3d8-runtime-policy.h"
#include "umd-d3d8-runtime-callbacks.h"
#include "../src/umd/umd_runtime_imports.h"

static_assert(sizeof(void*) == 4, "Microsoft D3D8 on the target requires an I386 frontend");
static_assert(D3DDDICAPS_GETD3D8CAPS == 12);
namespace {
namespace policy = dxvk::test::runtime8;
struct Adapter {
  D3DDDI_ADAPTERFUNCS original; HANDLE runtime; UINT version;
  policy::Mode mode;
  std::wstring corePath, coreSha256, coreCommit;
};
using Callbacks = dxvk::test::RuntimeCallbacks8;
constexpr size_t functionBytes = offsetof(D3DDDI_DEVICEFUNCS, pfnRename) + sizeof(PFND3DDDI_RENAME);
static_assert(functionBytes == 99 * sizeof(void*));
struct Device {
  const D3DDDI_DEVICEFUNCS original;
  const std::shared_ptr<const Adapter> adapter;
  const Callbacks::Pin callbacks;
  std::atomic<bool> destroying{false};
  Device(const D3DDDI_DEVICEFUNCS& functions, std::shared_ptr<const Adapter> inputAdapter, Callbacks::Pin inputCallbacks)
    : original(functions), adapter(std::move(inputAdapter)), callbacks(std::move(inputCallbacks)) { }
};
std::mutex registryMutex;
std::unordered_map<HANDLE, std::shared_ptr<const Adapter>> adapters;
std::unordered_map<HANDLE, std::shared_ptr<Device>> devices;
std::atomic<unsigned> traceSequence{0};

void trace(const char* format, ...) {
  const DWORD lastError = GetLastError();
  char line[2048];
  va_list args; va_start(args, format);
  const int length = std::vsnprintf(line, sizeof(line) - 2, format, args); va_end(args);
  if (length >= 0 && size_t(length) < sizeof(line) - 2) {
    const int bytes = length && line[length - 1] == '\n' ? length : length + 1;
    if (bytes != length) line[length] = '\n';
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, DWORD(bytes), &written, nullptr);
  }
  SetLastError(lastError);
}
policy::Mode permission() {
  WCHAR value[96]{};
  const DWORD count = GetEnvironmentVariableW(policy::permissionName, value, DWORD(std::size(value)));
  return count && count < std::size(value)
    ? policy::permissionMode(std::wstring_view(value, count)) : policy::Mode::Denied;
}
std::wstring modulePath(HMODULE module) {
  WCHAR path[32768];
  const DWORD count = GetModuleFileNameW(module, path, DWORD(std::size(path)));
  return count && count < std::size(path) ? std::wstring(path, count) : std::wstring();
}
uint16_t moduleMachine(HMODULE module) {
  MODULEINFO info{};
  if (!K32GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) return 0;
  const dxvk::umd::diagnostic::RuntimeImage image{
    static_cast<const uint8_t*>(info.lpBaseOfDll), info.SizeOfImage};
  uint32_t signature = 0, offset = 0; uint16_t machine = 0;
  const bool valid = dxvk::umd::diagnostic::runtimeImageRead(image, 0x3c, offset)
    && dxvk::umd::diagnostic::runtimeImageRead(image, offset, signature) && signature == 0x4550
    && dxvk::umd::diagnostic::runtimeImageRead(image, size_t(offset) + 4, machine);
  return valid ? machine : 0;
}
bool i386(HMODULE module) { return moduleMachine(module) == IMAGE_FILE_MACHINE_I386; }
bool system8Caller(const void* returnAddress, std::wstring& actual) {
  HMODULE caller = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(returnAddress), &caller)) return false;
  actual = modulePath(caller);
  // IsWow64Process alone does not identify I386 emulation on ARM64.
  using Machines = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);
  using Directory = UINT (WINAPI*)(LPWSTR, UINT, WORD);
  const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  if (!kernel) return false;
  const FARPROC machineAddress = GetProcAddress(kernel, "IsWow64Process2");
  const FARPROC directoryAddress = GetProcAddress(kernel, "GetSystemWow64Directory2W");
  Machines machines = nullptr; Directory wowDirectory = nullptr;
  static_assert(sizeof(machines) == sizeof(machineAddress) && sizeof(wowDirectory) == sizeof(directoryAddress));
  std::memcpy(&machines, &machineAddress, sizeof(machines));
  std::memcpy(&wowDirectory, &directoryAddress, sizeof(wowDirectory));
  if (!machines || !wowDirectory) return false;
  USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN, nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
  if (!machines(GetCurrentProcess(), &processMachine, &nativeMachine)) return false;
  const USHORT effectiveMachine = processMachine == IMAGE_FILE_MACHINE_UNKNOWN ? nativeMachine : processMachine;
  BOOL legacyWow = FALSE;
  const BOOL legacyStatus = IsWow64Process(GetCurrentProcess(), &legacyWow);
  trace("SYSTEM_D3D8_CALLER_MACHINE process=%04x native=%04x effective=%04x pointer_bytes=%zu legacy_status=%u legacy_wow=%u",
    unsigned(processMachine), unsigned(nativeMachine), unsigned(effectiveMachine), sizeof(void*),
    unsigned(legacyStatus != FALSE), unsigned(legacyWow != FALSE));
  if (effectiveMachine != IMAGE_FILE_MACHINE_I386) return false;
  WCHAR directory[MAX_PATH]{};
  const UINT count = processMachine == IMAGE_FILE_MACHINE_UNKNOWN
    ? GetSystemDirectoryW(directory, MAX_PATH)
    : wowDirectory(directory, MAX_PATH, IMAGE_FILE_MACHINE_I386);
  if (!count || count >= MAX_PATH) return false;
  const auto expected = std::wstring(directory, count) + L"\\d3d8.dll";
  const uint16_t callerMachine = moduleMachine(caller);
  trace("SYSTEM_D3D8_CALLER_PATH actual=%ls expected=%ls machine=%04x pointer_bytes=%zu directory_api=%s",
    actual.c_str(), expected.c_str(), unsigned(callerMachine), sizeof(void*),
    processMachine == IMAGE_FILE_MACHINE_UNKNOWN ? "GetSystemDirectoryW" : "GetSystemWow64Directory2W");
  return !actual.empty() && !_wcsicmp(actual.c_str(), expected.c_str()) && callerMachine == IMAGE_FILE_MACHINE_I386;
}

std::wstring environment(const wchar_t* name, size_t limit) {
  std::vector<wchar_t> value(limit + 1);
  const DWORD count = GetEnvironmentVariableW(name, value.data(), DWORD(value.size()));
  return count && count < value.size() ? std::wstring(value.data(), count) : std::wstring();
}
struct CoreIdentity {
  const std::wstring path, sha256, commit;
  bool valid() const {
    return policy::ownedCorePath(std::wstring_view(path), std::wstring_view(commit))
      && policy::hexIdentity(std::wstring_view(sha256), 64);
  }
  bool operator==(const CoreIdentity& other) const {
    return path == other.path && sha256 == other.sha256 && commit == other.commit;
  }
};
struct File {
  HANDLE value = INVALID_HANDLE_VALUE;
  ~File() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Algorithm {
  BCRYPT_ALG_HANDLE value = nullptr;
  ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
};
struct Hash {
  BCRYPT_HASH_HANDLE value = nullptr;
  ~Hash() { if (value) BCryptDestroyHash(value); }
};
HRESULT cryptoResult(NTSTATUS status) {
  return status >= 0 ? S_OK : HRESULT(uint32_t(status) | 0x10000000u);
}
HRESULT fileHash(HANDLE file, std::wstring& text) {
  Algorithm algorithm; DWORD bytes = 0, returned = 0;
  NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (status < 0) return cryptoResult(status);
  status = BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH,
    reinterpret_cast<PUCHAR>(&bytes), sizeof(bytes), &returned, 0);
  if (status < 0) return cryptoResult(status);
  if (returned != sizeof(bytes) || !bytes) return E_FAIL;
  std::vector<UCHAR> object(bytes);
  Hash hash;
  status = BCryptCreateHash(algorithm.value, &hash.value, object.data(), bytes, nullptr, 0, 0);
  if (status < 0) return cryptoResult(status);
  LARGE_INTEGER begin{};
  if (!SetFilePointerEx(file, begin, nullptr, FILE_BEGIN)) return HRESULT_FROM_WIN32(GetLastError());
  std::array<UCHAR, 65536> buffer{};
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) return HRESULT_FROM_WIN32(GetLastError());
    if (!count) break;
    status = BCryptHashData(hash.value, buffer.data(), count, 0);
    if (status < 0) return cryptoResult(status);
  }
  std::array<UCHAR, 32> digest{};
  status = BCryptFinishHash(hash.value, digest.data(), ULONG(digest.size()), 0);
  if (status < 0) return cryptoResult(status);
  constexpr wchar_t digits[] = L"0123456789abcdef";
  text.resize(64);
  for (size_t i = 0; i < digest.size(); ++i) {
    text[2 * i] = digits[digest[i] >> 4];
    text[2 * i + 1] = digits[digest[i] & 15];
  }
  return S_OK;
}
bool fileI386(HANDLE file) {
  std::array<uint8_t, 64> dos{}; DWORD count = 0;
  if (!ReadFile(file, dos.data(), DWORD(dos.size()), &count, nullptr) || count != dos.size()
      || dos[0] != 'M' || dos[1] != 'Z') return false;
  uint32_t offset = 0; std::memcpy(&offset, dos.data() + 0x3c, sizeof(offset));
  LARGE_INTEGER size{}, position{}; position.QuadPart = offset;
  if (offset < dos.size() || !GetFileSizeEx(file, &size) || size.QuadPart < 6
      || LONGLONG(offset) > size.QuadPart - 6
      || !SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) return false;
  std::array<uint8_t, 6> pe{};
  if (!ReadFile(file, pe.data(), DWORD(pe.size()), &count, nullptr) || count != pe.size()) return false;
  uint32_t signature = 0; uint16_t machine = 0;
  std::memcpy(&signature, pe.data(), sizeof(signature));
  std::memcpy(&machine, pe.data() + 4, sizeof(machine));
  return signature == 0x4550 && machine == IMAGE_FILE_MACHINE_I386;
}
struct Core {
  const CoreIdentity identity;
  File file;
  HMODULE module = nullptr;
  HRESULT result = E_FAIL;
  explicit Core(const CoreIdentity& input) : identity(input) {
    if (!identity.valid()) { result = E_INVALIDARG; return; }
    // Hold a read-only file handle without write/delete sharing through load
    // and the remaining process lifetime. Do not adopt a preloaded core.
    if (GetModuleHandleW(L"viogpudxvk.dll")) { result = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS); return; }
    file.value = CreateFileW(identity.path.c_str(), GENERIC_READ, FILE_SHARE_READ,
      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.value == INVALID_HANDLE_VALUE) { result = HRESULT_FROM_WIN32(GetLastError()); return; }
    WCHAR resolved[32768];
    const DWORD count = GetFinalPathNameByHandleW(file.value, resolved, DWORD(std::size(resolved)), FILE_NAME_NORMALIZED);
    if (!count || count >= std::size(resolved) || count < 4 || std::wcsncmp(resolved, L"\\\\?\\", 4)
        || _wcsicmp(resolved + 4, identity.path.c_str()) || !fileI386(file.value)) {
      result = E_NOINTERFACE; return;
    }
    std::wstring actualHash;
    result = fileHash(file.value, actualHash);
    if (FAILED(result)) return;
    if (!policy::equalName(std::wstring_view(actualHash), std::wstring_view(identity.sha256))) {
      result = HRESULT_FROM_WIN32(ERROR_INVALID_DATA); return;
    }
    module = LoadLibraryExW(identity.path.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) { result = HRESULT_FROM_WIN32(GetLastError()); return; }
    const auto loaded = modulePath(module);
    if (loaded.empty() || _wcsicmp(loaded.c_str(), identity.path.c_str()) || !i386(module)) {
      result = E_NOINTERFACE; return;
    }
    trace("SYSTEM_D3D8_CORE_PIN path=%ls sha256=%ls expected_ci_source_commit=%ls machine=014c file_locked=1 core_unchanged=1",
      loaded.c_str(), actualHash.c_str(), identity.commit.c_str());
    result = S_OK;
  }
};
std::shared_ptr<const Adapter> retain(HANDLE handle) {
  std::lock_guard<std::mutex> lock(registryMutex);
  const auto entry = adapters.find(handle);
  return entry == adapters.end() ? nullptr : entry->second;
}

HRESULT APIENTRY getCaps(HANDLE handle, const D3DDDIARG_GETCAPS* args) {
  const auto owner = retain(handle);
  if (!owner || !args) return E_INVALIDARG;
  const auto input = *args;
  const unsigned id = ++traceSequence;
  trace("SYSTEM_D3D8_CAPS_BEGIN id=%u interface=8 type=%u bytes=%u info=%u adapter=%p",
        id, unsigned(input.Type), input.DataSize, unsigned(input.pInfo != nullptr), handle);
  // The production typed owner already validates size, identity and epoch.
  // Forward it unchanged: no diagnostic capability bits or API9 projection.
  const HRESULT hr = owner->original.pfnGetCaps(handle, args);
  trace("SYSTEM_D3D8_CAPS_END id=%u type=%u bytes=%u hr=%08lx caps_modified=0",
        id, unsigned(input.Type), input.DataSize, static_cast<unsigned long>(hr));
  if (hr == S_OK && input.pData) {
    if (input.Type == D3DDDICAPS_GETD3D8CAPS && input.DataSize == policy::capsBytes) {
      std::array<uint32_t, 53> caps{};
      std::memcpy(caps.data(), input.pData, sizeof(caps));
      trace("SYSTEM_D3D8_CAPS12 id=%u bytes=%zu device_type=%u devcaps=%08x caps2=%08x primitive=%08x vs=%08x constants=%u ps=%08x",
        id, sizeof(caps), caps[0], caps[7], caps[3], caps[8], caps[49], caps[50], caps[51]);
      for (unsigned i = 0; i < caps.size(); ++i)
        trace("SYSTEM_D3D8_CAPS12_WORD id=%u index=%u value=%08x", id, i, caps[i]);
    } else if ((input.Type == D3DDDICAPS_GETFORMATCOUNT || input.Type == D3DDDICAPS_GETD3DQUERYCOUNT)
               && input.DataSize == sizeof(UINT)) {
      UINT count = 0; std::memcpy(&count, input.pData, sizeof(count));
      trace("SYSTEM_D3D8_CAPS_COUNT id=%u type=%u count=%u", id, unsigned(input.Type), count);
    }
  }
  return hr;
}
std::shared_ptr<Device> retainDevice(HANDLE handle) {
  std::lock_guard<std::mutex> lock(registryMutex);
  const auto entry = devices.find(handle);
  return entry == devices.end() ? nullptr : entry->second;
}
HRESULT APIENTRY deviceCreateResource(HANDLE handle, D3DDDIARG_CREATERESOURCE* args) {
  const auto owner = retainDevice(handle);
  if (!owner || !args || owner->destroying.load()) return E_INVALIDARG;
  const auto input = *args;
  trace("SYSTEM_D3D8_RESOURCE_BEGIN device=%p runtime=%p flags=%08x format=%u pool=%u surfaces=%u mips=%u fvf=%08x",
    handle, input.hResource, input.Flags.Value, unsigned(input.Format), unsigned(input.Pool),
    input.SurfCount, input.MipLevels, input.Fvf);
  const HRESULT hr = owner->original.pfnCreateResource(handle, args);
  trace("SYSTEM_D3D8_RESOURCE_END device=%p runtime=%p driver=%p hr=%08lx",
    handle, input.hResource, args->hResource, static_cast<unsigned long>(hr));
  return hr;
}
HRESULT APIENTRY deviceDestroyResource(HANDLE handle, HANDLE resource) {
  const auto owner = retainDevice(handle);
  if (!owner) return E_INVALIDARG;
  const HRESULT hr = owner->original.pfnDestroyResource(handle, resource);
  trace("SYSTEM_D3D8_RESOURCE_DESTROY device=%p resource=%p hr=%08lx", handle, resource, static_cast<unsigned long>(hr));
  return hr;
}
HRESULT APIENTRY devicePresent(HANDLE handle, const D3DDDIARG_PRESENT* args) {
  const auto owner = retainDevice(handle);
  if (!owner || !args || owner->destroying.load()) return E_INVALIDARG;
  trace("SYSTEM_D3D8_PRESENT_BEGIN device=%p", handle);
  const HRESULT hr = owner->original.pfnPresent(handle, args);
  trace("SYSTEM_D3D8_PRESENT_END device=%p hr=%08lx", handle, static_cast<unsigned long>(hr));
  return hr;
}
HRESULT APIENTRY deviceDestroy(HANDLE handle) {
  const auto owner = retainDevice(handle);
  if (!owner || owner->destroying.exchange(true)) return E_INVALIDARG;
  // Original callbacks stay registered and pinned until actual core teardown
  // returns. In-flight callback wrappers retain their own shared owners.
  trace("SYSTEM_D3D8_DEVICE_DESTROY_BEGIN device=%p callback_owner_live=1", handle);
  const HRESULT hr = owner->original.pfnDestroyDevice(handle);
  if (hr != S_OK) {
    owner->destroying.store(false); Callbacks::summary(owner->callbacks, "destroy-failed");
    trace("SYSTEM_D3D8_DEVICE_DESTROY_FAILED device=%p hr=%08lx callback_owner_retained=1", handle, static_cast<unsigned long>(hr));
    return hr;
  }
  size_t remaining = 0;
  {
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto entry = devices.find(handle);
    if (entry != devices.end() && entry->second == owner) devices.erase(entry);
    remaining = devices.size();
  }
  Callbacks::summary(owner->callbacks, "destroyed");
  Callbacks::remove(owner->callbacks);
  trace("SYSTEM_D3D8_DEVICE_DESTROY device=%p hr=%08lx remaining=%zu callback_owner_released=1",
    handle, static_cast<unsigned long>(hr), remaining);
  return hr;
}
HRESULT APIENTRY createDevice(HANDLE handle, D3DDDIARG_CREATEDEVICE* args) {
  const auto adapter = retain(handle);
  if (!adapter || !args) return E_INVALIDARG;
  const auto input = *args;
  trace("SYSTEM_D3D8_CREATE_CONTRACT adapter=%p runtime=%p interface=%u version=%u flags=%08x callbacks=%u functions=%u command=%u allocation_list=%u patch_list=%u captured_mode=%u",
    handle, input.hDevice, input.Interface, input.Version, input.Flags.Value,
    unsigned(input.pCallbacks != nullptr), unsigned(input.pDeviceFuncs != nullptr),
    input.CommandBufferSize, input.AllocationListSize, input.PatchLocationListSize, unsigned(adapter->mode));
  if (!policy::mayCreateDevice(adapter->mode, permission(), input.Interface)) {
    trace("SYSTEM_D3D8_CREATE_BLOCKED adapter=%p core_create_calls=0 hr=%08lx",
      handle, static_cast<unsigned long>(D3DERR_NOTAVAILABLE));
    return D3DERR_NOTAVAILABLE;
  }
  // Keep the exact core pins immutable across OpenAdapter and device creation.
  if (environment(policy::corePathName, 259) != adapter->corePath
      || environment(policy::coreSha256Name, 64) != adapter->coreSha256
      || environment(policy::coreCommitName, 40) != adapter->coreCommit) return E_INVALIDARG;
  if (!input.hDevice || !input.pDeviceFuncs || !input.pCallbacks) return E_INVALIDARG;
  Callbacks::Pin callbacks;
  try { callbacks = Callbacks::install(input.hDevice, adapter->runtime, input.pCallbacks, trace); }
  catch (...) { return E_OUTOFMEMORY; }
  if (!callbacks) return E_INVALIDARG;
  struct Guard {
    D3DDDIARG_CREATEDEVICE* args;
    const D3DDDI_DEVICECALLBACKS* original;
    Callbacks::Pin callbacks;
    bool published = false;
    ~Guard() { args->pCallbacks = original; if (!published) Callbacks::remove(callbacks); }
  } guard{args, input.pCallbacks, callbacks};
  trace("SYSTEM_D3D8_CALLBACK_TABLE runtime=%p adapter_runtime=%p original=%p wrapped=%p bytes=%zu owned_snapshot=1 borrowed_table_reread=0",
    input.hDevice, adapter->runtime, input.pCallbacks, &callbacks->wrapped, Callbacks::callbackBytes);
  args->pCallbacks = &callbacks->wrapped;
  const HRESULT hr = adapter->original.pfnCreateDevice(handle, args);
  trace("SYSTEM_D3D8_CREATE_RETURN runtime=%p driver=%p hr=%08lx interface=8 core_create_calls=1",
    input.hDevice, args->hDevice, static_cast<unsigned long>(hr));
  if (hr != S_OK) { Callbacks::summary(callbacks, "create-failed"); return FAILED(hr) ? hr : E_FAIL; }
  D3DDDI_DEVICEFUNCS original{};
  std::memcpy(&original, input.pDeviceFuncs, functionBytes);
  if (!args->hDevice || !original.pfnDestroyDevice || !original.pfnCreateResource
      || !original.pfnDestroyResource || !original.pfnPresent) {
    if (original.pfnDestroyDevice) original.pfnDestroyDevice(args->hDevice);
    return E_NOINTERFACE;
  }
  bool inserted = false;
  try {
    auto owner = std::make_shared<Device>(original, adapter, callbacks);
    std::lock_guard<std::mutex> lock(registryMutex);
    inserted = devices.emplace(args->hDevice, std::move(owner)).second;
  } catch (...) { original.pfnDestroyDevice(args->hDevice); return E_OUTOFMEMORY; }
  if (!inserted) { original.pfnDestroyDevice(args->hDevice); return E_FAIL; }
  input.pDeviceFuncs->pfnCreateResource = deviceCreateResource;
  input.pDeviceFuncs->pfnDestroyResource = deviceDestroyResource;
  input.pDeviceFuncs->pfnPresent = devicePresent;
  input.pDeviceFuncs->pfnDestroyDevice = deviceDestroy;
  guard.published = true;
  trace("SYSTEM_D3D8_DEVICE_FUNCTIONS bytes=%zu interface=%u published=1", functionBytes, unsigned(D3D_UMD_INTERFACE_VERSION_VISTA));
  return S_OK;
}
HRESULT APIENTRY closeAdapter(HANDLE handle) {
  const auto owner = retain(handle); if (!owner) return E_INVALIDARG;
  const HRESULT hr = owner->original.pfnCloseAdapter(handle);
  size_t remaining = 0, liveDevices = 0;
  {
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto entry = adapters.find(handle);
    if (hr == S_OK && entry != adapters.end() && entry->second == owner) adapters.erase(entry);
    remaining = adapters.size();
    for (const auto& device : devices) liveDevices += unsigned(device.second->adapter == owner);
  }
  trace("SYSTEM_D3D8_CLOSE adapter=%p runtime=%p hr=%08lx remaining=%zu live_devices=%zu",
    handle, owner->runtime, static_cast<unsigned long>(hr), remaining, liveDevices);
  return hr;
}
}

extern "C" HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* args) {
  try {
  const auto mode = permission();
  if (mode == policy::Mode::Denied) return D3DERR_NOTAVAILABLE;
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Interface != 8) return D3DERR_NOTAVAILABLE;
  if (!input.pAdapterFuncs || !input.pAdapterCallbacks) return E_INVALIDARG;
  std::wstring caller;
  if (!system8Caller(_ReturnAddress(), caller)) return D3DERR_NOTAVAILABLE;
  if (!input.pAdapterCallbacks->pfnQueryAdapterInfoCb) return E_INVALIDARG;
  trace("SYSTEM_D3D8_OPEN_BEGIN interface=%u version=%u runtime=%p caller=%ls pointer_bytes=%zu readonly=%u",
        input.Interface, input.Version, input.hAdapter, caller.c_str(), sizeof(void*), unsigned(mode == policy::Mode::ReadOnly));
  // All three pins come from an actual successful consolidated I386
  // artifact receipt. No core is built, patched or relabeled by this harness.
  const CoreIdentity identity{environment(policy::corePathName, 259),
    environment(policy::coreSha256Name, 64), environment(policy::coreCommitName, 40)};
  if (!identity.valid()) return E_INVALIDARG;
  static const Core pinned(identity);
  if (!(identity == pinned.identity)) return E_INVALIDARG;
  if (FAILED(pinned.result)) return pinned.result;
  const HMODULE core = pinned.module;
  const auto loaded = modulePath(core);
  const FARPROC symbol = GetProcAddress(core, "VioGpuDxvkOpenAdapter9ForTest");
  using Open = HRESULT (APIENTRY*)(D3DDDIARG_OPENADAPTER*);
  Open open = nullptr; static_assert(sizeof(open) == sizeof(symbol));
  std::memcpy(&open, &symbol, sizeof(open));
  if (!open) return E_NOINTERFACE;
  auto local = input; D3DDDI_ADAPTERFUNCS original{};
  local.pAdapterFuncs = &original;
  const HRESULT hr = open(&local);
  trace("SYSTEM_D3D8_OPEN_END hr=%08lx interface=%u driver_version=%u adapter=%p core=%ls expected_ci_source_commit=%ls machine=014c core_create_calls=0",
        static_cast<unsigned long>(hr), input.Interface, local.DriverVersion, local.hAdapter,
        loaded.c_str(), pinned.identity.commit.c_str());
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!original.pfnGetCaps || !original.pfnCreateDevice || !original.pfnCloseAdapter
      || local.DriverVersion != D3D_UMD_INTERFACE_VERSION_VISTA) {
    if (original.pfnCloseAdapter) original.pfnCloseAdapter(local.hAdapter);
    return E_NOINTERFACE;
  }
  try {
    auto owner = std::make_shared<Adapter>(Adapter{original, input.hAdapter, input.Version, mode, identity.path, identity.sha256, identity.commit});
    bool inserted = false;
    {
      std::lock_guard<std::mutex> lock(registryMutex);
      inserted = adapters.emplace(local.hAdapter, std::move(owner)).second;
    }
    if (!inserted) { original.pfnCloseAdapter(local.hAdapter); return E_FAIL; }
  } catch (...) { original.pfnCloseAdapter(local.hAdapter); return E_OUTOFMEMORY; }
  const D3DDDI_ADAPTERFUNCS publication{getCaps, createDevice, closeAdapter};
  *input.pAdapterFuncs = publication;
  args->hAdapter = local.hAdapter;
  args->DriverVersion = local.DriverVersion;
  return S_OK;
  } catch (...) { return E_OUTOFMEMORY; }
}
