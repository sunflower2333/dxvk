// SPDX-License-Identifier: MIT
#include "../src/umd/umd_shader.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace dxvk::umd;
using namespace dxbc_spv;
static unsigned checks;
// Parse production bytes independently rather than accepting a builder return value alone.
static void check(bool result, unsigned line) {
  checks++;
  if (!result) { std::fprintf(stderr,"FAIL MRT signature line=%u\n",line); std::exit(1); }
}
#define CHECK(x) check(!!(x),__LINE__)

// RET-only tokens test the container contract, not executable pixel output.
int main() {
  const uint32_t code[]{0x40,3,0x0100003e};
  std::vector<unsigned char> binary;
  ShaderSignatureEntry outputs[]{{0,7,15},{0,0,3},{0,3,1}};
  CHECK(buildShaderContainer(ShaderStage::Pixel,code,3,nullptr,0,outputs,3,binary));
  dxbc::Container container(binary.data(),binary.size());
  CHECK(container && container.validateHash());
  auto chunk=container.getCodeChunk();
  CHECK(chunk.getSize()==sizeof(code)+8 && !std::memcmp(chunk.getData(8),code,sizeof(code)));
  dxbc::Signature signature(container.getOutputSignatureChunk());
  const ShaderSignatureEntry canonical[]{{0,0,3},{0,3,1},{0,7,15}};
  size_t count=0;
  for (const auto& entry:signature) {
    CHECK(count<3);
    const auto& expected=canonical[count++];
    CHECK(!std::strcmp(entry.getSemanticName(),"SV_Target"));
    CHECK(entry.getSemanticIndex()==expected.registerIndex);
    CHECK(entry.getRegisterIndex()==int32_t(expected.registerIndex));
    CHECK(uint8_t(entry.getComponentMask())==expected.mask);
    CHECK(entry.getScalarType()==ir::ScalarType::eF32);
    CHECK(entry.getSystemValue()==dxbc::SignatureSysval::eTarget);
  }
  CHECK(count==3);
  for (uint32_t slot=0;slot<8;slot++) for (uint8_t mask=1;mask<16;mask++) {
    ShaderSignatureEntry value{0,slot,mask};
    CHECK(buildShaderContainer(ShaderStage::Pixel,code,3,nullptr,0,&value,1,binary));
    dxbc::Container c(binary.data(),binary.size()); CHECK(c && c.validateHash());
    dxbc::Signature s(c.getOutputSignatureChunk());
    CHECK(s.begin()!=s.end());
    CHECK(s.begin()->getSemanticIndex()==slot && s.begin()->getRegisterIndex()==int32_t(slot));
    CHECK(uint8_t(s.begin()->getComponentMask())==mask);
  }
  for (auto invalid : {ShaderSignatureEntry{0,8,15}, ShaderSignatureEntry{1,0,15},
                       ShaderSignatureEntry{0,0,0}, ShaderSignatureEntry{0,0,15,ShaderScalar::Uint32}}) {
    binary.assign(12,0xaa);
    CHECK(!buildShaderContainer(ShaderStage::Pixel,code,3,nullptr,0,&invalid,1,binary));
    CHECK(binary.empty());
  }
  outputs[2]=outputs[0];
  CHECK(!buildShaderContainer(ShaderStage::Pixel,code,3,nullptr,0,outputs,3,binary) && binary.empty());
  std::printf("PASS MRT signature: %u checks; actual DXBC bytes, no graphics runtime\n",checks);
}
