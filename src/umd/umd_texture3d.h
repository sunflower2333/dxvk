#pragma once
// SPDX-License-Identifier: MIT
#include "umd_texture1d.h"
#include "umd_transfer_format.h"
#include "umd_volume_policy.h"

namespace dxvk::umd {

inline bool texture3DDesc(const D3D10DDIARG_CREATERESOURCE& args, UINT miscFlags,
    D3D11_TEXTURE3D_DESC& out, bool native11 = false) {
  out = {};
  const UINT allowedBindings = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET
    | (native11 ? D3D11_DDI_BIND_UNORDERED_ACCESS : 0);
  const UINT allowedMisc = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP
    | (native11 ? D3D11_DDI_RESOURCE_MISC_RESOURCE_CLAMP : 0);
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE3D || !args.pMipInfoList
      || !args.MipLevels || args.MipLevels > D3D11_REQ_MIP_LEVELS || args.ArraySize != 1
      || args.SampleDesc.Count != 1 || args.SampleDesc.Quality || args.pPrimaryDesc
      || args.Usage > D3D10_DDI_USAGE_STAGING || (args.BindFlags & ~allowedBindings)
      || (args.MapFlags & ~D3D10_DDI_CPU_ACCESS_MASK) || (args.MiscFlags & ~allowedMisc)
      || !transferTexelBytes(args.Format)) return false;
  constexpr UINT mipBindings = D3D10_DDI_BIND_SHADER_RESOURCE | D3D10_DDI_BIND_RENDER_TARGET;
  if ((miscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      && (args.Usage != D3D10_DDI_USAGE_DEFAULT || args.MapFlags
          || (args.BindFlags & mipBindings) != mipBindings)) return false;
  const auto& first = args.pMipInfoList[0];
  const VolumeExtent base{first.TexelWidth, first.TexelHeight, first.TexelDepth};
  if (!base.width || !base.height || !base.depth
      || base.width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
      || base.height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
      || base.depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) return false;
  UINT maximumMips = 1;
  for (UINT extent = std::max({base.width, base.height, base.depth}); extent > 1; extent >>= 1)
    ++maximumMips;
  if (args.MipLevels > maximumMips) return false;
  for (UINT mip = 0; mip < args.MipLevels; ++mip) {
    VolumeExtent extent;
    if (!volumeMipExtent(base, mip, extent)) return false;
    const auto& supplied = args.pMipInfoList[mip];
    if (supplied.TexelWidth != extent.width || supplied.TexelHeight != extent.height
        || supplied.TexelDepth != extent.depth) return false;
  }
  out.Width = base.width; out.Height = base.height; out.Depth = base.depth;
  out.MipLevels = args.MipLevels; out.Format = args.Format;
  out.Usage = static_cast<D3D11_USAGE>(args.Usage); out.BindFlags = args.BindFlags;
  out.CPUAccessFlags = ((args.MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
    | ((args.MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
  out.MiscFlags = miscFlags;
  return true;
}

inline bool texture3DInitialData(const D3D10DDIARG_CREATERESOURCE& args) {
  if (!args.pMipInfoList || !args.MipLevels || args.MipLevels > D3D11_REQ_MIP_LEVELS) return false;
  if (!args.pInitialDataUP) return true;
  for (UINT mip = 0; mip < args.MipLevels; ++mip) {
    const auto& shape = args.pMipInfoList[mip];
    const auto& data = args.pInitialDataUP[mip];
    uint64_t span = 0;
    if (!data.pSysMem || !uploadVolumeSpan({shape.TexelWidth, shape.TexelHeight, shape.TexelDepth},
        transferTexelBytes(args.Format), data.SysMemPitch, data.SysMemSlicePitch,
        uint64_t(UINTPTR_MAX) - reinterpret_cast<uintptr_t>(data.pSysMem) + 1, span)) return false;
  }
  return true;
}

inline bool texture3DCopy(const D3D11_TEXTURE3D_DESC& destination,
    const D3D11_TEXTURE3D_DESC& source) {
  return destination.Usage != D3D11_USAGE_IMMUTABLE && destination.Width == source.Width
    && destination.Height == source.Height && destination.Depth == source.Depth
    && destination.MipLevels == source.MipLevels && destination.Format == source.Format;
}

inline bool textureShaderView(const D3D10DDIARG_CREATESHADERRESOURCEVIEW& args,
    const D3D11_TEXTURE3D_DESC& resource, D3D11_SHADER_RESOURCE_VIEW_DESC& out) {
  out = {};
  UINT mips = 0;
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE3D
      || !(resource.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      || !texture1DRange(args.Tex3D.MostDetailedMip, args.Tex3D.MipLevels, resource.MipLevels, mips)) return false;
  out.Format = args.Format; out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
  out.Texture3D.MostDetailedMip = args.Tex3D.MostDetailedMip; out.Texture3D.MipLevels = mips;
  return true;
}

inline bool textureTargetView(const D3D10DDIARG_CREATERENDERTARGETVIEW& args,
    const D3D11_TEXTURE3D_DESC& resource, D3D11_RENDER_TARGET_VIEW_DESC& out) {
  out = {};
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE3D
      || !(resource.BindFlags & D3D11_BIND_RENDER_TARGET)
      || args.Tex3D.MipSlice >= resource.MipLevels || args.Tex3D.MipSlice >= D3D11_REQ_MIP_LEVELS) return false;
  UINT slices = 0;
  if (!texture1DRange(args.Tex3D.FirstW, args.Tex3D.WSize,
      std::max(1u, resource.Depth >> args.Tex3D.MipSlice), slices)) return false;
  out.Format = args.Format; out.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
  out.Texture3D = {args.Tex3D.MipSlice, args.Tex3D.FirstW, slices};
  return true;
}

inline HRESULT mipGenerationStatus(const D3D11_TEXTURE3D_DESC& resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC& view) {
  constexpr UINT required = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  if (!(resource.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      || (resource.BindFlags & required) != required) return E_FAIL;
  return view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D
      && viewRange(view.Texture3D.MostDetailedMip, view.Texture3D.MipLevels, resource.MipLevels)
    ? S_OK : E_INVALIDARG;
}

}
