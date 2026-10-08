// SPDX-License-Identifier: MIT
#include "../src/umd/umd_shader11.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace dxvk::umd;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"distance stream policy line %d: %s\n",__LINE__,#x);std::abort(); } } while (0)
int main() {
  // Clip1 and Cull0 share a physical register. Public component numbering
  // starts at each semantic's first lane, and Clip0's unused row stays present.
  const std::vector<ShaderIo11> signature={{1,0,15,ShaderScalar::Float32,0,0},
    {2,1,15,ShaderScalar::Float32,0,0},{2,2,3,ShaderScalar::Float32,0,1},
    {3,2,12,ShaderScalar::Float32,0,0},{0,3,1,ShaderScalar::Uint32,0,3}};
  for(uint8_t mask=1;mask<16;++mask) {
    ShaderStreamDeclaration11 native{0,0,2,mask};ShaderStreamOutput11 out;
    const uint32_t stride=32;
    CHECK(shader11StreamOutput(signature,&native,1,&stride,1,UINT32_MAX,out));
    uint32_t captured=0;
    for(const auto& entry:out.entries) {
      const bool clip=entry.semantic=="SV_ClipDistance";
      CHECK(clip||entry.semantic=="SV_CullDistance");CHECK(entry.semanticIndex==(clip?1u:0u));
      CHECK(entry.stream==0&&entry.slot==0&&entry.start+entry.count<=2);
      for(uint32_t c=0;c<entry.count;++c)captured|=1u<<(entry.start+c+(clip?0:2));
    }
    CHECK(captured==mask&&out.strides[0]==32&&out.rasterizedStream==UINT32_MAX);
  }
  ShaderStreamOutput11 out;ShaderStreamDeclaration11 bad{0,0,2,15};
  auto changed=signature;changed[3].mask=14;CHECK(!shader11StreamOutput(changed,&bad,1,nullptr,0,UINT32_MAX,out));
  changed=signature;changed[3].mask=4;CHECK(!shader11StreamOutput(changed,&bad,1,nullptr,0,UINT32_MAX,out));
  const uint32_t tooSmall=12;CHECK(!shader11StreamOutput(signature,&bad,1,&tooSmall,1,UINT32_MAX,out));
  bad.stream=1;CHECK(!shader11StreamOutput(signature,&bad,1,nullptr,0,UINT32_MAX,out));
  bad={0,0,UINT32_MAX,5};CHECK(shader11StreamOutput(signature,&bad,1,nullptr,0,UINT32_MAX,out));
  CHECK(out.entries.size()==2&&out.entries[0].semantic.empty()&&out.entries[1].semantic.empty()
    &&out.entries[0].start==0&&out.entries[1].start==0&&out.strides[0]==8);
  std::printf("distance stream policy PASS checks=%u packed_semantics=1 full_union_ordinal=1 hardware_admission=0\n",checks);
}
