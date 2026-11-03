#pragma once
// SPDX-License-Identifier: MIT
#include "umd_ddi.h"
#include "umd_multisample_policy.h"
#include <d3d11.h>

namespace dxvk::umd {

// These are the five optional bits in the negotiated 10.0/10.1/11.0 DDI.
// Public D3D11 IA, buffer, UAV and video bits are not this table's ABI.
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

inline bool isDepthStencilAspectFormat(DXGI_FORMAT format) {
  return format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS
      || format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
      || format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS
      || format == DXGI_FORMAT_X24_TYPELESS_G8_UINT;
}

inline UINT nativeFormatCaps(DXGI_FORMAT format, UINT api) {
  UINT result = nativeFormatCaps(api);
  // These are shader-resource view formats. Their backing depth/stencil
  // image's attachment capabilities do not apply to the aspect view itself.
  // See Microsoft's feature-level format tables and CheckFormatSupport DDI.
  if (isDepthStencilAspectFormat(format)) {
    result &= ~(D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET
              | D3D10_DDI_FORMAT_SUPPORT_BLENDABLE
              | D3D10_DDI_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET);
    if (format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
        || format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
      result &= ~D3D10_DDI_FORMAT_SUPPORT_SHADER_SAMPLE;
  }
  return result;
}

template<typename Check>
HRESULT queryNativeFormatCaps(DXGI_FORMAT format, UINT* output, Check&& check) {
  if (!output) return E_INVALIDARG;
  *output = 0;
  // XR_BIAS has scan-out/CPU-lock/cast attributes, not sampling or RT
  // attributes. Our paired primary allocation path implements RGBA8/BGRA8,
  // so even a renderer DISPLAY bit cannot establish native XR scan-out.
  if (format == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM) {
    *output = D3D10_DDI_FORMAT_SUPPORT_NOT_SUPPORTED;
    return S_OK;
  }
  UINT support = 0;
  const HRESULT hr = check(format, &support);
  if (hr != S_OK) return hr == E_INVALIDARG || SUCCEEDED(hr) ? E_FAIL : hr;
  *output = nativeFormatCaps(format, support);
  return S_OK;
}

template<typename Check>
HRESULT queryNativeMultisampleLevels(DXGI_FORMAT format, UINT count, UINT* output, Check&& check) {
  if (!output) return E_INVALIDARG;
  *output = 0;
  // MSAA quality describes renderable resources, not an SRV of one aspect.
  // Count one still follows the DDI's successful single-sample contract.
  if (count > 1 && (isDepthStencilAspectFormat(format)
      || format == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)) return S_OK;
  HRESULT hr = S_OK;
  uint32_t levels = 0;
  const bool complete = multisampleQualityLevels(count, levels, [&](uint32_t& staged) {
    // Query even for count1: a nonexistent format must still report the
    // backend's error. Normalize only an actual successful query.
    hr = check(format, count, &staged);
    return hr == S_OK;
  });
  if (!complete) return FAILED(hr) ? hr : E_FAIL;
  *output = levels;
  return S_OK;
}

}
