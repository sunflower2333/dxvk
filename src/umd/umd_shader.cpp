#include "umd_shader.h"

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

bool buildShaderContainer(ShaderStage stage, const uint32_t* code, size_t words,
    const ShaderSignatureEntry* inputs, size_t inputCount,
    const ShaderSignatureEntry* outputs, size_t outputCount,
    std::vector<unsigned char>& container) {
  using namespace dxbc_spv;
  container.clear();
  if (!code || words < 3 || words > 1024 * 1024 || code[1] != words ||
      code[0] != ((uint32_t(stage) << 16) | 0x40) ||
      outputCount != 1 || !outputs || outputs[0].registerIndex != 0 ||
      outputs[0].mask != 15 || inputCount > 32 || (inputCount && !inputs))
    return false;

  // Bound each instruction before giving it to the upstream parser. The
  // parser accepts unknown opcodes with an empty layout, so it is not by
  // itself an SM4 bytecode validator. Custom-data/ICB reconstruction remains
  // outside this initial profile.
  for (size_t offset = 2; offset < words;) {
    const uint32_t opcode = code[offset] & 0x7ff;
    const uint32_t count = (code[offset] >> 24) & 0x7f;
    if (opcode > uint32_t(dxbc::OpCode::eDclGlobalFlags) ||
        opcode == uint32_t(dxbc::OpCode::eCustomData) || !count || count > words - offset)
      return false;
    offset += count;
  }

  dxbc::Signature input(util::FourCC("ISGN"));
  dxbc::Signature output(util::FourCC("OSGN"));
  switch (stage) {
    case ShaderStage::Vertex:
      if (outputs[0].systemValue != 1) return false;
      for (size_t i = 0; i < inputCount; i++) {
        const auto& entry = inputs[i];
        if (entry.registerIndex >= 32 || !entry.mask || (entry.mask & ~15)) return false;
        for (size_t j = 0; j < i; j++)
          if (inputs[j].registerIndex == entry.registerIndex
              || (entry.systemValue && inputs[j].systemValue == entry.systemValue)) return false;
        if (entry.systemValue == 6) {
          if (entry.mask != 1) return false;
          input.add(dxbc::SignatureEntry("SV_VertexID", 0, entry.registerIndex, 0, 1,
            dxbc::SignatureSysval::eVertexId, ir::ScalarType::eU32));
        } else if (entry.systemValue == 0) {
          ir::ScalarType type;
          switch (entry.scalar) {
            case ShaderScalar::Float32: type = ir::ScalarType::eF32; break;
            case ShaderScalar::Uint32: type = ir::ScalarType::eU32; break;
            case ShaderScalar::Sint32: type = ir::ScalarType::eI32; break;
            default: return false;
          }
          // Both native shader and input layout use register-index semantics.
          // There is no attempt to reconstruct the app's original names.
          input.add(dxbc::SignatureEntry(inputRegisterSemantic, entry.registerIndex, entry.registerIndex,
            0, entry.mask, dxbc::SignatureSysval::eNone, type));
        } else return false;
      }
      output.add(dxbc::SignatureEntry("SV_Position", 0, 0, 0, 15,
        dxbc::SignatureSysval::ePosition, ir::ScalarType::eF32));
      break;
    case ShaderStage::Pixel:
      if (inputCount || outputs[0].systemValue != 0) return false;
      output.add(dxbc::SignatureEntry("SV_Target", 0, 0, 0, 15,
        dxbc::SignatureSysval::eTarget, ir::ScalarType::eF32));
      break;
    default:
      return false;
  }

  // Parser expects a chunk header; the DDI supplies the chunk payload only.
  util::ByteWriter chunk;
  chunk.write(util::FourCC("SHDR"));
  chunk.write(uint32_t(words * sizeof(uint32_t)));
  for (size_t i = 0; i < words; i++) chunk.write(code[i]);
  auto chunkBytes = std::move(chunk).extract();
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
