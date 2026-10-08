#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

namespace dxvk::test::runtime8 {

inline constexpr wchar_t permissionName[] = L"VIOGPU_DXVK_RUNTIME_DIAGNOSTIC";
inline constexpr wchar_t permissionValue[] = L"read-only-d3d8-interface8-v1";
inline constexpr wchar_t devicePermissionValue[] = L"device-render-d3d8-interface8-v1";
inline constexpr wchar_t enumeratePermissionValue[] = L"enumerate-device-d3d8-interface8-v1";
inline constexpr wchar_t corePathName[] = L"VIOGPU_DXVK_D3D8_CORE_PATH";
inline constexpr wchar_t coreSha256Name[] = L"VIOGPU_DXVK_D3D8_CORE_SHA256";
inline constexpr wchar_t coreCommitName[] = L"VIOGPU_DXVK_D3D8_CORE_COMMIT";
inline constexpr std::array<const wchar_t*, 4> diagnosticNames{
  permissionName, corePathName, coreSha256Name, coreCommitName};
inline constexpr uint32_t umdNameQuery = 1; // Original SDK KMTQAITYPE_UMDRIVERNAME.
inline constexpr uint32_t dx9DriverNameVersion = 0; // D3D8 uses the legacy DX9 slot.
inline constexpr size_t capsBytes = 53 * sizeof(uint32_t);

enum class Mode : uint32_t { Denied, ReadOnly, Device, EnumerateDevice };

template<typename Char>
Mode permissionMode(std::basic_string_view<Char> value) {
  const auto exact = [value](const wchar_t* expected) {
    size_t count = 0;
    while (expected[count]) ++count;
    if (count != value.size()) return false;
    for (size_t i = 0; i < count; ++i)
      if (value[i] != Char(expected[i])) return false;
    return true;
  };
  if (exact(permissionValue)) return Mode::ReadOnly;
  if (exact(devicePermissionValue)) return Mode::Device;
  if (exact(enumeratePermissionValue)) return Mode::EnumerateDevice;
  return Mode::Denied;
}

// An adapter never gains device permission from later environment mutation.
inline bool mayCreateDevice(Mode captured, Mode current, uint32_t api) {
  return (captured == Mode::Device || captured == Mode::EnumerateDevice)
      && current == captured && api == 8;
}

// Microsoft D3D8 constructs an internal driver device before querying HAL
// caps. This separate diagnostic permission accepts only the observed normal
// Interface8 contract. Version is an opaque runtime value, paired with the
// adapter snapshot; obsolete command/list storage is deliberately irrelevant.
inline bool enumerationContract(uint32_t api, uint32_t adapterVersion,
                                uint32_t deviceVersion, uint32_t flags) {
  return api == 8 && adapterVersion == deviceVersion && flags == 0;
}

inline bool mayRender(Mode captured, Mode current) {
  return captured == Mode::Device && current == captured;
}

template<typename Char> constexpr Char asciiFold(Char value) {
  return value >= Char('A') && value <= Char('Z') ? Char(value + Char('a' - 'A')) : value;
}
template<typename Char>
bool equalName(std::basic_string_view<Char> left, std::basic_string_view<Char> right) {
  if (left.size() != right.size()) return false;
  for (size_t i = 0; i < left.size(); ++i)
    if (asciiFold(left[i]) != asciiFold(right[i])) return false;
  return true;
}

template<typename Char>
bool hexIdentity(std::basic_string_view<Char> value, size_t digits) {
  if (value.size() != digits) return false;
  for (const Char input : value) {
    const Char character = asciiFold(input);
    if (!((character >= Char('0') && character <= Char('9'))
        || (character >= Char('a') && character <= Char('f')))) return false;
  }
  return true;
}

// The source commit is an independently joined CI receipt, not information
// inferred from DLL bytes. Its prefix and actual CI run identify the owned
// copy; the full SHA256 is checked against that original file before loading.
template<typename Char>
bool ownedCorePath(std::basic_string_view<Char> path, std::basic_string_view<Char> commit) {
  constexpr char prefix[] = "C:\\Users\\Public\\DxvkD3D8Candidate-";
  constexpr char suffix[] = "\\viogpudxvk.dll";
  constexpr size_t prefixBytes = sizeof(prefix) - 1, suffixBytes = sizeof(suffix) - 1;
  if (!hexIdentity(commit, 40) || path.size() >= 260
      || path.size() <= prefixBytes + 8 + suffixBytes) return false;
  for (size_t i = 0; i < prefixBytes; ++i)
    if (asciiFold(path[i]) != asciiFold(Char(prefix[i]))) return false;
  for (size_t i = 0; i < 7; ++i)
    if (asciiFold(path[prefixBytes + i]) != asciiFold(commit[i])) return false;
  if (path[prefixBytes + 7] != Char('-')) return false;
  const size_t runEnd = path.size() - suffixBytes;
  if (path[prefixBytes + 8] == Char('0') || runEnd - (prefixBytes + 8) > 20) return false;
  for (size_t i = prefixBytes + 8; i < runEnd; ++i)
    if (path[i] < Char('0') || path[i] > Char('9')) return false;
  for (size_t i = 0; i < suffixBytes; ++i)
    if (asciiFold(path[runEnd + i]) != asciiFold(Char(suffix[i]))) return false;
  return true;
}

// The selector accepts a canonical owned absolute path. Reject slash aliases,
// traversal, alternate streams and embedded NULs before touching any IAT.
template<typename Char>
bool ownedFrontPath(std::basic_string_view<Char> path) {
  constexpr char prefix[] = "C:\\Users\\Public\\DxvkD3D8Runtime-";
  constexpr char suffix[] = "\\viogpu-d3d8-runtime-front.dll";
  constexpr size_t prefixBytes = sizeof(prefix) - 1, suffixBytes = sizeof(suffix) - 1;
  if (path.size() >= 260 || path.size() <= prefixBytes + suffixBytes) return false;
  for (size_t i = 0; i < prefixBytes; ++i)
    if (asciiFold(path[i]) != asciiFold(Char(prefix[i]))) return false;
  for (size_t i = 0; i < suffixBytes; ++i)
    if (asciiFold(path[path.size() - suffixBytes + i]) != asciiFold(Char(suffix[i]))) return false;
  for (size_t i = 0; i < path.size(); ++i) {
    if (!path[i] || path[i] == Char('/') || (path[i] == Char(':') && i != 1)) return false;
    if (i && path[i] == Char('.') && path[i - 1] == Char('.')) return false;
  }
  return true;
}

// Preserve every original query status and every unmatched driver's bytes.
// Both expected and replacement names belong to immutable selector storage.
// Only a successful legacy name query with the exact installed filename can
// publish the separately owned frontend. Copy the complete zero-padded buffer.
template<typename Char, size_t Count>
bool selectDriverName(int32_t originalStatus, uint32_t type, size_t privateBytes,
                      size_t expectedPrivateBytes, uint32_t version,
                      Char (&name)[Count], std::basic_string_view<Char> expected,
                      std::basic_string_view<Char> replacement) {
  if (originalStatus != 0 || type != umdNameQuery || privateBytes != expectedPrivateBytes
      || version != dx9DriverNameVersion || expected.empty() || expected.size() >= Count
      || !ownedFrontPath(replacement) || replacement.size() >= Count) return false;
  const auto end = std::find(std::begin(name), std::end(name), Char(0));
  if (end == std::end(name)
      || !equalName(std::basic_string_view<Char>(name, size_t(end - name)), expected)) return false;
  std::array<Char, Count> publication{};
  std::copy(replacement.begin(), replacement.end(), publication.begin());
  std::copy(publication.begin(), publication.end(), std::begin(name));
  return true;
}

}
