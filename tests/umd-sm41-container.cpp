// SPDX-License-Identifier: MIT
#include "../src/umd/umd_shader.h"
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_parser.h>
#include <dxbc/dxbc_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <d3d10TokenizedProgramFormat.hpp>
static_assert(D3D10_SB_OPCODE_RESERVED0 == 107 && D3D10_1_SB_OPCODE_LOD == 108
  && D3D10_1_SB_OPCODE_GATHER4 == 109 && D3D10_1_SB_OPCODE_SAMPLE_POS == 110
  && D3D10_1_SB_OPCODE_SAMPLE_INFO == 111 && D3D10_SB_NUM_OPCODES == 112);
static_assert(D3D10_SB_NAME_SAMPLE_INDEX == 10);
static_assert(D3D10_SB_OPCODE_DCL_INPUT_PS_SGV == 99
  && D3D10_SB_INTERPOLATION_UNDEFINED == 0 && D3D10_SB_INTERPOLATION_CONSTANT == 1);
#endif
using namespace dxvk::umd;
using namespace dxbc_spv;
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, \
  "SM4.1 container failure line %d: %s\n", __LINE__, #value); std::abort(); } } while (0)
static uint32_t dst(dxbc::RegisterType type, uint8_t mask = 15) {
  return 2 | (uint32_t(mask) << 4) | (uint32_t(type) << 12) | (1u << 20);
}
static uint32_t src(dxbc::RegisterType type, bool indexed = true) {
  return 2 | (1u << 2) | (0xe4u << 4) | (uint32_t(type) << 12) | (indexed ? 1u << 20 : 0);
}
static void instruction(std::vector<uint32_t>& code, dxbc::OpCode opcode,
    std::initializer_list<uint32_t> operands, uint32_t flags = 0) {
  code.push_back(uint32_t(opcode) | (uint32_t(operands.size() + 1) << 24) | flags);
  code.insert(code.end(), operands);
}
static bool build(ShaderStage stage, std::vector<uint32_t>& code,
    const ShaderSignatureEntry* inputs = nullptr, size_t inputCount = 0,
    std::vector<unsigned char>* result = nullptr) {
  code[1] = uint32_t(code.size());
  const ShaderSignatureEntry output = {stage == ShaderStage::Pixel ? 0u : 1u, 0, 15};
  std::vector<unsigned char> bytes{0xad};
  const bool ok = buildShaderContainer(stage, code.data(), code.size(), inputs,
    inputCount, &output, 1, bytes);
  if (ok) {
    dxbc::Container container(bytes.data(), bytes.size());
    CHECK(container && container.validateHash());
    CHECK(container.getCodeChunk().getSize() == (code.size() + 2) * sizeof(uint32_t));
    CHECK(!std::memcmp(container.getCodeChunk().getData(8), code.data(), code.size() * sizeof(uint32_t)));
  } else CHECK(bytes.empty());
  if (result) *result = std::move(bytes);
  return ok;
}
static std::vector<uint32_t> queryCode(ShaderStage stage, dxbc::OpCode opcode) {
  std::vector<uint32_t> code{(uint32_t(stage) << 16) | 0x41, 0};
  instruction(code, dxbc::OpCode::eDclTemps, {1});
  if (opcode == dxbc::OpCode::eLod || opcode == dxbc::OpCode::eGather4)
    instruction(code, opcode, {dst(dxbc::RegisterType::eTemp), 0,
      src(dxbc::RegisterType::eInput), 0, src(dxbc::RegisterType::eResource), 0,
      2 | (2u << 2) | (uint32_t(dxbc::RegisterType::eSampler) << 12) | (1u << 20), 0});
  else if (opcode == dxbc::OpCode::eSamplePos)
    instruction(code, opcode, {dst(dxbc::RegisterType::eTemp), 0,
      src(dxbc::RegisterType::eRasterizer, false), src(dxbc::RegisterType::eInput), 0});
  else instruction(code, opcode, {dst(dxbc::RegisterType::eTemp), 0,
    src(dxbc::RegisterType::eRasterizer, false)});
  instruction(code, dxbc::OpCode::eRet, {});
  code[1] = uint32_t(code.size());
  return code;
}
int main() {
  for (uint32_t stage = 0; stage <= 6; ++stage) {
    const auto typedStage = ShaderStage(stage);
    for (bool allow : {false, true}) {
      CHECK(validLegacyShaderVersion(typedStage, (stage << 16) | 0x40, allow) == (stage <= 2));
      CHECK(validLegacyShaderVersion(typedStage, (stage << 16) | 0x41, allow) == (allow && stage <= 2));
      for (uint32_t version : {0u, 0x30u, 0x42u, 0x50u, 0x51u, 0x140u, 0xffffu})
        CHECK(!validLegacyShaderVersion(typedStage, (stage << 16) | version, allow));
      CHECK(!validLegacyShaderVersion(typedStage, ((stage + 1) << 16) | 0x40, allow));
    }
  }
  for (auto stage : {ShaderStage::Pixel, ShaderStage::Vertex, ShaderStage::Geometry}) {
    for (uint32_t version : {0x40u, 0x41u}) {
      std::vector<uint32_t> code{(uint32_t(stage) << 16) | version, 3, 0x0100003e};
      CHECK(build(stage, code));
      for (uint32_t reserved : {107u, 112u, 113u, 126u, 235u, 0x7ffu}) {
        code[2] = 0x01000000 | reserved;
        CHECK(!build(stage, code));
      }
    }
    for (auto opcode : {dxbc::OpCode::eLod, dxbc::OpCode::eGather4,
        dxbc::OpCode::eSamplePos, dxbc::OpCode::eSampleInfo}) {
      auto code = queryCode(stage, opcode);
      const bool pixelOnly = opcode == dxbc::OpCode::eLod || opcode == dxbc::OpCode::eSamplePos;
      CHECK(build(stage, code) == (!pixelOnly || stage == ShaderStage::Pixel));
      code[0] = (uint32_t(stage) << 16) | 0x40;
      CHECK(!build(stage, code));
      code[0] = (uint32_t(stage) << 16) | 0x41;
      for (size_t words = 0; words < code.size(); ++words) {
        const ShaderSignatureEntry output{stage == ShaderStage::Pixel ? 0u : 1u, 0, 15};
        std::vector<unsigned char> bytes{0x5a};
        CHECK(!buildShaderContainer(stage, code.data(), words, nullptr, 0, &output, 1, bytes) && bytes.empty());
      }
      // Opcode length is bounded independently of the shader's declared size.
      code[4] = (code[4] & 0xffffff) | 0x7f000000;
      CHECK(!build(stage, code));
      code[4] = uint32_t(opcode) | 0x01000000;
      CHECK(!build(stage, code));
    }
  }
  // Sample index is generated by the rasterizer; it must not demand a VS/GS
  // producer or be reconstructed as a floating point SV_Position input.
  std::vector<uint32_t> pixel{0x41, 0};
  instruction(pixel, dxbc::OpCode::eDclInputPsSgv, {dst(dxbc::RegisterType::eInput, 1), 3, 10});
  instruction(pixel, dxbc::OpCode::eRet, {});
  const ShaderSignatureEntry sample{10, 3, 1};
  std::vector<ShaderSignatureEntry> resolved, linked;
  std::vector<unsigned char> bytes;
  CHECK(build(ShaderStage::Pixel, pixel, &sample, 1, &bytes));
  CHECK(resolvePixelInputs(pixel.data(), pixel.size(), &sample, 1, resolved));
  CHECK(resolved.size() == 1 && resolved[0].systemValue == 10
    && resolved[0].registerIndex == 3 && resolved[0].mask == 1 && resolved[0].scalar == ShaderScalar::Uint32);
  dxbc::Container container(bytes.data(), bytes.size());
  dxbc::Signature signature(container.getInputSignatureChunk());
  CHECK(signature.begin() != signature.end());
  CHECK(signature.begin()->getSystemValue() == dxbc::SignatureSysval::eSampleIndex
    && signature.begin()->getRegisterIndex() == 3 && signature.begin()->getScalarType() == ir::ScalarType::eU32);
  CHECK(!std::strcmp(signature.begin()->getSemanticName(), "SV_SampleIndex"));
  const ShaderSignatureEntry position{1, 0, 15};
  CHECK(linkVertexOutputs(&position, 1, resolved.data(), resolved.size(), linked) && linked.size() == 1);
  for (ShaderSignatureEntry invalid : {ShaderSignatureEntry{10, 3, 3, ShaderScalar::Uint32},
      ShaderSignatureEntry{10, 3, 1, ShaderScalar::Float32}})
    CHECK(!linkVertexOutputs(&position, 1, &invalid, 1, linked) && linked.empty());
  pixel[0] = 0x40; CHECK(!build(ShaderStage::Pixel, pixel, &sample, 1)); pixel[0] = 0x41;
  pixel[2] = (pixel[2] & ~0x7ffu) | uint32_t(dxbc::OpCode::eDclInputPsSiv);
  CHECK(!build(ShaderStage::Pixel, pixel, &sample, 1));
  pixel[2] = (pixel[2] & ~0x7ffu) | uint32_t(dxbc::OpCode::eDclInputPsSgv);
  pixel[3] = dst(dxbc::RegisterType::eInput, 3); CHECK(!build(ShaderStage::Pixel, pixel, &sample, 1));
  pixel[3] = dst(dxbc::RegisterType::eInput, 1);
  for (uint32_t reserved : {1u << 11, 1u << 15, 1u << 23}) {
    pixel[2] = (4u << 24) | uint32_t(dxbc::OpCode::eDclInputPsSgv) | reserved;
    CHECK(build(ShaderStage::Pixel, pixel, &sample, 1) == (reserved == (1u << 11)));
  }
  // Exact original native FXC program30 (ps_4_1/ps_index): CONSTANT SGV,
  // Uint SV_SampleIndex v0.x, and original uint-to-float/color operations.
  // Its native ISGN shares register0 with VS SV_Position; linkage must still
  // treat sample index as a generated value without an upstream producer.
  std::vector<uint32_t> original{0x41, 0x21, 0x0100086a, 0x04000863,
    0x00101012, 0, 10, 0x03000065, 0x001020f2, 0, 0x02000068, 1,
    0x05000056, 0x00100012, 0, 0x0010100a, 0, 0x07000038, 0x00102012,
    0, 0x0010000a, 0, 0x00004001, 0x3eaaaaab, 0x08000036, 0x001020e2,
    0, 0x00004002, 0, 0x3e800000, 0, 0x3f800000, 0x0100003e};
  const ShaderSignatureEntry originalSample{10, 0, 1};
  CHECK(original.size() == 33 && original[1] == original.size());
  CHECK(build(ShaderStage::Pixel, original, &originalSample, 1, &bytes));
  CHECK(resolvePixelInputs(original.data(), original.size(), &originalSample, 1, resolved));
  CHECK(resolved.size() == 1 && resolved[0].systemValue == 10 && resolved[0].registerIndex == 0
    && resolved[0].mask == 1 && resolved[0].scalar == ShaderScalar::Uint32);
  CHECK(linkVertexOutputs(&position, 1, resolved.data(), resolved.size(), linked)
    && linked.size() == 1 && linked[0].systemValue == 1 && linked[0].scalar == ShaderScalar::Float32);
  dxbc::Container originalContainer(bytes.data(), bytes.size());
  dxbc::Signature originalSignature(originalContainer.getInputSignatureChunk());
  CHECK(originalSignature.begin() != originalSignature.end()
    && originalSignature.begin()->getSystemValue() == dxbc::SignatureSysval::eSampleIndex
    && originalSignature.begin()->getRegisterIndex() == 0
    && originalSignature.begin()->getScalarType() == ir::ScalarType::eU32);
  for (uint32_t mode = 0; mode < 16; ++mode) {
    auto mutated = original; mutated[3] = (4u << 24) | uint32_t(dxbc::OpCode::eDclInputPsSgv) | (mode << 11);
    CHECK(build(ShaderStage::Pixel, mutated, &originalSample, 1) == (mode <= 1));
  }
  for (uint32_t reserved : {1u << 15, 1u << 23, 1u << 31}) {
    auto mutated = original; mutated[3] |= reserved;
    CHECK(!build(ShaderStage::Pixel, mutated, &originalSample, 1));
  }
  for (size_t words = 0; words < original.size(); ++words) {
    std::vector<unsigned char> failed{0x5a};
    const ShaderSignatureEntry output{0, 0, 15};
    CHECK(!buildShaderContainer(ShaderStage::Pixel, original.data(), words, &originalSample,
      1, &output, 1, failed) && failed.empty());
  }
  for (uint32_t count : {1u, 2u, 3u, 5u, 127u}) {
    auto mutated = original; mutated[3] = (mutated[3] & 0xffffff) | (count << 24);
    CHECK(!build(ShaderStage::Pixel, mutated, &originalSample, 1));
  }
  for (uint32_t mask : {0u, 3u, 15u}) {
    auto mutated = original; mutated[4] = dst(dxbc::RegisterType::eInput, uint8_t(mask));
    CHECK(!build(ShaderStage::Pixel, mutated, &originalSample, 1));
  }
  for (ShaderSignatureEntry invalid : {ShaderSignatureEntry{0, 0, 1}, ShaderSignatureEntry{1, 0, 1},
      ShaderSignatureEntry{10, 1, 1}})
    CHECK(!build(ShaderStage::Pixel, original, &invalid, 1));
  CHECK(!build(ShaderStage::Pixel, original));
  auto mutated = original; mutated[0] = 0x40;
  CHECK(!build(ShaderStage::Pixel, mutated, &originalSample, 1));
  mutated = original; mutated[6] = 1;
  const ShaderSignatureEntry wrongSystem{1, 0, 1};
  CHECK(!build(ShaderStage::Pixel, mutated, &wrongSystem, 1));
  mutated = original; mutated[3] = (mutated[3] & ~0x7ffu) | uint32_t(dxbc::OpCode::eDclInputPsSiv);
  CHECK(!build(ShaderStage::Pixel, mutated, &originalSample, 1));
  for (auto stage : {ShaderStage::Vertex, ShaderStage::Geometry}) {
    mutated = original; mutated[0] = (uint32_t(stage) << 16) | 0x41;
    CHECK(!build(stage, mutated, &originalSample, 1));
  }
  // Sample interpolation belongs to 4.1; all other supported interpolations
  // retain the established 4.0 declaration behavior.
  const ShaderSignatureEntry varying{0, 3, 1};
  for (uint32_t interpolation = 1; interpolation <= 7; ++interpolation) {
    std::vector<uint32_t> code{0x41, 0};
    instruction(code, dxbc::OpCode::eDclInputPs, {dst(dxbc::RegisterType::eInput, 1), 3}, interpolation << 11);
    instruction(code, dxbc::OpCode::eRet, {});
    CHECK(build(ShaderStage::Pixel, code, &varying, 1));
    code[0] = 0x40; CHECK(build(ShaderStage::Pixel, code, &varying, 1) == (interpolation < 6));
  }
  std::printf("SM4.0/4.1 containers verified checks=%u typed_models=2 new_opcodes=4 hardware_admission=0\n", checks);
}
