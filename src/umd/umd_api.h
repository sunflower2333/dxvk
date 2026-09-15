#pragma once

#include <d3d11.h>
#include "umd_runtime_bridge.h"

// This header intentionally does not include DXVK's private KMT definitions:
// Windows DDI translation units use the WDK's canonical definitions instead.
namespace dxvk::umd {
// Stream-output translation uses the D3D11 no-rasterized-stream contract.
// This is the PRIVATE renderer minimum, not a published native DDI feature level.
inline constexpr D3D_FEATURE_LEVEL implementationFeatureLevel(D3D_FEATURE_LEVEL requested) {
  return requested < D3D_FEATURE_LEVEL_11_0 ? D3D_FEATURE_LEVEL_11_0 : requested;
}
HRESULT createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
                     ID3D11Device** device, ID3D11DeviceContext** context,
                     const RuntimeBackend* runtime = nullptr) noexcept;
// The context and resource must belong to the embedded backend above.
HRESULT isStagingResourceBusy(ID3D11DeviceContext* context,
                             ID3D11Resource* resource, BOOL* busy) noexcept;
// Joins recording and kernel submission, without waiting for GPU completion.
HRESULT flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept;
}
