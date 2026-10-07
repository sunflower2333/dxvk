#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dxvk::umd::diagnostic {

// A loaded image, not a PE file's disk layout. The caller obtains this extent
// from the Windows loader. Parsing never changes image bytes or caller output.
struct RuntimeImage {
  const uint8_t* data = nullptr;
  size_t bytes = 0;
};

struct RuntimeImport {
  size_t offset = 0;
  uint16_t machine = 0;
  uint8_t pointerBytes = 0;
};

inline bool runtimeImageRange(RuntimeImage image, size_t offset, size_t bytes) {
  return image.data && offset <= image.bytes && bytes <= image.bytes - offset;
}

template<typename T>
inline bool runtimeImageRead(RuntimeImage image, size_t offset, T& value) {
  if (!runtimeImageRange(image, offset, sizeof(T))) return false;
  std::memcpy(&value, image.data + offset, sizeof(T));
  return true;
}

inline bool runtimeImageString(RuntimeImage image, size_t offset, const char*& value) {
  if (!runtimeImageRange(image, offset, 1)) return false;
  const auto* start = reinterpret_cast<const char*>(image.data + offset);
  if (!std::memchr(start, 0, image.bytes - offset)) return false;
  value = start;
  return true;
}

inline bool runtimeLibraryEqual(const char* a, const char* b) {
  while (*a && *b) {
    const auto lower = [](unsigned char c) {
      return c >= 'A' && c <= 'Z' ? unsigned(c - 'A' + 'a') : unsigned(c);
    };
    if (lower(static_cast<unsigned char>(*a++)) != lower(static_cast<unsigned char>(*b++)))
      return false;
  }
  return *a == *b;
}

// The ARM64-only D3D9 diagnostic cannot be reused by reinterpreting x86 thunks
// as 64-bit pointers. Find exact, naturally aligned slots in either ABI, with
// bounded names/descriptors/thunks. Publish no partial list on malformed input.
inline bool findRuntimeImports(RuntimeImage image, const char* library,
                               const char* symbol, std::vector<RuntimeImport>& output) {
  if (!library || !symbol) return false;
  uint16_t dos = 0, machine = 0, optionalBytes = 0, magic = 0;
  uint32_t pe = 0, signature = 0, imageBytes = 0, directories = 0;
  if (!runtimeImageRead(image, 0, dos) || dos != 0x5a4d
      || !runtimeImageRead(image, 0x3c, pe) || pe < 0x40
      || !runtimeImageRange(image, pe, 24)
      || !runtimeImageRead(image, pe, signature) || signature != 0x00004550
      || !runtimeImageRead(image, size_t(pe) + 4, machine)
      || !runtimeImageRead(image, size_t(pe) + 20, optionalBytes)
      || !runtimeImageRange(image, size_t(pe) + 24, optionalBytes)
      || !runtimeImageRead(image, size_t(pe) + 24, magic)) return false;

  const bool narrow = magic == 0x10b && machine == 0x014c;
  const bool wide = magic == 0x20b && (machine == 0x8664 || machine == 0xaa64);
  if (!narrow && !wide) return false;
  const size_t optional = size_t(pe) + 24;
  const size_t directoryBase = narrow ? 96 : 112;
  if (optionalBytes < directoryBase + 16
      || !runtimeImageRead(image, optional + 56, imageBytes) || imageBytes != image.bytes
      || !runtimeImageRead(image, optional + directoryBase - 4, directories)
      || directories < 2) return false;
  uint32_t imports = 0, importBytes = 0;
  if (!runtimeImageRead(image, optional + directoryBase + 8, imports)
      || !runtimeImageRead(image, optional + directoryBase + 12, importBytes)) return false;
  std::vector<RuntimeImport> found;
  if (!imports && !importBytes) { output.swap(found); return true; }
  if (!imports || importBytes < 20 || !runtimeImageRange(image, imports, importBytes)) return false;
  const size_t pointerBytes = narrow ? 4 : 8;
  bool descriptorEnd = false;
  for (size_t descriptor = imports; descriptor <= size_t(imports) + importBytes - 20;
       descriptor += 20) {
    uint32_t fields[5] = {};
    std::memcpy(fields, image.data + descriptor, sizeof(fields));
    if (!fields[0] && !fields[1] && !fields[2] && !fields[3] && !fields[4]) {
      descriptorEnd = true;
      break;
    }
    const char* name = nullptr;
    if (!fields[3] || !runtimeImageString(image, fields[3], name)) return false;
    if (!runtimeLibraryEqual(name, library)) continue;
    if (!fields[0] || !fields[4] || fields[0] % pointerBytes || fields[4] % pointerBytes
        || !runtimeImageRange(image, fields[0], pointerBytes)
        || !runtimeImageRange(image, fields[4], pointerBytes)) return false;
    const size_t maxNames = (image.bytes - fields[0]) / pointerBytes;
    const size_t maxSlots = (image.bytes - fields[4]) / pointerBytes;
    bool thunkEnd = false;
    for (size_t i = 0; i < maxNames && i < maxSlots; ++i) {
      uint64_t thunk = 0, value = 0;
      std::memcpy(&thunk, image.data + fields[0] + i * pointerBytes, pointerBytes);
      std::memcpy(&value, image.data + fields[4] + i * pointerBytes, pointerBytes);
      if (!thunk) {
        if (value) return false;
        thunkEnd = true;
        break;
      }
      if (!value) return false;
      const uint64_t ordinal = narrow ? 0x80000000ull : 0x8000000000000000ull;
      if (thunk & ordinal) continue;
      if (thunk >= image.bytes || !runtimeImageRange(image, size_t(thunk), 3)) return false;
      const char* imported = nullptr;
      if (!runtimeImageString(image, size_t(thunk) + 2, imported)) return false;
      if (std::strcmp(imported, symbol)) continue;
      found.push_back({size_t(fields[4]) + i * pointerBytes, machine, uint8_t(pointerBytes)});
    }
    if (!thunkEnd) return false;
  }
  if (!descriptorEnd) return false;
  output.swap(found);
  return true;
}

}
