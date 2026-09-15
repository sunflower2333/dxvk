// SPDX-License-Identifier: MIT
#include "../src/umd/umd_transfer_policy.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

static unsigned checks;
// Keep verdicts enabled under NDEBUG and preserve the exact failing oracle.
static void check(bool condition, unsigned line) {
  ++checks;
  if (!condition) { std::fprintf(stderr, "FAIL upload span line=%u\n", line); std::exit(1); }
}
#define CHECK(c) check(!!(c), __LINE__)

// Test real production arithmetic against a small independently bounded oracle.
int main() {
  using dxvk::umd::uploadSpan;
  for (uint32_t bpp : {1u,2u,4u,8u,12u,16u}) {
    for (uint32_t width = 0; width < 10; ++width)
      for (uint32_t height = 0; height < 6; ++height)
        for (uint32_t pitch = 0; pitch < 180; pitch += 3)
          for (uint64_t limit : {0ull,31ull,255ull,1024ull}) {
            uint64_t result = 0xabcdef;
            const uint64_t bytes = height ? uint64_t(height-1)*pitch + width*bpp : 0;
            const bool valid = width && height && pitch >= width*bpp && bytes <= limit;
            CHECK(uploadSpan(width,height,bpp,pitch,true,limit,result) == valid);
            CHECK(result == (valid ? bytes : 0xabcdef));
          }
    uint64_t result = 0;
    CHECK(uploadSpan(8,1,bpp,0,false,128,result) && result == 8*bpp);
    CHECK(!uploadSpan(8,2,bpp,0,false,128,result));
  }
  uint64_t result = 0;
  const uint64_t max32 = UINT32_MAX;
  CHECK(!uploadSpan(16,3,4,UINT32_MAX,true,max32,result));
  CHECK(uploadSpan(16,2,4,UINT32_MAX-64,true,max32,result) && result == max32);
  CHECK(!uploadSpan(16,2,4,UINT32_MAX-63,true,max32,result));
  CHECK(uploadSpan(16,2,4,UINT32_MAX,true,UINT64_MAX,result) && result == max32+64);
  CHECK(uploadSpan(1,1,1,UINT32_MAX,true,1,result) && result == 1);
  CHECK(!uploadSpan(UINT32_MAX,1,16,0,false,max32,result));
  CHECK(uploadSpan(UINT32_MAX,1,16,0,false,UINT64_MAX,result) && result == max32*16);
  CHECK(!uploadSpan(1,1,0,4,true,64,result));
  CHECK(!uploadSpan(2,1,4,8,true,7,result));
  CHECK(uploadSpan(2,1,4,8,true,8,result) && result == 8);
  std::printf("PASS upload span: %u checks; explicit 32-bit and 64-bit limits\n", checks);
  std::puts("BACKEND=none; GPU_ACCEPTANCE=NOT_RUN");
}
