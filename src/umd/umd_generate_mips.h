#pragma once
// SPDX-License-Identifier: MIT
#include "umd_texture1d.h"
#include "umd_texture3d.h"
#include <wrl/client.h>

namespace dxvk::umd {

// Normalize a volume SRV's source mip to level zero before generation. Only
// generated levels are copied back, so the source and excluded tail stay intact.
inline HRESULT generateVolumeViewMips(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Resource* resource,
    ID3D11ShaderResourceView* view) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D11Texture3D> texture;
  HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&texture));
  if (FAILED(hr)) return hr;
  if (!texture) return E_FAIL;
  D3D11_TEXTURE3D_DESC desc{}; texture->GetDesc(&desc);
  D3D11_SHADER_RESOURCE_VIEW_DESC vd{}; view->GetDesc(&vd);
  hr = mipGenerationStatus(desc, vd);
  if (FAILED(hr)) return hr;
  const UINT firstMip = vd.Texture3D.MostDetailedMip;
  const UINT mipCount = vd.Texture3D.MipLevels;
  if (!desc.Width || !desc.Height || !desc.Depth
      || desc.MipLevels > D3D11_REQ_MIP_LEVELS
      || firstMip >= D3D11_REQ_MIP_LEVELS) return E_INVALIDARG;
  if (mipCount == 1) return S_OK;

  D3D11_TEXTURE3D_DESC scratchDesc{};
  scratchDesc.Width = std::max(1u, desc.Width >> firstMip);
  scratchDesc.Height = std::max(1u, desc.Height >> firstMip);
  scratchDesc.Depth = std::max(1u, desc.Depth >> firstMip);
  scratchDesc.MipLevels = mipCount;
  scratchDesc.Format = vd.Format;
  scratchDesc.Usage = D3D11_USAGE_DEFAULT;
  scratchDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  scratchDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  ComPtr<ID3D11Texture3D> scratch;
  hr = device->CreateTexture3D(&scratchDesc, nullptr, &scratch);
  if (FAILED(hr)) return hr;
  if (!scratch) return E_FAIL;
  D3D11_SHADER_RESOURCE_VIEW_DESC scratchViewDesc{};
  scratchViewDesc.Format = vd.Format;
  scratchViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
  scratchViewDesc.Texture3D.MipLevels = mipCount;
  ComPtr<ID3D11ShaderResourceView> scratchView;
  hr = device->CreateShaderResourceView(scratch.Get(), &scratchViewDesc, &scratchView);
  if (FAILED(hr)) return hr;
  if (!scratchView) return E_FAIL;

  // Allocate both owners before recording. Backend command retention keeps
  // these GPU-only copies/blits alive without CPU texels or a global idle.
  context->CopySubresourceRegion(scratch.Get(), 0, 0, 0, 0,
    texture.Get(), firstMip, nullptr);
  context->GenerateMips(scratchView.Get());
  for (UINT mip = 1; mip < mipCount; ++mip)
    context->CopySubresourceRegion(texture.Get(), firstMip + mip, 0, 0, 0,
      scratch.Get(), mip, nullptr);
  return S_OK;
}

// Normalize each selected 2D slice or cube face independently. A level-zero
// scratch view removes both offsets from backend mip generation; only generated
// levels are copied back, preserving the source, excluded tail and other faces.
inline HRESULT generateTexture2DViewMips(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Resource* resource,
    ID3D11ShaderResourceView* view) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&texture));
  if (FAILED(hr)) return hr;
  if (!texture) return E_FAIL;
  D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
  D3D11_SHADER_RESOURCE_VIEW_DESC vd{}; view->GetDesc(&vd);
  hr = mipGenerationStatus(desc, vd);
  if (FAILED(hr)) return hr;
  UINT firstMip = 0, mipCount = 0, firstSlice = 0, sliceCount = 1;
  switch (vd.ViewDimension) {
    case D3D11_SRV_DIMENSION_TEXTURE2D:
      firstMip = vd.Texture2D.MostDetailedMip;
      mipCount = vd.Texture2D.MipLevels;
      break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
      firstMip = vd.Texture2DArray.MostDetailedMip;
      mipCount = vd.Texture2DArray.MipLevels;
      firstSlice = vd.Texture2DArray.FirstArraySlice;
      sliceCount = vd.Texture2DArray.ArraySize;
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE:
      firstMip = vd.TextureCube.MostDetailedMip;
      mipCount = vd.TextureCube.MipLevels;
      sliceCount = 6;
      break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
      firstMip = vd.TextureCubeArray.MostDetailedMip;
      mipCount = vd.TextureCubeArray.MipLevels;
      firstSlice = vd.TextureCubeArray.First2DArrayFace;
      // The validated cube range bounds this multiplication by ArraySize.
      sliceCount = vd.TextureCubeArray.NumCubes * 6;
      break;
    default:
      return E_INVALIDARG;
  }
  if (!desc.Width || !desc.Height
      || desc.MipLevels > D3D11_REQ_MIP_LEVELS
      || firstMip >= D3D11_REQ_MIP_LEVELS
      || desc.ArraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION
      || !viewRange(firstSlice, sliceCount, desc.ArraySize)) return E_INVALIDARG;
  if (mipCount == 1) return S_OK;

  D3D11_TEXTURE2D_DESC scratchDesc{};
  scratchDesc.Width = std::max(1u, desc.Width >> firstMip);
  scratchDesc.Height = std::max(1u, desc.Height >> firstMip);
  scratchDesc.MipLevels = mipCount;
  scratchDesc.ArraySize = 1;
  scratchDesc.Format = vd.Format;
  scratchDesc.SampleDesc.Count = 1;
  scratchDesc.Usage = D3D11_USAGE_DEFAULT;
  scratchDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  scratchDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  ComPtr<ID3D11Texture2D> scratch;
  hr = device->CreateTexture2D(&scratchDesc, nullptr, &scratch);
  if (FAILED(hr)) return hr;
  if (!scratch) return E_FAIL;
  D3D11_SHADER_RESOURCE_VIEW_DESC scratchViewDesc{};
  scratchViewDesc.Format = vd.Format;
  scratchViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  scratchViewDesc.Texture2D.MipLevels = mipCount;
  ComPtr<ID3D11ShaderResourceView> scratchView;
  hr = device->CreateShaderResourceView(scratch.Get(), &scratchViewDesc, &scratchView);
  if (FAILED(hr)) return hr;
  if (!scratchView) return E_FAIL;

  // Create both owners before recording. Ordered GPU copies/blits and backend
  // command retention own asynchronous lifetime; no CPU texels or global idle.
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
  if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
    return generateVolumeViewMips(device, context, resource.Get(), view);
  if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
    return generateTexture2DViewMips(device, context, resource.Get(), view);
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
