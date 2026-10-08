// SPDX-License-Identifier: MIT
#include "../src/umd/umd_srv_range.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
  std::fprintf(stderr, "SRV range failure line=%d expression=%s\n", __LINE__, #x); std::exit(1); \
} } while (0)

static void verify(uint32_t first, uint32_t count, uint32_t total) {
  constexpr uint32_t sentinel = UINT32_MAX;
  struct { uint32_t before, output, after; } guarded{0x176abcde, 0x981abcde, 0x45bcdeaf};
  // A separate 64-bit interval oracle does not use the production subtraction.
  const uint64_t end = count == sentinel ? uint64_t(total) : uint64_t(first) + count;
  const bool valid = count && uint64_t(first) < total && end <= total;
  const bool accepted = dxvk::umd::shaderResourceViewRange(first, count, total, guarded.output);
  CHECK(accepted == valid);
  CHECK(guarded.output == (valid ? uint32_t(end - first) : 0x981abcde));
  CHECK(guarded.before == 0x176abcde && guarded.after == 0x45bcdeaf);
}

int main() {
  for (uint32_t total = 0; total <= 48; ++total)
    for (uint32_t first = 0; first <= 52; ++first) {
      for (uint32_t count = 0; count <= 52; ++count) verify(first, count, total);
      verify(first, UINT32_MAX, total); verify(first, UINT32_MAX - 1, total);
    }
  const std::array<uint32_t, 14> boundaries{0, 1, 2, 3, 15, 16, 31, 32,
    2048, 16384, 0x7fffffff, 0x80000000, UINT32_MAX - 1, UINT32_MAX};
  for (auto total : boundaries)
    for (auto first : boundaries)
      for (auto count : boundaries) verify(first, count, total);
  std::printf("native SRV remaining range policy verified checks=%u\n", checks);
}
