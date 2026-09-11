#include "../src/umd/umd_view.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using Microsoft::WRL::ComPtr;
using namespace dxvk::umd;
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { \
  std::fprintf(stderr, "view check %u line %d: %s\n", checks, __LINE__, #c); \
  std::exit(1); } } while (0)

static void pixels(ID3D11Device* device, ID3D11DeviceContext* context,
    ID3D11Texture2D* texture, UINT index, UINT color) {
  D3D11_TEXTURE2D_DESC desc = {}; texture->GetDesc(&desc);
  desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = desc.MiscFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture2D> staging;
  CHECK(device->CreateTexture2D(&desc, nullptr, &staging) == S_OK);
  context->CopyResource(staging.Get(), texture);
  D3D11_MAPPED_SUBRESOURCE map = {};
  CHECK(context->Map(staging.Get(), index, D3D11_MAP_READ, 0, &map) == S_OK && map.pData);
  const UINT width = std::max(1u, desc.Width >> (index % desc.MipLevels));
  const UINT height = std::max(1u, desc.Height >> (index % desc.MipLevels));
  for (UINT y = 0; y < height; y++) {
    const auto row = reinterpret_cast<const UINT*>(static_cast<const char*>(map.pData) + y * map.RowPitch);
    for (UINT x = 0; x < width; x++) CHECK(row[x] == color);
  }
  context->Unmap(staging.Get(), index);
}

int main() {
  D3D11_TEXTURE2D_DESC resource = {};
  resource.Width = resource.Height = 8;
  resource.MipLevels = 2; resource.ArraySize = 2;
  resource.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS; resource.SampleDesc.Count = 1;
  resource.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  D3D10DDIARG_CREATESHADERRESOURCEVIEW native = {};
  native.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  native.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  native.Tex2D.MostDetailedMip = 1; native.Tex2D.MipLevels = 1;
  native.Tex2D.FirstArraySlice = 1; native.Tex2D.ArraySize = 1;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
  CHECK(textureShaderView(native, resource, srv));
  CHECK(srv.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
  auto bad = native; bad.Tex2D.ArraySize = UINT(-1);
  CHECK(!textureShaderView(bad, resource, srv));
  bad = native; bad.Tex2D.FirstArraySlice = UINT(-1);
  CHECK(!textureShaderView(bad, resource, srv));
  bad = native; bad.Tex2D.MipLevels = 2;
  CHECK(!textureShaderView(bad, resource, srv));
  bad = native; bad.Tex2D.MipLevels = 0;
  CHECK(!textureShaderView(bad, resource, srv));
  bad = native; bad.Tex2D.MostDetailedMip = UINT(-1);
  CHECK(!textureShaderView(bad, resource, srv));
  auto unavailable = resource; unavailable.BindFlags = D3D11_BIND_RENDER_TARGET;
  CHECK(!textureShaderView(native, unavailable, srv));
  CHECK(textureShaderView(native, resource, srv));

  D3D10DDIARG_CREATERENDERTARGETVIEW nativeTarget = {};
  nativeTarget.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  nativeTarget.Format = native.Format;
  nativeTarget.Tex2D.MipSlice = 1; nativeTarget.Tex2D.FirstArraySlice = 1;
  nativeTarget.Tex2D.ArraySize = 1;
  D3D11_RENDER_TARGET_VIEW_DESC rtv = {};
  CHECK(textureTargetView(nativeTarget, resource, rtv));
  auto badTarget = nativeTarget; badTarget.Tex2D.MipSlice = 2;
  CHECK(!textureTargetView(badTarget, resource, rtv));
  badTarget = nativeTarget; badTarget.Tex2D.ArraySize = UINT(-1);
  CHECK(!textureTargetView(badTarget, resource, rtv));
  CHECK(textureTargetView(nativeTarget, resource, rtv));
  CHECK(nativeFormatCaps(D3D11_FORMAT_SUPPORT_BLENDABLE) == 0);
  CHECK(nativeFormatCaps(D3D11_FORMAT_SUPPORT_BUFFER | D3D11_FORMAT_SUPPORT_DISPLAY) == 0);
  CHECK(nativeFormatCaps(D3D11_FORMAT_SUPPORT_SHADER_SAMPLE | D3D11_FORMAT_SUPPORT_RENDER_TARGET
    | D3D11_FORMAT_SUPPORT_BLENDABLE | D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
    | D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD) == 31);

  // Independent Microsoft implementation executes the translated native
  // subresource descriptions. This is CPU WARP proof, never target GPU proof.
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
    D3D11_SDK_VERSION, &device, nullptr, &context) == S_OK);
  std::array<UINT, 64> zeros = {};
  D3D11_SUBRESOURCE_DATA data[4] = {{zeros.data(),32,256}, {zeros.data(),16,64},
    {zeros.data(),32,256}, {zeros.data(),16,64}};
  ComPtr<ID3D11Texture2D> texture;
  CHECK(device->CreateTexture2D(&resource, data, &texture) == S_OK);
  ComPtr<ID3D11ShaderResourceView> shaderView;
  ComPtr<ID3D11RenderTargetView> targetView;
  CHECK(device->CreateShaderResourceView(texture.Get(), &srv, &shaderView) == S_OK);
  CHECK(device->CreateRenderTargetView(texture.Get(), &rtv, &targetView) == S_OK);
  const FLOAT green[4] = {0,1,0,1};
  context->ClearRenderTargetView(targetView.Get(), green);
  for (UINT i = 0; i < 4; i++) pixels(device.Get(), context.Get(), texture.Get(), i, i == 3 ? 0xff00ff00 : 0);

  // Read relative slice0/mip0 through a view selecting absolute slice1/mip1.
  const char* vertex = "float4 main(uint i:SV_VertexID):SV_Position { return float4((i==1)?3:-1,(i==2)?-3:1,0,1); }";
  const char* pixel = "Texture2DArray<float4> t:register(t0); float4 main(float4 p:SV_Position):SV_Target { return t.Load(int4(int2(p.xy),0,0)); }";
  ComPtr<ID3DBlob> vsCode, psCode, errors;
  CHECK(D3DCompile(vertex, std::strlen(vertex), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsCode, &errors) == S_OK);
  CHECK(D3DCompile(pixel, std::strlen(pixel), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psCode, &errors) == S_OK);
  ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
  CHECK(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &vs) == S_OK);
  CHECK(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &ps) == S_OK);
  auto outputDesc = resource; outputDesc.Width = outputDesc.Height = 4;
  outputDesc.ArraySize = outputDesc.MipLevels = 1; outputDesc.Format = native.Format;
  ComPtr<ID3D11Texture2D> output;
  CHECK(device->CreateTexture2D(&outputDesc, nullptr, &output) == S_OK);
  ComPtr<ID3D11RenderTargetView> outputView;
  CHECK(device->CreateRenderTargetView(output.Get(), nullptr, &outputView) == S_OK);
  auto target = outputView.Get(); auto sampled = shaderView.Get();
  context->OMSetRenderTargets(1, &target, nullptr);
  context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
  context->PSSetShaderResources(0, 1, &sampled);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D11_VIEWPORT viewport = {0,0,4,4,0,1}; context->RSSetViewports(1, &viewport);
  context->Draw(3, 0);
  pixels(device.Get(), context.Get(), output.Get(), 0, 0xff00ff00);
  context->ClearState();

  auto multisample = outputDesc; multisample.SampleDesc.Count = 4; multisample.ArraySize = 2;
  UINT levels = 0;
  CHECK(device->CheckMultisampleQualityLevels(multisample.Format, 4, &levels) == S_OK && levels);
  ComPtr<ID3D11Texture2D> msTexture;
  CHECK(device->CreateTexture2D(&multisample, nullptr, &msTexture) == S_OK);
  nativeTarget.Tex2D.MipSlice = 0;
  CHECK(textureTargetView(nativeTarget, multisample, rtv));
  CHECK(rtv.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY);
  targetView.Reset();
  CHECK(device->CreateRenderTargetView(msTexture.Get(), &rtv, &targetView) == S_OK);
  context->ClearRenderTargetView(targetView.Get(), green);
  native.Tex2D.MostDetailedMip = 0;
  CHECK(textureShaderView(native, multisample, srv));
  CHECK(srv.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY);
  shaderView.Reset();
  CHECK(device->CreateShaderResourceView(msTexture.Get(), &srv, &shaderView) == S_OK);
  native.Tex2D.MostDetailedMip = 1;
  CHECK(!textureShaderView(native, multisample, srv));
  nativeTarget.Tex2D.MipSlice = 1;
  CHECK(!textureTargetView(nativeTarget, multisample, rtv));
  CHECK(resolveSubresources(outputDesc, 0, multisample, 1, outputDesc.Format));
  CHECK(!resolveSubresources(outputDesc, 1, multisample, 1, outputDesc.Format));
  CHECK(!resolveSubresources(outputDesc, 0, multisample, 2, outputDesc.Format));
  CHECK(!resolveSubresources(outputDesc, 0, multisample, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB));
  auto badResolve = outputDesc; badResolve.Width = 8;
  CHECK(!resolveSubresources(badResolve, 0, multisample, 1, outputDesc.Format));
  badResolve = outputDesc; badResolve.Usage = D3D11_USAGE_STAGING;
  CHECK(!resolveSubresources(badResolve, 0, multisample, 1, outputDesc.Format));
  context->ResolveSubresource(output.Get(), 0, msTexture.Get(), 1, outputDesc.Format);
  pixels(device.Get(), context.Get(), output.Get(), 0, 0xff00ff00);
  std::printf("native texture view/range/MSAA PASS checks=%u; independent CPU WARP\n", checks);
}
