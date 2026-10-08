// SPDX-License-Identifier: MIT
// Actual typed primary/SetDisplayMode/Present callbacks; controlled WARP cache
// and callback-owned backing. No real KMD, scanout or factory admission.
#include <wrl/client.h>
#define main inheritedPrimaryMain
#include "umd-dxgi-primary.cpp"
#undef main
static unsigned extendedImages;
template<typename F>
static void typedClear(F& f, Texture<F>& texture, DXGI_FORMAT format) {
  D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = texture.handle; args.Format = format;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D.ArraySize = 1;
  Storage storage(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &args)); D3D10DDI_HRENDERTARGETVIEW view{storage.bytes};
  f.table.pfnCreateRenderTargetView(f.device, &args, view, {}); CHECK(lastError == S_OK);
  FLOAT values[]{.5f, 0, 1, .25f}; f.table.pfnClearRenderTargetView(f.device, view, values); CHECK(lastError == S_OK);
  f.table.pfnDestroyRenderTargetView(f.device, view);
}
static void extendedCapture(unsigned profile, unsigned scene, D3DKMT_HANDLE handle) {
  const auto& backing = owned.at(handle);
  char name[100]; std::snprintf(name, sizeof(name), "extended-primary-%u-%u.actual.bin", profile, scene); save(name, backing.data(), 128);
  std::snprintf(name, sizeof(name), "extended-primary-%u-%u.allocation.bin", profile, scene); save(name, &backing.info, 80);
  const UINT metadata[]{profile, scene, 8, 4, handle, backing.info.flags, backing.info.format};
  std::snprintf(name, sizeof(name), "extended-primary-%u-%u.metadata.bin", profile, scene); save(name, metadata, sizeof(metadata)); ++extendedImages;
}
template<typename Table>
static void extendedProfile(unsigned profile, UINT pipeline) {
  Fixture<Table, true> f(true, pipeline);
  Texture<decltype(f)> texture(f, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
  CHECK(texture.primary.DriverFlags == 0); expectedHandle = texture.allocation;
  typedClear(f, texture, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
  CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == S_OK);
  extendedCapture(profile, 0, texture.allocation);
  // A second upload through a linear cast must replace the old encoded frame.
  typedClear(f, texture, DXGI_FORMAT_B8G8R8A8_UNORM);
  CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(texture.handle.pDrvPrivate)) == S_OK);
  extendedCapture(profile, 1, texture.allocation);
  for (unsigned scene = 2; scene <= 3; ++scene) {
    const DXGI_FORMAT format = scene == 2 ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
    Texture<decltype(f)> optional(f, format, true); CHECK(optional.primary.DriverFlags == 1);
    typedClear(f, optional, format); expectedHandle = optional.allocation;
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(optional.handle.pDrvPrivate)) == DXGI_DDI_ERR_UNSUPPORTED);
    DXGI_DDI_ARG_PRESENT request{}; request.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);
    request.hSurfaceToPresent = reinterpret_cast<DXGI_DDI_HRESOURCE>(optional.handle.pDrvPrivate); request.pDXGIContext = &adapterCookie; request.Flags.Blt = 1;
    CHECK(f.dxgi.table.pfnPresent(&request) == S_OK); extendedCapture(profile, scene, optional.allocation);
  }
  {
    Texture<decltype(f)> rgba(f, DXGI_FORMAT_R8G8B8A8_UNORM, false, 0, D3D10_DDI_BIND_SHADER_RESOURCE);
    expectedHandle = rgba.allocation;
    typedClear(f, rgba, DXGI_FORMAT_UNKNOWN);
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(rgba.handle.pDrvPrivate)) == S_OK);
    extendedCapture(profile, 4, rgba.allocation);
    UINT observed[8]{28, 0, 0, 0, 0, 0, 0, 0};
    for (const auto format : {DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_UNKNOWN}) {
      D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = rgba.handle; args.Format = format;
      args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D.ArraySize = 1;
      Storage out(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &args)); D3D10DDI_HRENDERTARGETVIEW view{out.bytes};
      f.table.pfnCreateRenderTargetView(f.device, &args, view, {}); CHECK(lastError == S_OK);
      if constexpr (std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>) f.table.pfnSetRenderTargets(f.device, &view, 1, 0, {}, nullptr, nullptr, 1, 0, 1, 0);
      else f.table.pfnSetRenderTargets(f.device, &view, 1, 0, {});
      CHECK(lastError == S_OK);
      // Borrow the actual renderer context captured by this WARP factory.
      auto nativeContext = primaryRendererContext; CHECK(nativeContext);
      Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv; nativeContext->OMGetRenderTargets(1, &rtv, nullptr); CHECK(rtv);
      D3D11_RENDER_TARGET_VIEW_DESC desc{}; rtv->GetDesc(&desc);
      CHECK(desc.Format == (format == DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_R8G8B8A8_UNORM : format));
      observed[format == DXGI_FORMAT_UNKNOWN ? 4 : 2] = UINT(desc.Format);
      Microsoft::WRL::ComPtr<ID3D11Resource> image; rtv->GetResource(&image);
      Microsoft::WRL::ComPtr<ID3D11Texture2D> native; CHECK(image.As(&native) == S_OK);
      D3D11_TEXTURE2D_DESC nativeDesc{}; native->GetDesc(&nativeDesc); CHECK(nativeDesc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS);
      observed[1] = UINT(nativeDesc.Format); nativeContext->ClearState(); f.table.pfnDestroyRenderTargetView(f.device, view);
      using Arg = std::conditional_t<std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>, D3D11DDIARG_CREATESHADERRESOURCEVIEW, D3D10_1DDIARG_CREATESHADERRESOURCEVIEW>;
      Arg srvArgs{}; srvArgs.hDrvResource = rgba.handle; srvArgs.Format = format; srvArgs.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
      srvArgs.Tex2D.ArraySize = srvArgs.Tex2D.MipLevels = 1;
      Storage srvOut(f.table.pfnCalcPrivateShaderResourceViewSize(f.device, &srvArgs)); D3D10DDI_HSHADERRESOURCEVIEW srvView{srvOut.bytes};
      f.table.pfnCreateShaderResourceView(f.device, &srvArgs, srvView, {}); CHECK(lastError == S_OK);
      f.table.pfnPsSetShaderResources(f.device, 0, 1, &srvView); CHECK(lastError == S_OK);
      Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; nativeContext->PSGetShaderResources(0, 1, &srv); CHECK(srv);
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{}; srv->GetDesc(&srvDesc);
      CHECK(srvDesc.Format == (format == DXGI_FORMAT_UNKNOWN ? DXGI_FORMAT_R8G8B8A8_UNORM : format));
      observed[format == DXGI_FORMAT_UNKNOWN ? 5 : 3] = UINT(srvDesc.Format);
      nativeContext->ClearState(); f.table.pfnDestroyShaderResourceView(f.device, srvView);
    }
    char name[100]; std::snprintf(name, sizeof(name), "extended-primary-%u.cast-formats.bin", profile); save(name, observed, sizeof(observed));
    CHECK(f.set(reinterpret_cast<DXGI_DDI_HRESOURCE>(rgba.handle.pDrvPrivate)) == S_OK);
    extendedCapture(profile, 5, rgba.allocation);
  }
  // BGRX sRGB lacks scanout. No backend or allocation is published on failure.
  Texture<decltype(f)> rejected(f, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, false, 19);
  CHECK(lastError == DXGI_DDI_ERR_UNSUPPORTED); lastError = S_OK;
}
int main() {
  caller = GetCurrentThreadId();
  extendedProfile<D3D10_1DDI_DEVICEFUNCS>(0, D3D11DDI_3DPIPELINELEVEL_10_1);
  extendedProfile<D3D11DDI_DEVICEFUNCS>(1, D3D11DDI_3DPIPELINELEVEL_10_0);
  extendedProfile<D3D11DDI_DEVICEFUNCS>(2, D3D11DDI_3DPIPELINELEVEL_10_1);
  CHECK(extendedImages == 18 && owned.empty() && contexts == contextCloses && locks == unlocks);
  std::printf("DXGI extended primary PASS profiles=3 images=18 pixels=576 hardware_admission=0\n");
}
