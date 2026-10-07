#include "umd_shader11.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_interface.h>
#include <dxbc/dxbc_parser.h>
#include <dxbc/dxbc_signature.h>

namespace dxbc_spv::dxbc {
util::md5::Digest hashDxbcBinary(const void*, size_t);
}
namespace dxvk::umd {
namespace {
using namespace dxbc_spv;

bool addIo(std::vector<ShaderIo11>& entries, ShaderIo11 entry) {
  if (!entry.mask || (entry.mask & ~15u) || entry.stream >= 4
      || (entry.registerIndex != UINT32_MAX && entry.registerIndex >= 32)) return false;
  for (auto& previous : entries) {
    if (previous.registerIndex != entry.registerIndex || previous.stream != entry.stream) continue;
    if (entry.registerIndex == UINT32_MAX && previous.systemValue != entry.systemValue) continue;
    if (previous.mask & entry.mask) return false;
    if (previous.systemValue == entry.systemValue && previous.scalar == entry.scalar) {
      previous.mask |= entry.mask; return true;
    }
  }
  entries.push_back(entry); return entries.size() <= 128;
}

ShaderScalar systemScalar(uint32_t value) {
  return value == 1 || value == 2 || value == 3 || (value >= 11 && value <= 22)
    || value == 65 || value == 67 || value == 68 ? ShaderScalar::Float32 : ShaderScalar::Uint32;
}
ir::ScalarType scalarType(ShaderScalar value) {
  switch (value) {
    case ShaderScalar::Float32: return ir::ScalarType::eF32;
    case ShaderScalar::Uint32: return ir::ScalarType::eU32;
    case ShaderScalar::Sint32: return ir::ScalarType::eI32;
    default: return ir::ScalarType::eUnknown;
  }
}
bool writeSignature(const std::vector<ShaderIo11>& entries, util::FourCC tag,
    bool vertexInputs, bool input, std::vector<unsigned char>& bytes) {
  dxbc::Signature signature(tag);
  std::array<uint32_t, 4> clip{}, cull{};
  auto sorted = entries;
  std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
    if (a.stream != b.stream) return a.stream < b.stream;
    if (a.registerIndex != b.registerIndex) return a.registerIndex < b.registerIndex;
    return a.mask < b.mask;
  });
  for (const auto& entry : sorted) {
    const char* name = vertexInputs ? inputRegisterSemantic : varyingRegisterSemantic;
    uint32_t index = entry.registerIndex;
    auto system = dxbc::SignatureSysval::eNone;
    switch (entry.systemValue) {
      case 0: break;
      case 1: name = "SV_Position"; index = 0; system = dxbc::SignatureSysval::ePosition; break;
      case 2: name = "SV_ClipDistance"; index = entry.semanticIndex == UINT32_MAX ? clip[entry.stream]++ : entry.semanticIndex; system = dxbc::SignatureSysval::eClipDistance; break;
      case 3: name = "SV_CullDistance"; index = entry.semanticIndex == UINT32_MAX ? cull[entry.stream]++ : entry.semanticIndex; system = dxbc::SignatureSysval::eCullDistance; break;
      case 4: name = "SV_RenderTargetArrayIndex"; index = 0; system = dxbc::SignatureSysval::eRenderTargetArrayIndex; break;
      case 5: name = "SV_ViewportArrayIndex"; index = 0; system = dxbc::SignatureSysval::eViewportIndex; break;
      case 6: name = "SV_VertexID"; index = 0; system = dxbc::SignatureSysval::eVertexId; break;
      case 7: name = "SV_PrimitiveID"; index = 0; system = dxbc::SignatureSysval::ePrimitiveId; break;
      case 8: name = "SV_InstanceID"; index = 0; system = dxbc::SignatureSysval::eInstanceId; break;
      case 9: name = "SV_IsFrontFace"; index = 0; system = dxbc::SignatureSysval::eIsFrontFace; break;
      case 10: name = "SV_SampleIndex"; index = 0; system = dxbc::SignatureSysval::eSampleIndex; break;
      case 11: case 12: case 13: case 14:
        name = "SV_TessFactor"; index = entry.systemValue - 11; system = dxbc::SignatureSysval::eQuadEdgeTessFactor; break;
      case 15: case 16:
        name = "SV_InsideTessFactor"; index = entry.systemValue - 15; system = dxbc::SignatureSysval::eQuadInsideTessFactor; break;
      case 17: case 18: case 19:
        name = "SV_TessFactor"; index = entry.systemValue - 17; system = dxbc::SignatureSysval::eTriEdgeTessFactor; break;
      case 20: name = "SV_InsideTessFactor"; index = 0; system = dxbc::SignatureSysval::eTriInsideTessFactor; break;
      case 21: name = "SV_TessFactor"; index = 0; system = dxbc::SignatureSysval::eLineDetailTessFactor; break;
      case 22: name = "SV_TessFactor"; index = 1; system = dxbc::SignatureSysval::eLineDensityTessFactor; break;
      case 64: name = "SV_Target"; system = dxbc::SignatureSysval::eTarget; break;
      case 65: name = "SV_Depth"; index = 0; system = dxbc::SignatureSysval::eDepth; break;
      case 66: name = "SV_Coverage"; index = 0; system = dxbc::SignatureSysval::eCoverage; break;
      case 67: name = "SV_DepthGreaterEqual"; index = 0; system = dxbc::SignatureSysval::eDepthGreaterEqual; break;
      case 68: name = "SV_DepthLessEqual"; index = 0; system = dxbc::SignatureSysval::eDepthLessEqual; break;
      default: return false;
    }
    const auto type = scalarType(entry.scalar);
    if (type == ir::ScalarType::eUnknown) return false;
    signature.add(dxbc::SignatureEntry(name, index, int32_t(entry.registerIndex), entry.stream,
      uint32_t(entry.mask) | (input ? uint32_t(entry.mask) << 8 : 0), system, type));
  }
  util::ByteWriter writer;
  if (!signature.write(writer)) return false;
  bytes = std::move(writer).extract(); return true;
}

bool interfaceChunk(const ShaderCode11& shader, std::vector<unsigned char>& bytes) {
  std::vector<uint32_t> tables;
  for (const auto& iface : shader.interfaces) for (uint32_t table : iface.tables)
    if (std::find(tables.begin(), tables.end(), table) == tables.end()) tables.push_back(table);
  if (tables.size() > UINT16_MAX) return false;
  // IFCE offsets are relative to its payload, excluding the chunk header.
  util::ByteWriter writer;
  writer.write(util::FourCC("IFCE")); writer.write(uint32_t(0));
  const size_t base = writer.moveToEnd();
  writer.write(uint32_t(0)); writer.write(uint32_t(tables.size()));
  writer.write(shader.interfaceSlots); writer.write(shader.interfaceSlots); writer.write(uint32_t(0));
  writer.write(uint32_t(28)); writer.write(uint32_t(28 + tables.size() * 12));
  const auto typeOffset = writer.moveToEnd();
  for (size_t i = 0; i < tables.size(); ++i) {
    writer.write(uint32_t(0)); writer.write(uint16_t(i));
    writer.write(uint16_t(0)); writer.write(uint16_t(0)); writer.write(uint16_t(0));
  }
  const auto slotOffset = writer.moveToEnd();
  for (uint32_t i = 0; i < shader.interfaceSlots; ++i) {
    writer.write(uint32_t(1)); writer.write(uint32_t(0));
    writer.write(uint32_t(0)); writer.write(uint32_t(0));
  }
  for (size_t i = 0; i < tables.size(); ++i) {
    const auto name = shader11ClassName(tables[i]);
    const uint32_t offset = uint32_t(writer.moveToEnd() - base);
    if (!writer.write(name.size() + 1, name.c_str())) return false;
    writer.moveTo(typeOffset + i * 12); writer.write(offset);
  }
  for (uint32_t slot = 0; slot < shader.interfaceSlots; ++slot) {
    const ShaderInterface11* iface = nullptr;
    for (const auto& candidate : shader.interfaces)
      if (slot >= candidate.first && slot - candidate.first < candidate.count) iface = &candidate;
    const uint32_t count = iface ? uint32_t(iface->tables.size()) : 0;
    const uint32_t types = uint32_t(writer.moveToEnd() - base);
    if (iface) for (uint32_t table : iface->tables)
      writer.write(uint16_t(std::find(tables.begin(), tables.end(), table) - tables.begin()));
    while (writer.moveToEnd() % 4) writer.write(uint8_t(0));
    const uint32_t ids = uint32_t(writer.moveToEnd() - base);
    if (iface) for (uint32_t table : iface->tables) writer.write(table);
    writer.moveTo(slotOffset + slot * 16 + 4); writer.write(count); writer.write(types); writer.write(ids);
  }
  while (writer.moveToEnd() % 4) writer.write(uint8_t(0));
  const uint32_t size = uint32_t(writer.moveToEnd() - 8);
  writer.moveTo(4); writer.write(size); bytes = std::move(writer).extract();
  dxbc::InterfaceChunk parsed(util::ByteReader(bytes.data(), bytes.size()));
  return bool(parsed);
}
}

std::string shader11ClassName(uint32_t table) { return "VIOGPU_TABLE_" + std::to_string(table); }
bool shader11ClassPointer(uint32_t cb, uint32_t byteOffset, uint32_t texture,
    uint32_t sampler, uint32_t& vectorOffset) {
  if (cb >= 14 || byteOffset >= 4096 || byteOffset % 16 || texture >= 128 || sampler >= 16) return false;
  vectorOffset = byteOffset / 16; return true;
}
bool shader11InterfaceTable(const ShaderCode11& shader, uint32_t slot, uint32_t table) {
  for (const auto& iface : shader.interfaces)
    if (slot >= iface.first && slot - iface.first < iface.count)
      return std::find(iface.tables.begin(), iface.tables.end(), table) != iface.tables.end();
  return false;
}

bool linkShader11Outputs(const std::vector<ShaderIo11>& original,
    const std::vector<ShaderIo11>& inputs, uint32_t stream, std::vector<ShaderIo11>& output) {
  output.clear();
  if (stream >= 4 || original.size() > 128 || inputs.size() > 128) return false;
  auto staged = original;
  for (const auto& input : inputs) {
    if (!input.mask || (input.mask & ~15u) || input.scalar == ShaderScalar::Unknown) return false;
    // These values are supplied by fixed pipeline stages when a previous
    // shader does not explicitly output them. Position uses clip-space on
    // output and screen-space on PS input, so its masks need not match.
    if (input.systemValue == 1 || (input.systemValue >= 6 && input.systemValue <= 10)) continue;
    uint8_t remaining = input.mask;
    for (size_t i = 0, count = staged.size(); i < count; ++i) {
      auto& entry = staged[i];
      if (entry.stream != stream || entry.systemValue != input.systemValue) continue;
      if (!input.systemValue && entry.registerIndex != input.registerIndex) continue;
      if (input.systemValue && entry.semanticIndex != input.semanticIndex) continue;
      const auto mask = uint8_t(entry.mask & remaining);
      if (!mask) continue;
      remaining &= ~mask;
      if (entry.scalar == input.scalar) continue;
      if (entry.systemValue) return false;
      auto part = entry; part.mask = mask; part.scalar = input.scalar;
      if (mask == entry.mask) entry.scalar = input.scalar;
      else { entry.mask &= ~mask; staged.push_back(part); }
    }
    if (remaining) return false;
  }
  output = std::move(staged); return true;
}

bool shader11StreamOutput(const ShaderCode11& shader,
    const ShaderStreamDeclaration11* entries, size_t count,
    const uint32_t* strides, size_t strideCount, uint32_t rasterizedStream,
    ShaderStreamOutput11& output) {
  using namespace dxbc_spv;
  output = {};
  if (count > 512 || (count && !entries) || strideCount > 4 || (strideCount && !strides)
      || (rasterizedStream >= 4 && rasterizedStream != UINT32_MAX)) return false;
  std::vector<unsigned char> binary;
  if (!buildShader11Container(shader, binary)) return false;
  dxbc::Container container(binary.data(), binary.size());
  dxbc::Signature signature(container.getOutputSignatureChunk());
  if (!signature) return false;
  ShaderStreamOutput11 staged; staged.rasterizedStream = rasterizedStream;
  std::array<uint32_t, 4> slotStreams; slotStreams.fill(UINT32_MAX);
  std::array<uint32_t, 4> streamComponents{};
  for (size_t i = 0; i < count; ++i) {
    const auto& native = entries[i];
    if (native.stream >= 4 || native.slot >= 4 || !native.mask || (native.mask & ~15u)
        || (slotStreams[native.slot] != UINT32_MAX && slotStreams[native.slot] != native.stream)) return false;
    slotStreams[native.slot] = native.stream;
    const bool gap = native.registerIndex == UINT32_MAX;
    if (!gap && native.registerIndex >= 32) return false;
    for (uint32_t component = 0; component < 4;) {
      if (!(native.mask & (1u << component))) { ++component; continue; }
      const dxbc::SignatureEntry* matched = nullptr;
      if (!gap) {
        for (const auto& entry : signature)
          if (entry.getStreamIndex() == native.stream && uint32_t(entry.getRegisterIndex()) == native.registerIndex
              && (uint8_t(entry.getComponentMask()) & (1u << component))) {
            if (matched) return false;
            matched = &entry;
          }
        if (!matched) return false;
      }
      uint32_t end = component + 1;
      while (end < 4 && (native.mask & (1u << end))
          && (!matched || (uint8_t(matched->getComponentMask()) & (1u << end)))) ++end;
      ShaderStreamEntry11 entry;
      entry.stream = native.stream; entry.slot = uint8_t(native.slot);
      entry.start = gap ? 0 : uint8_t(component); entry.count = uint8_t(end - component);
      if (matched) { entry.semantic = matched->getSemanticName(); entry.semanticIndex = matched->getSemanticIndex(); }
      staged.entries.push_back(std::move(entry));
      streamComponents[native.stream] += end - component;
      staged.strides[native.slot] += (end - component) * 4;
      if (streamComponents[native.stream] > 128 || staged.strides[native.slot] > 512) return false;
      staged.strideCount = std::max(staged.strideCount, native.slot + 1);
      component = end;
    }
  }
  for (size_t i = 0; i < strideCount; ++i) {
    if (strides[i] % 4 || strides[i] > 2048 || strides[i] < staged.strides[i]) return false;
    staged.strides[i] = strides[i];
  }
  staged.strideCount = std::max(staged.strideCount, uint32_t(strideCount));
  output = std::move(staged); return true;
}

bool decodeShader11(ShaderStage stage, const uint32_t* code, size_t words, ShaderCode11& output) {
  using namespace dxbc_spv;
  output = {};
  if (!code || words < 3 || words > 1024 * 1024 || code[1] != words
      || uint32_t(stage) > 5 || (code[0] >> 16) != uint32_t(stage)) return false;
  const uint32_t version = code[0] & 0xffff;
  if (version != 0x40 && version != 0x41 && version != 0x50) return false;
  if (stage >= ShaderStage::Hull && version != 0x50) return false;
  for (size_t offset = 2; offset < words;) {
    const uint32_t opcode = code[offset] & 0x7ff;
    uint32_t length = (code[offset] >> 24) & 0x7f;
    if (opcode == 107 || opcode == 112 || opcode > (version == 0x50 ? 206u : version == 0x41 ? 111u : 106u)) return false;
    if (opcode == uint32_t(dxbc::OpCode::eCustomData)) {
      if (words - offset < 2) return false;
      length = code[offset + 1];
      if (length < 2) return false;
    }
    if (!length || length > words - offset) return false;
    offset += length;
  }
  ShaderCode11 candidate; candidate.stage = stage; candidate.tokens.assign(code, code + words);
  util::ByteWriter chunk; chunk.write(util::FourCC(version == 0x50 ? "SHEX" : "SHDR"));
  chunk.write(uint32_t(words * 4)); for (size_t i = 0; i < words; ++i) chunk.write(code[i]);
  auto bytes = std::move(chunk).extract();
  dxbc::Parser parser(util::ByteReader(bytes.data(), bytes.size()));
  if (!parser.getShaderInfo()) return false;
  uint32_t stream = 0;
  bool patchPhase = false, computeGroup = false;
  std::vector<uint32_t> tables;
  while (parser) {
    const auto instruction = parser.parseInstruction(); if (!instruction) return false;
    const auto token = instruction.getOpToken(); const auto op = token.getOpCode();
    if (op == dxbc::OpCode::eDclStream) {
      if (stage != ShaderStage::Geometry || instruction.getDstCount() != 1) return false;
      const auto& operand = instruction.getDst(0);
      if (operand.getRegisterType() != dxbc::RegisterType::eStream || operand.getIndexDimensions() != 1
          || operand.getIndex(0) >= 4) return false;
      stream = operand.getIndex(0); continue;
    }
    if (op == dxbc::OpCode::eHsControlPointPhase) { candidate.hullControlPhase = true; patchPhase = false; continue; }
    if (op == dxbc::OpCode::eHsForkPhase || op == dxbc::OpCode::eHsJoinPhase) { patchPhase = true; continue; }
    if (op == dxbc::OpCode::eDclFunctionTable) {
      if (instruction.getImmCount() != 2 || instruction.getImm(1).getImmediate<uint32_t>(0) != instruction.getExtraCount()) return false;
      const uint32_t table = instruction.getImm(0).getImmediate<uint32_t>(0);
      if (std::find(tables.begin(), tables.end(), table) != tables.end()) return false;
      tables.push_back(table); continue;
    }
    if (op == dxbc::OpCode::eDclInterface) {
      if (instruction.getImmCount() != 3) return false;
      ShaderInterface11 iface; iface.first = instruction.getImm(0).getImmediate<uint32_t>(0);
      const uint32_t metadata = instruction.getImm(2).getImmediate<uint32_t>(0);
      iface.count = metadata >> 16; const uint32_t count = metadata & 0xffff;
      if (!iface.count || iface.first > 253 || iface.count > 253 - iface.first
          || !count || count != instruction.getExtraCount()) return false;
      for (uint32_t i = 0; i < count; ++i) iface.tables.push_back(instruction.getExtra(i).getImmediate<uint32_t>(0));
      for (const auto& previous : candidate.interfaces)
        if (iface.first < previous.first + previous.count && previous.first < iface.first + iface.count) return false;
      candidate.interfaceSlots = std::max(candidate.interfaceSlots, iface.first + iface.count);
      candidate.interfaces.push_back(std::move(iface)); continue;
    }
    if (op == dxbc::OpCode::eDclThreadGroup) {
      if (stage != ShaderStage::Compute || computeGroup || instruction.getImmCount() != 3) return false;
      const uint32_t x = instruction.getImm(0).getImmediate<uint32_t>(0);
      const uint32_t y = instruction.getImm(1).getImmediate<uint32_t>(0);
      const uint32_t z = instruction.getImm(2).getImmediate<uint32_t>(0);
      if (!x || !y || !z || x > 1024 || y > 1024 || z > 64 || uint64_t(x) * y * z > 1024) return false;
      computeGroup = true; continue;
    }
    const bool input = op == dxbc::OpCode::eDclInput || op == dxbc::OpCode::eDclInputSgv
      || op == dxbc::OpCode::eDclInputSiv || op == dxbc::OpCode::eDclInputPs
      || op == dxbc::OpCode::eDclInputPsSgv || op == dxbc::OpCode::eDclInputPsSiv;
    const bool result = op == dxbc::OpCode::eDclOutput || op == dxbc::OpCode::eDclOutputSgv
      || op == dxbc::OpCode::eDclOutputSiv;
    if (!input && !result) continue;
    if (instruction.getDstCount() != 1) return false;
    const auto& operand = instruction.getDst(0); const auto type = operand.getRegisterType();
    ShaderIo11 entry; entry.mask = uint8_t(operand.getWriteMask()); entry.stream = result ? stream : 0;
    if (instruction.getImmCount()) entry.systemValue = instruction.getImm(0).getImmediate<uint32_t>(0);
    if (entry.systemValue > 22) return false;
    bool dedicated = false;
    switch (type) {
      case dxbc::RegisterType::eDepth: entry.systemValue = 65; dedicated = true; break;
      case dxbc::RegisterType::eDepthGe: entry.systemValue = 67; dedicated = true; break;
      case dxbc::RegisterType::eDepthLe: entry.systemValue = 68; dedicated = true; break;
      case dxbc::RegisterType::eCoverageOut: entry.systemValue = 66; dedicated = true; break;
      case dxbc::RegisterType::ePrimitiveId: entry.systemValue = 7; dedicated = true; break;
      case dxbc::RegisterType::eCoverageIn:
      case dxbc::RegisterType::eControlPointId:
      case dxbc::RegisterType::eTessCoord:
      case dxbc::RegisterType::eGsInstanceId: continue;
      case dxbc::RegisterType::eInput: case dxbc::RegisterType::eOutput:
      case dxbc::RegisterType::eControlPointIn: case dxbc::RegisterType::eControlPointOut:
      case dxbc::RegisterType::ePatchConstant: break;
      default: return false;
    }
    if (dedicated) { entry.registerIndex = UINT32_MAX; entry.mask = 1; }
    else {
      const uint32_t dimensions = operand.getIndexDimensions();
      if (!dimensions || dimensions > 2 || operand.getIndexOperand(dimensions - 1) != UINT32_MAX) return false;
      entry.registerIndex = operand.getIndex(dimensions - 1);
    }
    entry.scalar = entry.systemValue ? systemScalar(entry.systemValue)
      : stage == ShaderStage::Vertex && input ? ShaderScalar::Float32 : ShaderScalar::Uint32;
    if (stage == ShaderStage::Pixel) {
      if (result && !entry.systemValue) { entry.systemValue = 64; entry.scalar = ShaderScalar::Float32; }
      if (input && !entry.systemValue && token.getInterpolationMode() != dxbc::InterpolationMode::eConstant)
        entry.scalar = ShaderScalar::Float32;
    }
    auto& entries = type == dxbc::RegisterType::ePatchConstant || (stage == ShaderStage::Hull && result && patchPhase)
      ? candidate.patch : input ? candidate.inputs : candidate.outputs;
    if (!addIo(entries, entry)) return false;
  }
  if (stage == ShaderStage::Compute && !computeGroup) return false;
  for (const auto& iface : candidate.interfaces) for (uint32_t table : iface.tables)
    if (std::find(tables.begin(), tables.end(), table) == tables.end()) return false;
  output = std::move(candidate); return true;
}

bool buildShader11Container(const ShaderCode11& shader, std::vector<unsigned char>& output) {
  using namespace dxbc_spv;
  output.clear();
  if (shader.tokens.empty()) return false;
  std::vector<std::vector<unsigned char>> chunks;
  chunks.resize(shader.stage == ShaderStage::Hull || shader.stage == ShaderStage::Domain ? 3 : 2);
  if (!writeSignature(shader.inputs, util::FourCC("ISGN"), shader.stage == ShaderStage::Vertex, true, chunks[0])
      || !writeSignature(shader.outputs, util::FourCC(shader.stage == ShaderStage::Geometry ? "OSG5" : "OSGN"), false, false, chunks[1])) return false;
  if (chunks.size() == 3 && !writeSignature(shader.patch, util::FourCC("PCSG"), false,
      shader.stage == ShaderStage::Domain, chunks[2])) return false;
  if (shader.interfaceSlots) {
    chunks.emplace_back(); if (!interfaceChunk(shader, chunks.back())) return false;
  }
  util::ByteWriter code; code.write(util::FourCC((shader.tokens[0] & 0xff) == 0x50 ? "SHEX" : "SHDR"));
  code.write(uint32_t(shader.tokens.size() * 4));
  for (uint32_t token : shader.tokens) code.write(token);
  chunks.push_back(std::move(code).extract());
  util::ByteWriter writer; writer.write(util::FourCC("DXBC"));
  for (unsigned i = 0; i < 4; ++i) writer.write(uint32_t(0));
  writer.write(uint32_t(1)); writer.write(uint32_t(0)); writer.write(uint32_t(chunks.size()));
  for (size_t i = 0; i < chunks.size(); ++i) writer.write(uint32_t(0));
  for (size_t i = 0; i < chunks.size(); ++i) {
    const uint32_t offset = uint32_t(writer.moveToEnd());
    if (!writer.write(chunks[i].size(), chunks[i].data())) return false;
    writer.moveTo(32 + i * 4); writer.write(offset);
  }
  const uint32_t size = uint32_t(writer.moveToEnd()); writer.moveTo(24); writer.write(size);
  output = std::move(writer).extract();
  const auto hash = dxbc::hashDxbcBinary(output.data(), output.size());
  std::memcpy(output.data() + offsetof(dxbc::FileHeader, hash), hash.data.data(), hash.data.size());
  dxbc::Container check(output.data(), output.size());
  if (!check.validateHash() || check.getCodeChunk().getSize() != (shader.tokens.size() + 2) * 4
      || std::memcmp(check.getCodeChunk().getData(8), shader.tokens.data(), shader.tokens.size() * 4)) {
    output.clear(); return false;
  }
  return true;
}
}
