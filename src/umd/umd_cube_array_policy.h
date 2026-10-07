#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

// D3D10.1 retains D3D10's texture axis limits. ArraySize counts individual
// faces, so its 512-element limit accommodates at most 85 complete cubes.
constexpr uint32_t cubeArray10_1MaxFaces = 512;
constexpr uint32_t cubeArray10_1MaxEdge = 8192;

inline bool cubeArray10_1Shape(uint32_t edge, uint32_t mips, uint32_t faces) {
  if (!edge || edge > cubeArray10_1MaxEdge || !mips
      || !faces || faces > cubeArray10_1MaxFaces || faces % 6) return false;
  uint32_t maximumMips = 1;
  for (uint32_t extent = edge; extent > 1; extent >>= 1) ++maximumMips;
  return mips <= maximumMips;
}

}
