#pragma once
// SPDX-License-Identifier: MIT
#include "umd_transfer_policy.h"
#include <algorithm>
#include <cstdint>

namespace dxvk::umd {

struct VolumeExtent { uint32_t width = 0, height = 0, depth = 0; };

// A volume subresource is one complete mip, including every depth slice.
inline bool volumeMipExtent(VolumeExtent base, uint32_t mip, VolumeExtent& out) {
  if (!base.width || !base.height || !base.depth || mip >= 32) return false;
  out = {std::max(1u, base.width >> mip), std::max(1u, base.height >> mip),
    std::max(1u, base.depth >> mip)};
  return true;
}

inline bool volumeCopyFits(VolumeExtent destination, uint32_t x, uint32_t y,
    uint32_t z, VolumeExtent source) {
  return x <= destination.width && source.width <= destination.width - x
    && y <= destination.height && source.height <= destination.height - y
    && z <= destination.depth && source.depth <= destination.depth - z;
}

// Validate the last source byte in a padded volume, independently of its base
// address. A depth pitch is needed only when another depth slice is read.
// Leave the caller's output unchanged on every rejection.
inline bool uploadVolumeSpan(VolumeExtent extent, uint32_t texelBytes,
    uint32_t rowPitch, uint32_t depthPitch, uint64_t addressableBytes,
    uint64_t& requiredBytes) {
  if (!extent.depth) return false;
  uint64_t sliceBytes = 0;
  if (!uploadSpan(extent.width, extent.height, texelBytes, rowPitch, true,
      addressableBytes, sliceBytes)) return false;
  const uint64_t remainingSlices = extent.depth - 1;
  if (remainingSlices && (depthPitch < sliceBytes
      || remainingSlices > (addressableBytes - sliceBytes) / depthPitch)) return false;
  requiredBytes = remainingSlices * depthPitch + sliceBytes;
  return true;
}

}
