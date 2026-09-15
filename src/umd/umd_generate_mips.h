#pragma once
// SPDX-License-Identifier: MIT
#include "umd_texture1d.h"
#include <wrl/client.h>

namespace dxvk::umd {

// Generate exactly the validated SRV mip/slice range with GPU-only operations.
// Some D3D11 implementations mishandle a nonzero first mip on 1D array SRVs.
// A full single-slice scratch chain removes both offsets from GenerateMips;
// copy back only generated levels, never the source mip or excluded slices.
inline HRESULT generateViewMips(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* view) {
  using Microsoft::WRL::ComPtr;
  if (!device || !context || !view) return E_INVALIDARG;
  ComPtr<ID3D11Resource> resource;
  view->GetResource(&resource);
  if (!resource) return E_INVALIDARG;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  resource->GetType(&dimension);
  if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
    context->GenerateMips(view);
    return S_OK;
  }
  ComPtr<ID3D11Texture1D> texture;
  HRESULT hr = resource.As(&texture);
  if (FAILED(hr)) return hr;
  D3D11_TEXTURE1D_DESC desc{}; texture->GetDesc(&desc);
  D3D11_SHADER_RESOURCE_VIEW_DESC vd{}; view->GetDesc(&vd);
  hr = mipGenerationStatus(desc, vd);
  if (FAILED(hr)) return hr;
  UINT firstMip = 0, mipCount = 0, firstSlice = 0, sliceCount = 1;
  if (vd.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D) {
    firstMip = vd.Texture1D.MostDetailedMip;
    mipCount = vd.Texture1D.MipLevels;
  } else if (vd.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY) {
    firstMip = vd.Texture1DArray.MostDetailedMip;
    mipCount = vd.Texture1DArray.MipLevels;
    firstSlice = vd.Texture1DArray.FirstArraySlice;
    sliceCount = vd.Texture1DArray.ArraySize;
  } else return E_INVALIDARG;
  if (!desc.Width || desc.MipLevels > D3D11_REQ_MIP_LEVELS ||
      firstMip >= D3D11_REQ_MIP_LEVELS) return E_INVALIDARG;
  if (mipCount == 1) return S_OK;

  D3D11_TEXTURE1D_DESC scratchDesc{};
  scratchDesc.Width = std::max(1u, desc.Width >> firstMip);
  scratchDesc.MipLevels = mipCount;
  scratchDesc.ArraySize = 1;
  // Use the typed SRV format; copy remains within the original format group.
  scratchDesc.Format = vd.Format;
  scratchDesc.Usage = D3D11_USAGE_DEFAULT;
  scratchDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  scratchDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  ComPtr<ID3D11Texture1D> scratch;
  hr = device->CreateTexture1D(&scratchDesc, nullptr, &scratch);
  if (FAILED(hr)) return hr;
  if (!scratch) return E_FAIL;
  D3D11_SHADER_RESOURCE_VIEW_DESC scratchViewDesc{};
  scratchViewDesc.Format = vd.Format;
  scratchViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
  scratchViewDesc.Texture1D.MipLevels = mipCount;
  ComPtr<ID3D11ShaderResourceView> scratchView;
  hr = device->CreateShaderResourceView(scratch.Get(), &scratchViewDesc, &scratchView);
  if (FAILED(hr)) return hr;
  if (!scratchView) return E_FAIL;

  // Allocate everything before recording. These are ordered GPU copies/blits,
  // not CPU readback, a global idle or replacement of the original allocation.
  // Backend command retention and hazard tracking own asynchronous lifetimes.
  for (UINT slice = 0; slice < sliceCount; ++slice) {
    const UINT base = D3D11CalcSubresource(firstMip, firstSlice + slice, desc.MipLevels);
    context->CopySubresourceRegion(scratch.Get(), 0, 0, 0, 0, texture.Get(), base, nullptr);
    context->GenerateMips(scratchView.Get());
    for (UINT mip = 1; mip < mipCount; ++mip)
      context->CopySubresourceRegion(texture.Get(), base + mip, 0, 0, 0,
        scratch.Get(), mip, nullptr);
  }
  return S_OK;
}

}
