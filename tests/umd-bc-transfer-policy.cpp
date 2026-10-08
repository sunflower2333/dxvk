// SPDX-License-Identifier: MIT
#include "../src/umd/umd_block_transfer.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"BC policy line=%d\n",__LINE__); std::exit(1); } } while (0)
static void compare(uint32_t w, uint32_t h, dxvk::umd::BlockBox2D box,
    uint32_t bytes, uint32_t pitch, uint64_t accessible) {
  // Independent oracle: count actual compressed row intervals, including only
  // bytes read in the final row. No production ceil/span helper is used here.
  bool expected = w && h && (bytes == 8 || bytes == 16) && box.left < box.right
    && box.top < box.bottom && box.right <= (uint64_t(w)+3)/4*4 && box.bottom <= (uint64_t(h)+3)/4*4
    && (box.left & 3) == 0 && (box.top & 3) == 0
    && (box.right & 3) == 0 && (box.bottom & 3) == 0;
  uint64_t span = 0;
  if (expected) {
    const uint64_t row = uint64_t((uint64_t(box.right) + 3) / 4 - box.left / 4) * bytes;
    const uint64_t rows = (uint64_t(box.bottom) + 3) / 4 - box.top / 4;
    expected = pitch >= row;
    if (expected) { span = (rows - 1) * pitch + row; expected = span <= accessible; }
  }
  uint64_t result = 0xa813734799cba219ull;
  const bool actual = dxvk::umd::uploadBlockSpan(w,h,box,bytes,pitch,accessible,result);
  CHECK(actual == expected);
  CHECK(result == (expected ? span : 0xa813734799cba219ull));
}
int main() {
  for (uint32_t w = 1; w <= 17; ++w) for (uint32_t h = 1; h <= 17; ++h)
    for (uint32_t bytes : {8u,16u}) for (uint32_t left : {0u,1u,4u,8u,16u})
      for (uint32_t top : {0u,1u,4u,8u,16u}) for (uint32_t right : {w,4u,8u,12u,18u})
        for (uint32_t bottom : {h,4u,8u,12u,18u})
          for (uint32_t pitch : {0u,7u,8u,15u,16u,31u,32u,79u})
            compare(w,h,{left,top,right,bottom},bytes,pitch,4096);
  for (uint32_t bytes : {0u,1u,4u,8u,16u,32u})
    for (uint64_t accessible : {uint64_t(0),uint64_t(7),uint64_t(8),uint64_t(15),uint64_t(16),uint64_t(31),
        uint64_t(32),uint64_t(55),uint64_t(56),uint64_t(63),uint64_t(64),uint64_t(UINT64_MAX)}) {
      compare(8,8,{0,0,8,8},bytes,bytes*2+7,accessible);
      compare(UINT32_MAX,UINT32_MAX,{0,0,UINT32_MAX,UINT32_MAX},bytes,UINT32_MAX,accessible);
      compare(UINT32_MAX,UINT32_MAX,{UINT32_MAX-3,UINT32_MAX-3,UINT32_MAX,UINT32_MAX},bytes,32,accessible);
    }
  std::printf("PASS BC block upload policy: checks=%u\n",checks);
}
