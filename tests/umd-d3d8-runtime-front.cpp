// Process-local read-only frontend for Microsoft's genuine I386 D3D8 runtime.
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
#include "../src/umd/umd_runtime_imports.h"

static_assert(sizeof(void*) == 4, "Microsoft D3D8 on the target requires an I386 frontend");
static_assert(D3DDDICAPS_GETD3D8CAPS == 12);
namespace {
namespace policy = dxvk::test::runtime8;
struct Adapter { D3DDDI_ADAPTERFUNCS original; HANDLE runtime; UINT version; };
std::mutex registryMutex;
std::unordered_map<HANDLE, std::shared_ptr<const Adapter>> adapters;
std::atomic<unsigned> traceSequence{0};

void trace(const char* format, ...) {
  const DWORD lastError = GetLastError();
  char line[2048];
  va_list args; va_start(args, format);
  const int length = std::vsnprintf(line, sizeof(line) - 2, format, args); va_end(args);
  if (length >= 0 && size_t(length) < sizeof(line) - 2) {
    line[length] = '\n';
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, DWORD(length + 1), &written, nullptr);
  }
  SetLastError(lastError);
}
bool permitted() {
  WCHAR value[96]{};
  const DWORD count = GetEnvironmentVariableW(policy::permissionName, value, DWORD(std::size(value)));
  return count && count < std::size(value) && !std::wcscmp(value, policy::permissionValue);
}
std::wstring modulePath(HMODULE module) {
  WCHAR path[32768];
  const DWORD count = GetModuleFileNameW(module, path, DWORD(std::size(path)));
  return count && count < std::size(path) ? std::wstring(path, count) : std::wstring();
}
bool i386(HMODULE module) {
  MODULEINFO info{};
  if (!K32GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) return false;
  const dxvk::umd::diagnostic::RuntimeImage image{
    static_cast<const uint8_t*>(info.lpBaseOfDll), info.SizeOfImage};
  uint32_t signature = 0, offset = 0; uint16_t machine = 0;
  return dxvk::umd::diagnostic::runtimeImageRead(image, 0x3c, offset)
    && dxvk::umd::diagnostic::runtimeImageRead(image, offset, signature) && signature == 0x4550
    && dxvk::umd::diagnostic::runtimeImageRead(image, size_t(offset) + 4, machine)
    && machine == IMAGE_FILE_MACHINE_I386;
}
bool system8Caller(const void* returnAddress, std::wstring& actual) {
  HMODULE caller = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(returnAddress), &caller)) return false;
  actual = modulePath(caller);
  WCHAR directory[MAX_PATH]; BOOL wow = FALSE;
  if (!IsWow64Process(GetCurrentProcess(), &wow)) return false;
  const UINT count = wow ? GetSystemWow64DirectoryW(directory, MAX_PATH)
                         : GetSystemDirectoryW(directory, MAX_PATH);
  if (!count || count >= MAX_PATH) return false;
  const auto expected = std::wstring(directory, count) + L"\\d3d8.dll";
  return !actual.empty() && !_wcsicmp(actual.c_str(), expected.c_str()) && i386(caller);
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
HRESULT APIENTRY createDevice(HANDLE handle, D3DDDIARG_CREATEDEVICE* args) {
  const auto owner = retain(handle);
  if (!owner || !args) return E_INVALIDARG;
  const auto input = *args;
  trace("SYSTEM_D3D8_CREATE_BLOCKED adapter=%p runtime=%p interface=%u version=%u flags=%08x callbacks=%u functions=%u command=%u allocation_list=%u patch_list=%u core_create_calls=0 hr=%08lx",
    handle, input.hDevice, input.Interface, input.Version, input.Flags.Value,
    unsigned(input.pCallbacks != nullptr), unsigned(input.pDeviceFuncs != nullptr),
    input.CommandBufferSize, input.AllocationListSize, input.PatchLocationListSize,
    static_cast<unsigned long>(D3DERR_NOTAVAILABLE));
  return D3DERR_NOTAVAILABLE;
}
HRESULT APIENTRY closeAdapter(HANDLE handle) {
  std::shared_ptr<const Adapter> owner; size_t remaining = 0;
  {
    std::lock_guard<std::mutex> lock(registryMutex);
    const auto entry = adapters.find(handle);
    if (entry == adapters.end()) return E_INVALIDARG;
    owner = entry->second; adapters.erase(entry); remaining = adapters.size();
  }
  const HRESULT hr = owner->original.pfnCloseAdapter(handle);
  trace("SYSTEM_D3D8_CLOSE adapter=%p runtime=%p hr=%08lx remaining=%zu",
        handle, owner->runtime, static_cast<unsigned long>(hr), remaining);
  return hr;
}
}

extern "C" HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* args) {
  try {
  if (!permitted()) return D3DERR_NOTAVAILABLE;
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Interface != 8) return D3DERR_NOTAVAILABLE;
  if (!input.pAdapterFuncs || !input.pAdapterCallbacks) return E_INVALIDARG;
  std::wstring caller;
  if (!system8Caller(_ReturnAddress(), caller)) return D3DERR_NOTAVAILABLE;
  if (!input.pAdapterCallbacks->pfnQueryAdapterInfoCb) return E_INVALIDARG;
  trace("SYSTEM_D3D8_OPEN_BEGIN interface=%u version=%u runtime=%p caller=%ls pointer_bytes=%zu readonly=1",
        input.Interface, input.Version, input.hAdapter, caller.c_str(), sizeof(void*));
  // All three pins come from the actual future successful consolidated I386
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
    auto owner = std::make_shared<Adapter>(Adapter{original, input.hAdapter, input.Version});
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
