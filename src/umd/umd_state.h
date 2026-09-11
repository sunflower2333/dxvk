#pragma once

#include "umd_ddi.h"
#include <d3d11.h>
#include <array>
#include <cmath>

namespace dxvk::umd {

inline bool primitiveTopology(D3D10_DDI_PRIMITIVE_TOPOLOGY native,
    D3D11_PRIMITIVE_TOPOLOGY& api) {
  switch (native) {
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_UNDEFINED: api = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_POINTLIST: api = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_LINELIST: api = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_LINESTRIP: api = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST: api = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: api = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: api = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ: api = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ: api = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ; break;
    case D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ: api = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ; break;
    // Patch lists belong to the D3D11 table, not this D3D10.0 interface.
    default: return false;
  }
  return true;
}

template<typename Apply>
bool replaceViewports(UINT count, UINT clear, const D3D10_DDI_VIEWPORT* views,
    Apply&& apply) {
  constexpr UINT slots = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  if (count > slots || clear > slots - count || (count && !views)) return false;
  std::array<D3D11_VIEWPORT, slots> translated = {};
  for (UINT i = 0; i < count; i++) {
    const auto& v = views[i];
    // The native DDI represents an unbound viewport with six NaNs. Keep its
    // index while disabling rasterization with a zero-area API viewport;
    // never pass a NaN coordinate to Vulkan or compact later viewports.
    if (std::isnan(v.TopLeftX) && std::isnan(v.TopLeftY) && std::isnan(v.Width)
        && std::isnan(v.Height) && std::isnan(v.MinDepth) && std::isnan(v.MaxDepth))
      translated[i] = {0, 0, 0, 0, 0, 1};
    else
      translated[i] = {v.TopLeftX, v.TopLeftY, v.Width, v.Height, v.MinDepth, v.MaxDepth};
  }
  // ClearViewports is a hint. Every call replaces the complete binding,
  // including count=0/clear=0 after a previously nonempty state.
  apply(count, count ? translated.data() : nullptr);
  return true;
}

}
