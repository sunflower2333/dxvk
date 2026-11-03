#include "../src/umd/umd_native_surface_wire.h"
#include <cstdio>
#include <cstdlib>

using namespace dxvk::umd;
static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "native surface wire line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)
int main() {
  NativeSurfaceInfo value;
  value.key = 23; value.size = 25346048; value.generation = 7; value.resource = 83;
  value.width = 3040; value.height = 1904; value.fourcc = 0x34325241;
  value.stride = 13312; value.planes = value.layoutFlags = 1;
  auto valid = [](const NativeSurfaceInfo& v) { return validNativeSurface(v, 7, 3040, 1904, 0x34325241); };
  CHECK(valid(value));
  CHECK(!validNativeSurface(value, 8, 3040, 1904, 0x34325241));
  CHECK(!validNativeSurface(value, 7, 3040, 0, 0x34325241));
  for (unsigned n = 0; n < 26; ++n) {
    auto bad = value;
    switch (n) {
      case 0: bad.magic ^= 1; break; case 1: bad.version = 1; break;
      case 2: --bad.headerSize; break; case 3: bad.reserved = 1; break;
      case 4: bad.opcode = 9; break; case 5: bad.flags = 1; break;
      case 6: bad.expected = 7; break; case 7: bad.key = 0; break;
      case 8: bad.key = uint64_t(UINT32_MAX) + 1; break; case 9: bad.size = 0; break;
      case 10: --bad.size; break; case 11: bad.generation = 8; break;
      case 12: bad.modifier = 1; break; case 13: bad.offset = 4096; break;
      case 14: bad.resource = 0; break; case 15: bad.context = 1; break;
      case 16: ++bad.width; break; case 17: ++bad.height; break; case 18: ++bad.fourcc; break;
      case 19: bad.stride = 12156; break; case 20: bad.stride = 13313; break;
      case 21: bad.planes = 2; break; case 22: bad.layoutFlags = 3; break;
      case 23: bad.reserved2[0] = 1; break; case 24: bad.reserved2[1] = 1; break;
      case 25: bad.reserved2[2] = 1; break;
    }
    CHECK(!valid(bad));
  }
  auto shortBuffer = value; shortBuffer.size -= 4096; CHECK(!valid(shortBuffer));
  auto bigBuffer = value; bigBuffer.size = 0x100000000ull; CHECK(!valid(bigBuffer));
  NativeResourceShare share; share.key = value.key; share.stride = value.stride;
  CHECK(validNativeResourceShare(share, value.width));
  for (unsigned n = 0; n < 10; ++n) {
    auto bad = share;
    switch (n) {
      case 0: bad.magic ^= 1; break; case 1: ++bad.version; break; case 2: --bad.headerSize; break;
      case 3: bad.reserved = 1; break; case 4: bad.key = 0; break; case 5: bad.key = 0x100000000ull; break;
      case 6: bad.stride = value.width * 4 - 4; break; case 7: bad.flags = 3; break;
      case 8: bad.reserved2[0] = 1; break; case 9: bad.reserved2[1] = 1; break;
    }
    CHECK(!validNativeResourceShare(bad, value.width));
  }
  CHECK(!validNativeResourceShare(share, UINT32_MAX));
  std::printf("Native surface wire PASS checks=%u\n", checks);
}
