// SPDX-License-Identifier: MIT
#include "../src/umd/umd_volume_policy.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

static unsigned checks;
static void check(bool value, unsigned line) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL volume policy line=%u\n", line); std::exit(1); }
}
#define CHECK(x) check(!!(x), __LINE__)

int main() {
  using namespace dxvk::umd;
  // Independent small integer oracle includes row and slice padding, final
  // byte boundaries, zero pitches, and zero dimensions. No Windows API runs.
  for (uint32_t bytes : {1u, 2u, 4u, 8u, 12u, 16u})
    for (uint32_t w = 0; w < 5; ++w)
      for (uint32_t h = 0; h < 4; ++h)
        for (uint32_t d = 0; d < 4; ++d)
          for (uint32_t row = 0; row < 40; row += 6)
            for (uint32_t slice = 0; slice < 180; slice += 13)
              for (uint64_t limit : {0ull, 31ull, 255ull, 1024ull}) {
                const uint64_t tail = h ? uint64_t(h - 1) * row + w * bytes : 0;
                const uint64_t total = d ? uint64_t(d - 1) * slice + tail : 0;
                const bool valid = w && h && d && row >= w * bytes
                  && (d == 1 || slice >= tail) && total <= limit;
                uint64_t result = 0xabcdef;
                CHECK(uploadVolumeSpan({w,h,d}, bytes, row, slice, limit, result) == valid);
                CHECK(result == (valid ? total : 0xabcdef));
              }
  uint64_t result = 0xabcdef;
  CHECK(uploadVolumeSpan({2,3,2}, 4, 16, 48, 88, result) && result == 88);
  CHECK(!uploadVolumeSpan({2,3,2}, 4, 16, 48, 87, result) && result == 88);
  CHECK(!uploadVolumeSpan({2,3,2}, 4, 16, 39, 1024, result));
  CHECK(uploadVolumeSpan({2,3,1}, 4, 16, 0, 40, result) && result == 40);
  CHECK(uploadVolumeSpan({1,1,2}, 1, 1, UINT32_MAX, uint64_t(UINT32_MAX)+1, result));
  CHECK(!uploadVolumeSpan({1,1,2}, 1, 1, UINT32_MAX, UINT32_MAX, result));
  CHECK(!uploadVolumeSpan({UINT32_MAX, UINT32_MAX, UINT32_MAX}, 16,
    UINT32_MAX, UINT32_MAX, UINT64_MAX, result));
  CHECK(!uploadVolumeSpan({1,UINT32_MAX,2}, 1, UINT32_MAX, UINT32_MAX, UINT64_MAX, result));

  for (uint32_t w : {1u, 7u, 16u, 2048u, UINT32_MAX})
    for (uint32_t h : {1u, 5u, 8u, 1024u})
      for (uint32_t d : {1u, 3u, 32u, 2048u}) {
        uint64_t ew=w, eh=h, ed=d;
        for (uint32_t mip=0; mip<32; ++mip) {
          VolumeExtent out;
          CHECK(volumeMipExtent({w,h,d}, mip, out));
          CHECK(out.width == ew && out.height == eh && out.depth == ed);
          ew=std::max(uint64_t(1), ew/2); eh=std::max(uint64_t(1), eh/2); ed=std::max(uint64_t(1), ed/2);
        }
      }
  VolumeExtent untouched{7,8,9};
  CHECK(!volumeMipExtent({1,1,1}, 32, untouched));
  CHECK(!volumeMipExtent({0,1,1}, 0, untouched));
  CHECK(untouched.width == 7 && untouched.height == 8 && untouched.depth == 9);
  for (uint32_t x : {0u,1u,7u,UINT32_MAX})
    for (uint32_t y : {0u,2u,5u,UINT32_MAX})
      for (uint32_t z : {0u,1u,3u,UINT32_MAX})
        for (uint32_t w : {0u,1u,7u,UINT32_MAX})
          for (uint32_t h : {0u,1u,5u,UINT32_MAX})
            for (uint32_t d : {0u,1u,3u,UINT32_MAX})
              CHECK(volumeCopyFits({7,5,3},x,y,z,{w,h,d})
                == (uint64_t(x)+w<=7 && uint64_t(y)+h<=5 && uint64_t(z)+d<=3));
  std::printf("PASS volume policy: %u checks; independent padded volume and xyz bounds\n", checks);
  std::puts("BACKEND=none; GPU_ACCEPTANCE=NOT_RUN");
}
