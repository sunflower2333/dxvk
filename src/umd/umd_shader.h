#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxvk::umd {

enum class ShaderStage : uint32_t { Pixel = 0, Vertex = 1, Geometry = 2 };
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

// GS inputs are per-vertex arrays without interpolation. Preserve generic
// registers as raw32 and position as F32; validate declarations before the
// compiler can invent an interface for an absent runtime signature entry.
bool resolveGeometryInputs(const uint32_t* code, size_t words,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  std::vector<ShaderSignatureEntry>& resolved);

// Match producer outputs to the active PS by register/mask, including
// SV_Position. Unconsumed generic outputs use a raw32 interface.
bool linkVertexOutputs(const ShaderSignatureEntry* outputs, size_t outputCount,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  std::vector<ShaderSignatureEntry>& linked);

// SM4.0 VS/GS/PS development profile. VS input types come from the bound
// layout, GS inputs from resolveGeometryInputs, generic outputs from the
// next active stage, and PS inputs from resolvePixelInputs. PS supports up to
// eight float color outputs, preserving sparse target indices and masks.
// Integer/depth outputs and additional system values remain unsupported.
// Tokens remain unchanged; this does not advertise a native feature level.
bool buildShaderContainer(ShaderStage stage, const uint32_t* code, size_t words,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  const ShaderSignatureEntry* outputs, size_t outputCount,
  std::vector<unsigned char>& container);

}
