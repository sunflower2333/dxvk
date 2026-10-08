#pragma once
// SPDX-License-Identifier: MIT
#include "umd_transfer_policy.h"

namespace dxvk::umd {

struct BlockBox2D { uint32_t left, top, right, bottom; };

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
