#pragma once
#include <array>
#include <cstdint>

namespace dxvk::umd::probe {

constexpr uint32_t ComputeElementCount = 96;
using ComputeElement = std::array<uint32_t, 4>;

inline ComputeElement expectedComputeElement(uint32_t index) {
  const uint32_t x = index % 4;
  const uint32_t y = (index / 4) % 6;
  const uint32_t z = index / 24;
  return {x + (y << 8) + (z << 16),
    x / 2 + ((y / 3) << 8) + ((z / 2) << 16),
    x % 2 + ((y % 3) << 8) + ((z % 2) << 16),
    (z % 2) * 6 + (y % 3) * 2 + x % 2};
}

inline bool computeReadbackMatches(const std::array<ComputeElement, ComputeElementCount>& values) {
  for (uint32_t i = 0; i < ComputeElementCount; ++i)
    if (values[i] != expectedComputeElement(i)) return false;
  return true;
}

}
