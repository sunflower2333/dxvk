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
int main() {
  ShaderCode11 shader;
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
