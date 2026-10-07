#pragma once
#include "umd_ddi.h"
#include <d3d11.h>

namespace dxvk::umd {

// DDI misc bits are not public API bits (raw/structured buffers in particular).
// A field-wise translation also avoids reading newer optional DDI trailers.
constexpr bool resource11Flags(const D3D11DDIARG_CREATERESOURCE& native,
    UINT& misc, UINT& bindings) {
  constexpr UINT known = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP
    | D3D10_DDI_RESOURCE_MISC_SHARED | D3D11_DDI_RESOURCE_MISC_DRAWINDIRECT_ARGS
    | D3D11_DDI_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
    | D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED | D3D11_DDI_RESOURCE_MISC_RESOURCE_CLAMP;
  constexpr UINT bind = D3D10_DDI_BIND_PIPELINE_MASK | D3D10_DDI_BIND_PRESENT
    | D3D11_DDI_BIND_UNORDERED_ACCESS;
  if ((native.MiscFlags & ~known) || (native.BindFlags & ~bind)
      || (native.MapFlags & ~D3D10_DDI_CPU_ACCESS_MASK)) return false;
  misc = 0;
  bindings = native.BindFlags & D3D10_DDI_BIND_PIPELINE_MASK;
  // Native UAV bind is 0x100; the public D3D11 resource bind is 0x80.
  if (native.BindFlags & D3D11_DDI_BIND_UNORDERED_ACCESS) bindings |= D3D11_BIND_UNORDERED_ACCESS;
  if (native.MiscFlags & D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP) misc |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
  if (native.MiscFlags & D3D11_DDI_RESOURCE_MISC_DRAWINDIRECT_ARGS) misc |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
  if (native.MiscFlags & D3D11_DDI_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) misc |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  if (native.MiscFlags & D3D11_DDI_RESOURCE_MISC_BUFFER_STRUCTURED) misc |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  if (native.MiscFlags & D3D11_DDI_RESOURCE_MISC_RESOURCE_CLAMP) misc |= D3D11_RESOURCE_MISC_RESOURCE_CLAMP;
  if (native.ResourceDimension == D3D10DDIRESOURCE_TEXTURECUBE) misc |= D3D11_RESOURCE_MISC_TEXTURECUBE;
  return true;
}

constexpr UINT resource11CpuAccess(UINT native) {
  return ((native & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
    | ((native & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
}

// WDK cube SRV MipLevels=-1 selects the remaining chain beginning at
// MostDetailedMip. NumCubes remains a finite count; validate in cube units to
// avoid overflow in First2DArrayFace + 6 * NumCubes. Publish only a complete
// valid descriptor so a failed conversion cannot modify its caller's output.
inline bool cubeArrayShaderView11Desc(const D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW& native,
    DXGI_FORMAT format, const D3D11_TEXTURE2D_DESC& resource, D3D11_SHADER_RESOURCE_VIEW_DESC& out) {
  if (!(resource.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      || !(resource.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      || native.First2DArrayFace % 6 || !native.NumCubes
      || native.MostDetailedMip >= resource.MipLevels) return false;
  const UINT totalCubes = resource.ArraySize / 6;
  const UINT firstCube = native.First2DArrayFace / 6;
  if (firstCube >= totalCubes || native.NumCubes > totalCubes - firstCube) return false;
  const UINT remaining = resource.MipLevels - native.MostDetailedMip;
  const UINT levels = native.MipLevels == UINT(-1) ? remaining : native.MipLevels;
  if (!levels || levels > remaining) return false;
  D3D11_SHADER_RESOURCE_VIEW_DESC staged{};
  staged.Format = format; staged.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
  staged.TextureCubeArray = {native.MostDetailedMip, levels, native.First2DArrayFace, native.NumCubes};
  out = staged;
  return true;
}

inline D3D10DDIARG_CREATERESOURCE resource10Fields(const D3D11DDIARG_CREATERESOURCE& native) {
  D3D10DDIARG_CREATERESOURCE out = {};
  out.pMipInfoList = native.pMipInfoList; out.pInitialDataUP = native.pInitialDataUP;
  out.ResourceDimension = native.ResourceDimension; out.Usage = native.Usage;
  out.BindFlags = native.BindFlags; out.MapFlags = native.MapFlags; out.MiscFlags = native.MiscFlags;
  out.Format = native.Format; out.SampleDesc = native.SampleDesc; out.MipLevels = native.MipLevels;
  out.ArraySize = native.ArraySize; out.pPrimaryDesc = native.pPrimaryDesc;
  return out;
}

constexpr bool buffer11Desc(const D3D11DDIARG_CREATERESOURCE& native, D3D11_BUFFER_DESC& out) {
  out = {};
  UINT misc = 0, bindings = 0;
  if (!resource11Flags(native, misc, bindings) || !native.pMipInfoList
      || native.ResourceDimension != D3D10DDIRESOURCE_BUFFER
      || native.MipLevels != 1 || native.ArraySize != 1 || native.pPrimaryDesc
      || (native.BindFlags & D3D10_DDI_BIND_PRESENT)
      || (native.MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED)
      || native.SampleDesc.Count != 1 || native.SampleDesc.Quality
      || !native.pMipInfoList[0].TexelWidth) return false;
  constexpr UINT allowed = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
    | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  if (misc & ~allowed) return false;
  const bool structured = (misc & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0;
  if (structured && ((misc & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
      || !native.ByteStride || native.ByteStride % 4 || native.ByteStride > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES
      || native.pMipInfoList[0].TexelWidth % native.ByteStride)) return false;
  if (!structured && native.ByteStride) return false;
  out.ByteWidth = native.pMipInfoList[0].TexelWidth;
  out.Usage = static_cast<D3D11_USAGE>(native.Usage); out.BindFlags = bindings;
  out.CPUAccessFlags = resource11CpuAccess(native.MapFlags); out.MiscFlags = misc;
  out.StructureByteStride = native.ByteStride;
  return true;
}

inline D3D11_BLEND_DESC blend11Desc(const D3D10_1_DDI_BLEND_DESC& native) {
  D3D11_BLEND_DESC out = {};
  out.AlphaToCoverageEnable = native.AlphaToCoverageEnable;
  out.IndependentBlendEnable = native.IndependentBlendEnable;
  for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
    const auto& source = native.RenderTarget[native.IndependentBlendEnable ? i : 0];
    auto& target = out.RenderTarget[i];
    target.BlendEnable = source.BlendEnable; target.SrcBlend = static_cast<D3D11_BLEND>(source.SrcBlend);
    target.DestBlend = static_cast<D3D11_BLEND>(source.DestBlend); target.BlendOp = static_cast<D3D11_BLEND_OP>(source.BlendOp);
    target.SrcBlendAlpha = static_cast<D3D11_BLEND>(source.SrcBlendAlpha);
    target.DestBlendAlpha = static_cast<D3D11_BLEND>(source.DestBlendAlpha);
    target.BlendOpAlpha = static_cast<D3D11_BLEND_OP>(source.BlendOpAlpha);
    target.RenderTargetWriteMask = source.RenderTargetWriteMask;
  }
  return out;
}

}
