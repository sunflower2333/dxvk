#pragma once
// SPDX-License-Identifier: MIT
#include "umd_shader11.h"
#include <cstring>
#include <dxbc/dxbc_parser.h>

namespace dxvk::umd {

inline bool shaderRasterPosition(const ShaderCode11& shader) {
  for (const auto& output : shader.outputs)
    if (output.stream == 0 && output.systemValue == 1 && output.mask == 15
        && output.scalar == ShaderScalar::Float32) return true;
  return false;
}

struct ShaderInputSignature10 {
  uint32_t systemValue, registerIndex, mask;
};

// GS bytecode declares clip/cull inputs as ordinary array registers. The
// runtime union retains their system names, including disjoint masks sharing
// a register with other distances or ordinary varyings. Annotate only the
// components actually declared by this shader; unused union rows stay unused.
inline bool shader10GeometryInputs(ShaderCode11& shader,
    const ShaderInputSignature10* signature, size_t count) {
  using namespace dxbc_spv;
  if (shader.stage != ShaderStage::Geometry || count > 32 || (count && !signature)
      || shader.tokens.size() < 3 || shader.tokens[1] != shader.tokens.size()
      || !validLegacyShaderVersion(shader.stage, shader.tokens[0], true)) return false;
  for (size_t i = 0; i < count; ++i) {
    const auto& row = signature[i];
    if (!row.mask || (row.mask & ~15u)
        || (row.registerIndex == UINT32_MAX
          ? row.systemValue != 7 || row.mask != 1
          : row.registerIndex >= 32 || row.systemValue > 3)) return false;
    for (size_t j = 0; j < i; ++j)
      if (signature[j].registerIndex == row.registerIndex && (signature[j].mask & row.mask)) return false;
  }
  // Do not let union metadata turn malformed GS array or dedicated operands
  // into a valid register-file input. Check the actual declaration shapes.
  for (size_t offset = 2; offset < shader.tokens.size();) {
    const uint32_t op = shader.tokens[offset] & 0x7ff;
    uint32_t length = (shader.tokens[offset] >> 24) & 0x7f;
    if (op == uint32_t(dxbc::OpCode::eCustomData)) {
      if (shader.tokens.size() - offset < 2) return false;
      length = shader.tokens[offset + 1];
    }
    if (!length || length > shader.tokens.size() - offset) return false;
    if (op == uint32_t(dxbc::OpCode::eDclInput) || op == uint32_t(dxbc::OpCode::eDclInputSgv)
        || op == uint32_t(dxbc::OpCode::eDclInputSiv)) {
      if (length < 2) return false;
      const uint32_t operand = shader.tokens[offset + 1];
      const uint32_t primitive = uint32_t(dxbc::RegisterType::ePrimitiveId) << 12;
      if (operand == primitive) {
        if (op != uint32_t(dxbc::OpCode::eDclInput) || length != 2) return false;
      } else {
        const uint32_t mask = (operand >> 4) & 15;
        if (!mask || operand != (2u | (mask << 4) | (uint32_t(dxbc::RegisterType::eInput) << 12) | (2u << 20))
            || length != (op == uint32_t(dxbc::OpCode::eDclInput) ? 4u : 5u)) return false;
      }
    }
    offset += length;
  }
  util::ByteWriter bytes;
  bytes.write(util::FourCC("SHDR")); bytes.write(uint32_t(shader.tokens.size() * 4));
  for (auto word : shader.tokens) bytes.write(word);
  const auto raw = std::move(bytes).extract();
  dxbc::Parser parser(util::ByteReader(raw.data(), raw.size()));
  if (!parser.getShaderInfo()) return false;
  uint32_t vertices = 0;
  std::vector<uint32_t> arraySizes;
  bool primitiveId = false;
  while (parser) {
    const auto instruction = parser.parseInstruction();
    if (!instruction) return false;
    const auto token = instruction.getOpToken(); const auto op = token.getOpCode();
    if (op == dxbc::OpCode::eDclGsInputPrimitive) {
      if (vertices || instruction.getDstCount() || instruction.getSrcCount()
          || instruction.getImmCount() || instruction.getExtraCount()) return false;
      switch (token.getPrimitiveType()) {
        case dxbc::PrimitiveType::ePoint: vertices = 1; break;
        case dxbc::PrimitiveType::eLine: vertices = 2; break;
        case dxbc::PrimitiveType::eTriangle: vertices = 3; break;
        case dxbc::PrimitiveType::eLineAdj: vertices = 4; break;
        case dxbc::PrimitiveType::eTriangleAdj: vertices = 6; break;
        default: return false;
      }
    }
    if (op != dxbc::OpCode::eDclInput && op != dxbc::OpCode::eDclInputSgv
        && op != dxbc::OpCode::eDclInputSiv) continue;
    if (instruction.getDstCount() != 1) return false;
    const auto& operand = instruction.getDst(0);
    if (operand.getRegisterType() == dxbc::RegisterType::ePrimitiveId) {
      if (primitiveId || op != dxbc::OpCode::eDclInput || instruction.getImmCount()
          || instruction.getExtraCount() || operand.getIndexDimensions() || operand.getModifiers()
          || operand.getComponentCount() != dxbc::ComponentCount::e0Component) return false;
      util::ByteWriter encoded;
      if (!operand.write(encoded, instruction)) return false;
      const auto actual = std::move(encoded).extract();
      const uint32_t expected = uint32_t(dxbc::RegisterType::ePrimitiveId) << 12;
      if (actual.size() != sizeof(expected) || std::memcmp(actual.data(), &expected, sizeof(expected))) return false;
      primitiveId = true;
    } else {
      if (operand.getRegisterType() != dxbc::RegisterType::eInput
          || operand.getIndexDimensions() != 2 || operand.getIndexOperand(0) != UINT32_MAX
          || operand.getIndexOperand(1) != UINT32_MAX || operand.getModifiers()
          || operand.getComponentCount() != dxbc::ComponentCount::e4Component
          || operand.getSelectionMode() != dxbc::SelectionMode::eMask) return false;
      util::ByteWriter encoded;
      if (!operand.write(encoded, instruction)) return false;
      const auto actual = std::move(encoded).extract();
      const uint32_t expected[] = {2u | (uint32_t(uint8_t(operand.getWriteMask())) << 4)
        | (uint32_t(dxbc::RegisterType::eInput) << 12) | (2u << 20), operand.getIndex(0), operand.getIndex(1)};
      if (actual.size() != sizeof(expected) || std::memcmp(actual.data(), expected, sizeof(expected))) return false;
      arraySizes.push_back(operand.getIndex(0));
    }
  }
  for (auto size : arraySizes) if (!vertices || size != vertices) return false;
  std::vector<ShaderIo11> inputs;
  for (const auto& entry : shader.inputs) {
    if (entry.stream || !entry.mask || (entry.mask & ~15u)
        || (!entry.systemValue && entry.scalar != ShaderScalar::Uint32)) return false;
    if (entry.registerIndex == UINT32_MAX) {
      if (!primitiveId || entry.systemValue != 7 || entry.mask != 1 || entry.scalar != ShaderScalar::Uint32) return false;
      inputs.push_back(entry); continue;
    }
    uint32_t remaining = entry.mask;
    for (size_t i = 0; i < count; ++i) {
      const auto& row = signature[i];
      if (row.registerIndex != entry.registerIndex) continue;
      const auto mask = uint8_t(row.mask & remaining);
      if (!mask) continue;
      // Only clip/cull names may be absent from a raw ordinary GS declaration.
      if (entry.systemValue ? row.systemValue != entry.systemValue
          : row.systemValue && row.systemValue != 2 && row.systemValue != 3) return false;
      if (entry.systemValue && entry.scalar != ShaderScalar::Float32) return false;
      auto part = entry; part.mask = mask; part.systemValue = row.systemValue;
      if (part.systemValue) part.scalar = ShaderScalar::Float32;
      bool merged = false;
      for (auto& previous : inputs)
        if (previous.registerIndex == part.registerIndex && previous.systemValue == part.systemValue) {
          if (previous.scalar != part.scalar || (previous.mask & part.mask)) return false;
          previous.mask |= part.mask; merged = true; break;
        }
      if (!merged) inputs.push_back(part);
      remaining &= ~mask;
    }
    if (remaining || inputs.size() > 32) return false;
  }
  shader.inputs = std::move(inputs); return true;
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
