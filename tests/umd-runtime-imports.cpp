#include "../src/umd/umd_runtime_imports.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iterator>

namespace {
unsigned checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "CHECK failed line=%d: %s\n", __LINE__, #value); std::_Exit(1); } } while (0)
using namespace dxvk::umd::diagnostic;
using Bytes = std::vector<uint8_t>;
template<typename T> void put(Bytes& bytes, size_t offset, T value) {
  CHECK(offset + sizeof(value) <= bytes.size());
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
Bytes image(bool wide, uint16_t machine = 0) {
  Bytes bytes(1024);
  put<uint16_t>(bytes, 0, 0x5a4d); put<uint32_t>(bytes, 0x3c, 0x80);
  put<uint32_t>(bytes, 0x80, 0x00004550);
  put<uint16_t>(bytes, 0x84, machine ? machine : uint16_t(wide ? 0xaa64 : 0x14c));
  put<uint16_t>(bytes, 0x94, uint16_t(wide ? 240 : 224));
  put<uint16_t>(bytes, 0x98, uint16_t(wide ? 0x20b : 0x10b));
  put<uint32_t>(bytes, 0x98 + 56, uint32_t(bytes.size()));
  const size_t directories = 0x98 + (wide ? 112 : 96);
  put<uint32_t>(bytes, directories - 4, 16);
  put<uint32_t>(bytes, directories + 8, 0x200);
  put<uint32_t>(bytes, directories + 12, 40);
  put<uint32_t>(bytes, 0x200, 0x280);
  put<uint32_t>(bytes, 0x20c, 0x250);
  put<uint32_t>(bytes, 0x210, 0x300);
  std::memcpy(bytes.data() + 0x250, "gDi32.DlL", 10);
  put<uint32_t>(bytes, 0x280, 0x340);
  put<uint32_t>(bytes, 0x300, 0x12345678);
  std::memcpy(bytes.data() + 0x342, "D3DKMTQueryAdapterInfo", 22);
  return bytes;
}
bool find(const Bytes& bytes, std::vector<RuntimeImport>& slots) {
  return findRuntimeImports({bytes.data(), bytes.size()}, "GDI32.dll", "D3DKMTQueryAdapterInfo", slots);
}
void rejected(Bytes bytes) {
  std::vector<RuntimeImport> slots{{7, 8, 9}};
  CHECK(!find(bytes, slots));
  CHECK(slots.size() == 1 && slots[0].offset == 7 && slots[0].machine == 8 && slots[0].pointerBytes == 9);
}
}

int main(int argc, char** argv) {
  if (argc != 1 && (argc != 3 || std::strcmp(argv[1], "--mapped-image"))) return 64;
  for (const bool wide : {false, true}) {
    auto bytes = image(wide);
    std::vector<RuntimeImport> slots;
    CHECK(find(bytes, slots));
    CHECK(slots.size() == 1 && slots[0].offset == 0x300 && slots[0].pointerBytes == (wide ? 8 : 4));
    const Bytes unchanged = bytes;
    CHECK(find(bytes, slots)); CHECK(bytes == unchanged);
    auto malformed = bytes; put<uint32_t>(malformed, 0x3c, 0xfffffff0); rejected(malformed);
    malformed = bytes; put<uint16_t>(malformed, 0x94, 95); rejected(malformed);
    malformed = bytes; put<uint16_t>(malformed, 0x84, wide ? 0x014c : 0xaa64); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x98 + 56, 2048); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x200, 0); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x210, 0x301); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x280, 1023); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x300, 0); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x98 + (wide ? 112 : 96) + 12, 20); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x20c, 1000);
    std::memset(malformed.data() + 1000, 'X', 24); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x280, 998);
    std::memset(malformed.data() + 1000, 'X', 24); rejected(malformed);
    malformed = bytes; put<uint32_t>(malformed, 0x200, 1016);
    put<uint32_t>(malformed, 1016, 0x340); put<uint32_t>(malformed, 1020, 0x340); rejected(malformed);
    malformed = bytes; std::memcpy(malformed.data() + 0x250, "GDI32.dll.bad", 14);
    CHECK(find(malformed, slots) && slots.empty());
    malformed = bytes; std::memcpy(malformed.data() + 0x342, "D3DKMTQueryAdapterInfoSuffix", 28);
    CHECK(find(malformed, slots) && slots.empty());
    malformed = bytes;
    if (wide) put<uint64_t>(malformed, 0x280, 0x8000000000000001ull);
    else put<uint32_t>(malformed, 0x280, 0x80000001u);
    CHECK(find(malformed, slots) && slots.empty());
    // Every truncated extent must reject without publishing partial slots.
    for (size_t length = 0; length < bytes.size(); ++length) {
      std::vector<RuntimeImport> retained{{7, 8, 9}};
      CHECK(!findRuntimeImports({bytes.data(), length}, "GDI32.dll", "D3DKMTQueryAdapterInfo", retained));
      CHECK(retained.size() == 1 && retained[0].offset == 7);
    }
  }
  auto amd64 = image(true, 0x8664);
  std::vector<RuntimeImport> slots;
  CHECK(find(amd64, slots) && slots.size() == 1 && slots[0].machine == 0x8664);
  CHECK(!findRuntimeImports({}, "GDI32.dll", "D3DKMTQueryAdapterInfo", slots));
  if (argc == 3) {
    std::ifstream file(argv[2], std::ios::binary);
    CHECK(bool(file));
    Bytes bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    CHECK(find(bytes, slots) && slots.size() == 1);
    std::printf("original runtime import machine=%04x pointer_bytes=%u slot_rva=%zx image_bytes=%zu\n",
      unsigned(slots[0].machine), unsigned(slots[0].pointerBytes), slots[0].offset, bytes.size());
  }
  std::printf("native runtime imports PASS checks=%u PE32/AMD64/ARM64 bounded slots; no runtime mutation\n", checks);
}
