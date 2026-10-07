#pragma once

#include "umd_shader.h"
#include <dxgiformat.h>
#include <algorithm>
#include <cstdint>

namespace dxvk::umd {

struct InputFormat {
  ShaderScalar scalar = ShaderScalar::Unknown;
  uint8_t mask = 0;
  uint8_t bytes = 0;
  uint8_t alignment = 0;
};

// Typed IA data is widened by the backend to this shader scalar. Normalized
// and half-float formats feed float32, whereas UINT/SINT preserve integers.
// Typeless, depth, sRGB and block-compressed formats are not vertex inputs.
inline constexpr InputFormat inputFormat(DXGI_FORMAT format) {
  ShaderScalar scalar = ShaderScalar::Unknown;
  uint8_t components = 0, bytes = 0;
#define VIOGPU_INPUT_FORMAT(name, type, count, size) \
  case DXGI_FORMAT_##name: scalar = ShaderScalar::type; components = count; bytes = size; break
  switch (format) {
    VIOGPU_INPUT_FORMAT(R32_FLOAT, Float32, 1, 4);
    VIOGPU_INPUT_FORMAT(R32G32_FLOAT, Float32, 2, 8);
    VIOGPU_INPUT_FORMAT(R32G32B32_FLOAT, Float32, 3, 12);
    VIOGPU_INPUT_FORMAT(R32G32B32A32_FLOAT, Float32, 4, 16);
    VIOGPU_INPUT_FORMAT(R32_UINT, Uint32, 1, 4);
    VIOGPU_INPUT_FORMAT(R32G32_UINT, Uint32, 2, 8);
    VIOGPU_INPUT_FORMAT(R32G32B32_UINT, Uint32, 3, 12);
    VIOGPU_INPUT_FORMAT(R32G32B32A32_UINT, Uint32, 4, 16);
    VIOGPU_INPUT_FORMAT(R32_SINT, Sint32, 1, 4);
    VIOGPU_INPUT_FORMAT(R32G32_SINT, Sint32, 2, 8);
    VIOGPU_INPUT_FORMAT(R32G32B32_SINT, Sint32, 3, 12);
    VIOGPU_INPUT_FORMAT(R32G32B32A32_SINT, Sint32, 4, 16);
    VIOGPU_INPUT_FORMAT(R16_FLOAT, Float32, 1, 2);
    VIOGPU_INPUT_FORMAT(R16G16_FLOAT, Float32, 2, 4);
    VIOGPU_INPUT_FORMAT(R16G16B16A16_FLOAT, Float32, 4, 8);
    VIOGPU_INPUT_FORMAT(R16_UNORM, Float32, 1, 2);
    VIOGPU_INPUT_FORMAT(R16G16_UNORM, Float32, 2, 4);
    VIOGPU_INPUT_FORMAT(R16G16B16A16_UNORM, Float32, 4, 8);
    VIOGPU_INPUT_FORMAT(R16_SNORM, Float32, 1, 2);
    VIOGPU_INPUT_FORMAT(R16G16_SNORM, Float32, 2, 4);
    VIOGPU_INPUT_FORMAT(R16G16B16A16_SNORM, Float32, 4, 8);
    VIOGPU_INPUT_FORMAT(R16_UINT, Uint32, 1, 2);
    VIOGPU_INPUT_FORMAT(R16G16_UINT, Uint32, 2, 4);
    VIOGPU_INPUT_FORMAT(R16G16B16A16_UINT, Uint32, 4, 8);
    VIOGPU_INPUT_FORMAT(R16_SINT, Sint32, 1, 2);
    VIOGPU_INPUT_FORMAT(R16G16_SINT, Sint32, 2, 4);
    VIOGPU_INPUT_FORMAT(R16G16B16A16_SINT, Sint32, 4, 8);
    VIOGPU_INPUT_FORMAT(R8_UNORM, Float32, 1, 1);
    VIOGPU_INPUT_FORMAT(R8G8_UNORM, Float32, 2, 2);
    VIOGPU_INPUT_FORMAT(R8G8B8A8_UNORM, Float32, 4, 4);
    VIOGPU_INPUT_FORMAT(R8_SNORM, Float32, 1, 1);
    VIOGPU_INPUT_FORMAT(R8G8_SNORM, Float32, 2, 2);
    VIOGPU_INPUT_FORMAT(R8G8B8A8_SNORM, Float32, 4, 4);
    VIOGPU_INPUT_FORMAT(R8_UINT, Uint32, 1, 1);
    VIOGPU_INPUT_FORMAT(R8G8_UINT, Uint32, 2, 2);
    VIOGPU_INPUT_FORMAT(R8G8B8A8_UINT, Uint32, 4, 4);
    VIOGPU_INPUT_FORMAT(R8_SINT, Sint32, 1, 1);
    VIOGPU_INPUT_FORMAT(R8G8_SINT, Sint32, 2, 2);
    VIOGPU_INPUT_FORMAT(R8G8B8A8_SINT, Sint32, 4, 4);
    VIOGPU_INPUT_FORMAT(R10G10B10A2_UNORM, Float32, 4, 4);
    VIOGPU_INPUT_FORMAT(R10G10B10A2_UINT, Uint32, 4, 4);
    VIOGPU_INPUT_FORMAT(R11G11B10_FLOAT, Float32, 3, 4);
    default: return {};
  }
#undef VIOGPU_INPUT_FORMAT
  return {scalar, uint8_t((1u << components) - 1), bytes, uint8_t(std::min<unsigned>(bytes, 4))};
}

inline bool inputElementOffset(const InputFormat& format, uint32_t requested,
    uint32_t previousEnd, uint32_t& result) {
  constexpr uint32_t limit = 2048;
  if (!format.bytes || !format.alignment || previousEnd > limit) return false;
  uint32_t offset = requested;
  if (requested == UINT32_MAX)
    offset = (previousEnd + format.alignment - 1) & ~(uint32_t(format.alignment) - 1);
  if (offset > limit || format.bytes > limit - offset || offset % format.alignment) return false;
  result = offset;
  return true;
}

}
