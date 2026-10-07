// SPDX-License-Identifier: Zlib
#include "../src/d3d9/d3d9_buffer_copy.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

static unsigned checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
  std::fprintf(stderr, "buffer copy check failed line=%d: %s\n", __LINE__, #expr); \
  std::exit(1); } } while (0)

static void verify(uint32_t size, uint32_t offset, int64_t first,
    uint32_t count, uint32_t stride, uint32_t extent) {
  const auto range = dxvk::computeD3D9BufferCopyRange(size, offset, first, count, stride, extent);
  const auto packedStride = std::min(stride, extent);
  std::vector<uint8_t> source(size);
  for (size_t i = 0; i < source.size(); ++i)
    source[i] = uint8_t(i * 73 + 11);

  // Enumerate the requested scalar bytes independently of the copy planner.
  std::vector<uint8_t> expected;
  if (first >= 0 && stride && packedStride) {
    for (uint32_t vertex = 0; vertex < count; ++vertex) {
      const auto position = uint64_t(offset) + (uint64_t(first) + vertex) * stride;
      for (uint32_t byte = 0; byte < packedStride; ++byte) {
        if (position + byte >= source.size()) break;
        expected.push_back(source[size_t(position + byte)]);
      }
    }
  }
  CHECK(range.bytes == expected.size());
  CHECK(range.bytes <= source.size());
  CHECK(range.fullElements <= count);
  std::vector<uint8_t> actual(range.bytes + 2, 0xa5);
  if (range.bytes) {
    CHECK(range.sourceOffset < source.size());
    for (uint32_t i = 0; i < range.fullElements; ++i) {
      CHECK(uint64_t(range.sourceOffset) + uint64_t(i) * stride + packedStride <= size);
      std::memcpy(actual.data() + 1 + i * packedStride,
        source.data() + range.sourceOffset + i * stride, packedStride);
    }
    const uint32_t tail = range.bytes - range.fullElements * packedStride;
    if (tail) {
      CHECK(uint64_t(range.sourceOffset) + uint64_t(range.fullElements) * stride + tail <= size);
      std::memcpy(actual.data() + 1 + range.fullElements * packedStride,
        source.data() + range.sourceOffset + range.fullElements * stride, tail);
    }
  }
  CHECK(actual.front() == 0xa5 && actual.back() == 0xa5);
  CHECK(std::equal(expected.begin(), expected.end(), actual.begin() + 1));
}

int main() {
  // Valid DDI draws with a full last declaration and no unused tail padding.
  const auto exact = dxvk::computeD3D9BufferCopyRange(60, 0, 1, 3, 16, 12);
  CHECK(exact.sourceOffset == 16 && exact.fullElements == 2 && exact.bytes == 36);
  verify(60, 0, 1, 3, 16, 12);
  verify(124, 4, 3, 3, 20, 16);
  verify(44, 4, 3, 3, 8, 4);
  verify(60, 16, 0, 3, 16, 12);
  verify(60, 16, 1, 2, 16, 12);
  verify(60, 0, -1, 3, 16, 12);
  for (uint32_t size = 0; size <= 40; ++size)
    for (uint32_t offset = 0; offset <= 44; offset += 4)
      for (int64_t first = -1; first <= 4; ++first)
        for (uint32_t count = 0; count <= 5; ++count)
          for (uint32_t stride : {0u, 1u, 3u, 4u, 8u, 16u})
            for (uint32_t extent : {0u, 1u, 3u, 4u, 8u, 12u, 20u})
              verify(size, offset, first, count, stride, extent);

  const auto maximum = std::numeric_limits<uint32_t>::max();
  CHECK(dxvk::computeD3D9BufferCopyRange(maximum, 0, maximum, maximum, maximum, 16).bytes == 0);
  CHECK(dxvk::computeD3D9BufferCopyRange(60, maximum, 0, 3, 16, 12).bytes == 0);
  CHECK(dxvk::computeD3D9BufferCopyRange(60, 0, int64_t(maximum) + maximum, 3, maximum, 12).bytes == 0);
  const auto wide = dxvk::computeD3D9BufferCopyRange(maximum, 0, 0, maximum, 1, 1);
  CHECK(wide.sourceOffset == 0 && wide.fullElements == maximum && wide.bytes == maximum);
  const auto tail = dxvk::computeD3D9BufferCopyRange(maximum, 0, 1, maximum, 16, 12);
  CHECK(tail.sourceOffset == 16 && tail.fullElements == 268435454u && tail.bytes == 3221225460u);
  std::printf("D3D9 vertex copy PASS checks=%u; bounded source offsets/partial tails/overflow, no GPU\n", checks);
}
