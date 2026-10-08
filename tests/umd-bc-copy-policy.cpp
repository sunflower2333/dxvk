// SPDX-License-Identifier: MIT
#include "../src/umd/umd_block_transfer.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <initializer_list>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"BC copy policy failure line=%d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
  using dxvk::umd::copyBlockRegion2D;
  for (uint32_t bytes : {8u,16u}) {
    CHECK(copyBlockRegion2D(24,16,24,16,{4,4,12,12},8,4,bytes));
    CHECK(copyBlockRegion2D(6,4,6,4,{4,0,6,4},4,0,bytes));
    CHECK(copyBlockRegion2D(3,2,24,16,{0,0,3,2},4,12,bytes));
    CHECK(copyBlockRegion2D(1,1,1,1,{0,0,1,1},0,0,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{1,4,12,12},8,4,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,1,12,12},8,4,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,4,10,12},8,4,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,4,12,10},8,4,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,4,12,12},2,4,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,4,12,12},8,2,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,4,12,12},20,4,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,4,12,12},8,12,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{0,0,28,4},0,0,bytes));
    CHECK(!copyBlockRegion2D(0,16,24,16,{0,0,4,4},0,0,bytes));
    CHECK(!copyBlockRegion2D(24,16,0,16,{0,0,4,4},0,0,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{4,0,4,4},0,0,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{8,0,4,4},0,0,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{0,4,4,4},0,0,bytes));
    CHECK(!copyBlockRegion2D(24,16,24,16,{0,8,4,4},0,0,bytes));
    CHECK(copyBlockRegion2D(UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,
      {UINT32_MAX-3,UINT32_MAX-3,UINT32_MAX,UINT32_MAX},UINT32_MAX-3,UINT32_MAX-3,bytes));
    CHECK(!copyBlockRegion2D(UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,
      {0,0,4,4},UINT32_MAX-3,0,bytes));
  }
  for (uint32_t bytes : {0u,1u,4u,12u,UINT32_MAX})
    CHECK(!copyBlockRegion2D(8,8,8,8,{0,0,4,4},0,0,bytes));
  // Independently enumerate physical block ranges for small virtual mip edges.
  for (uint32_t source=1;source<=17;++source) for (uint32_t destination=1;destination<=17;++destination)
    for (uint32_t left=0;left<source;++left) for (uint32_t right=left+1;right<=source;++right)
      for (uint32_t x=0;x<=destination;++x) {
        const bool aligned=left%4==0 && x%4==0 && (right%4==0 || right==source);
        const bool logical=right-left<=destination-x;
        const uint64_t firstSourceBlock=left/4,lastSourceBlock=(uint64_t(right)+3)/4;
        const uint64_t firstDestBlock=x/4,lastDestBlock=firstDestBlock+lastSourceBlock-firstSourceBlock;
        const bool physical=lastDestBlock<=(uint64_t(destination)+3)/4;
        CHECK(copyBlockRegion2D(source,1,destination,1,{left,0,right,1},x,0,8)==(aligned && logical && physical));
      }
  std::printf("BC regional copy policy PASS checks=%u edge_blocks=1 hardware_admission=0\n",checks);
}
