#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

struct SampleCopyShape {
  uint32_t width, height, samples, quality;
};

// The existing scalar region path validates its own box and destination
// extent. Multisampled color images only enter as complete equal-size
// subresources with a null box, zero offsets and identical sample layout.
constexpr bool sampleRegionCopyContract(SampleCopyShape source,
    SampleCopyShape destination, bool boxed, uint32_t x, uint32_t y, uint32_t z) {
  if (!source.samples || source.samples != destination.samples) return false;
  if (source.samples == 1) return true; // Single-sample quality is ignored.
  return source.quality == destination.quality && !boxed && !x && !y && !z
      && source.width && source.height && source.width == destination.width
      && source.height == destination.height;
}

}
