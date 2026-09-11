#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxvk::umd {

enum class ShaderStage : uint32_t { Pixel = 0, Vertex = 1 };
enum class ShaderScalar : uint8_t { Unknown, Float32, Uint32, Sint32 };
struct ShaderSignatureEntry {
  uint32_t systemValue;
  uint32_t registerIndex;
  uint8_t mask;
  ShaderScalar scalar = ShaderScalar::Unknown;
};

inline constexpr const char* inputRegisterSemantic = "VIOGPU_INPUT";
inline constexpr const char* varyingRegisterSemantic = "VIOGPU_VARYING";

// Native signatures lack scalar types. Pixel declarations supply enough
// information for interpolated F32 or bit-preserving flat U32 interfaces.
// A register with conflicting interpolation/types is rejected for now.
bool resolvePixelInputs(const uint32_t* code, size_t words,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  std::vector<ShaderSignatureEntry>& resolved);

// Match producer outputs to the active PS by register/mask, including
// SV_Position. Unconsumed generic outputs use a raw32 interface.
bool linkVertexOutputs(const ShaderSignatureEntry* outputs, size_t outputCount,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  std::vector<ShaderSignatureEntry>& linked);

// SM4.0 VS/PS development profile. Vertex input types come from the bound
// layout, generic outputs from linkVertexOutputs, and pixel input types from
// resolvePixelInputs. PS has one float target0. Other stages, integer targets
// and additional system values are not accepted. Tokens remain unchanged.
bool buildShaderContainer(ShaderStage stage, const uint32_t* code, size_t words,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  const ShaderSignatureEntry* outputs, size_t outputCount,
  std::vector<unsigned char>& container);

}
