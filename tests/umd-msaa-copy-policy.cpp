// SPDX-License-Identifier: MIT
#include "../src/umd/umd_sample_copy.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"MSAA copy policy failure line=%d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
using dxvk::umd::SampleCopyShape;
static bool full(SampleCopyShape source,SampleCopyShape destination) {
  return dxvk::umd::sampleRegionCopyContract(source,destination,false,0,0,0);
}
int main() {
  // Perturb one required field of a legal whole-subresource copy at a time.
  // Quality values are descriptor equality controls, not hardware claims.
  for (unsigned samples : {2u,4u,8u,16u}) for (unsigned quality : {0u,1u,0xffffffffu}) {
    const SampleCopyShape legal{8,4,samples,quality};
    CHECK(full(legal,legal));
    CHECK(!full(legal,{8,4,samples,quality^1}));
    CHECK(!full(legal,{8,4,samples*2,quality}));
    CHECK(!full({8,4,1,quality},legal));
    CHECK(!dxvk::umd::sampleRegionCopyContract(legal,legal,true,0,0,0));
    for (unsigned offset : {1u,0xffffffffu}) {
      CHECK(!dxvk::umd::sampleRegionCopyContract(legal,legal,false,offset,0,0));
      CHECK(!dxvk::umd::sampleRegionCopyContract(legal,legal,false,0,offset,0));
      CHECK(!dxvk::umd::sampleRegionCopyContract(legal,legal,false,0,0,offset));
    }
    CHECK(!full({0,4,samples,quality},legal));
    CHECK(!full({8,0,samples,quality},legal));
    CHECK(!full({4,4,samples,quality},legal));
    CHECK(!full({16,4,samples,quality},legal));
    CHECK(!full({8,2,samples,quality},legal));
    CHECK(!full({8,8,samples,quality},legal));
    CHECK(full({0xffffffffu,0xffffffffu,samples,quality},{0xffffffffu,0xffffffffu,samples,quality}));
  }
  // Retain scalar routing: its existing source box/destination-fit checks
  // remain responsible for geometry, and quality is ignored for sample1.
  for (unsigned srcQuality : {0u,1u,0xffffffffu}) for (unsigned dstQuality : {0u,1u,0xffffffffu}) {
    CHECK(full({8,4,1,srcQuality},{16,8,1,dstQuality}));
    CHECK(dxvk::umd::sampleRegionCopyContract({8,4,1,srcQuality},{16,8,1,dstQuality},true,3,1,0));
  }
  CHECK(!full({8,4,0,0},{8,4,0,0}));
  CHECK(!full({8,4,2,0},{8,4,0,0}));
  std::printf("MSAA color copy policy PASS checks=%u single_quality_ignored=1 hardware_admission=0\n",checks);
}
