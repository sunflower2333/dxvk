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
// Exact CI68 FXC02/05 GS tokens; the union below is independently specified
// by its original ISGN, not by the production annotation helper.
static void geometryUnionControls() {
  const uint32_t tokens[]={
    0x00020040u,0x00000090u,0x05000061u,0x002010f2u,0x00000003u,0x00000000u,0x00000001u,0x0400005fu,
    0x00201072u,0x00000003u,0x00000001u,0x0400005fu,0x00201082u,0x00000003u,0x00000001u,0x0400005fu,
    0x00201012u,0x00000003u,0x00000002u,0x0200005fu,0x0000b000u,0x02000068u,0x00000001u,0x0100185du,
    0x0100285cu,0x04000067u,0x001020f2u,0x00000000u,0x00000001u,0x04000067u,0x00102072u,0x00000001u,
    0x00000002u,0x04000067u,0x00102082u,0x00000001u,0x00000003u,0x03000065u,0x00102012u,0x00000002u,
    0x04000066u,0x00102012u,0x00000003u,0x00000007u,0x0200005eu,0x00000003u,0x06000036u,0x001020f2u,
    0x00000000u,0x00201e46u,0x00000000u,0x00000000u,0x06000036u,0x00102072u,0x00000001u,0x00201246u,
    0x00000000u,0x00000001u,0x06000036u,0x00102082u,0x00000001u,0x0020103au,0x00000000u,0x00000001u,
    0x06000036u,0x00102012u,0x00000002u,0x0020100au,0x00000000u,0x00000002u,0x0600001eu,0x00100012u,
    0x00000000u,0x0000b001u,0x00004001u,0x00000025u,0x05000036u,0x00102012u,0x00000003u,0x0010000au,
    0x00000000u,0x01000013u,0x06000036u,0x001020f2u,0x00000000u,0x00201e46u,0x00000001u,0x00000000u,
    0x06000036u,0x00102072u,0x00000001u,0x00201246u,0x00000001u,0x00000001u,0x06000036u,0x00102082u,
    0x00000001u,0x0020103au,0x00000001u,0x00000001u,0x06000036u,0x00102012u,0x00000002u,0x0020100au,
    0x00000001u,0x00000002u,0x05000036u,0x00102012u,0x00000003u,0x0010000au,0x00000000u,0x01000013u,
    0x06000036u,0x001020f2u,0x00000000u,0x00201e46u,0x00000002u,0x00000000u,0x06000036u,0x00102072u,
    0x00000001u,0x00201246u,0x00000002u,0x00000001u,0x06000036u,0x00102082u,0x00000001u,0x0020103au,
    0x00000002u,0x00000001u,0x06000036u,0x00102012u,0x00000002u,0x0020100au,0x00000002u,0x00000002u,
    0x05000036u,0x00102012u,0x00000003u,0x0010000au,0x00000000u,0x01000013u,0x01000009u,0x0100003eu,
  };
  ShaderCode11 raw; CHECK(decodeShader11(ShaderStage::Geometry,tokens,sizeof(tokens)/4,raw));
  CHECK(raw.inputs.size()==4&&raw.inputs[1].systemValue==0&&raw.inputs[1].registerIndex==1&&raw.inputs[1].mask==15);
  const std::vector<ShaderInputSignature10> signature={{1,0,15},{2,1,7},{3,1,8},{0,2,1}};
  const std::vector<ShaderIo11> producer={{1,0,15,ShaderScalar::Float32},{2,1,7,ShaderScalar::Float32},
    {3,1,8,ShaderScalar::Float32},{0,2,1,ShaderScalar::Uint32}};
  std::vector<ShaderIo11> linked;
  CHECK(!linkShader11Outputs(producer,raw.inputs,0,linked)&&linked.empty());
  auto annotated=raw;CHECK(shader10GeometryInputs(annotated,signature.data(),signature.size()));
  CHECK(annotated.tokens==raw.tokens&&annotated.inputs.size()==5);
  CHECK(annotated.inputs[1].systemValue==2&&annotated.inputs[1].mask==7&&annotated.inputs[1].scalar==ShaderScalar::Float32);
  CHECK(annotated.inputs[2].systemValue==3&&annotated.inputs[2].mask==8&&annotated.inputs[2].scalar==ShaderScalar::Float32);
  CHECK(annotated.inputs[3].systemValue==0&&annotated.inputs[3].registerIndex==2&&annotated.inputs[3].scalar==ShaderScalar::Uint32);
  CHECK(annotated.inputs[4].systemValue==7&&annotated.inputs[4].registerIndex==UINT32_MAX&&annotated.inputs[4].mask==1);
  CHECK(shader10Profile(annotated,false)&&shader10Profile(annotated,true));container(annotated);
  CHECK(linkShader11Outputs(producer,annotated.inputs,0,linked));
  // Every assignment of four physical components to ordinary/clip/cull.
  // This includes all three categories in one register and unaligned masks.
  for(uint32_t assignment=0;assignment<81;++assignment){
    uint32_t value=assignment,masks[3]{};
    for(uint32_t component=0;component<4;++component){masks[value%3]|=1u<<component;value/=3;}
    std::vector<ShaderInputSignature10> rows={{1,0,15},{0,2,1},{0,15,15}};
    for(uint32_t kind=0;kind<3;++kind)if(masks[kind])rows.push_back({kind?kind+1:0,1,masks[kind]});
    auto changed=raw;CHECK(shader10GeometryInputs(changed,rows.data(),rows.size()));
    CHECK(shader10Profile(changed,false)&&changed.tokens==raw.tokens);
    uint32_t seen=0;
    for(const auto& input:changed.inputs)if(input.registerIndex==1){
      CHECK(!(seen&input.mask));seen|=input.mask;
      const auto kind=input.systemValue?input.systemValue-1:0;
      CHECK(kind<3&&input.mask==masks[kind]);
      CHECK(input.scalar==(kind?ShaderScalar::Float32:ShaderScalar::Uint32));
    }
    CHECK(seen==15);
    for(const auto& input:changed.inputs)CHECK(input.registerIndex!=15);
    container(changed);
  }
  auto same=[](const auto& a,const auto& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(a[i].systemValue!=b[i].systemValue||a[i].registerIndex!=b[i].registerIndex
      ||a[i].mask!=b[i].mask||a[i].scalar!=b[i].scalar||a[i].stream!=b[i].stream||a[i].semanticIndex!=b[i].semanticIndex)return false;
    return true;
  };
  auto reject=[&](ShaderCode11 code,std::vector<ShaderInputSignature10> rows){
    const auto before=code.inputs;const auto beforeTokens=code.tokens;
    CHECK(!shader10GeometryInputs(code,rows.data(),rows.size()));CHECK(same(code.inputs,before)&&code.tokens==beforeTokens);
  };
  for(uint32_t invalid=0;invalid<14;++invalid){
    auto rows=signature;
    if(invalid==0)rows.push_back(rows[1]);
    else if(invalid==1)rows.push_back({0,1,1});
    else if(invalid==2)rows[1].mask=0;
    else if(invalid==3)rows[1].mask=16;
    else if(invalid==4)rows[1].registerIndex=32;
    else if(invalid==5)rows[1].systemValue=4;
    else if(invalid==6)rows[1].systemValue=7;
    else if(invalid==7)rows[1].systemValue=11;
    else if(invalid==8)rows[1].mask=3;
    else if(invalid==9)rows[0].systemValue=0;
    else if(invalid==10)rows[1].systemValue=1;
    else if(invalid==11)rows.push_back({0,UINT32_MAX,1});
    else if(invalid==12)rows.push_back({7,UINT32_MAX,2});
    else rows.resize(33,{0,30,1});
    reject(raw,rows);
  }
  auto changed=raw;CHECK(!shader10GeometryInputs(changed,nullptr,1));CHECK(same(changed.inputs,raw.inputs));
  for(auto stage:{ShaderStage::Vertex,ShaderStage::Pixel}){changed=raw;changed.stage=stage;reject(changed,signature);}
  changed=raw;changed.inputs[0].scalar=ShaderScalar::Uint32;reject(changed,signature);
  changed=raw;changed.inputs[1].scalar=ShaderScalar::Unknown;reject(changed,signature);
  changed=raw;changed.inputs[1].scalar=ShaderScalar::Float32;reject(changed,signature);
  changed=raw;changed.inputs.back().mask=2;reject(changed,signature);
  changed=raw;changed.inputs.back().scalar=ShaderScalar::Float32;reject(changed,signature);
  changed=raw;changed.tokens[20]|=1u<<20;reject(changed,signature); // indexed dedicated PrimitiveID
  changed=raw;changed.tokens[20]|=1u;reject(changed,signature); // altered scalar declaration
  changed=raw;changed.tokens[20]|=16u;reject(changed,signature); // reserved scalar mask bits
  for(uint32_t size:{0u,1u,2u,4u,6u}){changed=raw;changed.tokens[9]=size;reject(changed,signature);}
  for(uint32_t dimensions:{0u,1u,3u}){changed=raw;changed.tokens[8]=(changed.tokens[8]&~(3u<<20))|(dimensions<<20);reject(changed,signature);}
  changed=raw;changed.tokens[23]=(changed.tokens[23]&~(63u<<11))|(1u<<11);reject(changed,signature); // point versus array3
  auto withDedicated=signature;withDedicated.push_back({7,UINT32_MAX,1});changed=raw;
  CHECK(shader10GeometryInputs(changed,withDedicated.data(),withDedicated.size()));
  CHECK(same(changed.inputs,annotated.inputs));
  auto repeated=signature;repeated[1].mask=1;repeated.push_back({2,1,2});repeated.push_back({2,1,4});changed=raw;
  CHECK(shader10GeometryInputs(changed,repeated.data(),repeated.size()));
  CHECK(same(changed.inputs,annotated.inputs));container(changed);
  repeated={{1,0,15},{0,1,1},{0,1,2},{2,1,4},{3,1,8},{0,2,1}};changed=raw;
  CHECK(shader10GeometryInputs(changed,repeated.data(),repeated.size()));
  CHECK(changed.inputs.size()==6&&changed.inputs[1].systemValue==0&&changed.inputs[1].mask==3);
  container(changed);
}

int main() {
  geometryUnionControls();
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
    auto changed=instance;changed.inputs[0].mask=uint8_t(mask); CHECK(shader10Profile(changed,false)==(mask==2));
  }
  // Actual FXC may place generated scalar values in any one register
  // component. Decode the packed declaration rather than changing metadata
  // alone; malformed multi-component masks remain rejected.
  for (uint32_t system : {6u,8u,7u,9u,10u}) for (uint32_t mask=0;mask<=16;++mask) {
    const bool vertex=system==6||system==8;
    const auto source=program(vertex?ShaderStage::Vertex:ShaderStage::Pixel,system==10?0x41:0x40,system,vertex?1:64);
    auto tokens=source.tokens;tokens[3]=(tokens[3]&~0xf0u)|(mask<<4);
    ShaderCode11 decoded;
    const bool validMask=mask==1||mask==2||mask==4||mask==8;
    const bool decodedOk=decodeShader11(source.stage,tokens.data(),tokens.size(),decoded);
    CHECK(decodedOk==bool(mask&&mask<=15));
    if(decodedOk) {
      CHECK(shader10Profile(decoded,system==10)==validMask);
      if(validMask){CHECK(decoded.inputs.size()==1&&decoded.inputs[0].mask==mask);container(decoded);}
    }
  }
  for (uint32_t system : {4u,5u}) for (uint32_t mask : {1u,2u,4u,8u,3u,15u}) {
    auto packed=program(ShaderStage::Pixel,0x40,system,64);packed.inputs[0].mask=uint8_t(mask);
    CHECK(shader10Profile(packed,false)==(mask==1||mask==2||mask==4||mask==8));
  }
  for (uint32_t mask : {2u,4u,8u,3u,15u}) {
    auto dedicated=depth;dedicated.outputs[0].mask=uint8_t(mask);CHECK(!shader10Profile(dedicated,false));
    dedicated=coverage;dedicated.outputs[0].mask=uint8_t(mask);CHECK(!shader10Profile(dedicated,true));
    dedicated=instance;dedicated.stage=ShaderStage::Geometry;dedicated.tokens[0]=(2u<<16)|0x40;
    dedicated.inputs={{7,UINT32_MAX,uint8_t(mask),ShaderScalar::Uint32}};CHECK(!shader10Profile(dedicated,false));
  }
  // Byte-exact PS SHDR retained by both failed CI67 architectures. Its
  // ordinary uint varying uses v2.x and generated PrimitiveID uses v2.y.
  const uint32_t ci67Pixel[]={
    0x00000040u,0x0000003fu,0x03000862u,0x00101012u,0x00000002u,0x04000863u,0x00101022u,0x00000002u,
    0x00000007u,0x04000863u,0x00101012u,0x00000003u,0x00000009u,0x03000065u,0x001020f2u,0x00000000u,
    0x03000065u,0x001020f2u,0x00000001u,0x02000065u,0x0000c001u,0x0700001eu,0x00102022u,0x00000000u,
    0x0010101au,0x00000002u,0x00004001u,0x00000007u,0x07000001u,0x00102042u,0x00000000u,0x0010100au,
    0x00000003u,0x00004001u,0x00000001u,0x05000036u,0x00102012u,0x00000000u,0x0010100au,0x00000002u,
    0x05000036u,0x00102082u,0x00000000u,0x00004001u,0x7fc01234u,0x05000028u,0x00102012u,0x00000001u,
    0x0010100au,0x00000002u,0x08000036u,0x001020e2u,0x00000001u,0x00004002u,0x00000000u,0xfffffff9u,
    0x00000063u,0xffffffffu,0x04000036u,0x0000c001u,0x00004001u,0x3e800000u,0x0100003eu,
  };
  ShaderCode11 ci67;
  CHECK(decodeShader11(ShaderStage::Pixel,ci67Pixel,sizeof(ci67Pixel)/sizeof(*ci67Pixel),ci67));
  CHECK(shader10Profile(ci67,false)&&shader10Profile(ci67,true));
  CHECK(ci67.inputs.size()==3&&ci67.inputs[0].systemValue==0&&ci67.inputs[0].registerIndex==2&&ci67.inputs[0].mask==1);
  CHECK(ci67.inputs[1].systemValue==7&&ci67.inputs[1].registerIndex==2&&ci67.inputs[1].mask==2);
  container(ci67);
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
