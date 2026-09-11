#include "../src/umd/umd_shader.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include "umd-probe-shaders.h"
#include <d3d11shader.h>
#endif
#ifdef VIOGPU_SHADER_SPIRV_TEST
#include <dxbc/dxbc_api.h>
#include <spirv/spirv_builder.h>
#endif

using namespace dxvk::umd;
static unsigned checks;
static void checkAt(bool value, unsigned line) {
  checks++;
  if (!value) { std::fprintf(stderr,"failed check %u line=%u\n",checks,line); std::abort(); }
}
#define check(...) checkAt((__VA_ARGS__), __LINE__)

#ifdef VIOGPU_SHADER_SPIRV_TEST
static void checkSpirvInterface(const std::vector<unsigned char>& binary, bool vertex,
    const std::vector<ShaderSignatureEntry>& expected) {
  using namespace dxbc_spv;
  auto ir = dxbc::compileShaderToLegalizedIr(binary.data(),binary.size(),{},{});
  check(bool(ir));
  spirv::BasicResourceMapping mapping;
  spirv::SpirvBuilder::Options options;
  options.supportedRoundModesF32 = ir::RoundMode::eNearestEven | ir::RoundMode::eZero;
  options.supportedDenormModesF32 = ir::DenormMode::eFlush | ir::DenormMode::ePreserve;
  options.supportsZeroInfNanPreserveF32 = true;
  spirv::SpirvBuilder builder(*ir,mapping,options);
  builder.buildSpirvBinary();
  const auto words = builder.getSpirvBinary();
  check(words.size() >= 5 && words[0] == spv::MagicNumber && words[3] < 1024*1024);
  struct Id {
    uint32_t type = 0, base = 0, storage = uint32_t(-1), location = uint32_t(-1);
    uint32_t width = 0, signedness = 0, count = 1;
    spv::Op kind = spv::OpNop;
    bool flat = false;
  };
  std::vector<Id> ids(words[3]);
  for (size_t offset = 5; offset < words.size();) {
    const auto opcode = spv::Op(words[offset] & 0xffff);
    const uint32_t length = words[offset] >> 16;
    check(length && length <= words.size() - offset);
    const auto* op = words.data() + offset;
    if (opcode == spv::OpTypeFloat || opcode == spv::OpTypeInt) {
      check(length == (opcode == spv::OpTypeFloat ? 3u : 4u) && op[1] < ids.size());
      auto& id = ids[op[1]]; id.kind = opcode; id.width = op[2];
      if (opcode == spv::OpTypeInt) id.signedness = op[3];
    } else if (opcode == spv::OpTypeVector || opcode == spv::OpTypePointer) {
      check(length == 4 && op[1] < ids.size());
      auto& id = ids[op[1]]; id.kind = opcode;
      id.base = opcode == spv::OpTypeVector ? op[2] : op[3];
      if (opcode == spv::OpTypeVector) id.count = op[3];
    } else if (opcode == spv::OpVariable) {
      check(length >= 4 && op[2] < ids.size());
      ids[op[2]].type = op[1]; ids[op[2]].storage = op[3];
    } else if (opcode == spv::OpDecorate) {
      check(length >= 3 && op[1] < ids.size());
      auto& id = ids[op[1]];
      if (op[2] == spv::DecorationLocation) { check(length == 4); id.location = op[3]; }
      if (op[2] == spv::DecorationFlat) id.flat = true;
    }
    offset += length;
  }
  for (const auto& entry : expected) if (!entry.systemValue) {
    unsigned matches = 0;
    for (const auto& id : ids) {
      if (id.storage != uint32_t(vertex ? spv::StorageClassOutput : spv::StorageClassInput) ||
          id.location != entry.registerIndex) continue;
      matches++;
      check(id.type < ids.size() && ids[id.type].kind == spv::OpTypePointer);
      uint32_t type = ids[id.type].base;
      check(type < ids.size());
      uint32_t components = 1;
      if (ids[type].kind == spv::OpTypeVector) { components = ids[type].count; type = ids[type].base; }
      std::fprintf(stderr,"SPIRV_INTERFACE stage=%s register=%u components=%u mask=%u kind=%u width=%u flat=%u expected=%u\n",
        vertex ? "VS" : "PS",entry.registerIndex,components,unsigned(entry.mask),unsigned(ids[type].kind),
        ids[type].width,unsigned(id.flat),unsigned(entry.scalar));
      check(type < ids.size() && ids[type].width == 32);
      const bool raw = entry.scalar == ShaderScalar::Uint32;
      check(ids[type].kind == (raw ? spv::OpTypeInt : spv::OpTypeFloat));
      if (raw) check(ids[type].signedness == 0 && (vertex || id.flat));
      check(components == unsigned((entry.mask & 1) + ((entry.mask >> 1) & 1) +
        ((entry.mask >> 2) & 1) + ((entry.mask >> 3) & 1)));
    }
    if (matches != 1) std::fprintf(stderr,"SPIRV_INTERFACE stage=%s register=%u matches=%u\n",
      vertex ? "VS" : "PS",entry.registerIndex,matches);
    check(matches == 1);
  }
}
#endif

int main() {
  // RET is sufficient to test container reconstruction, not executable
  // rendering. The separate DDI probe compiles and draws real HLSL shaders.
  uint32_t code[] = {0x10040, 3, 0x0100003e};
  ShaderSignatureEntry input = {6,0,1}, output = {1,0,15};
  std::vector<unsigned char> binary;
  check(buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  dxbc_spv::dxbc::Container container(binary.data(), binary.size());
  check(bool(container) && container.validateHash());
  auto chunk = container.getCodeChunk();
  check(chunk.getSize() == sizeof(code) + 8);
  check(std::memcmp(chunk.getData(8), code, sizeof(code)) == 0);
  dxbc_spv::dxbc::Signature signature(container.getInputSignatureChunk());
  check(signature.begin() != signature.end());
  check(signature.begin()->getScalarType() == dxbc_spv::ir::ScalarType::eU32);
  check(signature.begin()->getSystemValue() == dxbc_spv::dxbc::SignatureSysval::eVertexId);
  check(uint8_t(signature.begin()->getUsedComponentMask()) == 1);
  for (size_t n = 0; n < 3; n++)
    check(!buildShaderContainer(ShaderStage::Vertex, code, n, &input, 1, &output, 1, binary) && binary.empty());
  check(!buildShaderContainer(ShaderStage::Vertex, nullptr, 3, &input, 1, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 1024*1024+1, &input, 1, &output, 1, binary));
  for (unsigned value : {0u,2u,4u,0xffffffffu}) {
    code[1] = value;
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  }
  code[1] = 3;
  for (unsigned value : {0x40u,0x10050u,0x20040u,0xffffffffu}) {
    code[0] = value;
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  }
  code[0] = 0x10040;
  // Illegal opcode and truncated extended/multiword opcodes must fail.
  for (unsigned value : {0x010007ffu,0x8100003eu,0x0200003eu,0u}) {
    code[2] = value;
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 1, binary));
  }
  code[2] = 0x0100003e;
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, nullptr, 1, &output, 1, binary));
  ShaderSignatureEntry duplicateInputs[] = {input,input};
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, duplicateInputs, 2, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 33, &output, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, nullptr, 1, binary));
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &output, 0, binary));
  for (auto wrong : {ShaderSignatureEntry{0,0,1}, {6,32,1}, {6,0,2}, {6,0,0}})
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &wrong, 1, &output, 1, binary));
  for (auto wrong : {ShaderSignatureEntry{0,0,15}, {1,32,15}, {1,0,1}, {1,0,31}})
    check(!buildShaderContainer(ShaderStage::Vertex, code, 3, &input, 1, &wrong, 1, binary));
  ShaderSignatureEntry typedInputs[] = {{0,0,3,ShaderScalar::Float32},
    {0,3,1,ShaderScalar::Uint32}, {0,7,1,ShaderScalar::Sint32}};
  check(buildShaderContainer(ShaderStage::Vertex, code, 3, typedInputs, 3, &output, 1, binary));
  dxbc_spv::dxbc::Container typedContainer(binary.data(), binary.size());
  dxbc_spv::dxbc::Signature typedSignature(typedContainer.getInputSignatureChunk());
  const dxbc_spv::ir::ScalarType expectedTypes[] = {dxbc_spv::ir::ScalarType::eF32,
    dxbc_spv::ir::ScalarType::eU32, dxbc_spv::ir::ScalarType::eI32};
  for (unsigned i = 0; i < 3; i++) {
    auto entry = typedSignature.findSemantic(0, inputRegisterSemantic, typedInputs[i].registerIndex);
    check(entry != typedSignature.end());
    check(entry->getRegisterIndex() == int32_t(typedInputs[i].registerIndex));
    check(entry->getScalarType() == expectedTypes[i]);
    check(uint8_t(entry->getUsedComponentMask()) == typedInputs[i].mask);
  }
  typedInputs[0].scalar = ShaderScalar::Unknown;
  check(!buildShaderContainer(ShaderStage::Vertex, code, 3, typedInputs, 3, &output, 1, binary));
  code[0] = 0x40; output = {0,0,15};
  check(buildShaderContainer(ShaderStage::Pixel, code, 3, nullptr, 0, &output, 1, binary));
  dxbc_spv::dxbc::Container pixel(binary.data(), binary.size());
  check(bool(pixel) && pixel.validateHash());
  dxbc_spv::dxbc::Signature color(pixel.getOutputSignatureChunk());
  check(color.begin()->getSystemValue() == dxbc_spv::dxbc::SignatureSysval::eTarget);
  check(!buildShaderContainer(ShaderStage::Pixel, code, 3, &input, 1, &output, 1, binary));
  output.systemValue = 1;
  check(!buildShaderContainer(ShaderStage::Pixel, code, 3, nullptr, 0, &output, 1, binary));

  // Register-based linkage works without application semantic names or
  // assuming that SV_Position is stored in output register zero.
  ShaderSignatureEntry vsOutputs[] = {{1,1,15}, {0,3,7}, {0,7,15}};
  ShaderSignatureEntry psInputs[] = {{1,1,3,ShaderScalar::Float32},
    {0,3,3,ShaderScalar::Float32}, {0,7,15,ShaderScalar::Uint32}};
  std::vector<ShaderSignatureEntry> linked, resolved;
  check(linkVertexOutputs(vsOutputs,3,psInputs,3,linked) && linked.size() == 3);
  check(linked[0].scalar == ShaderScalar::Float32 && linked[1].scalar == ShaderScalar::Float32
    && linked[2].scalar == ShaderScalar::Uint32);
  code[0] = 0x10040;
  check(buildShaderContainer(ShaderStage::Vertex,code,3,nullptr,0,linked.data(),linked.size(),binary));
  dxbc_spv::dxbc::Container linkedContainer(binary.data(),binary.size());
  dxbc_spv::dxbc::Signature linkedSignature(linkedContainer.getOutputSignatureChunk());
  auto flatOutput = linkedSignature.findSemantic(0,varyingRegisterSemantic,7);
  check(flatOutput != linkedSignature.end() && flatOutput->getRegisterIndex() == 7
    && flatOutput->getScalarType() == dxbc_spv::ir::ScalarType::eU32);
  psInputs[1].mask = 15;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked) && linked.empty());
  psInputs[1].mask = 3; psInputs[1].registerIndex = 2;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[1].registerIndex = 3; psInputs[2].scalar = ShaderScalar::Unknown;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[2].scalar = ShaderScalar::Sint32;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[2].scalar = ShaderScalar::Uint32; psInputs[0].systemValue = 0;
  check(!linkVertexOutputs(vsOutputs,3,psInputs,3,linked));
  psInputs[0].systemValue = 1;
  check(!linkVertexOutputs(vsOutputs+1,2,psInputs,3,linked));
  check(linkVertexOutputs(vsOutputs,3,nullptr,0,linked));
  check(linked[1].scalar == ShaderScalar::Uint32 && linked[2].scalar == ShaderScalar::Uint32);

  // dcl_input_ps v3.xy followed by ret. These are parsing/signature
  // fixtures only. The device probe below uses executable HLSL.
  uint32_t pixelCode[] = {0x40,6,0x03000062,0x00101032,3,0x0100003e};
  ShaderSignatureEntry varying = {0,3,15}, target = {0,0,15};
  for (uint32_t mode = 1; mode <= 7; mode++) {
    pixelCode[2] = 0x03000062 | (mode << 11);
    check(resolvePixelInputs(pixelCode,6,&varying,1,resolved) && resolved.size() == 1);
    check(resolved[0].mask == 3 && resolved[0].scalar ==
      (mode == 1 ? ShaderScalar::Uint32 : ShaderScalar::Float32));
    check(buildShaderContainer(ShaderStage::Pixel,pixelCode,6,&varying,1,&target,1,binary));
    dxbc_spv::dxbc::Container varyingContainer(binary.data(),binary.size());
    dxbc_spv::dxbc::Signature varyingSignature(varyingContainer.getInputSignatureChunk());
    auto entry = varyingSignature.findSemantic(0,varyingRegisterSemantic,3);
    check(entry != varyingSignature.end() && entry->getRegisterIndex() == 3 &&
      entry->getScalarType() == (mode == 1 ? dxbc_spv::ir::ScalarType::eU32 : dxbc_spv::ir::ScalarType::eF32));
    check(uint8_t(entry->getUsedComponentMask()) == 3);
  }
  for (uint32_t mode : {0u,8u,15u}) {
    pixelCode[2] = 0x03000062 | (mode << 11);
    check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved) && resolved.empty());
  }
  pixelCode[2] = 0x03001062;
  check(!resolvePixelInputs(pixelCode,6,nullptr,0,resolved));
  varying.mask = 1;
  check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved));
  varying.mask = 3; varying.registerIndex = 2;
  check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved));
  varying.registerIndex = 3; pixelCode[4] = 32;
  check(!resolvePixelInputs(pixelCode,6,&varying,1,resolved));
  pixelCode[4] = 3;
  check(!resolvePixelInputs(pixelCode,5,&varying,1,resolved));
  uint32_t conflicting[] = {0x40,9,0x03001062,0x00101012,3,0x03000862,0x00101022,3,0x0100003e};
  check(!resolvePixelInputs(conflicting,9,&varying,1,resolved));
  uint32_t immediate[] = {0x10040,9,0x1835,6,0x7fc01234,0x80000000,0x7f800000,0xff800000,0x0100003e};
  ShaderSignatureEntry immediateOutput = {1,0,15};
  check(buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  dxbc_spv::dxbc::Container immediateContainer(binary.data(),binary.size());
  check(immediateContainer.validateHash() && !std::memcmp(immediateContainer.getCodeChunk().getData(8),
    immediate,sizeof(immediate)));
  for (uint32_t length : {0u,1u,2u,3u,5u,7u,8u,0xffffffffu}) {
    immediate[3] = length;
    check(!buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  }
  immediate[3] = 6;
  for (uint32_t type : {2u,4u,5u,0x1fffffu}) {
    immediate[2] = (type << 11) | 0x35;
    check(!buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  }
  for (uint32_t type : {0u,1u}) {
    immediate[2] = (type << 11) | 0x35;
    check(buildShaderContainer(ShaderStage::Vertex,immediate,9,nullptr,0,&immediateOutput,1,binary));
  }
  uint32_t duplicateIcb[] = {0x10040,15,0x1835,6,1,2,3,4,0x1835,6,5,6,7,8,0x0100003e};
  check(!buildShaderContainer(ShaderStage::Vertex,duplicateIcb,15,nullptr,0,&immediateOutput,1,binary));
#ifdef _WIN32
  for (bool vertex : {true, false}) {
    std::vector<uint32_t> compiled;
    check(compileProbeShader(vertex, compiled));
    output.systemValue = vertex ? 1 : 0;
    const bool rebuilt = buildShaderContainer(vertex ? ShaderStage::Vertex : ShaderStage::Pixel,
      compiled.data(), compiled.size(), vertex ? &input : nullptr, vertex ? 1 : 0,
      &output, 1, binary);
    if (!rebuilt) {
      std::fprintf(stderr, "reconstruction failed stage=%s words=%zu\n", vertex ? "VS" : "PS", compiled.size());
      for (uint32_t word : compiled) std::fprintf(stderr, "%08x ", word);
      std::fputc('\n', stderr);
    }
    check(rebuilt);
    dxbc_spv::dxbc::Container real(binary.data(), binary.size());
    check(bool(real) && real.validateHash());
    auto realCode = real.getCodeChunk();
    check(realCode.getSize() == compiled.size() * 4 + 8);
    check(std::memcmp(realCode.getData(8), compiled.data(), compiled.size() * 4) == 0);
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflected;
    check(SUCCEEDED(D3DReflect(binary.data(), binary.size(), __uuidof(ID3D11ShaderReflection), &reflected)));
    D3D11_SHADER_DESC desc = {};
    check(SUCCEEDED(reflected->GetDesc(&desc)));
    check(desc.InputParameters == (vertex ? 1u : 0u) && desc.OutputParameters == 1);
  }
  std::vector<uint32_t> buffered;
  check(compileProbeShader(true, buffered, true));
  ShaderSignatureEntry bufferedInput = {0,0,3,ShaderScalar::Float32}, positionOutput = {1,0,15};
  check(buildShaderContainer(ShaderStage::Vertex, buffered.data(), buffered.size(),
    &bufferedInput, 1, &positionOutput, 1, binary));
  Microsoft::WRL::ComPtr<ID3D11ShaderReflection> bufferedReflection;
  check(SUCCEEDED(D3DReflect(binary.data(), binary.size(), __uuidof(ID3D11ShaderReflection), &bufferedReflection)));
  D3D11_SIGNATURE_PARAMETER_DESC reflectedInput = {};
  check(SUCCEEDED(bufferedReflection->GetInputParameterDesc(0, &reflectedInput)));
  check(reflectedInput.Register == 0 && reflectedInput.Mask == 3
    && reflectedInput.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32
    && !std::strcmp(reflectedInput.SemanticName, inputRegisterSemantic));

  std::vector<uint32_t> linkedVs, linkedPs;
  std::vector<ShaderSignatureEntry> vsi, vso, psi, pso;
  check(compileLinkageProbeShader(true,linkedVs,vsi,vso));
  check(compileLinkageProbeShader(false,linkedPs,psi,pso));
  check(resolvePixelInputs(linkedPs.data(),linkedPs.size(),psi.data(),psi.size(),resolved));
  check(linkVertexOutputs(vso.data(),vso.size(),resolved.data(),resolved.size(),linked));
  unsigned flatCount = 0, interpolatedCount = 0;
  for (const auto& entry : resolved) if (!entry.systemValue) {
    flatCount += entry.scalar == ShaderScalar::Uint32;
    interpolatedCount += entry.scalar == ShaderScalar::Float32;
  }
  check(flatCount == 2 && interpolatedCount == 1);
  for (auto& entry : vsi) if (!entry.systemValue) entry.scalar = ShaderScalar::Float32;
  for (bool vertex : {true,false}) {
    const auto& tokens = vertex ? linkedVs : linkedPs;
    const auto& ins = vertex ? vsi : resolved;
    const auto& outs = vertex ? linked : pso;
    check(buildShaderContainer(vertex ? ShaderStage::Vertex : ShaderStage::Pixel,tokens.data(),tokens.size(),
      ins.data(),ins.size(),outs.data(),outs.size(),binary));
    Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflected;
    check(SUCCEEDED(D3DReflect(binary.data(),binary.size(),__uuidof(ID3D11ShaderReflection),&reflected)));
    D3D11_SHADER_DESC desc = {};
    check(SUCCEEDED(reflected->GetDesc(&desc)));
    unsigned reflectedFlat = 0;
    for (UINT i = 0; i < (vertex ? desc.OutputParameters : desc.InputParameters); i++) {
      D3D11_SIGNATURE_PARAMETER_DESC entry = {};
      check(SUCCEEDED(vertex ? reflected->GetOutputParameterDesc(i,&entry) : reflected->GetInputParameterDesc(i,&entry)));
      if (entry.ComponentType == D3D_REGISTER_COMPONENT_UINT32) {
        check(!std::strcmp(entry.SemanticName,varyingRegisterSemantic) && entry.SemanticIndex == entry.Register);
        reflectedFlat++;
      }
    }
    check(reflectedFlat == 2);
#ifdef VIOGPU_SHADER_SPIRV_TEST
    checkSpirvInterface(binary,vertex,vertex ? linked : resolved);
#endif
  }
  std::vector<uint32_t> immediateTokens;
  check(compileImmediateProbeShader(immediateTokens));
  bool hasIcb = false;
  for (size_t i = 2; i < immediateTokens.size();) {
    if ((immediateTokens[i] & 0x7ff) == 0x35) {
      hasIcb |= (immediateTokens[i] >> 11) == 3;
      i += immediateTokens[i+1];
    } else i += (immediateTokens[i] >> 24) & 0x7f;
  }
  check(hasIcb);
  check(buildShaderContainer(ShaderStage::Vertex,immediateTokens.data(),immediateTokens.size(),
    &input,1,&immediateOutput,1,binary));
#ifdef VIOGPU_SHADER_SPIRV_TEST
  checkSpirvInterface(binary,true,{});
#endif
#endif
  std::printf("shader container validation PASS checks=%u\n", checks);
}
