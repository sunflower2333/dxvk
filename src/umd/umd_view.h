#pragma once
#include "umd_ddi.h"
#include <d3d11.h>
#include <algorithm>

namespace dxvk::umd {

inline bool viewRange(UINT first, UINT count, UINT total) {
  return count && first < total && count <= total - first;
}

// `shared` reports D3D10_DDI_RESOURCE_MISC_SHARED, which is never a D3D11 misc
// flag here: the embedded renderer has no cross-process image of its own, so
// sharing is carried by a kernel allocation beside the cache rather than by
// D3D11_RESOURCE_MISC_SHARED on it. See umd_shared_surface.h.
inline bool textureMiscFlags(const D3D10DDIARG_CREATERESOURCE& args, UINT& flags,
    bool* shared = nullptr) {
  flags = 0;
  if (shared) *shared = false;
  constexpr UINT known = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP | D3D10_DDI_RESOURCE_MISC_SHARED;
  if (args.MiscFlags & ~known) return false;
  if (args.MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED) {
    // A shared surface is one linear image: AllocationInfo carries a single
    // width, height and pitch, so a generated mip chain has nowhere to live in
    // it. A caller that passes no out-parameter is asking about pipeline flags
    // alone and must not be handed a surface it will not publish.
    if (!shared || (args.MiscFlags & D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP)) return false;
    *shared = true;
  }
  if (!(args.MiscFlags & D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP)) return true;
  constexpr UINT required = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
  if ((args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D
       && args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE1D)
      || args.Usage != D3D10_DDI_USAGE_DEFAULT || args.MapFlags
      || args.SampleDesc.Count != 1 || args.SampleDesc.Quality
      || (args.BindFlags & required) != required) return false;
  flags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  return true;
}

inline HRESULT mipGenerationStatus(const D3D11_TEXTURE2D_DESC& resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC& view) {
  constexpr UINT required = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  if (!(resource.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      || (resource.BindFlags & required) != required) return E_FAIL;
  if (resource.SampleDesc.Count != 1) return E_INVALIDARG;
  if (view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
    return viewRange(view.Texture2D.MostDetailedMip, view.Texture2D.MipLevels,
      resource.MipLevels) ? S_OK : E_INVALIDARG;
  if (view.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
    return viewRange(view.Texture2DArray.MostDetailedMip, view.Texture2DArray.MipLevels,
        resource.MipLevels) && viewRange(view.Texture2DArray.FirstArraySlice,
        view.Texture2DArray.ArraySize, resource.ArraySize) ? S_OK : E_INVALIDARG;
  return E_INVALIDARG;
}

inline bool textureShaderView(const D3D10DDIARG_CREATESHADERRESOURCEVIEW& args,
    const D3D11_TEXTURE2D_DESC& resource, D3D11_SHADER_RESOURCE_VIEW_DESC& out) {
  out = {};
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D
      || !(resource.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      || !viewRange(args.Tex2D.FirstArraySlice, args.Tex2D.ArraySize, resource.ArraySize)
      || !viewRange(args.Tex2D.MostDetailedMip, args.Tex2D.MipLevels, resource.MipLevels)
      || !resource.SampleDesc.Count) return false;
  out.Format = args.Format;
  if (resource.SampleDesc.Count > 1) {
    if (args.Tex2D.MostDetailedMip || args.Tex2D.MipLevels != 1) return false;
    if (resource.ArraySize == 1) out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
    else {
      out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
      out.Texture2DMSArray.FirstArraySlice = args.Tex2D.FirstArraySlice;
      out.Texture2DMSArray.ArraySize = args.Tex2D.ArraySize;
    }
  } else if (resource.ArraySize == 1) {
    out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    out.Texture2D.MostDetailedMip = args.Tex2D.MostDetailedMip;
    out.Texture2D.MipLevels = args.Tex2D.MipLevels;
  } else {
    out.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    out.Texture2DArray.MostDetailedMip = args.Tex2D.MostDetailedMip;
    out.Texture2DArray.MipLevels = args.Tex2D.MipLevels;
    out.Texture2DArray.FirstArraySlice = args.Tex2D.FirstArraySlice;
    out.Texture2DArray.ArraySize = args.Tex2D.ArraySize;
  }
  // The embedded device validates typed/typeless format compatibility.
  return true;
}

inline bool textureTargetView(const D3D10DDIARG_CREATERENDERTARGETVIEW& args,
    const D3D11_TEXTURE2D_DESC& resource, D3D11_RENDER_TARGET_VIEW_DESC& out) {
  out = {};
  if (args.ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D
      || !(resource.BindFlags & D3D11_BIND_RENDER_TARGET)
      || !viewRange(args.Tex2D.FirstArraySlice, args.Tex2D.ArraySize, resource.ArraySize)
      || args.Tex2D.MipSlice >= resource.MipLevels || !resource.SampleDesc.Count)
    return false;
  out.Format = args.Format;
  if (resource.SampleDesc.Count > 1) {
    if (args.Tex2D.MipSlice) return false;
    out.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
    out.Texture2DMSArray.FirstArraySlice = args.Tex2D.FirstArraySlice;
    out.Texture2DMSArray.ArraySize = args.Tex2D.ArraySize;
  } else {
    // The array view also covers a resource with one slice.
    out.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    out.Texture2DArray.MipSlice = args.Tex2D.MipSlice;
    out.Texture2DArray.FirstArraySlice = args.Tex2D.FirstArraySlice;
    out.Texture2DArray.ArraySize = args.Tex2D.ArraySize;
  }
  return true;
}

inline bool resolveSubresources(const D3D11_TEXTURE2D_DESC& destination, UINT dst,
    const D3D11_TEXTURE2D_DESC& source, UINT src, DXGI_FORMAT format) {
  if (destination.Usage != D3D11_USAGE_DEFAULT || destination.SampleDesc.Count != 1
      || source.SampleDesc.Count <= 1 || !destination.MipLevels || source.MipLevels != 1
      || dst / destination.MipLevels >= destination.ArraySize || src >= source.ArraySize
      || destination.Format != format || source.Format != format) return false;
  const UINT mip = dst % destination.MipLevels;
  if (mip >= D3D11_REQ_MIP_LEVELS) return false;
  return std::max(1u, destination.Width >> mip) == source.Width
      && std::max(1u, destination.Height >> mip) == source.Height;
}

inline UINT nativeFormatCaps(UINT api) {
  UINT result = 0;
  if (api & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) result |= D3D10_DDI_FORMAT_SUPPORT_SHADER_SAMPLE;
  if (api & D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
    result |= D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET;
    if (api & D3D11_FORMAT_SUPPORT_BLENDABLE) result |= D3D10_DDI_FORMAT_SUPPORT_BLENDABLE;
    if (api & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET)
      result |= D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET;
  }
  if (api & D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD) result |= D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_LOAD;
  return result;
}

}
