#pragma once
// SPDX-License-Identifier: MIT
#include "umd_view.h"
#include <cstdint>

namespace dxvk::umd {

// Resolve the WDK all-remaining sentinel without unsigned range overflow.
inline bool texture1DRange(UINT first, UINT count, UINT total, UINT& resolved) {
  if (first >= total || !count) return false;
  resolved = count == UINT32_MAX ? total - first : count;
  return resolved <= total - first;
}

// Validate one-dimensional logical mip extents before reading initial data.
inline bool texture1DDesc(
    const D3D10DDIARG_CREATERESOURCE& args,
    UINT miscFlags,
    D3D11_TEXTURE1D_DESC& out) {
  out = {};
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE1D || !args.pMipInfoList ||
      !args.MipLevels || args.MipLevels > D3D11_REQ_MIP_LEVELS || !args.ArraySize ||
      args.ArraySize > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION ||
      args.SampleDesc.Count != 1 || args.SampleDesc.Quality || args.pPrimaryDesc ||
      args.Usage > D3D10_DDI_USAGE_STAGING ||
      (args.BindFlags & ~(D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET |
                          D3D10_DDI_BIND_DEPTH_STENCIL)) ||
      (args.MapFlags & ~D3D10_DDI_CPU_ACCESS_MASK) ||
      (args.MiscFlags & ~D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP)) return false;
  const UINT width = args.pMipInfoList[0].TexelWidth;
  if (!width || width > D3D11_REQ_TEXTURE1D_U_DIMENSION) return false;
  UINT maximumMips = 1;
  for (UINT extent = width; extent > 1; extent >>= 1) ++maximumMips;
  if (args.MipLevels > maximumMips) return false;
  for (UINT mip = 0; mip < args.MipLevels; ++mip) {
    const auto& extent = args.pMipInfoList[mip];
    if (extent.TexelWidth != std::max(1u, width >> mip)) return false;
  }
  // Backend layout owns physical padding; do not treat PhysicalWidth as texel width.
  out.Width = width; out.MipLevels = args.MipLevels; out.ArraySize = args.ArraySize;
  out.Format = args.Format; out.Usage = static_cast<D3D11_USAGE>(args.Usage);
  out.BindFlags = args.BindFlags; out.MiscFlags = miscFlags;
  out.CPUAccessFlags = ((args.MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0) |
                      ((args.MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
  return true;
}

// Preserve the native array and mip range, including UINT_MAX all-remaining values.
inline bool textureShaderView(
    const D3D10DDIARG_CREATESHADERRESOURCEVIEW& args,
    const D3D11_TEXTURE1D_DESC& resource,
    D3D11_SHADER_RESOURCE_VIEW_DESC& out) {
  out = {};
  UINT mips = 0, layers = 0;
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE1D ||
      !(resource.BindFlags & D3D11_BIND_SHADER_RESOURCE) ||
      !texture1DRange(args.Tex1D.MostDetailedMip, args.Tex1D.MipLevels, resource.MipLevels, mips) ||
      !texture1DRange(args.Tex1D.FirstArraySlice, args.Tex1D.ArraySize, resource.ArraySize, layers))
    return false;
  out.Format = args.Format;
  if (resource.ArraySize == 1) {
    out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    out.Texture1D.MostDetailedMip = args.Tex1D.MostDetailedMip;
    out.Texture1D.MipLevels = mips;
  } else {
    out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
    out.Texture1DArray.MostDetailedMip = args.Tex1D.MostDetailedMip;
    out.Texture1DArray.MipLevels = mips;
    out.Texture1DArray.FirstArraySlice = args.Tex1D.FirstArraySlice;
    out.Texture1DArray.ArraySize = layers;
  }
  return true;
}

// Convert a Texture1D RTV using its actual mip and finite slice count.
inline bool textureTargetView(
    const D3D10DDIARG_CREATERENDERTARGETVIEW& args,
    const D3D11_TEXTURE1D_DESC& resource,
    D3D11_RENDER_TARGET_VIEW_DESC& out) {
  out = {};
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE1D ||
      !(resource.BindFlags & D3D11_BIND_RENDER_TARGET) ||
      args.Tex1D.MipSlice >= resource.MipLevels ||
      !viewRange(args.Tex1D.FirstArraySlice, args.Tex1D.ArraySize, resource.ArraySize)) return false;
  out.Format = args.Format;
  if (resource.ArraySize == 1) {
    out.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
    out.Texture1D.MipSlice = args.Tex1D.MipSlice;
  } else {
    out.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
    out.Texture1DArray.MipSlice = args.Tex1D.MipSlice;
    out.Texture1DArray.FirstArraySlice = args.Tex1D.FirstArraySlice;
    out.Texture1DArray.ArraySize = args.Tex1D.ArraySize;
  }
  return true;
}

// Build a depth view without inventing Texture1D multisample support.
inline bool textureDepthView(
    const D3D10DDIARG_CREATEDEPTHSTENCILVIEW& args,
    const D3D11_TEXTURE1D_DESC& resource,
    D3D11_DEPTH_STENCIL_VIEW_DESC& out) {
  out = {};
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE1D ||
      !(resource.BindFlags & D3D11_BIND_DEPTH_STENCIL) ||
      args.Tex1D.MipSlice >= resource.MipLevels ||
      !viewRange(args.Tex1D.FirstArraySlice, args.Tex1D.ArraySize, resource.ArraySize)) return false;
  out.Format = args.Format;
  if (resource.ArraySize == 1) {
    out.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
    out.Texture1D.MipSlice = args.Tex1D.MipSlice;
  } else {
    out.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
    out.Texture1DArray.MipSlice = args.Tex1D.MipSlice;
    out.Texture1DArray.FirstArraySlice = args.Tex1D.FirstArraySlice;
    out.Texture1DArray.ArraySize = args.Tex1D.ArraySize;
  }
  return true;
}

// Admit GenerateMips only for a previously created, compatible Texture1D SRV.
inline HRESULT mipGenerationStatus(
    const D3D11_TEXTURE1D_DESC& resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC& view) {
  constexpr UINT required = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  if (!(resource.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) ||
      (resource.BindFlags & required) != required) return E_FAIL;
  if (view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
    return resource.ArraySize == 1 && viewRange(view.Texture1D.MostDetailedMip,
      view.Texture1D.MipLevels, resource.MipLevels) ? S_OK : E_INVALIDARG;
  if (view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
    return viewRange(view.Texture1DArray.MostDetailedMip, view.Texture1DArray.MipLevels,
      resource.MipLevels) && viewRange(view.Texture1DArray.FirstArraySlice,
      view.Texture1DArray.ArraySize, resource.ArraySize) ? S_OK : E_INVALIDARG;
  return E_INVALIDARG;
}

}
