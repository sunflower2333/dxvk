// SPDX-License-Identifier: MIT
#include "../src/umd/umd_uav_texture_policy.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using dxvk::umd::TextureUavDimension;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "UAV shape failure line=%d\n", __LINE__); std::exit(1); } } while (0)

static void compare(uint32_t total, uint32_t first, uint32_t count) {
  // Independently enumerate a bounded view's first and last absolute slices
  // in a wider type; UINT32 wrap cannot turn a bad range into a valid one.
  const uint64_t end = uint64_t(first) + count;
  const bool valid = count && first < total && end <= total;
  const auto dimension = dxvk::umd::textureUavDimension(total, first, count);
  CHECK((dimension != TextureUavDimension::Invalid) == valid);
  if (valid) CHECK(dimension == (total == 1 ? TextureUavDimension::Single : TextureUavDimension::Array));
}

int main() {
  for (uint32_t total = 0; total < 33; ++total)
    for (uint32_t first = 0; first < 36; ++first)
      for (uint32_t count = 0; count < 36; ++count) compare(total, first, count);
  constexpr auto maximum = std::numeric_limits<uint32_t>::max();
  constexpr std::array<uint32_t, 9> edges = {0,1,2,3,16,maximum/2,maximum-2,maximum-1,maximum};
  for (auto total : edges) for (auto first : edges) for (auto count : edges) compare(total, first, count);
  CHECK(dxvk::umd::textureUavDimension(1,0,1) == TextureUavDimension::Single);
  CHECK(dxvk::umd::textureUavDimension(3,2,1) == TextureUavDimension::Array);
  CHECK(dxvk::umd::textureUavDimension(maximum,maximum-1,1) == TextureUavDimension::Array);
  std::printf("D3D11 texture UAV shape policy verified checks=%u\n", checks);
}
