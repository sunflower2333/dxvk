#pragma once
// SPDX-License-Identifier: MIT
#include "umd_transfer_policy.h"

namespace dxvk::umd {

struct BlockBox2D { uint32_t left, top, right, bottom; };

inline uint64_t physicalBlockExtent(uint32_t logicalExtent) {
  return (uint64_t(logicalExtent) + 3) / 4 * 4;
}

// D3D resource manipulation uses complete 4x4 physical blocks, including a
// lower mip's padding. Shader dimensions remain logical. Use wide rounded
// bounds and subtraction in block units without overflowing UINT dimensions.
inline bool copyBlockRegion2D(uint32_t sourceWidth, uint32_t sourceHeight,
    uint32_t destinationWidth, uint32_t destinationHeight, BlockBox2D box,
    uint32_t x, uint32_t y, uint32_t blockBytes) {
  if (!sourceWidth || !sourceHeight || !destinationWidth || !destinationHeight
      || (blockBytes != 8 && blockBytes != 16)
      || box.left >= box.right || box.top >= box.bottom
      || box.right > physicalBlockExtent(sourceWidth) || box.bottom > physicalBlockExtent(sourceHeight)
      || box.left % 4 || box.top % 4 || box.right % 4 || box.bottom % 4 || x % 4 || y % 4) return false;
  const auto blocks = [](uint32_t value) { return value / 4 + (value % 4 != 0); };
  if (x / 4 >= blocks(destinationWidth) || y / 4 >= blocks(destinationHeight)) return false;
  const uint32_t columns = (box.right - box.left) / 4, rows = (box.bottom - box.top) / 4;
  return columns <= blocks(sourceWidth) - box.left / 4
      && rows <= blocks(sourceHeight) - box.top / 4
      && columns <= blocks(destinationWidth) - x / 4
      && rows <= blocks(destinationHeight) - y / 4;
}

// BC1–BC5 store complete 4x4 blocks, including the physical padding of a
// smaller logical mip. Explicit update boxes use aligned physical bounds.
// Reject before the backend can read source data; keep output atomic on error.
inline bool uploadBlockSpan(uint32_t width, uint32_t height, BlockBox2D box,
    uint32_t blockBytes, uint32_t rowPitch, uint64_t addressableBytes,
    uint64_t& requiredBytes) {
  if (!width || !height || (blockBytes != 8 && blockBytes != 16)
      || box.left >= box.right || box.top >= box.bottom
      || box.right > physicalBlockExtent(width) || box.bottom > physicalBlockExtent(height)
      || box.left % 4 || box.top % 4 || box.right % 4 || box.bottom % 4) return false;
  const uint32_t columns = (box.right - box.left) / 4;
  const uint32_t rows = (box.bottom - box.top) / 4;
  return uploadSpan(columns, rows, blockBytes, rowPitch, true, addressableBytes, requiredBytes);
}

}
