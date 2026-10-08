#pragma once
// SPDX-License-Identifier: MIT
#include "umd_copy_format.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace dxvk::umd {

// Inspect the real backend descriptors before issuing a void CopyResource.
// Cube compatibility follows its real Texture2D resource type in D3D10.1/11.
inline bool resourceCopyCompatible(ID3D11Resource* destination, ID3D11Resource* source) {
  if (!destination || !source || destination == source) return false;
  D3D11_RESOURCE_DIMENSION dstKind, srcKind;
  destination->GetType(&dstKind); source->GetType(&srcKind);
  if (dstKind != srcKind) return false;
  using Microsoft::WRL::ComPtr;
  if (dstKind == D3D11_RESOURCE_DIMENSION_BUFFER) {
    ComPtr<ID3D11Buffer> dst, src;
    if (FAILED(destination->QueryInterface(IID_PPV_ARGS(&dst)))
        || FAILED(source->QueryInterface(IID_PPV_ARGS(&src)))) return false;
    D3D11_BUFFER_DESC d{}, s{}; dst->GetDesc(&d); src->GetDesc(&s);
    return d.Usage != D3D11_USAGE_IMMUTABLE && d.ByteWidth == s.ByteWidth;
  }
  if (dstKind == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
    ComPtr<ID3D11Texture1D> dst, src;
    if (FAILED(destination->QueryInterface(IID_PPV_ARGS(&dst)))
        || FAILED(source->QueryInterface(IID_PPV_ARGS(&src)))) return false;
    D3D11_TEXTURE1D_DESC d{}, s{}; dst->GetDesc(&d); src->GetDesc(&s);
    return d.Usage != D3D11_USAGE_IMMUTABLE && d.Width == s.Width
      && d.MipLevels == s.MipLevels && d.ArraySize == s.ArraySize
      && copyFormatsCompatible(d.Format, s.Format);
  }
  if (dstKind == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
    ComPtr<ID3D11Texture2D> dst, src;
    if (FAILED(destination->QueryInterface(IID_PPV_ARGS(&dst)))
        || FAILED(source->QueryInterface(IID_PPV_ARGS(&src)))) return false;
    D3D11_TEXTURE2D_DESC d{}, s{}; dst->GetDesc(&d); src->GetDesc(&s);
    return d.Usage != D3D11_USAGE_IMMUTABLE && d.Width == s.Width && d.Height == s.Height
      && d.MipLevels == s.MipLevels && d.ArraySize == s.ArraySize
      && d.SampleDesc.Count == s.SampleDesc.Count
      && (d.SampleDesc.Count == 1 || d.SampleDesc.Quality == s.SampleDesc.Quality)
      && copyFormatsCompatible(d.Format, s.Format);
  }
  if (dstKind == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
    ComPtr<ID3D11Texture3D> dst, src;
    if (FAILED(destination->QueryInterface(IID_PPV_ARGS(&dst)))
        || FAILED(source->QueryInterface(IID_PPV_ARGS(&src)))) return false;
    D3D11_TEXTURE3D_DESC d{}, s{}; dst->GetDesc(&d); src->GetDesc(&s);
    return d.Usage != D3D11_USAGE_IMMUTABLE && d.Width == s.Width && d.Height == s.Height
      && d.Depth == s.Depth && d.MipLevels == s.MipLevels && copyFormatsCompatible(d.Format, s.Format);
  }
  return false;
}

}
