#include "../src/umd/umd_identity.h"
#include <cstdio>

int main() {
  using namespace dxvk::umd;
  const AdapterLuid luid = {0x12, 0, 0x80, 0, 0, 0, 0x34, 0xff};
  unsigned cases = 0;
  auto expect = [&](bool value, bool wanted) {
    cases++;
    if (value != wanted) {
      std::fprintf(stderr, "identity case %u failed\n", cases);
      return false;
    }
    return true;
  };
  if (!expect(matchesAdapter(luid, true, luid, 18, 18), true) ||
      !expect(matchesAdapter({}, true, {}, 18, 18), false) ||
      !expect(matchesAdapter(luid, false, luid, 18, 18), false) ||
      !expect(matchesAdapter(luid, true, luid, 18, 13), false) ||
      !expect(matchesAdapter(luid, true, luid, 0, 0), false))
    return 1;
  for (size_t i = 0; i < luid.size(); i++)
    for (unsigned bit = 0; bit < 8; bit++) {
      auto other = luid;
      other[i] ^= uint8_t(1u << bit);
      if (!expect(matchesAdapter(luid, true, other, 18, 18), false))
        return 1;
    }
  std::printf("identity PASS cases=%u\n", cases);
}
