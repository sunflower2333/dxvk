#pragma once

#include <d3d11.h>

// This header intentionally does not include DXVK's private KMT definitions:
// Windows DDI translation units use the WDK's canonical definitions instead.
namespace dxvk::umd {
HRESULT createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
                     ID3D11Device** device, ID3D11DeviceContext** context) noexcept;
// The context and resource must belong to the embedded backend above.
HRESULT isStagingResourceBusy(ID3D11DeviceContext* context,
                             ID3D11Resource* resource, BOOL* busy) noexcept;
}
