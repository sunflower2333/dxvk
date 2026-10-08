#pragma once

#include "../src/umd/umd_runtime_imports.h"
#include "umd-d3d8-runtime-policy.h"
#include <array>
#include <string>
#include <string_view>

namespace dxvk::test::runtime8::system {

struct Pe {
  uint16_t machine = 0, magic = 0, sections = 0;
  uint32_t offset = 0, timestamp = 0, imageBytes = 0, headerBytes = 0, checksum = 0, entry = 0;
};
inline bool readPe(umd::diagnostic::RuntimeImage data, Pe& output) {
  using umd::diagnostic::runtimeImageRead;
  using umd::diagnostic::runtimeImageRange;
  Pe value; uint16_t dos = 0, optional = 0; uint32_t signature = 0;
  if (!runtimeImageRead(data, 0, dos) || dos != 0x5a4d
      || !runtimeImageRead(data, 0x3c, value.offset) || value.offset < 0x40
      || !runtimeImageRange(data, value.offset, 24)
      || !runtimeImageRead(data, value.offset, signature) || signature != 0x4550
      || !runtimeImageRead(data, size_t(value.offset) + 4, value.machine)
      || !runtimeImageRead(data, size_t(value.offset) + 6, value.sections)
      || !runtimeImageRead(data, size_t(value.offset) + 8, value.timestamp)
      || !runtimeImageRead(data, size_t(value.offset) + 20, optional) || optional < 68
      || !runtimeImageRange(data, size_t(value.offset) + 24, optional)
      || !runtimeImageRead(data, size_t(value.offset) + 24, value.magic)
      || !runtimeImageRead(data, size_t(value.offset) + 40, value.entry)
      || !runtimeImageRead(data, size_t(value.offset) + 80, value.imageBytes)
      || !runtimeImageRead(data, size_t(value.offset) + 84, value.headerBytes)
      || !runtimeImageRead(data, size_t(value.offset) + 88, value.checksum)) return false;
  if (value.machine != 0x014c || value.magic != 0x010b || !value.sections
      || !value.imageBytes || value.headerBytes < size_t(value.offset) + 24 + optional
      || value.headerBytes > value.imageBytes || value.entry >= value.imageBytes) return false;
  output = value; return true;
}
inline bool samePe(const Pe& a, const Pe& b) {
  return a.machine == b.machine && a.magic == b.magic && a.sections == b.sections
    && a.offset == b.offset && a.timestamp == b.timestamp && a.imageBytes == b.imageBytes
    && a.headerBytes == b.headerBytes && a.checksum == b.checksum && a.entry == b.entry;
}
struct FileKey {
  uint32_t volume = 0, high = 0, low = 0, bytesHigh = 0, bytesLow = 0;
};
inline bool sameFileKey(const FileKey& a, const FileKey& b) {
  return a.volume == b.volume && a.high == b.high && a.low == b.low
    && a.bytesHigh == b.bytesHigh && a.bytesLow == b.bytesLow;
}
struct FileObservation {
  bool complete = false, unchanged = false;
  FileKey key{}; Pe pe{};
  std::array<uint8_t, 32> hash{};
  std::wstring finalNt;
};
// The actual Win32 helper below supplies these observations while both files
// are locked against writers/deletion. No directory-suffix whitelist exists.
inline bool joinsMappedImage(const Pe& loaded, std::wstring_view mapped,
    const FileObservation& logical, const FileObservation& explicitI386) {
  return loaded.machine == 0x014c && loaded.magic == 0x010b && !mapped.empty()
    && logical.complete && explicitI386.complete && logical.unchanged && explicitI386.unchanged
    && !logical.finalNt.empty() && !explicitI386.finalNt.empty()
    && equalName<wchar_t>(mapped, logical.finalNt) && equalName<wchar_t>(mapped, explicitI386.finalNt)
    && sameFileKey(logical.key, explicitI386.key) && logical.hash == explicitI386.hash
    && samePe(loaded, logical.pe) && samePe(loaded, explicitI386.pe);
}
inline bool processI386OnArm64(uint16_t process, uint16_t native, size_t pointerBytes) {
  return process == 0x014c && native == 0xaa64 && pointerBytes == 4;
}

}

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <bcrypt.h>
#include <cwchar>
#include <cstring>
#include <vector>

namespace dxvk::test::runtime8::system {
using Log = void (*)(const char*, ...);
inline void tracePe(const wchar_t* name, const char* view, const Pe& value, Log log) {
  log("D3D8_SYSTEM_PE name=%ls view=%s machine=%04x magic=%04x sections=%u pe=%08lx timestamp=%08lx image_bytes=%lu header_bytes=%lu checksum=%08lx entry=%08lx",
    name, view, unsigned(value.machine), unsigned(value.magic), unsigned(value.sections),
    static_cast<unsigned long>(value.offset), static_cast<unsigned long>(value.timestamp),
    static_cast<unsigned long>(value.imageBytes), static_cast<unsigned long>(value.headerBytes),
    static_cast<unsigned long>(value.checksum), static_cast<unsigned long>(value.entry));
}
inline std::wstring modulePath(HMODULE module) {
  std::array<wchar_t, 32768> data{};
  const DWORD count = GetModuleFileNameW(module, data.data(), DWORD(data.size()));
  return count && count < data.size() ? std::wstring(data.data(), count) : std::wstring();
}
inline bool loadedPe(HMODULE module, Pe& output) {
  MODULEINFO info{};
  return K32GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)) != FALSE
    && readPe({static_cast<const uint8_t*>(info.lpBaseOfDll), info.SizeOfImage}, output)
    && output.imageBytes == info.SizeOfImage;
}
inline bool digest(HANDLE file, std::array<uint8_t, 32>& output) {
  struct Algorithm { BCRYPT_ALG_HANDLE value = nullptr; ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); } } algorithm;
  DWORD objectBytes = 0, returned = 0;
  if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0
      || BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
        sizeof(objectBytes), &returned, 0) < 0 || returned != sizeof(objectBytes)
      || !objectBytes || objectBytes > 65536) return false;
  std::vector<UCHAR> object(objectBytes);
  struct Hash { BCRYPT_HASH_HANDLE value = nullptr; ~Hash() { if (value) BCryptDestroyHash(value); } } hash;
  LARGE_INTEGER zero{};
  if (BCryptCreateHash(algorithm.value, &hash.value, object.data(), objectBytes, nullptr, 0, 0) < 0
      || !SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) return false;
  std::array<UCHAR, 65536> buffer{}; uint64_t total = 0;
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) return false;
    if (!count) break;
    total += count;
    if (total > 32u * 1024u * 1024u || BCryptHashData(hash.value, buffer.data(), count, 0) < 0) return false;
  }
  std::array<uint8_t, 32> value{};
  if (BCryptFinishHash(hash.value, value.data(), DWORD(value.size()), 0) < 0) return false;
  output = value; return true;
}
inline FileKey fileKey(const BY_HANDLE_FILE_INFORMATION& value) {
  return {value.dwVolumeSerialNumber, value.nFileIndexHigh, value.nFileIndexLow, value.nFileSizeHigh, value.nFileSizeLow};
}
struct File {
  HANDLE handle = INVALID_HANDLE_VALUE;
  const wchar_t* name; const char* view; const Log log;
  File(const wchar_t* inputName, const char* inputView, Log logger) : name(inputName), view(inputView), log(logger) { }
  File(const File&) = delete; File& operator=(const File&) = delete;
  ~File() { close(); }
  bool close() {
    if (handle == INVALID_HANDLE_VALUE) return true;
    const HANDLE original = handle; handle = INVALID_HANDLE_VALUE;
    const BOOL status = CloseHandle(original);
    log("D3D8_SYSTEM_FILE_CLOSE name=%ls view=%s handle=%p status=%u", name, view, original, unsigned(status != FALSE));
    return status != FALSE;
  }
  bool observe(const std::wstring& input, FileObservation& output) {
    handle = CreateFileW(input.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    FileObservation value; BY_HANDLE_FILE_INFORMATION before{}, after{};
    if (!GetFileInformationByHandle(handle, &before)) return false;
    value.key = fileKey(before);
    std::array<wchar_t, 32768> finalName{};
    const DWORD finalCount = GetFinalPathNameByHandleW(handle, finalName.data(), DWORD(finalName.size()),
      FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
    if (!finalCount || finalCount >= finalName.size()) return false;
    value.finalNt.assign(finalName.data(), finalCount);
    std::array<uint8_t, 65536> headers{}; DWORD count = 0; LARGE_INTEGER zero{};
    if (!SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN)
        || !ReadFile(handle, headers.data(), DWORD(headers.size()), &count, nullptr)
        || !readPe({headers.data(), count}, value.pe)) return false;
    std::array<uint8_t, 32> afterHash{};
    if (!digest(handle, value.hash) || !digest(handle, afterHash)
        || !GetFileInformationByHandle(handle, &after)) return false;
    value.unchanged = value.hash == afterHash && sameFileKey(value.key, fileKey(after));
    value.complete = value.unchanged && before.nFileSizeHigh == 0 && before.nFileSizeLow >= value.pe.headerBytes;
    char hex[65]{};
    constexpr char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < value.hash.size(); ++index) {
      hex[index * 2] = digits[value.hash[index] >> 4];
      hex[index * 2 + 1] = digits[value.hash[index] & 15];
    }
    tracePe(name, view, value.pe, log);
    log("D3D8_SYSTEM_FILE_ID name=%ls view=%s input=%ls final_nt=%ls volume=%08lx index_high=%08lx index_low=%08lx bytes=%lu machine=%04x sha256=%s unchanged=%u locked=1",
      name, view, input.c_str(), value.finalNt.c_str(), static_cast<unsigned long>(value.key.volume),
      static_cast<unsigned long>(value.key.high), static_cast<unsigned long>(value.key.low),
      static_cast<unsigned long>(value.key.bytesLow), unsigned(value.pe.machine), hex, unsigned(value.unchanged));
    if (!value.complete) return false;
    output = std::move(value); return true;
  }
};
inline bool moduleIdentity(HMODULE module, const wchar_t* name, const std::wstring& directory, Log log) {
  if (!module || GetModuleHandleW(name) != module || directory.empty()) return false;
  const auto logical = modulePath(module); const auto explicitPath = directory + L"\\" + name;
  Pe before{}, after{}; std::array<wchar_t, 32768> mappedBuffer{};
  if (logical.empty() || !loadedPe(module, before)) return false;
  tracePe(name, "loaded", before, log);
  const DWORD count = K32GetMappedFileNameW(GetCurrentProcess(), reinterpret_cast<LPVOID>(module),
    mappedBuffer.data(), DWORD(mappedBuffer.size()));
  if (!count || count >= mappedBuffer.size()) return false;
  const std::wstring mapped(mappedBuffer.data(), count);
  File logicalFile(name, "logical", log), explicitFile(name, "explicit-I386", log);
  FileObservation logicalValue, explicitValue;
  const bool inputs = logicalFile.observe(logical, logicalValue) && explicitFile.observe(explicitPath, explicitValue);
  const bool joined = inputs && joinsMappedImage(before, mapped, logicalValue, explicitValue)
    && loadedPe(module, after) && samePe(before, after) && GetModuleHandleW(name) == module;
  const bool explicitClosed = explicitFile.close(), logicalClosed = logicalFile.close();
  const bool accepted = joined && explicitClosed && logicalClosed;
  log("D3D8_SYSTEM_MODULE_ID name=%ls logical=%ls explicit=%ls mapped_nt=%ls identity=%u machine=%04x readonly=1",
    name, logical.c_str(), explicitPath.c_str(), mapped.c_str(), unsigned(accepted), unsigned(before.machine));
  return accepted;
}
struct Api {
  FARPROC address = nullptr; DWORD error = ERROR_SUCCESS; HMODULE owner = nullptr;
};
inline Api lookup(HMODULE provider, const char* name) {
  SetLastError(ERROR_SUCCESS);
  const FARPROC address = provider ? GetProcAddress(provider, name) : nullptr;
  const DWORD error = provider ? GetLastError() : ERROR_MOD_NOT_FOUND;
  return {address, error};
}
inline bool resolveOwner(Api& api) {
  return api.address && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(api.address), &api.owner) != FALSE;
}
inline bool directory(std::wstring& output, Log log) {
  using Machines = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);
  using Directory = UINT (WINAPI*)(LPWSTR, UINT, WORD);
  const HMODULE kernel = GetModuleHandleW(L"kernel32.dll"), base = GetModuleHandleW(L"kernelbase.dll");
  Api machine = lookup(kernel, "IsWow64Process2"), location = lookup(kernel, "GetSystemWow64Directory2W");
  // Retain both original lookup errors before tracing or querying an owner.
  log("D3D8_SYSTEM_API_LOOKUP provider=kernel32.dll process_present=%u process_error=%lu directory_present=%u directory_error=%lu",
    unsigned(machine.address != nullptr), machine.error, unsigned(location.address != nullptr), location.error);
  const bool alternate = !location.address && location.error == ERROR_PROC_NOT_FOUND;
  if (alternate) {
    location = lookup(base, "GetSystemWow64Directory2W");
    log("D3D8_SYSTEM_API_LOOKUP provider=kernelbase.dll directory_present=%u directory_error=%lu exact_API_only=1",
      unsigned(location.address != nullptr), location.error);
  }
  Machines machines = nullptr; Directory directoryApi = nullptr;
  static_assert(sizeof(machines) == sizeof(machine.address) && sizeof(directoryApi) == sizeof(location.address));
  std::memcpy(&machines, &machine.address, sizeof(machines));
  std::memcpy(&directoryApi, &location.address, sizeof(directoryApi));
  if (!machines || !directoryApi) return false;
  USHORT process = 0, native = 0;
  if (!machines(GetCurrentProcess(), &process, &native) || !processI386OnArm64(process, native, sizeof(void*))) return false;
  log("D3D8_SYSTEM_PROCESS_ID process=%04x native=%04x pointer_bytes=%zu api=IsWow64Process2 identity=1",
    unsigned(process), unsigned(native), sizeof(void*));
  std::array<wchar_t, MAX_PATH> value{};
  const UINT count = directoryApi(value.data(), UINT(value.size()), IMAGE_FILE_MACHINE_I386);
  if (!count || count >= value.size()) return false;
  const std::wstring canonical(value.data(), count);
  if (!resolveOwner(machine) || !resolveOwner(location)
      || (machine.owner != kernel && machine.owner != base) || (location.owner != kernel && location.owner != base)
      || (alternate && location.owner != base)
      || !moduleIdentity(kernel, L"kernel32.dll", canonical, log)
      || !moduleIdentity(base, L"kernelbase.dll", canonical, log)) return false;
  output = canonical;
  log("D3D8_SYSTEM_DIRECTORY machine=014c api=GetSystemWow64Directory2W path=%ls", canonical.c_str());
  return true;
}
}
#endif
