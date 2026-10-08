#pragma once
// SPDX-License-Identifier: MIT
#include "umd_shader11.h"
#include <dxbc/dxbc_parser.h>

namespace dxvk::umd {

inline bool shaderRasterPosition(const ShaderCode11& shader) {
  for (const auto& output : shader.outputs)
    if (output.stream == 0 && output.systemValue == 1 && output.mask == 15
        && output.scalar == ShaderScalar::Float32) return true;
  return false;
}

// The shared decoder also accepts SM5. Keep the historical D3D10 profile
// explicit, including dedicated operands that do not produce signature rows.
inline bool shader10Profile(const ShaderCode11& shader, bool allow4_1) {
  using namespace dxbc_spv;
  if (uint32_t(shader.stage) > uint32_t(ShaderStage::Geometry)
      || shader.tokens.size() < 3 || shader.tokens[1] != shader.tokens.size()
      || !validLegacyShaderVersion(shader.stage, shader.tokens[0], allow4_1)
      || shader.interfaceSlots || !shader.interfaces.empty() || !shader.patch.empty()
      || shader.inputs.size() > 32 || shader.outputs.size() > 32) return false;
  const bool model41 = (shader.tokens[0] & 0xffff) == 0x41;
  bool immediateConstants = false;
  for (size_t offset = 2; offset < shader.tokens.size();) {
    const uint32_t opcode = shader.tokens[offset] & 0x7ff;
    uint32_t count = (shader.tokens[offset] >> 24) & 0x7f;
    if (opcode == 107 || opcode == 112 || opcode > (model41 ? 111u : 106u)) return false;
    if (opcode == uint32_t(dxbc::OpCode::eCustomData)) {
      if (shader.tokens.size() - offset < 2) return false;
      count = shader.tokens[offset + 1];
      if (count < 2 || count > shader.tokens.size() - offset) return false;
      const auto kind = dxbc::CustomDataType(shader.tokens[offset] >> 11);
      if (kind == dxbc::CustomDataType::eDclIcb) {
        if (immediateConstants || count < 6 || (count - 2) % 4) return false;
        immediateConstants = true;
      } else if (kind != dxbc::CustomDataType::eComment && kind != dxbc::CustomDataType::eDebugInfo) return false;
    } else if (!count || count > shader.tokens.size() - offset) return false;
    if (opcode == uint32_t(dxbc::OpCode::eDclInputPsSgv)) {
      const uint32_t flags = shader.tokens[offset] & 0x80fff800;
      // Generated scalar values do not interpolate. Preserve the observed
      // FXC CONSTANT encoding for SampleIndex, rejecting extended/reserved
      // flags and arbitrary interpolation modes on these declarations.
      if (flags && (flags != (uint32_t(dxbc::InterpolationMode::eConstant) << 11)
          || shader.stage != ShaderStage::Pixel || count != 4
          || (shader.tokens[offset + 3] != 7 && shader.tokens[offset + 3] != 9
              && !(model41 && shader.tokens[offset + 3] == 10)))) return false;
    }
    offset += count;
  }
  auto legalIo = [&](const ShaderIo11& io, bool input) {
    if (io.stream || !io.mask || (io.mask & ~15u)) return false;
    bool legal = io.systemValue == 0;
    if (shader.stage == ShaderStage::Vertex)
      legal |= input ? io.systemValue == 6 || io.systemValue == 8
        : io.systemValue >= 1 && io.systemValue <= 3;
    else if (shader.stage == ShaderStage::Geometry)
      legal |= input ? (io.systemValue >= 1 && io.systemValue <= 3) || io.systemValue == 7
        : (io.systemValue >= 1 && io.systemValue <= 5) || io.systemValue == 7 || io.systemValue == 9;
    else
      legal |= input ? (io.systemValue >= 1 && io.systemValue <= 5) || io.systemValue == 7
          || io.systemValue == 9 || (io.systemValue == 10 && model41)
        : io.systemValue == 64 || io.systemValue == 65 || (io.systemValue == 66 && model41);
    if (!legal) return false;
    const bool dedicated = io.registerIndex == UINT32_MAX;
    if (dedicated != (io.systemValue == 65 || io.systemValue == 66
        || (input && shader.stage == ShaderStage::Geometry && io.systemValue == 7))) return false;
    if (!dedicated && io.registerIndex >= (io.systemValue == 64 ? 8u : 32u)) return false;
    if (io.systemValue >= 4 && io.systemValue <= 10)
      // Scalar register-file values may occupy x, y, z or w. FXC packs
      // SV_PrimitiveID beside an ordinary varying in v2.y; dedicated
      // operands still use their canonical x mask below.
      return !(io.mask & (io.mask - 1)) && (!dedicated || io.mask == 1)
        && io.scalar == ShaderScalar::Uint32;
    if (io.systemValue == 65 || io.systemValue == 66)
      return io.mask == 1 && io.scalar == (io.systemValue == 65 ? ShaderScalar::Float32 : ShaderScalar::Uint32);
    if (io.systemValue >= 1 && io.systemValue <= 3)
      return io.scalar == ShaderScalar::Float32 && (input || io.systemValue != 1 || io.mask == 15);
    return io.scalar != ShaderScalar::Unknown;
  };
  auto legalSignature = [&](const auto& signature, bool input) {
    uint32_t distanceComponents = 0, distanceRegisters = 0;
    for (const auto& io : signature) {
      if (!legalIo(io, input)) return false;
      if (io.systemValue == 2 || io.systemValue == 3) {
        for (uint32_t component = 0; component < 4; ++component)
          distanceComponents += bool(io.mask & (1u << component));
        distanceRegisters |= 1u << io.registerIndex;
      }
    }
    // D3D10 exposes eight clip/cull distances in at most two registers.
    uint32_t registers = 0;
    for (; distanceRegisters; distanceRegisters &= distanceRegisters - 1) ++registers;
    return distanceComponents <= 8 && registers <= 2;
  };
  if (!legalSignature(shader.inputs, true) || !legalSignature(shader.outputs, false)) return false;
  util::ByteWriter bytes;
  bytes.write(util::FourCC("SHDR")); bytes.write(uint32_t(shader.tokens.size() * 4));
  for (auto token : shader.tokens) bytes.write(token);
  const auto raw = std::move(bytes).extract();
  dxbc::Parser parser(util::ByteReader(raw.data(), raw.size()));
  if (!parser.getShaderInfo()) return false;
  while (parser) {
    const auto instruction = parser.parseInstruction();
    if (!instruction) return false;
    const auto token = instruction.getOpToken();
    const auto opcode = token.getOpCode();
    if (instruction.getImmCount() && (opcode == dxbc::OpCode::eDclInputSgv
        || opcode == dxbc::OpCode::eDclInputSiv || opcode == dxbc::OpCode::eDclInputPsSgv
        || opcode == dxbc::OpCode::eDclInputPsSiv)) {
      const auto system = instruction.getImm(0).getImmediate<uint32_t>(0);
      if ((system == 6 || system == 8) && opcode != dxbc::OpCode::eDclInputSgv) return false;
      if ((system == 7 || system == 9 || system == 10) && opcode != dxbc::OpCode::eDclInputPsSgv) return false;
    }
    if (opcode == dxbc::OpCode::eDclGlobalFlags && (uint32_t(token.getGlobalFlags()) & ~1u)) return false;
    if ((opcode == dxbc::OpCode::eDerivRtx || opcode == dxbc::OpCode::eDerivRty
        || opcode == dxbc::OpCode::eDiscard || opcode == dxbc::OpCode::eSample
        || opcode == dxbc::OpCode::eSampleB || opcode == dxbc::OpCode::eSampleC
        || opcode == dxbc::OpCode::eLod || opcode == dxbc::OpCode::eSamplePos)
        && shader.stage != ShaderStage::Pixel) return false;
    if ((opcode == dxbc::OpCode::eCut || opcode == dxbc::OpCode::eEmit
        || opcode == dxbc::OpCode::eEmitThenCut || opcode == dxbc::OpCode::eDclGsInputPrimitive
        || opcode == dxbc::OpCode::eDclGsOutputPrimitiveTopology
        || opcode == dxbc::OpCode::eDclMaxOutputVertexCount) && shader.stage != ShaderStage::Geometry) return false;
    for (uint32_t i = 0; i < instruction.getDstCount(); ++i) {
      const auto type = instruction.getDst(i).getRegisterType();
      if (type == dxbc::RegisterType::eImm64) return false;
      if (type == dxbc::RegisterType::eRasterizer) return false;
      if (uint32_t(type) > uint32_t(dxbc::RegisterType::eNull)
          && type != dxbc::RegisterType::eRasterizer && type != dxbc::RegisterType::eCoverageOut) return false;
      if (type == dxbc::RegisterType::eCoverageOut && (shader.stage != ShaderStage::Pixel || !model41)) return false;
      if (type == dxbc::RegisterType::eDepth && shader.stage != ShaderStage::Pixel) return false;
      if (type == dxbc::RegisterType::ePrimitiveId && shader.stage != ShaderStage::Geometry) return false;
    }
    for (uint32_t i = 0; i < instruction.getSrcCount(); ++i) {
      const auto type = instruction.getSrc(i).getRegisterType();
      if (type == dxbc::RegisterType::eImm64) return false;
      if (uint32_t(type) > uint32_t(dxbc::RegisterType::eNull)
          && type != dxbc::RegisterType::eRasterizer) return false;
      if (type == dxbc::RegisterType::eRasterizer && !model41) return false;
      if (type == dxbc::RegisterType::ePrimitiveId && shader.stage != ShaderStage::Geometry) return false;
    }
    if (opcode == dxbc::OpCode::eDclInputPs || opcode == dxbc::OpCode::eDclInputPsSgv
        || opcode == dxbc::OpCode::eDclInputPsSiv) {
      if (shader.stage != ShaderStage::Pixel) return false;
      const auto interpolation = token.getInterpolationMode();
      if (!model41 && (interpolation == dxbc::InterpolationMode::eLinearSample
          || interpolation == dxbc::InterpolationMode::eLinearNoPerspectiveSample)) return false;
    }
  }
  return true;
}

}
