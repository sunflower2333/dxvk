#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

enum class TextureUavDimension { Invalid, Single, Array };

// The resource's array shape determines the shader-visible dimension. A
// one-slice view of an array resource is still an array view.
inline constexpr TextureUavDimension textureUavDimension(uint32_t resourceSlices,
    uint32_t firstSlice, uint32_t viewSlices) noexcept {
  if (!viewSlices || firstSlice >= resourceSlices || viewSlices > resourceSlices - firstSlice)
    return TextureUavDimension::Invalid;
  return resourceSlices == 1 ? TextureUavDimension::Single : TextureUavDimension::Array;
}

}
