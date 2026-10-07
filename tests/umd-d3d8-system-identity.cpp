#include "umd-d3d8-system-identity.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace sys = dxvk::test::runtime8::system;
unsigned checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "system identity CHECK %u: %s\n", __LINE__, #value); std::exit(1); } } while (0)
template<typename T> void put(std::vector<uint8_t>& bytes, size_t offset, T value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
std::vector<uint8_t> originalPe() {
  std::vector<uint8_t> bytes(512, 0);
  put<uint16_t>(bytes, 0, 0x5a4d); put<uint32_t>(bytes, 0x3c, 0x80);
  put<uint32_t>(bytes, 0x80, 0x4550); put<uint16_t>(bytes, 0x84, 0x014c);
  put<uint16_t>(bytes, 0x86, 5); put<uint32_t>(bytes, 0x88, 0x12345678);
  put<uint16_t>(bytes, 0x94, 224); put<uint16_t>(bytes, 0x98, 0x010b);
  put<uint32_t>(bytes, 0xa8, 4096); put<uint32_t>(bytes, 0xd0, 65536);
  put<uint32_t>(bytes, 0xd4, 512); put<uint32_t>(bytes, 0xd8, 17);
  return bytes;
}
int main() {
  const auto original = originalPe(); sys::Pe pe{};
  CHECK(sys::readPe({original.data(), original.size()}, pe));
  CHECK(pe.machine == 0x014c && pe.magic == 0x010b && pe.imageBytes == 65536);
  const auto malformed = [&](std::vector<uint8_t> bytes) {
    auto output = pe; CHECK(!sys::readPe({bytes.data(), bytes.size()}, output)); CHECK(sys::samePe(output, pe));
  };
  malformed({}); malformed(std::vector<uint8_t>(63));
  for (const auto value : {0u, 0x3fu, 0xfffffff0u}) { auto bytes = original; put<uint32_t>(bytes, 0x3c, value); malformed(bytes); }
  { auto bytes = original; bytes[0] = 0; malformed(bytes); }
  { auto bytes = original; bytes[0x80] = 0; malformed(bytes); }
  for (const auto value : {uint16_t(0), uint16_t(67), uint16_t(65535)}) { auto bytes = original; put<uint16_t>(bytes, 0x94, value); malformed(bytes); }
  for (const auto value : {uint16_t(0), uint16_t(0x8664), uint16_t(0xaa64)}) { auto bytes = original; put<uint16_t>(bytes, 0x84, value); malformed(bytes); }
  { auto bytes = original; put<uint16_t>(bytes, 0x98, 0x020b); malformed(bytes); }
  { auto bytes = original; put<uint16_t>(bytes, 0x86, 0); malformed(bytes); }
  { auto bytes = original; put<uint32_t>(bytes, 0xd0, 0); malformed(bytes); }
  { auto bytes = original; put<uint32_t>(bytes, 0xd4, 128); malformed(bytes); }
  { auto bytes = original; put<uint32_t>(bytes, 0xd4, 65537); malformed(bytes); }
  { auto bytes = original; put<uint32_t>(bytes, 0xa8, 65536); malformed(bytes); }
  CHECK(sys::processI386OnArm64(0x014c, 0xaa64, 4));
  CHECK(!sys::processI386OnArm64(0, 0xaa64, 4));
  CHECK(!sys::processI386OnArm64(0x014c, 0x8664, 4));
  CHECK(!sys::processI386OnArm64(0x014c, 0x014c, 4));
  CHECK(!sys::processI386OnArm64(0xaa64, 0xaa64, 8));
  CHECK(!sys::processI386OnArm64(0x014c, 0xaa64, 8));

  sys::FileObservation logical;
  logical.complete = logical.unchanged = true; logical.key = {17, 2, 3, 0, 8192}; logical.pe = pe;
  logical.hash.fill(0x67); logical.finalNt = L"\\Device\\HarddiskVolume3\\Windows\\SyChpe32\\gdi32.dll";
  const auto explicitI386 = logical; const std::wstring mapped = logical.finalNt;
  CHECK(sys::joinsMappedImage(pe, mapped, logical, explicitI386));
  { auto alias = explicitI386; alias.finalNt = L"\\device\\harddiskvolume3\\windows\\sychpe32\\GDI32.DLL";
    CHECK(sys::joinsMappedImage(pe, mapped, logical, alias)); }
  CHECK(!sys::joinsMappedImage(pe, L"", logical, explicitI386));
  CHECK(!sys::joinsMappedImage(pe, L"\\Device\\HarddiskVolume3\\Windows\\SysWOW64\\gdi32.dll", logical, explicitI386));
  CHECK(!sys::joinsMappedImage(pe, mapped + L".redirected", logical, explicitI386));
  const auto reject = [&](const sys::FileObservation& changed) {
    CHECK(!sys::joinsMappedImage(pe, mapped, logical, changed));
    CHECK(!sys::joinsMappedImage(pe, mapped, changed, explicitI386));
  };
  { auto value = logical; value.complete = false; reject(value); }
  { auto value = logical; value.unchanged = false; reject(value); }
  { auto value = logical; value.finalNt.clear(); reject(value); }
  { auto value = logical; value.finalNt += L".another"; reject(value); }
  { auto value = logical; ++value.key.volume; reject(value); }
  { auto value = logical; ++value.key.high; reject(value); }
  { auto value = logical; ++value.key.low; reject(value); }
  { auto value = logical; ++value.key.bytesHigh; reject(value); }
  { auto value = logical; ++value.key.bytesLow; reject(value); }
  for (const auto offset : {0u, 15u, 31u}) { auto value = logical; value.hash[offset] ^= 1; reject(value); }
  { auto value = logical; ++value.pe.offset; reject(value); }
  { auto value = logical; ++value.pe.timestamp; reject(value); }
  { auto value = logical; ++value.pe.sections; reject(value); }
  { auto value = logical; ++value.pe.imageBytes; reject(value); }
  { auto value = logical; ++value.pe.headerBytes; reject(value); }
  { auto value = logical; ++value.pe.checksum; reject(value); }
  { auto value = logical; ++value.pe.entry; reject(value); }
  { auto value = pe; value.machine = 0xaa64; CHECK(!sys::joinsMappedImage(value, mapped, logical, explicitI386)); }
  { auto value = pe; value.magic = 0x020b; CHECK(!sys::joinsMappedImage(value, mapped, logical, explicitI386)); }
  std::printf("D3D8 system image identity fixture PASS checks=%u runtime_calls=0 KMT_calls=0 core_loads=0\n", checks);
}
