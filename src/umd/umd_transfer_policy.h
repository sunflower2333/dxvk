#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

// Bound the last byte actually read, not rowPitch * height (which includes
// unused trailing padding). This proves arithmetic bounds, not pointer validity.
inline bool uploadSpan(
    uint32_t width, uint32_t height, uint32_t texelBytes,
    uint32_t rowPitch, bool twoDimensional,
    uint64_t addressableBytes, uint64_t& requiredBytes) {
  if (!width || !height || !texelBytes || (!twoDimensional && height != 1))
    return false;
  const uint64_t rowBytes = uint64_t(width) * texelBytes;
  if (rowBytes > addressableBytes || (twoDimensional && rowPitch < rowBytes))
    return false;
  const uint64_t remainingRows = height - 1;
  if (remainingRows && remainingRows > (addressableBytes - rowBytes) / rowPitch)
    return false;
  requiredBytes = remainingRows * rowPitch + rowBytes;
  return true;
}

}
