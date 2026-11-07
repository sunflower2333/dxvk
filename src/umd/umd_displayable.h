#pragma once
#include "umd_ddi.h"

namespace dxvk::umd {

// WDDM2 marks FlipEx back buffers and primaries as DISPLAYABLE_SURFACE. It is
// a scheduler contract, not a public D3D11 texture bit. Our renderer writes a
// private cache; publication writes the separate KMD backing through RenderCb
// with that allocation's WriteOperation set (or a synchronized LockCb). Thus
// it retains the allocation-list synchronization contract rather than using
// the GPU-VA SubmitCommandCb/WrittenPrimaries path.
template<typename Desc>
constexpr bool displayableSurfaceShape(const Desc& args) {
  if (!(args.MiscFlags & D3DWDDM2_0DDI_RESOURCE_MISC_DISPLAYABLE_SURFACE)) return true;
  constexpr UINT required = D3D10_DDI_BIND_PRESENT | D3D10_DDI_BIND_RENDER_TARGET;
  constexpr UINT allowed = required | D3D10_DDI_BIND_SHADER_RESOURCE;
  constexpr UINT misc = D3DWDDM2_0DDI_RESOURCE_MISC_DISPLAYABLE_SURFACE
    | D3D10_DDI_RESOURCE_MISC_SHARED | D3D10_DDI_RESOURCE_MISC_DISCARD_ON_PRESENT;
  return args.ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D
    && args.Usage == D3D10_DDI_USAGE_DEFAULT && !args.MapFlags
    && args.MipLevels == 1 && args.ArraySize == 1
    && args.SampleDesc.Count == 1 && !args.SampleDesc.Quality
    && (args.BindFlags & required) == required && !(args.BindFlags & ~allowed)
    && !(args.MiscFlags & ~misc);
}

}
