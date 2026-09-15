#pragma once
// SPDX-License-Identifier: MIT
#include "umd_output_policy.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>

namespace dxvk::umd {

struct OutputView {
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  OutputShape shape;
  UINT mip = 0, firstSlice = 0;
};

// Describe the supported Texture2D view range without allowing overflow or shifts >=32.
inline bool outputShape(const D3D11_TEXTURE2D_DESC& texture, UINT mip,
    UINT firstSlice, UINT layers, OutputShape& result) {
  if (!texture.Width || !texture.Height || !texture.SampleDesc.Count ||
      mip >= texture.MipLevels || mip >= D3D11_REQ_MIP_LEVELS || !layers ||
      firstSlice >= texture.ArraySize || layers > texture.ArraySize - firstSlice)
    return false;
  result = {std::max(1u, texture.Width >> mip), std::max(1u, texture.Height >> mip),
    layers, texture.SampleDesc.Count, texture.SampleDesc.Quality};
  return true;
}

// A 1D mip has one row, but is not a 2D texture whose height happens to be one.
inline bool outputShape(const D3D11_TEXTURE1D_DESC& texture, UINT mip,
    UINT firstSlice, UINT layers, OutputShape& result) {
  if (!texture.Width || mip >= texture.MipLevels || mip >= D3D11_REQ_MIP_LEVELS ||
      !layers || firstSlice >= texture.ArraySize || layers > texture.ArraySize - firstSlice)
    return false;
  result = {std::max(1u, texture.Width >> mip), 1, layers, 1, 0, OutputKind::Texture1D};
  return true;
}

// Decode the 1D RTV range before OM binding or alias checks.
inline bool texture1DOutputView(ID3D11RenderTargetView* view, OutputView& result) {
  Microsoft::WRL::ComPtr<ID3D11Texture1D> texture;
  if (FAILED(result.resource.As(&texture))) return false;
  D3D11_TEXTURE1D_DESC desc = {}; texture->GetDesc(&desc);
  D3D11_RENDER_TARGET_VIEW_DESC range = {}; view->GetDesc(&range);
  UINT layers = 1;
  result.mip = result.firstSlice = 0;
  if (range.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D) {
    if (desc.ArraySize != 1) return false;
    result.mip = range.Texture1D.MipSlice;
  } else if (range.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY) {
    result.mip = range.Texture1DArray.MipSlice;
    result.firstSlice = range.Texture1DArray.FirstArraySlice;
    layers = range.Texture1DArray.ArraySize;
  } else return false;
  return outputShape(desc, result.mip, result.firstSlice, layers, result.shape);
}

// Extract actual resource/view metadata before the single OM state change.
inline bool outputView(ID3D11RenderTargetView* view, OutputView& result) {
  if (!view) return false;
  result = {};
  view->GetResource(&result.resource);
  if (!result.resource) return false;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  result.resource->GetType(&dimension);
  if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D) return texture1DOutputView(view, result);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  if (!result.resource || FAILED(result.resource.As(&texture))) return false;
  D3D11_TEXTURE2D_DESC desc = {}; texture->GetDesc(&desc);
  D3D11_RENDER_TARGET_VIEW_DESC range = {}; view->GetDesc(&range);
  UINT layers = 1;
  result.mip = result.firstSlice = 0;
  switch (range.ViewDimension) {
    case D3D11_RTV_DIMENSION_TEXTURE2D:
      if (desc.SampleDesc.Count != 1) return false;
      result.mip = range.Texture2D.MipSlice;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
      if (desc.SampleDesc.Count != 1) return false;
      result.mip = range.Texture2DArray.MipSlice;
      result.firstSlice = range.Texture2DArray.FirstArraySlice;
      layers = range.Texture2DArray.ArraySize;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS:
      if (desc.SampleDesc.Count <= 1) return false;
      break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
      if (desc.SampleDesc.Count <= 1) return false;
      result.firstSlice = range.Texture2DMSArray.FirstArraySlice;
      layers = range.Texture2DMSArray.ArraySize;
      break;
    default: return false;
  }
  return outputShape(desc, result.mip, result.firstSlice, layers, result.shape);
}

// Reject binding the same subresource twice; non-overlapping array slices are valid.
inline bool overlappingOutputs(const OutputView& left, const OutputView& right) {
  return left.resource.Get() == right.resource.Get() && left.mip == right.mip &&
    outputRangesOverlap(left.firstSlice, left.shape.layers, right.firstSlice, right.shape.layers);
}

// Describe a real Texture1D depth view without reinterpreting its union members.
inline bool texture1DDepthShape(ID3D11DepthStencilView* view,
    ID3D11Resource* resource, OutputShape& result) {
  Microsoft::WRL::ComPtr<ID3D11Texture1D> texture;
  if (!resource || FAILED(resource->QueryInterface(IID_PPV_ARGS(&texture)))) return false;
  D3D11_TEXTURE1D_DESC desc = {}; texture->GetDesc(&desc);
  D3D11_DEPTH_STENCIL_VIEW_DESC range = {}; view->GetDesc(&range);
  UINT mip = 0, first = 0, layers = 1;
  if (range.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D) {
    if (desc.ArraySize != 1) return false;
    mip = range.Texture1D.MipSlice;
  } else if (range.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY) {
    mip = range.Texture1DArray.MipSlice;
    first = range.Texture1DArray.FirstArraySlice;
    layers = range.Texture1DArray.ArraySize;
  } else return false;
  return outputShape(desc, mip, first, layers, result);
}

// Depth participates in the same shape check but may have a different format.
inline bool depthOutputShape(ID3D11DepthStencilView* view, OutputShape& result) {
  if (!view) return false;
  Microsoft::WRL::ComPtr<ID3D11Resource> resource;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  view->GetResource(&resource);
  if (!resource) return false;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  resource->GetType(&dimension);
  if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
    return texture1DDepthShape(view, resource.Get(), result);
  if (FAILED(resource.As(&texture))) return false;
  D3D11_TEXTURE2D_DESC desc = {}; texture->GetDesc(&desc);
  D3D11_DEPTH_STENCIL_VIEW_DESC range = {}; view->GetDesc(&range);
  UINT mip = 0, first = 0, layers = 1;
  switch (range.ViewDimension) {
    case D3D11_DSV_DIMENSION_TEXTURE2D:
      if (desc.SampleDesc.Count != 1) return false;
      mip = range.Texture2D.MipSlice;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      if (desc.SampleDesc.Count != 1) return false;
      mip = range.Texture2DArray.MipSlice;
      first = range.Texture2DArray.FirstArraySlice;
      layers = range.Texture2DArray.ArraySize;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS:
      if (desc.SampleDesc.Count <= 1) return false;
      break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      if (desc.SampleDesc.Count <= 1) return false;
      first = range.Texture2DMSArray.FirstArraySlice;
      layers = range.Texture2DMSArray.ArraySize;
      break;
    default: return false;
  }
  return outputShape(desc, mip, first, layers, result);
}

}
