// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace dxvk::umd::probe::cube {
using Words = std::vector<std::vector<uint32_t>>;
struct Shape { unsigned edge, levels, faces; };

inline unsigned extent(Shape shape, unsigned mip) {
  const unsigned size = shape.edge >> mip;
  return size ? size : 1;
}
inline uint32_t bits(unsigned integer) {
  const float value = static_cast<float>(integer);
  uint32_t result;
  std::memcpy(&result, &value, sizeof(result));
  return result;
}
inline unsigned number(unsigned face, unsigned mip, unsigned x, unsigned y, unsigned edge, bool constant) {
  return constant ? 100 * face + 10 * mip + 1 : 1000 + 100 * face + 10 * mip + y * edge + x;
}
inline Words initial(Shape shape, bool constant = false) {
  Words result(shape.faces * shape.levels);
  for (unsigned face = 0; face < shape.faces; ++face) for (unsigned mip = 0; mip < shape.levels; ++mip) {
    const unsigned edge = extent(shape, mip);
    auto& row = result[face * shape.levels + mip];
    row.resize(edge * edge);
    for (unsigned y = 0; y < edge; ++y) for (unsigned x = 0; x < edge; ++x)
      row[y * edge + x] = bits(number(face, mip, x, y, edge, constant));
  }
  return result;
}
inline Words transferred() {
  auto result = initial({7, 3, 12});
  for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x)
    result[7 * 3 + 1][y * 3 + x] = bits(123456);
  for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x)
    result[11 * 3][(2 + y) * 7 + 3 + x] = result[0][(1 + y) * 7 + 1 + x];
  return result;
}
inline Words generated(unsigned firstFace, unsigned count) {
  auto result = initial({8, 4, 12}, true);
  const unsigned lastMip = count == UINT32_MAX ? 4 : 1 + count;
  for (unsigned face = firstFace; face < firstFace + 6; ++face)
    for (unsigned mip = 2; mip < lastMip; ++mip)
      for (auto& word : result[face * 4 + mip]) word = bits(number(face, 1, 0, 0, 4, true));
  return result;
}
inline std::vector<uint32_t> sampled(Shape shape, const Words& words, unsigned firstFace,
    unsigned cubes, unsigned firstMip, unsigned relativeMip) {
  const unsigned mip = firstMip + relativeMip, edge = extent(shape, mip);
  std::vector<uint32_t> result(cubes * 6);
  for (unsigned i = 0; i < result.size(); ++i)
    result[i] = words[(firstFace + i) * shape.levels + mip][(edge / 2) * edge + edge / 2];
  return result;
}
inline bool matches(const std::vector<uint32_t>& actual, const std::vector<uint32_t>& expected) {
  if (actual.size() != expected.size()) return false;
  uint32_t difference = 0;
  for (unsigned i = 0; i < actual.size(); ++i) difference |= actual[i] ^ expected[i];
  return difference == 0;
}
} // namespace dxvk::umd::probe::cube
