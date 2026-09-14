#pragma once
// SPDX-License-Identifier: MIT
#include "umd_shader.h"
#include <cstdint>

namespace dxvk::umd {

inline constexpr uint32_t colorOutputSlots = 8;

// Validate the complete native float PS signature without compacting its slots.
inline bool validFloatColorOutputs(const ShaderSignatureEntry* outputs, size_t count) {
  if (!outputs || !count || count > colorOutputSlots) return false;
  uint32_t seen = 0;
  for (size_t i = 0; i < count; i++) {
    const auto& entry = outputs[i];
    if (entry.systemValue || entry.registerIndex >= colorOutputSlots ||
        !entry.mask || (entry.mask & ~15) ||
        (entry.scalar != ShaderScalar::Unknown && entry.scalar != ShaderScalar::Float32))
      return false;
    const uint32_t slot = 1u << entry.registerIndex;
    if (seen & slot) return false;
    seen |= slot;
  }
  return true;
}

// Use subtraction for hostile counts so UINT_MAX cannot wrap past the bound.
inline bool validRenderTargetRange(uint32_t count, uint32_t clear) {
  return count <= colorOutputSlots && clear <= colorOutputSlots - count;
}

struct OutputShape {
  uint32_t width = 0, height = 0, layers = 0, samples = 0, quality = 0;
};

// Require every non-null color/depth view to have compatible effective dimensions.
inline bool mergeOutputShape(OutputShape& previous, const OutputShape& candidate) {
  if (!candidate.width || !candidate.height || !candidate.layers || !candidate.samples)
    return false;
  if (!previous.width) {
    previous = candidate;
    return true;
  }
  return previous.width == candidate.width && previous.height == candidate.height &&
      previous.layers == candidate.layers && previous.samples == candidate.samples &&
      previous.quality == candidate.quality;
}

// Inputs are individually validated nonempty slice ranges; avoid end overflow.
inline bool outputRangesOverlap(uint32_t firstA, uint32_t sizeA, uint32_t firstB, uint32_t sizeB) {
  return firstA <= firstB ? firstB - firstA < sizeA : firstA - firstB < sizeB;
}

}
