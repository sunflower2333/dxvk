#include "umd_shader.h"
#include "umd_output_policy.h"
#include <algorithm>

#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_parser.h>
#include <dxbc/dxbc_signature.h>

// The pinned dxbc-spirv implementation provides this routine to validate
// DXBC hashes, but buildContainer currently emits a regular MD5 digest.
// Reuse its real DXBC checksum routine without modifying the submodule.
namespace dxbc_spv::dxbc {
util::md5::Digest hashDxbcBinary(const void* data, size_t size);
}

namespace dxvk::umd {
namespace {
using namespace dxbc_spv;

bool makeCodeChunk(ShaderStage stage, const uint32_t* code, size_t words,
    std::vector<unsigned char>& bytes) {
  bytes.clear();
  if (!code || words < 3 || words > 1024 * 1024 || code[1] != words ||
      code[0] != ((uint32_t(stage) << 16) | 0x40))
    return false;
  // Bound each instruction before giving it to the upstream parser. The
  // parser accepts unknown opcodes with an empty layout, so it is not by
  // itself an SM4 bytecode validator. Custom-data lengths are stored in
  // the second dword, unlike ordinary opcode lengths. Preserve their bytes.
  bool hasImmediateConstants = false;
  for (size_t offset = 2; offset < words;) {
    const uint32_t opcode = code[offset] & 0x7ff;
    uint32_t count = (code[offset] >> 24) & 0x7f;
    if (opcode > uint32_t(dxbc::OpCode::eDclGlobalFlags)) return false;
    if (opcode == uint32_t(dxbc::OpCode::eCustomData)) {
      if (words - offset < 2) return false;
      count = code[offset + 1];
      if (count < 2 || count > words - offset) return false;
      const auto type = dxbc::CustomDataType(code[offset] >> 11);
      if (type == dxbc::CustomDataType::eDclIcb) {
        if (hasImmediateConstants || count < 6 || (count - 2) % 4) return false;
        hasImmediateConstants = true;
      } else if (type != dxbc::CustomDataType::eComment && type != dxbc::CustomDataType::eDebugInfo)
        return false;
    } else if (!count || count > words - offset) return false;
    offset += count;
  }
  util::ByteWriter chunk;
  chunk.write(util::FourCC("SHDR"));
  chunk.write(uint32_t(words * sizeof(uint32_t)));
  for (size_t i = 0; i < words; i++) chunk.write(code[i]);
  bytes = std::move(chunk).extract();
  return true;
}

bool validEntries(const ShaderSignatureEntry* entries, size_t count) {
  if (count > 32 || (count && !entries)) return false;
  for (size_t i = 0; i < count; i++) {
    const auto& entry = entries[i];
    if (entry.registerIndex >= 32 || !entry.mask || (entry.mask & ~15)) return false;
    for (size_t j = 0; j < i; j++)
      if (entries[j].registerIndex == entry.registerIndex ||
          (entry.systemValue && entries[j].systemValue == entry.systemValue)) return false;
  }
  return true;
}

ir::ScalarType scalarType(ShaderScalar scalar) {
  switch (scalar) {
    case ShaderScalar::Float32: return ir::ScalarType::eF32;
    case ShaderScalar::Uint32: return ir::ScalarType::eU32;
    case ShaderScalar::Sint32: return ir::ScalarType::eI32;
    default: return ir::ScalarType::eUnknown;
  }
}
}

bool resolvePixelInputs(const uint32_t* code, size_t words,
    const ShaderSignatureEntry* inputs, size_t inputCount,
    std::vector<ShaderSignatureEntry>& resolved) {
  using namespace dxbc_spv;
  resolved.clear();
  if (!validEntries(inputs, inputCount)) return false;
  std::vector<unsigned char> chunk;
  if (!makeCodeChunk(ShaderStage::Pixel, code, words, chunk)) return false;
  struct Declaration {
    uint8_t mask = 0;
    uint32_t systemValue = 0;
    ShaderScalar scalar = ShaderScalar::Unknown;
  } declarations[32];
  dxbc::Parser parser(util::ByteReader(chunk.data(), chunk.size()));
  if (!parser.getShaderInfo()) return false;
  while (parser) {
    const auto instruction = parser.parseInstruction();
    if (!instruction) return false;
    const auto token = instruction.getOpToken();
    const auto opcode = token.getOpCode();
    if (opcode != dxbc::OpCode::eDclInputPs && opcode != dxbc::OpCode::eDclInputPsSiv
        && opcode != dxbc::OpCode::eDclInputPsSgv) continue;
    if (instruction.getDstCount() != 1) return false;
    const auto& operand = instruction.getDst(0);
    if (operand.getRegisterType() != dxbc::RegisterType::eInput || operand.getIndexDimensions() != 1
        || operand.getIndexOperand(0) != uint32_t(-1) || operand.getIndex(0) >= 32) return false;
    uint32_t systemValue = 0;
    if (opcode != dxbc::OpCode::eDclInputPs) {
      if (instruction.getImmCount() != 1) return false;
      systemValue = instruction.getImm(0).getImmediate<uint32_t>(0);
      if (systemValue != 1) return false;
    }
    const auto interpolation = token.getInterpolationMode();
    if (interpolation < dxbc::InterpolationMode::eConstant ||
        interpolation > dxbc::InterpolationMode::eLinearNoPerspectiveSample) return false;
    const ShaderScalar scalar = systemValue || interpolation != dxbc::InterpolationMode::eConstant
      ? ShaderScalar::Float32 : ShaderScalar::Uint32;
    const uint8_t mask = uint8_t(operand.getWriteMask());
    auto& declaration = declarations[operand.getIndex(0)];
    if (!mask || (declaration.mask & mask) || (declaration.mask &&
        (declaration.systemValue != systemValue || declaration.scalar != scalar))) return false;
    declaration.mask |= mask; declaration.systemValue = systemValue; declaration.scalar = scalar;
  }
  std::vector<ShaderSignatureEntry> candidate;
  for (size_t i = 0; i < inputCount; i++) {
    auto entry = inputs[i];
    const auto& declaration = declarations[entry.registerIndex];
    // Runtime signatures are unions shared across shaders. Undeclared
    // entries do not consume any input and need no upstream producer.
    if (!declaration.mask) continue;
    if ((entry.systemValue != 0 && entry.systemValue != 1) ||
        declaration.systemValue != entry.systemValue ||
        (declaration.mask & entry.mask) != declaration.mask)
      return false;
    // Native signatures can retain components the compiler does not read.
    // Only declared components require a producer and a Vulkan interface.
    entry.mask = declaration.mask;
    entry.scalar = declaration.scalar;
    candidate.push_back(entry);
  }
  // Do not allow the upstream compiler's implicit float fallback for an
  // input declaration absent from the native signature.
  for (uint32_t reg = 0; reg < 32; reg++) if (declarations[reg].mask) {
    uint8_t mask = 0;
    for (const auto& entry : candidate) if (entry.registerIndex == reg) mask = entry.mask;
    if ((mask & declarations[reg].mask) != declarations[reg].mask) return false;
  }
  resolved = std::move(candidate);
  return true;
}

bool linkVertexOutputs(const ShaderSignatureEntry* outputs, size_t outputCount,
    const ShaderSignatureEntry* inputs, size_t inputCount,
    std::vector<ShaderSignatureEntry>& linked) {
  linked.clear();
  if (!outputCount || !validEntries(outputs, outputCount) || !validEntries(inputs, inputCount)) return false;
  std::vector<ShaderSignatureEntry> candidate;
  bool position = false;
  for (size_t i = 0; i < outputCount; i++) {
    auto output = outputs[i];
    if (output.systemValue == 1) {
      if (output.mask != 15) return false;
      position = true; output.scalar = ShaderScalar::Float32;
    } else if (output.systemValue == 0) output.scalar = ShaderScalar::Uint32;
    else return false;
    candidate.push_back(output);
  }
  if (!position) return false;
  for (size_t i = 0; i < inputCount; i++) {
    const auto& input = inputs[i];
    if ((input.systemValue != 0 && input.systemValue != 1) ||
        (input.scalar != ShaderScalar::Float32 && input.scalar != ShaderScalar::Uint32) ||
        (input.systemValue == 1 && input.scalar != ShaderScalar::Float32)) return false;
    bool found = false;
    for (auto& output : candidate) if (output.registerIndex == input.registerIndex) {
      if (output.systemValue != input.systemValue || (output.mask & input.mask) != input.mask) return false;
      output.scalar = input.scalar; found = true;
    }
    if (!found) return false;
  }
  linked = std::move(candidate);
  return true;
}

bool resolveGeometryInputs(const uint32_t* code, size_t words,
    const ShaderSignatureEntry* inputs, size_t inputCount,
    std::vector<ShaderSignatureEntry>& resolved) {
  using namespace dxbc_spv;
  resolved.clear();
  if (!validEntries(inputs, inputCount)) return false;
  std::vector<unsigned char> chunk;
  if (!makeCodeChunk(ShaderStage::Geometry, code, words, chunk)) return false;
  struct Declaration { uint8_t mask = 0; uint32_t systemValue = 0; } declarations[32];
  dxbc::Parser parser(util::ByteReader(chunk.data(), chunk.size()));
  if (!parser.getShaderInfo()) return false;
  while (parser) {
    const auto instruction = parser.parseInstruction();
    if (!instruction) return false;
    const auto opcode = instruction.getOpToken().getOpCode();
    if (opcode != dxbc::OpCode::eDclInput && opcode != dxbc::OpCode::eDclInputSiv
        && opcode != dxbc::OpCode::eDclInputSgv) continue;
    if (instruction.getDstCount() != 1) return false;
    const auto& operand = instruction.getDst(0);
    if (operand.getRegisterType() != dxbc::RegisterType::eInput || operand.getIndexDimensions() != 2
        || operand.getIndexOperand(0) != uint32_t(-1) || operand.getIndexOperand(1) != uint32_t(-1)
        || !operand.getIndex(0) || operand.getIndex(0) > 6 || operand.getIndex(1) >= 32) return false;
    uint32_t systemValue = 0;
    if (opcode != dxbc::OpCode::eDclInput) {
      if (instruction.getImmCount() != 1) return false;
      systemValue = instruction.getImm(0).getImmediate<uint32_t>(0);
      if (systemValue != 1) return false;
    }
    auto& declaration = declarations[operand.getIndex(1)];
    const uint8_t mask = uint8_t(operand.getWriteMask());
    if (!mask || (declaration.mask & mask) ||
        (declaration.mask && declaration.systemValue != systemValue)) return false;
    declaration.mask |= mask; declaration.systemValue = systemValue;
  }
  std::vector<ShaderSignatureEntry> candidate;
  for (size_t i = 0; i < inputCount; i++) {
    auto entry = inputs[i];
    const auto& declaration = declarations[entry.registerIndex];
    if (!declaration.mask) continue;
    if ((entry.systemValue != 0 && entry.systemValue != 1) ||
        declaration.systemValue != entry.systemValue ||
        (declaration.mask & entry.mask) != declaration.mask) return false;
    entry.mask = declaration.mask;
    entry.scalar = entry.systemValue ? ShaderScalar::Float32 : ShaderScalar::Uint32;
    candidate.push_back(entry);
  }
  for (uint32_t reg = 0; reg < 32; reg++) if (declarations[reg].mask) {
    bool found = false;
    for (const auto& entry : candidate) if (entry.registerIndex == reg) found = true;
    if (!found) return false;
  }
  resolved = std::move(candidate);
  return true;
}

bool buildShaderContainer(ShaderStage stage, const uint32_t* code, size_t words,
    const ShaderSignatureEntry* inputs, size_t inputCount,
    const ShaderSignatureEntry* outputs, size_t outputCount,
    std::vector<unsigned char>& container) {
  using namespace dxbc_spv;
  container.clear();
  if (!outputCount || !validEntries(inputs, inputCount) || !validEntries(outputs, outputCount)) return false;
  std::vector<unsigned char> chunkBytes;
  if (!makeCodeChunk(stage, code, words, chunkBytes)) return false;

  dxbc::Signature input(util::FourCC("ISGN"));
  dxbc::Signature output(util::FourCC("OSGN"));
  switch (stage) {
    case ShaderStage::Vertex:
    case ShaderStage::Geometry: {
      if (stage == ShaderStage::Geometry) {
        std::vector<ShaderSignatureEntry> resolved;
        if (!resolveGeometryInputs(code, words, inputs, inputCount, resolved)) return false;
        for (const auto& entry : resolved) {
          input.add(dxbc::SignatureEntry(entry.systemValue ? "SV_Position" : varyingRegisterSemantic,
            entry.systemValue ? 0 : entry.registerIndex, entry.registerIndex, 0,
            uint32_t(entry.mask) | (uint32_t(entry.mask) << 8),
            entry.systemValue ? dxbc::SignatureSysval::ePosition : dxbc::SignatureSysval::eNone,
            scalarType(entry.scalar)));
        }
      } else for (size_t i = 0; i < inputCount; i++) {
        const auto& entry = inputs[i];
        if (entry.systemValue == 6) {
          if (entry.mask != 1) return false;
          input.add(dxbc::SignatureEntry("SV_VertexID", 0, entry.registerIndex, 0, 0x101,
            dxbc::SignatureSysval::eVertexId, ir::ScalarType::eU32));
        } else if (entry.systemValue == 0) {
          const auto type = scalarType(entry.scalar);
          if (type == ir::ScalarType::eUnknown) return false;
          // Both native shader and input layout use register-index semantics.
          // There is no attempt to reconstruct the app's original names.
          input.add(dxbc::SignatureEntry(inputRegisterSemantic, entry.registerIndex, entry.registerIndex,
            0, uint32_t(entry.mask) | (uint32_t(entry.mask) << 8), dxbc::SignatureSysval::eNone, type));
        } else return false;
      }
      bool position = false;
      for (size_t i = 0; i < outputCount; i++) {
        const auto& entry = outputs[i];
        if (entry.systemValue == 1) {
          if (entry.mask != 15) return false;
          position = true;
          output.add(dxbc::SignatureEntry("SV_Position", 0, entry.registerIndex, 0, 15,
            dxbc::SignatureSysval::ePosition, ir::ScalarType::eF32));
        } else if (entry.systemValue == 0 &&
            (entry.scalar == ShaderScalar::Float32 || entry.scalar == ShaderScalar::Uint32)) {
          output.add(dxbc::SignatureEntry(varyingRegisterSemantic, entry.registerIndex,
            entry.registerIndex, 0, entry.mask, dxbc::SignatureSysval::eNone, scalarType(entry.scalar)));
        } else return false;
      }
      if (!position) return false;
      break;
    }
    case ShaderStage::Pixel: {
      if (!validFloatColorOutputs(outputs, outputCount)) return false;
      std::vector<ShaderSignatureEntry> resolved;
      if (!resolvePixelInputs(code, words, inputs, inputCount, resolved)) return false;
      for (const auto& entry : resolved) {
        input.add(dxbc::SignatureEntry(entry.systemValue ? "SV_Position" : varyingRegisterSemantic,
          entry.systemValue ? 0 : entry.registerIndex, entry.registerIndex, 0,
          uint32_t(entry.mask) | (uint32_t(entry.mask) << 8),
          entry.systemValue ? dxbc::SignatureSysval::ePosition : dxbc::SignatureSysval::eNone,
          scalarType(entry.scalar)));
      }
      // Emit register-ordered OSGN entries, preserving sparse SV_Target indices.
      // Windows shader validation expects canonical signature register order.
      std::vector<ShaderSignatureEntry> ordered(outputs, outputs + outputCount);
      std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.registerIndex < b.registerIndex;
      });
      for (const auto& entry : ordered) {
        output.add(dxbc::SignatureEntry("SV_Target", entry.registerIndex,
          entry.registerIndex, 0, entry.mask,
          dxbc::SignatureSysval::eTarget, ir::ScalarType::eF32));
      }
      break;
    }
    default:
      return false;
  }

  // Parser expects a chunk header; the DDI supplies the chunk payload only.
  dxbc::Parser parser(util::ByteReader(chunkBytes.data(), chunkBytes.size()));
  if (!parser.getShaderInfo()) return false;
  while (parser) {
    auto instruction = parser.parseInstruction();
    if (!instruction) return false;
  }
  // Preserve the supplied tokens verbatim. Re-encoding parsed instructions
  // can canonicalize operands and custom flags; container construction does
  // not require rewriting the shader program at all.
  util::ByteWriter writer;
  writer.write(util::FourCC("DXBC"));
  for (unsigned i = 0; i < 4; i++) writer.write(uint32_t(0));
  writer.write(uint32_t(1));
  writer.write(uint32_t(0)); // file size, filled below
  writer.write(uint32_t(3)); // ISGN, OSGN, SHDR
  for (unsigned i = 0; i < 3; i++) writer.write(uint32_t(0));
  const uint32_t inputOffset = uint32_t(writer.moveToEnd());
  if (!input.write(writer)) return false;
  const uint32_t outputOffset = uint32_t(writer.moveToEnd());
  if (!output.write(writer)) return false;
  const uint32_t codeOffset = uint32_t(writer.moveToEnd());
  if (!writer.write(chunkBytes.size(), chunkBytes.data())) return false;
  const uint32_t fileSize = uint32_t(writer.moveToEnd());
  writer.moveTo(24);
  writer.write(fileSize);
  writer.moveTo(32);
  writer.write(inputOffset);
  writer.write(outputOffset);
  writer.write(codeOffset);
  container = std::move(writer).extract();
  const auto checksum = dxbc::hashDxbcBinary(container.data(), container.size());
  std::memcpy(container.data() + offsetof(dxbc::FileHeader, hash), checksum.data.data(), checksum.data.size());
  dxbc::Container rebuilt(container.data(), container.size());
  const auto rebuiltCode = rebuilt.getCodeChunk();
  if (!rebuilt.validateHash() || rebuiltCode.getSize() != (words + 2) * sizeof(uint32_t) ||
      std::memcmp(rebuiltCode.getData(8), code, words * sizeof(uint32_t))) {
    container.clear();
    return false;
  }
  return true;
}

}
