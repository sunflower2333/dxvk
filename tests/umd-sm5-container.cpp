#include "../src/umd/umd_shader11.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_interface.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace dxvk::umd;
using namespace dxbc_spv;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"SM5 container failure line %d: %s\n",__LINE__,#x); std::abort(); } } while(0)
static uint32_t op(dxbc::OpCode code, uint32_t length) { return uint32_t(code) | (length << 24); }
static uint32_t reg(dxbc::RegisterType type, uint8_t mask = 15) {
  return 2 | (uint32_t(mask) << 4) | (uint32_t(type) << 12) | (1u << 20);
}
static void instruction(std::vector<uint32_t>& code, dxbc::OpCode opcode, std::initializer_list<uint32_t> args) {
  code.push_back(op(opcode, uint32_t(args.size()) + 1)); code.insert(code.end(), args);
}
static std::vector<unsigned char> build(ShaderStage stage, std::vector<uint32_t>& code, ShaderCode11& shader) {
  code[1] = uint32_t(code.size()); CHECK(decodeShader11(stage, code.data(), code.size(), shader));
  std::vector<unsigned char> bytes; CHECK(buildShader11Container(shader, bytes));
  dxbc::Container container(bytes.data(), bytes.size()); CHECK(container && container.validateHash());
  CHECK(container.getCodeChunk().getSize() == (code.size() + 2) * 4);
  CHECK(!std::memcmp(container.getCodeChunk().getData(8), code.data(), code.size() * 4));
  return bytes;
}
static void computeSystemInputs() {
  using dxbc::OpCode;
  using dxbc::RegisterType;
  const RegisterType registers[] = {RegisterType::eThreadId, RegisterType::eThreadGroupId,
    RegisterType::eThreadIdInGroup, RegisterType::eThreadIndexInGroup};
  auto program = [](ShaderStage stage, OpCode opcode, std::initializer_list<uint32_t> operands) {
    std::vector<uint32_t> code{(uint32_t(stage) << 16) | 0x50, 0};
    if (stage == ShaderStage::Compute) instruction(code, OpCode::eDclThreadGroup, {2,3,2});
    instruction(code, opcode, operands); instruction(code, OpCode::eRet, {});
    code[1] = uint32_t(code.size()); return code;
  };
  auto reject = [](ShaderStage stage, std::vector<uint32_t> code) {
    ShaderCode11 shader; shader.tokens = {0xcdcdcdcd}; shader.inputs.push_back({});
    CHECK(!decodeShader11(stage, code.data(), code.size(), shader));
    CHECK(shader.tokens.empty() && shader.inputs.empty() && shader.outputs.empty() && shader.patch.empty());
  };
  for (auto type : registers) {
    const bool scalar = type == RegisterType::eThreadIndexInGroup;
    for (uint32_t mask = 1; mask <= (scalar ? 1u : 7u); ++mask) {
      auto code = program(ShaderStage::Compute, OpCode::eDclInput,
        {2 | (mask << 4) | (uint32_t(type) << 12)});
      ShaderCode11 shader; auto bytes = build(ShaderStage::Compute, code, shader);
      CHECK(shader.inputs.empty() && shader.outputs.empty() && shader.patch.empty());
      dxbc::Container container(bytes.data(), bytes.size());
      dxbc::Signature inputs(container.getInputSignatureChunk()), outputs(container.getOutputSignatureChunk());
      CHECK(inputs && outputs && inputs.begin() == inputs.end() && outputs.begin() == outputs.end());
    }
    const uint32_t operand = 2 | (1u << 4) | (uint32_t(type) << 12);
    for (uint32_t stage = 0; stage < uint32_t(ShaderStage::Compute); ++stage)
      reject(ShaderStage(stage), program(ShaderStage(stage), OpCode::eDclInput, {operand}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclOutput, {operand}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInputSiv, {operand,0}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInputPs, {operand}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {operand | (1u << 20),0}));
    if (!scalar) reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {uint32_t(type) << 12}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {2 | (uint32_t(type) << 12)}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {2 | (8u << 4) | (uint32_t(type) << 12)}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {operand | 8}));
    reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {operand | 0x80000000u,0x41}));
    auto duplicate = program(ShaderStage::Compute, OpCode::eDclInput, {operand});
    duplicate.pop_back(); instruction(duplicate, OpCode::eDclInput, {operand});
    instruction(duplicate, OpCode::eRet, {}); duplicate[1] = uint32_t(duplicate.size());
    reject(ShaderStage::Compute, duplicate);
    if (!scalar) reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {1 | (uint32_t(type) << 12)}));
    else reject(ShaderStage::Compute, program(ShaderStage::Compute, OpCode::eDclInput, {2 | (2u << 4) | (uint32_t(type) << 12)}));
  }
  // FXC declares SV_GroupIndex with zero components and vector operands for
  // the other three values. Preserve all four together without creating ISGN.
  std::vector<uint32_t> all{0x50050,0};
  instruction(all,OpCode::eDclThreadGroup,{2,3,2});
  for (auto type : registers) instruction(all,OpCode::eDclInput,
    {type == RegisterType::eThreadIndexInGroup ? uint32_t(type) << 12
      : 2 | (7u << 4) | (uint32_t(type) << 12)});
  instruction(all,OpCode::eRet,{});
  ShaderCode11 shader; auto bytes = build(ShaderStage::Compute,all,shader);
  CHECK(shader.inputs.empty() && shader.outputs.empty() && shader.patch.empty());
  auto scalar = program(ShaderStage::Compute,OpCode::eDclInput,
    {1 | (uint32_t(RegisterType::eThreadIndexInGroup) << 12)});
  build(ShaderStage::Compute,scalar,shader);
  for (uint32_t bits : {4u,8u,16u,256u,1u<<22}) {
    auto invalidScalar = program(ShaderStage::Compute,OpCode::eDclInput,
      {(uint32_t(RegisterType::eThreadIndexInGroup) << 12) | bits});
    reject(ShaderStage::Compute,invalidScalar);
  }
  auto modifiedScalar = program(ShaderStage::Compute,OpCode::eDclInput,
    {(uint32_t(RegisterType::eThreadIndexInGroup) << 12) | 0x80000000u,0x41});
  reject(ShaderStage::Compute,modifiedScalar);
  auto duplicateScalar = all;
  duplicateScalar.pop_back(); instruction(duplicateScalar,OpCode::eDclInput,
    {1 | (uint32_t(RegisterType::eThreadIndexInGroup) << 12)});
  instruction(duplicateScalar,OpCode::eRet,{}); duplicateScalar[1] = uint32_t(duplicateScalar.size());
  reject(ShaderStage::Compute,duplicateScalar);
  all.pop_back(); instruction(all,OpCode::eDclInput,{2 | (8u << 4) | (uint32_t(RegisterType::eThreadId) << 12)});
  instruction(all,OpCode::eRet,{}); all[1] = uint32_t(all.size());
  reject(ShaderStage::Compute,all);
  // Actual ARM64 FXC SHEX from the eight-group ID oracle; public WARP
  // accepts its original and legacy containers. GroupIndex is 0x00024000.
  std::vector<uint32_t> actualFxc{
    0x00050050u,0x00000072u,0x0100086au,0x0400009eu,0x0011e000u,0x00000000u,
    0x00000010u,0x0200005fu,0x00024000u,0x0200005fu,0x00021072u,0x0200005fu,
    0x00022072u,0x0200005fu,0x00020072u,0x02000068u,0x00000002u,0x0400009bu,
    0x00000002u,0x00000003u,0x00000002u,0x07000023u,0x00100012u,0x00000000u,
    0x0002002au,0x00004001u,0x00000006u,0x0002001au,0x07000029u,0x00100012u,
    0x00000000u,0x0010000au,0x00000000u,0x00004001u,0x00000002u,0x0600001eu,
    0x00100012u,0x00000000u,0x0010000au,0x00000000u,0x0002000au,0x09000029u,
    0x00100062u,0x00000000u,0x00020656u,0x00004002u,0x00000000u,0x00000008u,
    0x00000010u,0x00000000u,0x0600001eu,0x00100022u,0x00000000u,0x0010001au,
    0x00000000u,0x0002000au,0x0700001eu,0x00100012u,0x00000001u,0x0010002au,
    0x00000000u,0x0010001au,0x00000000u,0x09000029u,0x00100062u,0x00000000u,
    0x00021656u,0x00004002u,0x00000000u,0x00000008u,0x00000010u,0x00000000u,
    0x0600001eu,0x00100022u,0x00000000u,0x0010001au,0x00000000u,0x0002100au,
    0x0700001eu,0x00100022u,0x00000001u,0x0010002au,0x00000000u,0x0010001au,
    0x00000000u,0x07000023u,0x00100022u,0x00000000u,0x0002201au,0x00004001u,
    0x00000100u,0x0002200au,0x08000023u,0x00100042u,0x00000001u,0x0002202au,
    0x00004001u,0x00010000u,0x0010001au,0x00000000u,0x04000036u,0x00100082u,
    0x00000001u,0x0002400au,0x090000a8u,0x0011e0f2u,0x00000000u,0x0010000au,
    0x00000000u,0x00004001u,0x00000000u,0x00100e46u,0x00000001u,0x0100003eu,
  };
  build(ShaderStage::Compute,actualFxc,shader);
  CHECK(shader.inputs.empty() && shader.outputs.empty() && shader.patch.empty());
  // Nonoverlapping masks on the same system register retain both tokens.
  auto split = program(ShaderStage::Compute,OpCode::eDclInput,{2 | (1u << 4) | (uint32_t(RegisterType::eThreadId) << 12)});
  split.pop_back(); instruction(split,OpCode::eDclInput,{2 | (2u << 4) | (uint32_t(RegisterType::eThreadId) << 12)});
  instruction(split,OpCode::eRet,{}); build(ShaderStage::Compute,split,shader);
}
int main() {
  ShaderCode11 shader;
  computeSystemInputs();
  // Input layouts require only a signature; preserve the logical shader
  // version and every scalar/register, including the higher10.1 input31.
  for (uint32_t version : {0x40u, 0x41u, 0x50u}) {
    const uint32_t code[] = {0x10000 | version, 3, op(dxbc::OpCode::eRet, 1)};
    CHECK(decodeShader11(ShaderStage::Vertex, code, 3, shader));
    const uint32_t registers = version == 0x40 ? 16 : 32;
    for (uint32_t i = 0; i < registers; ++i)
      shader.inputs.push_back({0, i, 15, i % 3 == 0 ? ShaderScalar::Float32
        : i % 3 == 1 ? ShaderScalar::Uint32 : ShaderScalar::Sint32});
    shader.outputs.push_back({1, 0, 15, ShaderScalar::Float32});
    std::vector<unsigned char> bytes;
    CHECK(buildShader11Container(shader, bytes));
    dxbc::Container layout(bytes.data(), bytes.size());
    CHECK(layout && layout.validateHash());
    CHECK(layout.getCodeChunk().getSize() == sizeof(code) + 8);
    CHECK(!std::memcmp(layout.getCodeChunk().getData(8), code, sizeof(code)));
    dxbc::Signature inputs(layout.getInputSignatureChunk());
    uint32_t count = 0;
    for (const auto& entry : inputs) {
      CHECK(entry.getRegisterIndex() == int32_t(count) && entry.getSemanticIndex() == count);
      CHECK(!std::strcmp(entry.getSemanticName(), inputRegisterSemantic));
      CHECK(entry.getScalarType() == (count % 3 == 0 ? ir::ScalarType::eF32
        : count % 3 == 1 ? ir::ScalarType::eU32 : ir::ScalarType::eI32));
      ++count;
    }
    CHECK(count == registers);
  }
  // Stream IDs come from code declarations, not from an interface table cast.
  std::vector<uint32_t> gs{0x20050,0};
  instruction(gs,dxbc::OpCode::eDclStream,{uint32_t(dxbc::RegisterType::eStream)<<12 | (1u<<20),0});
  instruction(gs,dxbc::OpCode::eDclOutputSiv,{reg(dxbc::RegisterType::eOutput),0,1});
  instruction(gs,dxbc::OpCode::eDclOutput,{reg(dxbc::RegisterType::eOutput,5),1});
  instruction(gs,dxbc::OpCode::eDclStream,{uint32_t(dxbc::RegisterType::eStream)<<12 | (1u<<20),2});
  instruction(gs,dxbc::OpCode::eDclOutput,{reg(dxbc::RegisterType::eOutput,3),0});
  instruction(gs,dxbc::OpCode::eRet,{});
  auto bytes = build(ShaderStage::Geometry,gs,shader);
  CHECK(shader.outputs.size() == 3 && shader.outputs[2].stream == 2);
  dxbc::Container geometry(bytes.data(),bytes.size());
  dxbc::Signature outputs(geometry.getOutputSignatureChunk());
  unsigned count = 0;
  for (const auto& entry : outputs) {
    CHECK(entry.getRegisterIndex() == (count == 1 ? 1 : 0));
    CHECK(entry.getStreamIndex() == (count == 2 ? 2u : 0u));
    CHECK(uint8_t(entry.getUsedComponentMask()) == 0);
    CHECK(entry.getScalarType() == (count == 0 ? ir::ScalarType::eF32 : ir::ScalarType::eU32));
    ++count;
  }
  CHECK(count == 3);

  ShaderStreamOutput11 stream;
  ShaderStreamDeclaration11 declarations[] = {{0,0,1,5},{0,0,UINT32_MAX,3},{2,2,0,3}};
  const uint32_t strides[] = {32,0,16};
  CHECK(shader11StreamOutput(shader,declarations,3,strides,3,2,stream));
  CHECK(stream.entries.size() == 4 && stream.strideCount == 3 && stream.rasterizedStream == 2);
  CHECK(stream.entries[0].semantic == varyingRegisterSemantic && stream.entries[0].semanticIndex == 1);
  CHECK(stream.entries[0].start == 0 && stream.entries[0].count == 1);
  CHECK(stream.entries[1].start == 2 && stream.entries[1].count == 1);
  CHECK(stream.entries[2].semantic.empty() && stream.entries[2].count == 2);
  CHECK(stream.entries[3].stream == 2 && stream.entries[3].slot == 2 && stream.strides[2] == 16);
  declarations[2].slot = 0;
  CHECK(!shader11StreamOutput(shader,declarations,3,strides,3,2,stream));
  CHECK(stream.entries.empty()); declarations[2].slot = 2;
  declarations[2].mask = 4;
  CHECK(!shader11StreamOutput(shader,declarations,3,strides,3,2,stream)); declarations[2].mask = 3;
  const uint32_t shortStrides[] = {12,0,16};
  CHECK(!shader11StreamOutput(shader,declarations,3,shortStrides,3,2,stream));
  CHECK(!shader11StreamOutput(shader,declarations,3,strides,3,4,stream));
  CHECK(!shader11StreamOutput(shader,declarations,3,nullptr,3,0,stream));
  CHECK(shader11StreamOutput(shader,declarations,3,nullptr,0,UINT32_MAX,stream));
  CHECK(stream.strides[0] == 16 && stream.strides[2] == 8);
  std::vector<ShaderStreamDeclaration11> excessive(129, {0,0,UINT32_MAX,1});
  CHECK(!shader11StreamOutput(shader,excessive.data(),excessive.size(),nullptr,0,0,stream));

  std::vector<ShaderIo11> produced = {{0,1,15,ShaderScalar::Uint32},{0,1,15,ShaderScalar::Uint32,2}};
  std::vector<ShaderIo11> consumed = {{0,1,3,ShaderScalar::Float32},{0,1,12,ShaderScalar::Sint32}};
  std::vector<ShaderIo11> linked;
  CHECK(linkShader11Outputs(produced,consumed,0,linked));
  CHECK(linked.size() == 3 && linked[0].mask == 12 && linked[0].scalar == ShaderScalar::Sint32);
  CHECK(linked[1].stream == 2 && linked[1].scalar == ShaderScalar::Uint32);
  CHECK(linked[2].mask == 3 && linked[2].scalar == ShaderScalar::Float32);
  consumed[1].registerIndex = 2;
  CHECK(!linkShader11Outputs(produced,consumed,0,linked) && linked.empty());
  CHECK(linkShader11Outputs({},{{7,UINT32_MAX,1,ShaderScalar::Uint32}},0,linked));

  // Token sysvals for individual tess factors differ from signature sysvals.
  std::vector<uint32_t> hs{0x30050,0};
  instruction(hs,dxbc::OpCode::eHsDecls,{});
  instruction(hs,dxbc::OpCode::eHsControlPointPhase,{});
  instruction(hs,dxbc::OpCode::eDclOutputSiv,{reg(dxbc::RegisterType::eOutput),0,1});
  instruction(hs,dxbc::OpCode::eHsForkPhase,{});
  for (uint32_t i = 0; i < 4; ++i)
    instruction(hs,dxbc::OpCode::eDclOutputSiv,{reg(dxbc::RegisterType::eOutput,1),i+1,17+i});
  instruction(hs,dxbc::OpCode::eRet,{});
  bytes = build(ShaderStage::Hull,hs,shader);
  CHECK(shader.hullControlPhase && shader.outputs.size() == 1 && shader.patch.size() == 4);
  dxbc::Container hull(bytes.data(),bytes.size()); dxbc::Signature patch(hull.getPatchConstantSignatureChunk());
  count = 0;
  for (const auto& entry : patch) {
    CHECK(entry.getSystemValue() == (count < 3 ? dxbc::SignatureSysval::eTriEdgeTessFactor : dxbc::SignatureSysval::eTriInsideTessFactor));
    CHECK(entry.getSemanticIndex() == (count < 3 ? count : 0));
    CHECK(entry.getScalarType() == ir::ScalarType::eF32); ++count;
  }
  CHECK(count == 4);

  std::vector<uint32_t> ps{0x50,0};
  instruction(ps,dxbc::OpCode::eDclOutput,{reg(dxbc::RegisterType::eOutput),7});
  instruction(ps,dxbc::OpCode::eDclOutput,{uint32_t(dxbc::RegisterType::eDepthGe)<<12 | 1});
  instruction(ps,dxbc::OpCode::eDclOutput,{uint32_t(dxbc::RegisterType::eCoverageOut)<<12 | 1});
  instruction(ps,dxbc::OpCode::eRet,{});
  bytes = build(ShaderStage::Pixel,ps,shader);
  CHECK(shader.outputs.size() == 3 && shader.outputs[0].systemValue == 64);
  shader.outputs[0].scalar = ShaderScalar::Sint32;
  CHECK(buildShader11Container(shader,bytes));
  dxbc::Container pixel(bytes.data(),bytes.size()); dxbc::Signature colors(pixel.getOutputSignatureChunk());
  bool depth = false, coverage = false, integer = false;
  for (const auto& entry : colors) {
    depth |= entry.getSystemValue() == dxbc::SignatureSysval::eDepthGreaterEqual && entry.getRegisterIndex() == -1;
    coverage |= entry.getSystemValue() == dxbc::SignatureSysval::eCoverage && entry.getRegisterIndex() == -1;
    integer |= entry.getSystemValue() == dxbc::SignatureSysval::eTarget && entry.getSemanticIndex() == 7 && entry.getScalarType() == ir::ScalarType::eI32;
  }
  CHECK(depth && coverage && integer);

  // Interface IDs are native function-table IDs, even when IDs are sparse.
  std::vector<uint32_t> cs{0x50050,0};
  instruction(cs,dxbc::OpCode::eDclThreadGroup,{1,1,1});
  instruction(cs,dxbc::OpCode::eDclFunctionBody,{0});
  instruction(cs,dxbc::OpCode::eDclFunctionTable,{0xabc,1,0});
  instruction(cs,dxbc::OpCode::eDclInterface,{0,1,(2u<<16)|1,0xabc});
  instruction(cs,dxbc::OpCode::eRet,{});
  bytes = build(ShaderStage::Compute,cs,shader);
  CHECK(shader.interfaceSlots == 2 && shader11InterfaceTable(shader,0,0xabc) && shader11InterfaceTable(shader,1,0xabc));
  CHECK(!shader11InterfaceTable(shader,2,0xabc) && !shader11InterfaceTable(shader,0,1));
  dxbc::Container compute(bytes.data(),bytes.size());
  dxbc::InterfaceChunk interfaces(compute.getInterfaceChunk()); CHECK(interfaces);
  const auto types = interfaces.getClassTypes(); CHECK(types.second - types.first == 1);
  CHECK(types.first->name == "VIOGPU_TABLE_2748" && types.first->id == 0);
  const auto slots = interfaces.getInterfaceSlots(); CHECK(slots.second - slots.first == 2);
  for (auto i = slots.first; i != slots.second; ++i)
    CHECK(i->count == 1 && i->entries.size() == 1 && i->entries[0].typeId == 0 && i->entries[0].tableId == 0xabc);
  uint32_t offset = UINT32_MAX;
  CHECK(shader11ClassPointer(13,4080,127,15,offset) && offset == 255);
  CHECK(!shader11ClassPointer(14,0,0,0,offset));
  CHECK(!shader11ClassPointer(0,1,0,0,offset));
  CHECK(!shader11ClassPointer(0,4096,0,0,offset));
  CHECK(!shader11ClassPointer(0,0,128,0,offset));
  CHECK(!shader11ClassPointer(0,0,0,16,offset));
  auto bad = cs; bad[0] = 0x50051;
  CHECK(!decodeShader11(ShaderStage::Compute,bad.data(),bad.size(),shader));
  bad = cs; bad[1]++;
  CHECK(!decodeShader11(ShaderStage::Compute,bad.data(),bad.size(),shader));
  bad = cs; bad[2] = op(dxbc::OpCode::eDclThreadGroup,100);
  CHECK(!decodeShader11(ShaderStage::Compute,bad.data(),bad.size(),shader));
  bad = cs; bad[13] = 253;
  CHECK(!decodeShader11(ShaderStage::Compute,bad.data(),bad.size(),shader));
  bad = gs; bad[14] = 4;
  CHECK(!decodeShader11(ShaderStage::Geometry,bad.data(),bad.size(),shader));
  CHECK(!decodeShader11(ShaderStage::Hull,ps.data(),ps.size(),shader));
  std::printf("SM5 signatures/interfaces PASS checks=%u exact tokens/hash, GS streams, patch factors, typed/depth outputs, native table IDs\n",checks);
}
