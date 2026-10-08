// SPDX-License-Identifier: MIT
#pragma once
#include "umd_ddi.h"
#include <d3d11.h>

namespace dxvk::umd {

// RTV/DSV cube intervals are array-face slices, unlike the SRV's cube counts.
// Use subtraction after bounding the first face to avoid UINT overflow.
inline bool cubeTargetFaceRange(const D3D11_TEXTURE2D_DESC& resource,
    UINT mip, UINT first, UINT count, UINT bind) {
  return (resource.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
    && resource.Width && resource.Width == resource.Height
    && resource.Width <= D3D11_REQ_TEXTURECUBE_DIMENSION
    && resource.ArraySize >= 6 && !(resource.ArraySize % 6)
    && resource.ArraySize <= D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION
    && resource.MipLevels && resource.MipLevels <= D3D11_REQ_MIP_LEVELS
    && resource.SampleDesc.Count == 1 && !resource.SampleDesc.Quality
    && (resource.BindFlags & bind) == bind && mip < resource.MipLevels
    && count && first < resource.ArraySize && count <= resource.ArraySize - first;
}

inline bool cubeArrayTargetViewDesc(const D3D10DDIARG_TEXCUBE_RENDERTARGETVIEW& native,
    DXGI_FORMAT format, const D3D11_TEXTURE2D_DESC& resource,
    D3D11_RENDER_TARGET_VIEW_DESC& output) {
  if (!cubeTargetFaceRange(resource, native.MipSlice, native.FirstArraySlice,
      native.ArraySize, D3D11_BIND_RENDER_TARGET)) return false;
  D3D11_RENDER_TARGET_VIEW_DESC staged{};
  staged.Format = format; staged.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
  staged.Texture2DArray = {native.MipSlice, native.FirstArraySlice, native.ArraySize};
  output = staged;
  return true;
}

inline bool cubeArrayDepthViewDesc(const D3D10DDIARG_TEXCUBE_DEPTHSTENCILVIEW& native,
    DXGI_FORMAT format, UINT flags, const D3D11_TEXTURE2D_DESC& resource,
    D3D11_DEPTH_STENCIL_VIEW_DESC& output) {
  constexpr UINT known = D3D11_DDI_CREATE_DSV_READ_ONLY_DEPTH | D3D11_DDI_CREATE_DSV_READ_ONLY_STENCIL;
  if ((flags & ~known) || !cubeTargetFaceRange(resource, native.MipSlice,
      native.FirstArraySlice, native.ArraySize, D3D11_BIND_DEPTH_STENCIL)) return false;
  D3D11_DEPTH_STENCIL_VIEW_DESC staged{};
  staged.Format = format; staged.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
  if (flags & D3D11_DDI_CREATE_DSV_READ_ONLY_DEPTH) staged.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
  if (flags & D3D11_DDI_CREATE_DSV_READ_ONLY_STENCIL) staged.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
  staged.Texture2DArray = {native.MipSlice, native.FirstArraySlice, native.ArraySize};
  output = staged;
  return true;
}

} // namespace dxvk::umd
