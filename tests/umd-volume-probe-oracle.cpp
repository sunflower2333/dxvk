// SPDX-License-Identifier: MIT
#include "umd-volume-probe-oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>

namespace oracle = dxvk::umd::probe::volume;
namespace {
unsigned checks, voxels, bitFlips, sampled;
void check(bool value) {
  ++checks;
  if (!value) { std::fprintf(stderr, "volume probe oracle failed check=%u\n", checks); std::abort(); }
}

// Flat indices and explicit logical mip dimensions form an independent
// scalar-byte reference for the accepted volume fixture's coordinate colors.
uint32_t color(unsigned mip, unsigned x, unsigned y, unsigned z) {
  return 0xff000000u + mip * 0x100000u + z * 0x1000u + y * 64u + x;
}
struct Shape { unsigned width, height, depth; };
constexpr std::array<Shape, 4> NPOT = {{{9, 5, 7}, {4, 2, 3}, {2, 1, 1}, {1, 1, 1}}};
constexpr std::array<Shape, 1> Dynamic = {{{7, 3, 5}}};
constexpr std::array<Shape, 3> Clear = {{{8, 4, 8}, {4, 2, 4}, {2, 1, 2}}};
constexpr std::array<Shape, 4> Mips = {{{8, 8, 8}, {4, 4, 4}, {2, 2, 2}, {1, 1, 1}}};

template<size_t N, typename Scalar>
void verify(oracle::Words values, const std::array<Shape, N>& shapes, Scalar&& scalar) {
  check(values.size() == N);
  for (unsigned mip = 0; mip < N; ++mip) {
    const Shape shape = shapes[mip];
    check(values[mip].size() == size_t(shape.width) * shape.height * shape.depth);
    const auto expected = values[mip];
    check(oracle::matches(values[mip], expected));
    for (unsigned i = 0; i < values[mip].size(); ++i) {
      const unsigned x = i % shape.width;
      const unsigned y = (i / shape.width) % shape.height;
      const unsigned z = i / (shape.width * shape.height);
      check(values[mip][i] == scalar(mip, x, y, z)); ++voxels;
      for (unsigned bit = 0; bit < 32; ++bit) {
        values[mip][i] ^= uint32_t(1) << bit;
        check(!oracle::matches(values[mip], expected));
        values[mip][i] ^= uint32_t(1) << bit; ++bitFlips;
      }
    }
    values[mip].pop_back(); check(!oracle::matches(values[mip], expected));
    values[mip] = expected; values[mip].push_back(0); check(!oracle::matches(values[mip], expected));
  }
}
} // namespace

int main() {
  verify(oracle::initial({9, 5, 7}, 4), NPOT, color);
  verify(oracle::transferred(), NPOT, [](unsigned mip, unsigned x, unsigned y, unsigned z) {
    if (!mip && x >= 5 && x < 8 && y >= 2 && y < 4 && z >= 4 && z < 6)
      return color(0, x - 4, y - 1, z - 3);
    if (!mip && x >= 2 && x < 5 && y >= 1 && y < 3 && z >= 2 && z < 5)
      return color(0, x - 2, y - 1, z - 2);
    return color(mip, x, y, z);
  });
  for (unsigned round = 0; round < 3; ++round)
    verify(oracle::dynamic(round), Dynamic, [round](unsigned mip, unsigned x, unsigned y, unsigned z) {
      return color(mip, x, y, z) ^ (round * 0x10101u);
    });
  verify(oracle::cleared(), Clear, [](unsigned mip, unsigned x, unsigned y, unsigned z) {
    if (mip == 1 && z == 1) return uint32_t(0xff0000ffu);
    if (mip == 1 && z >= 2) return uint32_t(0xff00ff00u);
    return color(mip, x, y, z);
  });
  for (unsigned count : {UINT32_MAX, 2u, 1u}) {
    verify(oracle::generated(count), Mips, [count](unsigned mip, unsigned x, unsigned y, unsigned z) {
      if (mip == 1 || (mip >= 2 && (count == UINT32_MAX || mip <= count))) return uint32_t(0xff00ffffu);
      return color(mip, x, y, z);
    });
    // Every SRV-relative mip exercised by the hardware shader is included.
    const unsigned levels = count == UINT32_MAX ? 3 : count;
    for (unsigned mip = 1; mip <= levels; ++mip)
      sampled += Mips[mip].width * Mips[mip].height * Mips[mip].depth;
  }
  for (Shape shape : NPOT) sampled += shape.width * shape.height * shape.depth;
  check(voxels == 3046 && bitFlips == 97472 && sampled == 551);
  std::printf("volume probe oracle verified checks=%u voxels=%u bit_flips=%u sampled=%u\n", checks, voxels, bitFlips, sampled);
}
