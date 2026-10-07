// SPDX-License-Identifier: Zlib
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace dxvk {

  struct D3D9BufferCopyRange {
    uint32_t sourceOffset = 0;
    uint32_t fullElements = 0;
    uint32_t bytes = 0;
  };

  inline bool appendD3D9BufferCopySize(uint32_t& total, uint32_t bytes) {
    if (bytes > std::numeric_limits<uint32_t>::max() - total)
      return false;
    total += bytes;
    return true;
  }

  inline D3D9BufferCopyRange computeD3D9BufferCopyRange(
          uint32_t bufferSize,
          uint32_t bindingOffset,
          int64_t  firstVertex,
          uint32_t vertexCount,
          uint32_t sourceStride,
          uint32_t vertexSize) {
    D3D9BufferCopyRange range;
    const uint32_t destinationStride = std::min(sourceStride, vertexSize);
    if (!sourceStride || !destinationStride || !vertexCount || firstVertex < 0
        || bindingOffset > bufferSize)
      return range;

    // Bound the vertex offset before multiplying. Both offsets must be
    // accounted for when a final vertex lacks unused stride padding.
    const uint32_t bindingBytes = bufferSize - bindingOffset;
    if (uint64_t(firstVertex) > bindingBytes / sourceStride)
      return range;

    range.sourceOffset = bindingOffset + uint32_t(firstVertex) * sourceStride;
    const uint32_t remaining = bufferSize - range.sourceOffset;
    range.fullElements = std::min(vertexCount, remaining / sourceStride);
    range.bytes = range.fullElements * destinationStride;
    if (range.fullElements < vertexCount)
      range.bytes += std::min(destinationStride, remaining % sourceStride);
    return range;
  }

}
