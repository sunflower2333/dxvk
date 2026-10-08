#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxvk::umd {

enum class ShaderStage : uint32_t { Pixel = 0, Vertex = 1, Geometry = 2, Hull = 3, Domain = 4, Compute = 5 };
enum class ShaderScalar : uint8_t { Unknown, Float32, Uint32, Sint32 };
struct ShaderSignatureEntry {
  uint32_t systemValue;
  uint32_t registerIndex;
  uint8_t mask;
  ShaderScalar scalar = ShaderScalar::Unknown;
};

inline constexpr const char* inputRegisterSemantic = "VIOGPU_INPUT";
inline constexpr const char* varyingRegisterSemantic = "VIOGPU_VARYING";

// Legacy typed VS/GS/PS callbacks carry SM4 tokens. The logical native
// device, rather than a broader embedded backend, decides whether 4.1 is
// permitted. Newer shader models use their separate typed shader path.
inline constexpr bool validLegacyShaderVersion(ShaderStage stage,
    uint32_t token, bool allow4_1) {
  return uint32_t(stage) <= uint32_t(ShaderStage::Geometry)
    && (token == ((uint32_t(stage) << 16) | 0x40)
      || (allow4_1 && token == ((uint32_t(stage) << 16) | 0x41)));
}

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

// Match producer outputs to the next stage by register/mask. SV_Position is
// required only for rasterized output: SO and the VS feeding a GS may carry
// data-only registers. Unconsumed generic outputs use a raw32 interface.
bool linkVertexOutputs(const ShaderSignatureEntry* outputs, size_t outputCount,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  std::vector<ShaderSignatureEntry>& linked, bool rasterizedOutput = true);

// SM4.0/4.1 VS/GS/PS development profile. VS input types come from the bound
// layout, GS inputs from resolveGeometryInputs, generic outputs from the
// next active stage, and PS inputs from resolvePixelInputs. PS supports up to
// eight float color outputs, preserving sparse target indices and masks.
// SM4.1 PS sample-index inputs and sample interpolation retain their built-in
// or interpolated semantics. Integer/depth outputs and other additional
// system values remain unsupported.
// Non-rasterized VS/GS output may omit position; if present it must still
// have its full four-component mask. Tokens remain unchanged; this does not
// advertise a native feature level.
bool buildShaderContainer(ShaderStage stage, const uint32_t* code, size_t words,
  const ShaderSignatureEntry* inputs, size_t inputCount,
  const ShaderSignatureEntry* outputs, size_t outputCount,
  std::vector<unsigned char>& container, bool rasterizedOutput = true);

// Compute has no IA/varying signatures. Validate bounded SM5.0 instructions
// with the upstream parser and retain the original code in a SHEX container.
bool buildComputeContainer(const uint32_t* code, size_t words,
  std::vector<unsigned char>& container);

}
