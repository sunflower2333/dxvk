#pragma once
// SPDX-License-Identifier: MIT
#include "umd_transfer_policy.h"

namespace dxvk::umd {

struct BlockBox2D { uint32_t left, top, right, bottom; };

// Copy complete encoded blocks. An unaligned source end denotes the logical
// mip edge; its physical block (including unused edge texels) is still copied.
// Destination origins remain block-aligned and both logical and physical
// extents must fit. Division/remainders avoid overflowing rounded UINT sizes.
inline bool copyBlockRegion2D(uint32_t sourceWidth, uint32_t sourceHeight,
    uint32_t destinationWidth, uint32_t destinationHeight, BlockBox2D box,
    uint32_t x, uint32_t y, uint32_t blockBytes) {
  if (!sourceWidth || !sourceHeight || !destinationWidth || !destinationHeight
      || (blockBytes != 8 && blockBytes != 16)
      || box.left >= box.right || box.top >= box.bottom
      || box.right > sourceWidth || box.bottom > sourceHeight
      || box.left % 4 || box.top % 4 || x % 4 || y % 4
      || (box.right % 4 && box.right != sourceWidth)
      || (box.bottom % 4 && box.bottom != sourceHeight)) return false;
  const uint32_t width = box.right - box.left, height = box.bottom - box.top;
  if (x > destinationWidth || y > destinationHeight
      || width > destinationWidth - x || height > destinationHeight - y) return false;
  const auto blocks = [](uint32_t value) { return value / 4 + (value % 4 != 0); };
  const uint32_t columns = blocks(width), rows = blocks(height);
  return columns <= blocks(sourceWidth) - box.left / 4
      && rows <= blocks(sourceHeight) - box.top / 4
      && columns <= blocks(destinationWidth) - x / 4
      && rows <= blocks(destinationHeight) - y / 4;
}

// BC1–BC5 store complete 4x4 blocks, including the physical padding of a
// smaller logical mip. An unaligned end is legal only at that mip's edge.
// Reject before the backend can read source data; keep output atomic on error.
inline bool uploadBlockSpan(uint32_t width, uint32_t height, BlockBox2D box,
    uint32_t blockBytes, uint32_t rowPitch, uint64_t addressableBytes,
    uint64_t& requiredBytes) {
  if (!width || !height || (blockBytes != 8 && blockBytes != 16)
      || box.left >= box.right || box.top >= box.bottom
      || box.right > width || box.bottom > height
      || box.left % 4 || box.top % 4
      || (box.right % 4 && box.right != width)
      || (box.bottom % 4 && box.bottom != height)) return false;
  const uint32_t columns = (box.right - box.left) / 4 + ((box.right - box.left) % 4 != 0);
  const uint32_t rows = (box.bottom - box.top) / 4 + ((box.bottom - box.top) % 4 != 0);
  return uploadSpan(columns, rows, blockBytes, rowPitch, true, addressableBytes, requiredBytes);
}

}
