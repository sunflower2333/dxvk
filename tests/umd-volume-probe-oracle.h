// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxvk::umd::probe::volume {

struct Extent { unsigned width, height, depth; };
using Words = std::vector<std::vector<uint32_t>>;
inline Extent extent(Extent base, unsigned mip) {
  return {std::max(1u, base.width >> mip), std::max(1u, base.height >> mip), std::max(1u, base.depth >> mip)};
}
inline size_t offset(Extent m, unsigned x, unsigned y, unsigned z) {
  return (size_t(z) * m.height + y) * m.width + x;
}
// Same coordinate/color oracle as the accepted original volume reference:
// unique logical coordinates reveal depth and mip aliasing despite padding.
inline Words initial(Extent base, unsigned levels) {
  Words out(levels);
  for (unsigned mip = 0; mip < levels; ++mip) {
    const auto m = extent(base, mip); out[mip].resize(size_t(m.width) * m.height * m.depth);
    for (unsigned z = 0; z < m.depth; ++z) for (unsigned y = 0; y < m.height; ++y) for (unsigned x = 0; x < m.width; ++x)
      out[mip][offset(m, x, y, z)] = 0xff000000u | (mip << 20) | (z << 12) | (y << 6) | x;
  }
  return out;
}
inline Words transferred() {
  const Extent base{9, 5, 7}, patch{3, 2, 3};
  const auto original = initial(base, 4), values = initial(patch, 1);
  auto expected = original;
  for (unsigned z = 0; z < 3; ++z) for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 3; ++x)
    expected[0][offset(base, x + 2, y + 1, z + 2)] = values[0][offset(patch, x, y, z)];
  for (unsigned z = 0; z < 2; ++z) for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 3; ++x)
    expected[0][offset(base, x + 5, y + 2, z + 4)] = original[0][offset(base, x + 1, y + 1, z + 1)];
  return expected;
}
inline Words dynamic(unsigned round) {
  auto expected = initial({7, 3, 5}, 1);
  for (auto& value : expected[0]) value ^= round * 0x00010101u;
  return expected;
}
inline Words cleared() {
  auto expected = initial({8, 4, 8}, 3);
  const auto m = extent({8, 4, 8}, 1);
  for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 4; ++x)
    expected[1][offset(m, x, y, 1)] = 0xff0000ffu;
  for (unsigned z = 2; z < 4; ++z) for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 4; ++x)
    expected[1][offset(m, x, y, z)] = 0xff00ff00u;
  return expected;
}
inline Words mipInput() {
  auto expected = initial({8, 8, 8}, 4);
  std::fill(expected[1].begin(), expected[1].end(), 0xff00ffffu);
  return expected;
}
inline Words generated(unsigned count) {
  auto expected = mipInput();
  const unsigned end = count == UINT32_MAX ? 4 : 1 + count;
  for (unsigned mip = 2; mip < end; ++mip)
    std::fill(expected[mip].begin(), expected[mip].end(), 0xff00ffffu);
  return expected;
}
inline bool matches(const std::vector<uint32_t>& actual, const std::vector<uint32_t>& expected) {
  return actual == expected;
}

} // namespace dxvk::umd::probe::volume
