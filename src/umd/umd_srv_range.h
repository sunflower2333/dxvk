// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace dxvk::umd {

// D3D10DDIARG_TEX2D_SHADERRESOURCEVIEW uses UINT(-1) for the mip or
// array-slice count remaining after the first index. Resolve before forming
// the backend descriptor, without addition that can wrap a runtime input.
inline constexpr bool shaderResourceViewRange(uint32_t first, uint32_t count,
    uint32_t total, uint32_t& resolved) noexcept {
  if (first >= total || !count) return false;
  const uint32_t remaining = total - first;
  const uint32_t finite = count == ~uint32_t{0} ? remaining : count;
  if (finite > remaining) return false;
  resolved = finite;
  return true;
}

}
