#pragma once

#include <d3d11.h>
#include <functional>
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
// Internal renderer entry: the token retains its real VidMm/AHB owner.
HRESULT createNativeTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc,
    const RuntimeBackend& runtime, const mwd_allocation& allocation, uint32_t stride,
    ID3D11Texture2D** texture, bool preserveContents = false) noexcept;
bool nativeSurfacesSupported(ID3D11Device* device) noexcept;
// The optional transaction validates/pins UMD owners and calls its supplied
// storage commit exactly once. That commit only changes prepared CPU state.
using TextureStorageCommit = std::function<HRESULT(const std::function<void()>&)>;
HRESULT rotateNativeTextureStorage(ID3D11DeviceContext* context, UINT count,
    ID3D11Texture2D* const* textures, const TextureStorageCommit& commit = {}) noexcept;
// Finish local writes and establish GENERAL before a runtime-owned write.
HRESULT prepareNativeTextureForExternalWrite(ID3D11DeviceContext*, ID3D11Resource*) noexcept;
// Join recording and wait for all GPU reads/writes to the native image.
// Used only before a runtime Blt takes exclusive external ownership.
HRESULT waitNativeTextureIdle(ID3D11DeviceContext*, ID3D11Resource*) noexcept;
}
