#pragma once

// Owned diagnostic inputs. This does not expose a graphics factory or create
// a KMT device: Microsoft D3D8 owns all device/render/presentation callbacks.
#include <windows.h>
#include <bcrypt.h>
#include <d3dkmthk.h>
#include <psapi.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>
#include "umd-d3d8-runtime-policy.h"

namespace dxvk::test::runtime8 {
inline constexpr wchar_t hardwareCoreCommit[] = L"d7e5c7d46b8ce889e993bfab66a3b78b076c49d1";
inline constexpr wchar_t hardwareCoreHash[] = L"7be8cbb9850407ccc304528911a6fbd71b01971fbeb4a86550cc8dcc2f346a3f";
inline constexpr wchar_t hardwareCorePath[] = L"C:\\Users\\Public\\DxvkD3D8Candidate-d7e5c7d-37648387721\\viogpudxvk.dll";
inline constexpr wchar_t loaderHash[] = L"d459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7";
inline constexpr wchar_t icdHash[] = L"2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c";
inline constexpr wchar_t manifestHash[] = L"74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80";
inline constexpr wchar_t originalUserSid[] = L"S-1-5-21-362894365-441372107-2852668596-1000";

class D3d8HardwarePins {
public:
  using Log = void (*)(const char*, ...);
  explicit D3d8HardwarePins(Log logger) : log(logger) { }
  ~D3d8HardwarePins() { restore(); }
  D3d8HardwarePins(const D3d8HardwarePins&) = delete;
  D3d8HardwarePins& operator=(const D3d8HardwarePins&) = delete;
  bool open(const wchar_t* core, const wchar_t* hash, const wchar_t* commit) {
    if (std::wcscmp(commit, hardwareCoreCommit) || std::wcscmp(hash, hardwareCoreHash)
        || _wcsicmp(core, hardwareCorePath)) return false;
    folder = core; folder.resize(folder.find_last_of(L'\\') + 1);
    const std::array<std::pair<const wchar_t*, const wchar_t*>, 4> inputs{{
      {L"viogpudxvk.dll", hardwareCoreHash}, {L"viogpu_gl_loader_x86.dll", loaderHash},
      {L"viogpu_gl_vk_x86.dll", icdHash}, {L"freedreno_icd.json", manifestHash}}};
    for (size_t i = 0; i < inputs.size(); ++i) {
      files[i] = std::make_unique<File>();
      const auto name = folder + inputs[i].first;
      files[i]->handle = CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
      if (files[i]->handle == INVALID_HANDLE_VALUE) return false;
      std::array<wchar_t, 32768> finalPath{};
      const DWORD count = GetFinalPathNameByHandleW(files[i]->handle, finalPath.data(), DWORD(finalPath.size()), FILE_NAME_NORMALIZED);
      if (!count || count >= finalPath.size()) return false;
      std::wstring actual(finalPath.data(), count);
      if (actual.rfind(L"\\\\?\\", 0) == 0) actual.erase(0, 4);
      if (_wcsicmp(actual.c_str(), name.c_str())) return false;
      std::wstring actualHash;
      if (!sha256(files[i]->handle, actualHash) || actualHash != inputs[i].second) return false;
      if (i != 3 && !diskI386(files[i]->handle)) return false;
      log("D3D8_PAYLOAD_PIN path=%ls sha256=%ls machine=%s locked=1 original_bytes=1",
        name.c_str(), actualHash.c_str(), i == 3 ? "json" : "014c");
    }
    for (const auto* name : {L"viogpudxvk.dll", L"viogpu_gl_loader_x86.dll", L"viogpu_gl_vk_x86.dll",
        L"vulkan-1.dll", L"winevulkan.dll", L"d3d10warp.dll"}) if (GetModuleHandleW(name)) return false;
    for (const auto* name : {L"VK_DRIVER_FILES", L"VK_ICD_FILENAMES"}) {
      SetLastError(ERROR_SUCCESS);
      if (GetEnvironmentVariableW(name, nullptr, 0) || GetLastError() != ERROR_ENVVAR_NOT_FOUND) return false;
    }
    const auto json = folder + L"freedreno_icd.json";
    if (!SetEnvironmentVariableW(L"VK_DRIVER_FILES", json.c_str())) return false;
    envCount = 1;
    if (!SetEnvironmentVariableW(L"VK_ICD_FILENAMES", json.c_str())) return false;
    envCount = 2;
    log("D3D8_HARDWARE_SOURCE core_commit=%ls ci_run=37648387721 mesa_commit=8443c71a5ab32b9d58b904fa51f4bf2f9089db8d mesa_ci_run=37453381660 driver_selection=owned-json raw_architecture=014c",
      hardwareCoreCommit);
    return true;
  }
  bool loaded() const {
    for (const auto* name : {L"vulkan-1.dll", L"winevulkan.dll", L"d3d10warp.dll"})
      if (GetModuleHandleW(name)) return false;
    for (const auto* name : {L"viogpudxvk.dll", L"viogpu_gl_loader_x86.dll", L"viogpu_gl_vk_x86.dll"}) {
      const HMODULE module = GetModuleHandleW(name); if (!module) return false;
      std::array<wchar_t, 32768> actual{};
      const DWORD count = GetModuleFileNameW(module, actual.data(), DWORD(actual.size()));
      if (!count || count >= actual.size() || _wcsicmp(actual.data(), (folder + name).c_str())) return false;
      MODULEINFO info{};
      if (!K32GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)) || info.SizeOfImage < 0x40) return false;
      const auto* bytes = static_cast<const uint8_t*>(info.lpBaseOfDll);
      uint32_t offset = 0, signature = 0; uint16_t machine = 0;
      std::memcpy(&offset, bytes + 0x3c, 4);
      if (offset > info.SizeOfImage - 6) return false;
      std::memcpy(&signature, bytes + offset, 4); std::memcpy(&machine, bytes + offset + 4, 2);
      if (signature != 0x4550 || machine != IMAGE_FILE_MACHINE_I386) return false;
      log("D3D8_PRIVATE_MODULE name=%ls path=%ls machine=014c", name, actual.data());
    }
    return true;
  }
  bool restore() noexcept {
    bool restored = true;
    if (envCount == 2) restored = SetEnvironmentVariableW(L"VK_ICD_FILENAMES", nullptr) != FALSE && restored;
    if (envCount) restored = SetEnvironmentVariableW(L"VK_DRIVER_FILES", nullptr) != FALSE && restored;
    if (restored) envCount = 0;
    return restored;
  }
private:
  struct File { HANDLE handle = INVALID_HANDLE_VALUE; ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); } };
  struct Algorithm { BCRYPT_ALG_HANDLE handle = nullptr; ~Algorithm() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); } };
  struct Hash { BCRYPT_HASH_HANDLE handle = nullptr; ~Hash() { if (handle) BCryptDestroyHash(handle); } };
  const Log log;
  std::wstring folder;
  std::array<std::unique_ptr<File>, 4> files;
  unsigned envCount = 0;
  static bool seek(HANDLE file, LONGLONG position) {
    LARGE_INTEGER offset{}; offset.QuadPart = position;
    return SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) != FALSE;
  }
  static bool diskI386(HANDLE file) {
    std::array<unsigned char, 64> dos{}; DWORD read = 0;
    if (!seek(file, 0) || !ReadFile(file, dos.data(), DWORD(dos.size()), &read, nullptr)
        || read != dos.size() || dos[0] != 'M' || dos[1] != 'Z') return false;
    uint32_t offset = 0; std::memcpy(&offset, dos.data() + 0x3c, 4);
    std::array<unsigned char, 6> pe{};
    if (!seek(file, offset) || !ReadFile(file, pe.data(), DWORD(pe.size()), &read, nullptr) || read != pe.size()) return false;
    uint32_t signature = 0; uint16_t machine = 0;
    std::memcpy(&signature, pe.data(), 4); std::memcpy(&machine, pe.data() + 4, 2);
    return signature == 0x4550 && machine == IMAGE_FILE_MACHINE_I386;
  }
  static bool sha256(HANDLE file, std::wstring& text) {
    Algorithm algorithm; DWORD bytes = 0, returned = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0
        || BCryptGetProperty(algorithm.handle, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&bytes),
          sizeof(bytes), &returned, 0) < 0 || returned != sizeof(bytes) || !bytes) return false;
    std::vector<UCHAR> object(bytes); Hash hash;
    if (BCryptCreateHash(algorithm.handle, &hash.handle, object.data(), bytes, nullptr, 0, 0) < 0 || !seek(file, 0)) return false;
    std::array<UCHAR, 65536> buffer{};
    for (;;) {
      DWORD count = 0;
      if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &count, nullptr)) return false;
      if (!count) break;
      if (BCryptHashData(hash.handle, buffer.data(), count, 0) < 0) return false;
    }
    std::array<UCHAR, 32> digest{};
    if (BCryptFinishHash(hash.handle, digest.data(), DWORD(digest.size()), 0) < 0) return false;
    constexpr wchar_t digits[] = L"0123456789abcdef"; text.clear();
    for (const auto byte : digest) { text.push_back(digits[byte >> 4]); text.push_back(digits[byte & 15]); }
    return true;
  }
};
}
