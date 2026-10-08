// SPDX-License-Identifier: MIT
#include "../src/umd/umd_shader10_policy.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace dxvk::umd;
using namespace dxbc_spv;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"shader10 profile line %d: %s\n",__LINE__,#x); std::abort(); } } while (0)
static void instruction(std::vector<uint32_t>& code, dxbc::OpCode op, std::initializer_list<uint32_t> args) {
  code.push_back(uint32_t(op) | (uint32_t(args.size()+1)<<24)); code.insert(code.end(),args);
}
static uint32_t reg(dxbc::RegisterType type, uint32_t mask=15) {
  return 2 | (mask<<4) | (uint32_t(type)<<12) | (1u<<20);
}
static ShaderCode11 program(ShaderStage stage, uint32_t version, uint32_t inputSystem, uint32_t outputSystem) {
  std::vector<uint32_t> code{(uint32_t(stage)<<16)|version,0};
  const bool pixel = stage == ShaderStage::Pixel;
  if (inputSystem != UINT32_MAX) {
    const auto op = pixel ? dxbc::OpCode::eDclInputPsSgv : dxbc::OpCode::eDclInputSgv;
    instruction(code,op,{reg(dxbc::RegisterType::eInput,inputSystem >= 4 && inputSystem <= 10 ? 1 : 15),0,inputSystem});
  }
  if (outputSystem == 65 || outputSystem == 66)
    instruction(code,dxbc::OpCode::eDclOutput,{1 | (uint32_t(outputSystem == 65 ? dxbc::RegisterType::eDepth : dxbc::RegisterType::eCoverageOut)<<12)});
  else if (outputSystem == 64 || outputSystem == 0)
    instruction(code,dxbc::OpCode::eDclOutput,{reg(dxbc::RegisterType::eOutput),0});
  else instruction(code,dxbc::OpCode::eDclOutputSiv,{reg(dxbc::RegisterType::eOutput,outputSystem == 1 ? 15 : 1),0,outputSystem});
  instruction(code,dxbc::OpCode::eRet,{}); code[1]=uint32_t(code.size());
  ShaderCode11 shader; CHECK(decodeShader11(stage,code.data(),code.size(),shader)); return shader;
}
static void container(const ShaderCode11& shader) {
  std::vector<unsigned char> binary; CHECK(buildShader11Container(shader,binary));
  dxbc::Container parsed(binary.data(),binary.size()); CHECK(parsed && parsed.validateHash());
  const auto chunk = parsed.getCodeChunk(); CHECK(chunk.getSize() == (shader.tokens.size()+2)*4);
  CHECK(!std::memcmp(chunk.getData(8),shader.tokens.data(),shader.tokens.size()*4));
}
int main() {
  auto instance = program(ShaderStage::Vertex,0x40,8,1); CHECK(shader10Profile(instance,false)); container(instance);
  auto front = program(ShaderStage::Pixel,0x40,9,64); CHECK(shader10Profile(front,false)); container(front);
  auto primitive = program(ShaderStage::Pixel,0x40,7,64); CHECK(shader10Profile(primitive,false)); container(primitive);
  auto depth = program(ShaderStage::Pixel,0x40,UINT32_MAX,65); CHECK(shader10Profile(depth,false)); container(depth);
  for (uint32_t system : {2u,3u}) { auto clip = program(ShaderStage::Vertex,0x40,6,system); CHECK(shader10Profile(clip,false)); container(clip); }
  auto coverage = program(ShaderStage::Pixel,0x41,UINT32_MAX,66);
  CHECK(shader10Profile(coverage,true) && !shader10Profile(coverage,false)); container(coverage);
  auto sample = program(ShaderStage::Pixel,0x41,10,64); CHECK(shader10Profile(sample,true)); container(sample);
  auto invalidCoverage = program(ShaderStage::Pixel,0x40,UINT32_MAX,66); CHECK(!shader10Profile(invalidCoverage,false));
  auto invalidSample = program(ShaderStage::Pixel,0x40,10,64); CHECK(!shader10Profile(invalidSample,true));
  // One table independently specifies the legal input systems per old stage.
  const uint32_t legal[] = {(1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<7)|(1u<<9),
    (1u<<0)|(1u<<6)|(1u<<8), (1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<7)};
  for (uint32_t stage=0;stage<3;++stage) for (uint32_t system=0;system<=22;++system) {
    auto shader = stage == 0 ? front : instance; shader.stage=ShaderStage(stage);
    shader.tokens[0]=(stage<<16)|0x40;
    shader.inputs={{system,0,uint8_t(system>=4&&system<=10?1:15),
      system>=1&&system<=3?ShaderScalar::Float32:ShaderScalar::Uint32}};
    if (stage==2 && system==7) shader.inputs[0].registerIndex=UINT32_MAX;
    shader.outputs={{stage==0?64u:1u,0,15,ShaderScalar::Float32}};
    CHECK(shader10Profile(shader,false) == bool(system<32&&(legal[stage]&(1u<<system))));
  }
  for (uint32_t version : {0x30u,0x41u,0x50u,0x51u,0u}) {
    auto changed=instance; changed.tokens[0]=(1u<<16)|version; CHECK(!shader10Profile(changed,false));
  }
  for (uint32_t mask : {0u,2u,3u,15u,16u}) {
    auto changed=instance;changed.inputs[0].mask=uint8_t(mask); CHECK(!shader10Profile(changed,false));
  }
  for (uint32_t system : {4u,5u,6u,7u,8u,9u,10u,11u,22u,64u,65u,66u}) {
    auto changed=instance;changed.outputs[0].systemValue=system; CHECK(!shader10Profile(changed,false));
  }
  auto malformed=instance; malformed.tokens[1]++; CHECK(!shader10Profile(malformed,false));
  malformed=instance; malformed.interfaceSlots=1; CHECK(!shader10Profile(malformed,false));
  malformed=instance; malformed.patch.push_back({}); CHECK(!shader10Profile(malformed,false));
  malformed=instance; malformed.outputs[0].mask=3; CHECK(!shader10Profile(malformed,false));
  for (uint32_t system : {4u,5u}) {
    auto arrayIndex=program(ShaderStage::Pixel,0x40,system,64);
    arrayIndex.inputs[0].mask=1; CHECK(shader10Profile(arrayIndex,false)); container(arrayIndex);
    arrayIndex.inputs[0].scalar=ShaderScalar::Float32; CHECK(!shader10Profile(arrayIndex,false));
    auto geometry=instance;geometry.stage=ShaderStage::Geometry;geometry.tokens[0]=(2u<<16)|0x40;
    geometry.inputs.clear();geometry.outputs={{system,0,1,ShaderScalar::Uint32}};
    CHECK(shader10Profile(geometry,false));
    geometry.outputs[0].mask=15;CHECK(!shader10Profile(geometry,false));
  }
  malformed=front;malformed.outputs[0].registerIndex=8;CHECK(!shader10Profile(malformed,false));
  for (const auto op : {dxbc::OpCode::eEmit,dxbc::OpCode::eCut,dxbc::OpCode::eEmitThenCut}) {
    auto changed=instance;changed.tokens.insert(changed.tokens.end()-1,uint32_t(op)|(1u<<24));
    changed.tokens[1]=uint32_t(changed.tokens.size());CHECK(!shader10Profile(changed,false));
    changed.stage=ShaderStage::Geometry;changed.tokens[0]=(2u<<16)|0x40;
    changed.inputs.clear();CHECK(shader10Profile(changed,false));
  }
  malformed=instance;malformed.outputs={{2,0,15,ShaderScalar::Float32},{3,1,15,ShaderScalar::Float32}};
  CHECK(shader10Profile(malformed,false));
  malformed.outputs.push_back({2,2,1,ShaderScalar::Float32});CHECK(!shader10Profile(malformed,false));
  malformed.outputs={{2,0,1,ShaderScalar::Float32},{2,1,1,ShaderScalar::Float32},{3,2,1,ShaderScalar::Float32}};
  CHECK(!shader10Profile(malformed,false));
  for (uint32_t flag : {0u,1u<<11,1u<<12,1u<<13,1u<<14}) {
    auto changed=instance;changed.tokens.insert(changed.tokens.end()-1,106u|(1u<<24)|flag);
    changed.tokens[1]=uint32_t(changed.tokens.size());CHECK(shader10Profile(changed,false)==(flag<=(1u<<11)));
  }
  for (uint32_t interpolation=0;interpolation<16;++interpolation) {
    auto changed=sample;changed.tokens[2]=(changed.tokens[2]&~0x00fff800u)|(interpolation<<11);
    CHECK(shader10Profile(changed,true)==(interpolation<=1));
  }
  for (uint32_t kind=0;kind<6;++kind) {
    auto changed=instance;
    const std::vector<uint32_t> custom={53u|(kind<<11),6,1,2,3,4};
    changed.tokens.insert(changed.tokens.end()-1,custom.begin(),custom.end());changed.tokens[1]=uint32_t(changed.tokens.size());
    CHECK(shader10Profile(changed,false)==(kind==0||kind==1||kind==3));
    if(kind==3){changed.tokens.insert(changed.tokens.end()-1,custom.begin(),custom.end());
      changed.tokens[1]=uint32_t(changed.tokens.size());CHECK(!shader10Profile(changed,false));}
  }
  for (auto stage : {ShaderStage::Vertex,ShaderStage::Geometry,ShaderStage::Pixel})
    for (const auto opcode : {dxbc::OpCode::eSamplePos,dxbc::OpCode::eSampleInfo}) {
      auto changed=program(stage,0x41,UINT32_MAX,stage==ShaderStage::Pixel?64:1);
      std::vector<uint32_t> query={uint32_t(opcode),reg(dxbc::RegisterType::eTemp),0,
        2u|(1u<<2)|(0xe4u<<4)|(uint32_t(dxbc::RegisterType::eRasterizer)<<12)};
      if(opcode==dxbc::OpCode::eSamplePos) {
        query.push_back(1u|(uint32_t(dxbc::RegisterType::eImm32)<<12));query.push_back(0);
      }
      query[0]|=uint32_t(query.size())<<24;
      changed.tokens.insert(changed.tokens.end()-1,query.begin(),query.end());changed.tokens[1]=uint32_t(changed.tokens.size());
      CHECK(shader10Profile(changed,true)==(opcode==dxbc::OpCode::eSampleInfo||stage==ShaderStage::Pixel));
    }
  malformed=sample;malformed.tokens[2]=(malformed.tokens[2]&~0x7ffu)|100u;CHECK(!shader10Profile(malformed,true));
  malformed=instance;malformed.tokens[2]=(malformed.tokens[2]&~0x7ffu)|97u;CHECK(!shader10Profile(malformed,false));
  for(uint32_t opcode:{107u,112u,113u,126u,0x7ffu}) {
    malformed=instance;malformed.tokens[2]=(malformed.tokens[2]&~0x7ffu)|opcode;CHECK(!shader10Profile(malformed,false));
  }
  // The old container rejects InstanceID; the new path is an actual added
  // profile, rather than tests silently reusing the old provisional metadata.
  ShaderSignatureEntry oldIn{8,0,1,ShaderScalar::Uint32}, oldOut{1,0,15,ShaderScalar::Float32};
  std::vector<unsigned char> rejected;
  CHECK(!buildShaderContainer(ShaderStage::Vertex,instance.tokens.data(),instance.tokens.size(),&oldIn,1,&oldOut,1,rejected));
  std::printf("D3D10 shader profile PASS checks=%u retained_tokens=1 hardware_admission=0\n",checks);
}
