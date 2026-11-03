// Standalone DDI tests inject WARP or a CPU-only backend. Native AHB import
// belongs to the embedded renderer and is deliberately unavailable there.
#include "../src/umd/umd_api.h"
namespace dxvk::umd {
bool nativeSurfacesSupported(ID3D11Device*) noexcept { return false; }
HRESULT createNativeTexture2D(ID3D11Device*, const D3D11_TEXTURE2D_DESC&,
    const RuntimeBackend&, const mwd_allocation&, uint32_t, ID3D11Texture2D** out, bool) noexcept {
  if (out) *out = nullptr;
  return DXGI_ERROR_UNSUPPORTED;
}
HRESULT rotateNativeTextureStorage(ID3D11DeviceContext*, UINT, ID3D11Texture2D* const*, const TextureStorageCommit&) noexcept {
  return DXGI_ERROR_UNSUPPORTED;
}
HRESULT prepareNativeTextureForExternalWrite(ID3D11DeviceContext*, ID3D11Resource*) noexcept {
  return DXGI_ERROR_UNSUPPORTED;
}
HRESULT waitNativeTextureIdle(ID3D11DeviceContext*, ID3D11Resource*) noexcept {
  return DXGI_ERROR_UNSUPPORTED;
}
}
