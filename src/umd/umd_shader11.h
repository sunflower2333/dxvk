#pragma once
#include "umd_shader.h"
#include <array>
#include <string>

namespace dxvk::umd {

// Native SM5 signatures omit names, scalar types and stream indices. The raw
// declarations identify actual register use; names below are stable by register.
struct ShaderIo11 {
  uint32_t systemValue = 0;
  uint32_t registerIndex = 0;
  uint8_t mask = 0;
  ShaderScalar scalar = ShaderScalar::Unknown;
  uint32_t stream = 0;
  uint32_t semanticIndex = UINT32_MAX;
};
struct ShaderInterface11 {
  uint32_t first = 0;
  uint32_t count = 0;
  std::vector<uint32_t> tables;
};
struct ShaderCode11 {
  ShaderStage stage = ShaderStage::Vertex;
  std::vector<uint32_t> tokens;
  std::vector<ShaderIo11> inputs, outputs, patch;
  std::vector<ShaderInterface11> interfaces;
  uint32_t interfaceSlots = 0;
  bool hullControlPhase = false;
};

struct ShaderStreamDeclaration11 {
  uint32_t stream = 0, slot = 0, registerIndex = 0;
  uint8_t mask = 0;
};
struct ShaderStreamEntry11 {
  uint32_t stream = 0, semanticIndex = 0;
  std::string semantic;
  uint8_t start = 0, count = 0, slot = 0;
};
struct ShaderStreamOutput11 {
  std::vector<ShaderStreamEntry11> entries;
  std::array<uint32_t, 4> strides{};
  uint32_t strideCount = 0;
  uint32_t rasterizedStream = UINT32_MAX;
};

bool decodeShader11(ShaderStage stage, const uint32_t* code, size_t words, ShaderCode11& output);
bool buildShader11Container(const ShaderCode11& shader, std::vector<unsigned char>& output);
std::string shader11ClassName(uint32_t table);
bool shader11InterfaceTable(const ShaderCode11& shader, uint32_t slot, uint32_t table);
// The native DDI pointer stores a byte offset. The D3D11 class API stores a
// constant-vector offset, so conversion requires a 16-byte aligned byte offset.
bool shader11ClassPointer(uint32_t cb, uint32_t byteOffset, uint32_t texture,
  uint32_t sampler, uint32_t& vectorOffset);
// Split a register's output masks where the downstream shader consumes
// different scalar types. Other streams retain their raw bit representation.
bool linkShader11Outputs(const std::vector<ShaderIo11>& original,
  const std::vector<ShaderIo11>& inputs, uint32_t stream, std::vector<ShaderIo11>& output);
bool shader11StreamOutput(const ShaderCode11& shader,
  const ShaderStreamDeclaration11* entries, size_t count,
  const uint32_t* strides, size_t strideCount, uint32_t rasterizedStream,
  ShaderStreamOutput11& output);
// A null-code SO object has an output signature without shader instructions.
// Resolve its declarations without constructing an invented shader program.
bool shader11StreamOutput(const std::vector<ShaderIo11>& signature,
  const ShaderStreamDeclaration11* entries, size_t count,
  const uint32_t* strides, size_t strideCount, uint32_t rasterizedStream,
  ShaderStreamOutput11& output);

}
