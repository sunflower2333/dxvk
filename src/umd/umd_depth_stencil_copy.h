#pragma once
// SPDX-License-Identifier: MIT
#include <dxgiformat.h>
#include <cstdint>

namespace dxvk::umd {

// Storage metadata for CopyRegion only. These bytes do not admit CPU uploads
// or partial depth/stencil copies, and do not change scalar transfer formats.
constexpr uint32_t depthStencilCopyBytes(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_D16_UNORM: return 2;
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return 4;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return 8;
    default: return 0;
  }
}

// D3D10.0 permits a depth/stencil source, but not a depth/stencil destination
// or multisampling. D3D10.1/11 permit both directions. All depth-bound copies
// require the complete source subresource, a null box and zero destination
// offsets; the caller separately checks format family, sample match and fit.
constexpr bool depthStencilRegionCopyContract(bool sourceDepth, bool destinationDepth,
    bool modern, uint32_t sourceSamples, uint32_t destinationSamples,
    bool hasBox, uint32_t x, uint32_t y, uint32_t z, bool sourceCube, bool destinationCube) {
  if (!sourceDepth && !destinationDepth) return true;
  return !hasBox && !x && !y && !z
      && (modern || (!destinationDepth && sourceSamples == 1 && destinationSamples == 1
          && sourceCube == destinationCube));
}

}
