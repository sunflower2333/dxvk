#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxvk::umd {

enum class ShaderStage : uint32_t { Pixel = 0, Vertex = 1 };
struct ShaderSignatureEntry {
  uint32_t systemValue;
  uint32_t registerIndex;
  uint8_t mask;
};

// Initial DDI development profile: SM4.0 VS with optional SV_VertexID and
// SV_Position output, and PS with no inputs and one float color output.
// User varyings, integer render targets and all other stages are not accepted.
// DDI signatures do not carry semantic strings or component types: these
// restricted interfaces have known types, so no arbitrary type is invented.
bool buildShaderContainer(ShaderStage stage, const uint32_t* code, size_t words,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  const ShaderSignatureEntry* outputs, size_t outputCount,
  std::vector<unsigned char>& container);

}
