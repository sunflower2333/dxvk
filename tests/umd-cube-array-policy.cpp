// SPDX-License-Identifier: MIT
#include "../src/umd/umd_cube_array_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

static unsigned checks;
static void check(bool value, unsigned line) {
  ++checks;
  if (!value) { std::fprintf(stderr, "FAIL cube-array policy line=%u\n", line); std::exit(1); }
}
#define CHECK(x) check(!!(x), __LINE__)

int main() {
  // Build an independent face-count set by appending complete six-face cubes,
  // and derive mip counts from an actual integer edge sequence.
  std::array<bool, 541> wholeCubes{};
  unsigned faceCount = 0;
  for (unsigned cube = 0; cube < 85; ++cube) {
    for (unsigned face = 0; face < 6; ++face) ++faceCount;
    wholeCubes[faceCount] = true;
  }
  for (uint32_t edge : {0u,1u,2u,3u,7u,15u,32u,8192u,8193u,UINT32_MAX}) {
    std::vector<uint32_t> levels;
    if (edge && edge <= 8192) {
      uint32_t extent = edge;
      for (;;) {
        levels.push_back(extent);
        if (extent == 1) break;
        extent /= 2;
      }
    }
    for (uint32_t mips = 0; mips < 18; ++mips)
      for (uint32_t faces = 0; faces <= 540; ++faces)
        CHECK(dxvk::umd::cubeArray10_1Shape(edge,mips,faces)
          == (mips && mips <= levels.size() && wholeCubes[faces]));
  }
  for (uint32_t faces : {UINT32_MAX,UINT32_MAX-1,UINT32_MAX-3,2046u})
    CHECK(!dxvk::umd::cubeArray10_1Shape(1,1,faces));
  CHECK(dxvk::umd::cubeArray10_1Shape(8192,14,510));
  CHECK(!dxvk::umd::cubeArray10_1Shape(8192,15,510));
  CHECK(!dxvk::umd::cubeArray10_1Shape(8192,14,512));
  std::printf("PASS cube-array policy: %u checks; independent complete-face and mip bounds\n",checks);
  std::puts("BACKEND=none; GPU_ACCEPTANCE=NOT_RUN");
}
