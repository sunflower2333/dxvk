// SPDX-License-Identifier: MIT
// Reuse the bounded typed-DDI WARP owner. This is controlled renderer proof,
// never a system factory, hardware, scanout or ordinary-runtime admission.
#define main inheritedBltMain
#include "umd-dxgi-blt.cpp"
#undef main
static unsigned extendedImages, extendedPixels;
template<typename F>
static void extendedRead(F& f, Texture<F>& texture, unsigned profile, unsigned format, unsigned scene) {
  Texture<F> staging(f, texture.width, texture.height, texture.format, 0, 0, false, 1, 1, 1, true);
  f.table.pfnResourceCopy(f.device, staging.handle, texture.handle); CHECK(lastError == S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE map{};
  f.table.pfnStagingResourceMap(f.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &map);
  CHECK(lastError == S_OK && map.pData && map.RowPitch >= texture.width * 4);
  std::vector<uint32_t> actual(size_t(texture.width) * texture.height);
  for (UINT y = 0; y < texture.height; ++y) std::memcpy(actual.data() + size_t(y) * texture.width,
    static_cast<const uint8_t*>(map.pData) + size_t(y) * map.RowPitch, texture.width * 4);
  f.table.pfnStagingResourceUnmap(f.device, staging.handle, 0); CHECK(lastError == S_OK);
  const uint32_t metadata[]{profile, format, scene, texture.width, texture.height, UINT(texture.format), map.RowPitch};
  char name[100]; std::snprintf(name, sizeof(name), "extended-blt-%u-%u-%u.actual.bin", profile, format, scene);
  save(name, actual.data(), actual.size() * 4);
  std::snprintf(name, sizeof(name), "extended-blt-%u-%u-%u.metadata.bin", profile, format, scene); save(name, metadata, sizeof(metadata));
  ++extendedImages; extendedPixels += UINT(actual.size());
}
template<typename F>
static void extendedClear(F& f, Texture<F>& texture, DXGI_FORMAT format, FLOAT* value) {
  D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = texture.handle; args.Format = format;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D.ArraySize = 1;
  Storage storage(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &args)); D3D10DDI_HRENDERTARGETVIEW view{storage.bytes};
  f.table.pfnCreateRenderTargetView(f.device, &args, view, {}); CHECK(lastError == S_OK);
  f.table.pfnClearRenderTargetView(f.device, view, value); CHECK(lastError == S_OK);
  f.table.pfnDestroyRenderTargetView(f.device, view);
}
// The real native renderer samples the SRV made by the production DDI. These
// pipeline objects are independent of the UMD's Blt normalized-format helper.
static void drawSampled(ID3D11DeviceContext* context, ID3D11ShaderResourceView* source,
    ID3D11RenderTargetView* target, UINT width, UINT height, UINT mask = ~0u) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D11Device> device; context->GetDevice(&device);
  std::vector<unsigned char> vsCode, psCode; CHECK(dxvk::umd::bltShaderContainers(vsCode, psCode));
  ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
  CHECK(device->CreateVertexShader(vsCode.data(), vsCode.size(), nullptr, &vs) == S_OK);
  CHECK(device->CreatePixelShader(psCode.data(), psCode.size(), nullptr, &ps) == S_OK);
  const D3D11_INPUT_ELEMENT_DESC elements[]{
    {dxvk::umd::inputRegisterSemantic, 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {dxvk::umd::inputRegisterSemantic, 1, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}};
  ComPtr<ID3D11InputLayout> layout; CHECK(device->CreateInputLayout(elements, 2, vsCode.data(), vsCode.size(), &layout) == S_OK);
  const auto vertices = dxvk::umd::bltVertices(1);
  D3D11_BUFFER_DESC vbDesc{}; vbDesc.ByteWidth = sizeof(vertices); vbDesc.Usage = D3D11_USAGE_IMMUTABLE; vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA data{vertices.data(), 0, 0}; ComPtr<ID3D11Buffer> buffer;
  CHECK(device->CreateBuffer(&vbDesc, &data, &buffer) == S_OK);
  D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxAnisotropy = 1;
  ComPtr<ID3D11SamplerState> sampler; CHECK(device->CreateSamplerState(&sd, &sampler) == S_OK);
  context->ClearState(); context->IASetInputLayout(layout.Get());
  ID3D11Buffer* buffers[]{buffer.Get()}; const UINT stride = sizeof(dxvk::umd::BltVertex), offset = 0;
  context->IASetVertexBuffers(0, 1, buffers, &stride, &offset); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &source); ID3D11SamplerState* samplers[]{sampler.Get()}; context->PSSetSamplers(0, 1, samplers);
  context->OMSetRenderTargets(1, &target, nullptr); context->OMSetBlendState(nullptr, nullptr, mask);
  const D3D11_VIEWPORT viewport{0, 0, FLOAT(width), FLOAT(height), 0, 1}; context->RSSetViewports(1, &viewport);
  context->Draw(3, 0); context->ClearState(); CHECK(device->GetDeviceRemovedReason() == S_OK);
}
template<typename F>
static void extendedBind(F& f, D3D10DDI_HRENDERTARGETVIEW view) {
  if constexpr (std::is_same_v<F, Fixture<D3D11DDI_DEVICEFUNCS>>)
    f.table.pfnSetRenderTargets(f.device, &view, 1, 0, {}, nullptr, nullptr, 1, 0, 1, 0);
  else f.table.pfnSetRenderTargets(f.device, &view, 1, 0, {});
  CHECK(lastError == S_OK);
}
template<typename F>
static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> extendedSrv(F& f, Texture<F>& texture, DXGI_FORMAT format) {
  using Arg = std::conditional_t<std::is_same_v<F, Fixture<D3D11DDI_DEVICEFUNCS>>, D3D11DDIARG_CREATESHADERRESOURCEVIEW, D3D10_1DDIARG_CREATESHADERRESOURCEVIEW>;
  Arg args{}; args.hDrvResource = texture.handle; args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  args.Format = format; args.Tex2D.MipLevels = args.Tex2D.ArraySize = 1;
  Storage storage(f.table.pfnCalcPrivateShaderResourceViewSize(f.device, &args)); D3D10DDI_HSHADERRESOURCEVIEW view{storage.bytes};
  f.table.pfnCreateShaderResourceView(f.device, &args, view, {}); CHECK(lastError == S_OK);
  f.table.pfnPsSetShaderResources(f.device, 0, 1, &view); CHECK(lastError == S_OK);
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> result; f.contextKey->PSGetShaderResources(0, 1, &result);
  CHECK(result); f.contextKey->ClearState(); f.table.pfnDestroyShaderResourceView(f.device, view); return result;
}
template<typename Table>
static void extendedProfile(unsigned profile, UINT pipeline) {
  using F = Fixture<Table>; F f(true, pipeline);
  constexpr UINT bind = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
  const DXGI_FORMAT formats[]{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB};
  for (unsigned format = 0; format < 2; ++format) {
    Texture<F> source(f, 6, 4, formats[format], bind | D3D10_DDI_BIND_PRESENT);
    std::array<uint32_t, 24> input{};
    for (UINT y = 0; y < 4; ++y) for (UINT x = 0; x < 6; ++x)
      input[y * 6 + x] = (color(x, y) & 0x00ffffffu) | ((16 + 8 * x + 16 * y) << 24);
    f.table.pfnResourceUpdateSubresourceUP(f.device, source.handle, 0, nullptr, input.data(), 24, 96); CHECK(lastError == S_OK);
    for (unsigned scene = 0; scene < 5; ++scene) {
      const UINT width = scene == 2 ? 4 : scene == 3 ? 3 : 6;
      const UINT height = scene == 2 ? 6 : scene == 3 ? 2 : 4;
      const DXGI_FORMAT destinationFormat = scene == 1 ? (format ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM)
        : scene == 4 ? DXGI_FORMAT_R8G8B8A8_UNORM : formats[format];
      Texture<F> destination(f, width, height, destinationFormat, bind);
      DXGI_DDI_ARG_BLT request{}; request.hSrcResource = source.dxgi(); request.hDstResource = destination.dxgi();
      request.DstRight = width; request.DstBottom = height; request.Rotate = scene == 2 ? DXGI_DDI_MODE_ROTATION_ROTATE90 : DXGI_DDI_MODE_ROTATION_IDENTITY;
      request.Flags.Value = scene == 1 || scene == 4 ? 2 : scene == 3 ? 4 : 0;
      CHECK(f.blt(request) == S_OK && lastError == S_OK); extendedRead(f, destination, profile, format, scene);
      if (scene == 1 || scene == 4) { request.Flags.Value = 0; CHECK(f.blt(request) == E_INVALIDARG); }
    }
    FLOAT value[]{.5f, 0, 1, .25f};
    const auto linear = format ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    extendedClear(f, source, linear, value); extendedRead(f, source, profile, format, 5);
    extendedClear(f, source, formats[format], value); extendedRead(f, source, profile, format, 6);
    auto srv = extendedSrv(f, source, formats[format]);
    Texture<F> sampled(f, 6, 4, DXGI_FORMAT_R8G8B8A8_UNORM, bind);
    D3D10DDIARG_CREATERENDERTARGETVIEW targetArgs{}; targetArgs.hDrvResource = sampled.handle;
    targetArgs.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; targetArgs.Format = sampled.format; targetArgs.Tex2D.ArraySize = 1;
    Storage targetStorage(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &targetArgs)); D3D10DDI_HRENDERTARGETVIEW target{targetStorage.bytes};
    f.table.pfnCreateRenderTargetView(f.device, &targetArgs, target, {}); CHECK(lastError == S_OK);
    extendedBind(f, target);
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv; f.contextKey->OMGetRenderTargets(1, &rtv, nullptr); CHECK(rtv);
    drawSampled(f.contextKey, srv.Get(), rtv.Get(), 6, 4); extendedRead(f, sampled, profile, format, 7);
    f.table.pfnDestroyRenderTargetView(f.device, target);
    // Same-family casts are a PRESENT contract; different families and
    // typeless views must not become legal because the cache is mutable.
    for (const auto bad : {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM}) {
      D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = source.handle; args.Format = bad;
      args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D.ArraySize = 1;
      Storage out(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &args)); std::memset(out.bytes, 0xa5, 64);
      lastError = S_OK; f.table.pfnCreateRenderTargetView(f.device, &args, {out.bytes}, {}); CHECK(lastError == E_INVALIDARG);
      for (unsigned i = 0; i < 64; ++i) CHECK(static_cast<const uint8_t*>(out.bytes)[i] == 0xa5);
    }
    lastError = S_OK; Texture<F> ordinary(f, 6, 4, formats[format], bind);
    D3D10DDIARG_CREATERENDERTARGETVIEW cast{}; cast.hDrvResource = ordinary.handle; cast.Format = linear;
    cast.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; cast.Tex2D.ArraySize = 1;
    Storage out(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &cast)); lastError = S_OK;
    f.table.pfnCreateRenderTargetView(f.device, &cast, {out.bytes}, {}); CHECK(lastError == E_INVALIDARG); lastError = S_OK;
  }
  // Two disjoint sample masks write black/white into each physical texel.
  // An encoded-domain resolve is 128; a decoded-domain resolve would be188.
  Texture<F> msaa(f, 6, 4, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, bind | D3D10_DDI_BIND_PRESENT, 0, false, 4);
  D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = msaa.handle; args.Format = msaa.format;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Tex2D.ArraySize = 1;
  Storage out(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, &args)); D3D10DDI_HRENDERTARGETVIEW view{out.bytes};
  f.table.pfnCreateRenderTargetView(f.device, &args, view, {}); CHECK(lastError == S_OK);
  extendedBind(f, view);
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv; f.contextKey->OMGetRenderTargets(1, &rtv, nullptr); CHECK(rtv);
  Microsoft::WRL::ComPtr<ID3D11Device> device; f.contextKey->GetDevice(&device);
  for (unsigned white = 0; white < 2; ++white) {
    const uint32_t pixel = white ? 0xffffffff : 0xff000000;
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{&pixel, 4, 4}; Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    CHECK(device->CreateTexture2D(&desc, &initial, &texture) == S_OK);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; CHECK(device->CreateShaderResourceView(texture.Get(), nullptr, &srv) == S_OK);
    drawSampled(f.contextKey, srv.Get(), rtv.Get(), 6, 4, white ? 10 : 5);
  }
  Texture<F> resolved(f, 6, 4, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, bind);
  DXGI_DDI_ARG_BLT request{}; request.hSrcResource = msaa.dxgi(); request.hDstResource = resolved.dxgi();
  request.DstRight = 6; request.DstBottom = 4; request.Rotate = DXGI_DDI_MODE_ROTATION_IDENTITY; request.Flags.Resolve = 1;
  CHECK(f.blt(request) == S_OK); extendedRead(f, resolved, profile, 0, 8);
  f.table.pfnDestroyRenderTargetView(f.device, view); f.contextKey->ClearState();
}
int main() {
  caller = GetCurrentThreadId();
  extendedProfile<D3D10_1DDI_DEVICEFUNCS>(0, D3D11DDI_3DPIPELINELEVEL_10_1);
  extendedProfile<D3D11DDI_DEVICEFUNCS>(1, D3D11DDI_3DPIPELINELEVEL_10_0);
  extendedProfile<D3D11DDI_DEVICEFUNCS>(2, D3D11DDI_3DPIPELINELEVEL_10_1);
  CHECK(extendedImages == 51 && extendedPixels == 1116 && backings.empty() && bridges.empty() && lastError == S_OK);
  std::printf("DXGI extended Blt PASS profiles=3 formats=2 images=51 pixels=1116 hardware_admission=0\n");
}
