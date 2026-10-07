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
  *output = nativeFormatCaps(support);
  return S_OK;
}

template<typename Check>
HRESULT queryNativeMultisampleLevels(DXGI_FORMAT format, UINT count, UINT* output, Check&& check) {
  if (!output) return E_INVALIDARG;
  *output = 0;
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
