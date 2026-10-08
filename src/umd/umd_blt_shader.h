#pragma once
// SPDX-License-Identifier: MIT
#include "umd_shader.h"
#include <array>

namespace dxvk::umd {

// Internal SM4 pass-through programs. These do not call a system compiler or
// change the application's shaders. Inputs are float4 position + float2 UV;
// the pixel program samples level zero of a single normalized 2D scratch.
inline constexpr uint32_t bltVertexTokens[] = {
  0x00010040, 26,
  0x0300005f, 0x001010f2, 0,
  0x0300005f, 0x00101032, 1,
  0x04000067, 0x001020f2, 0, 1,
  0x03000065, 0x00102032, 1,
  0x05000036, 0x001020f2, 0, 0x00101e46, 0,
  0x05000036, 0x00102032, 1, 0x00101046, 1,
  0x0100003e,
};
inline constexpr uint32_t bltPixelTokens[] = {
  0x00000040, 27,
  0x04001858, 0x00107000, 0, 0x00005555,
  0x0300005a, 0x00106000, 0,
  0x03001062, 0x00101032, 1,
  0x03000065, 0x001020f2, 0,
  0x0b000048, 0x001020f2, 0, 0x00101046, 1,
    0x00107e46, 0, 0x00106000, 0, 0x00004001, 0,
  0x0100003e,
};
static_assert(std::size(bltVertexTokens) == bltVertexTokens[1]);
static_assert(std::size(bltPixelTokens) == bltPixelTokens[1]);

inline bool bltShaderContainers(std::vector<unsigned char>& vertex,
    std::vector<unsigned char>& pixel) {
  constexpr ShaderSignatureEntry inputs[] = {
    {0, 0, 15, ShaderScalar::Float32}, {0, 1, 3, ShaderScalar::Float32}};
  constexpr ShaderSignatureEntry outputs[] = {
    {1, 0, 15, ShaderScalar::Float32}, {0, 1, 3, ShaderScalar::Float32}};
  constexpr ShaderSignatureEntry color{0, 0, 15, ShaderScalar::Float32};
  return buildShaderContainer(ShaderStage::Vertex, bltVertexTokens,
      std::size(bltVertexTokens), inputs, 2, outputs, 2, vertex)
    && buildShaderContainer(ShaderStage::Pixel, bltPixelTokens,
      std::size(bltPixelTokens), inputs + 1, 1, &color, 1, pixel);
}

struct BltVertex { float position[4]; float uv[2]; };

// DXGI rotates counter-clockwise in screen coordinates (top-left origin).
// Extend UV to the oversized triangle; CLAMP plus linear filtering samples
// the full source without a diagonal seam or a second destination pass.
inline std::array<BltVertex, 3> bltVertices(uint32_t rotation) {
  std::array<BltVertex, 3> vertices{{
    {{-1, 1, 0, 1}, {0, 0}}, {{3, 1, 0, 1}, {2, 0}},
    {{-1, -3, 0, 1}, {0, 2}},
  }};
  for (auto& vertex : vertices) {
    const float u = vertex.uv[0], v = vertex.uv[1];
    switch (rotation) {
      case 2: vertex.uv[0] = 1 - v; vertex.uv[1] = u; break;
      case 3: vertex.uv[0] = 1 - u; vertex.uv[1] = 1 - v; break;
      case 4: vertex.uv[0] = v; vertex.uv[1] = 1 - u; break;
      default: break;
    }
  }
  return vertices;
}
}
