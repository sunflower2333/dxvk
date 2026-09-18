#include "../src/umd/umd_view.h"
#include "../src/umd/umd_state.h"
#include <d3dcompiler.h>
#include <d3d11sdklayers.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace dxvk::umd;
static unsigned checks;
#define CHECK(c) do { checks++; if (!(c)) { \
  std::fprintf(stderr, "view check %u line %d: %s\n", checks, __LINE__, #c); \
  std::exit(1); } } while (0)

static void debugMessages(ID3D11InfoQueue* queue) {
  if (!queue) { std::puts("WARP_DEBUG_LAYER unavailable"); return; }
  const UINT64 count = queue->GetNumStoredMessagesAllowedByRetrievalFilter();
  std::printf("WARP_DEBUG_LAYER messages=%llu\n", static_cast<unsigned long long>(count));
  for (UINT64 i = 0; i < std::min<UINT64>(count, 16); i++) {
    SIZE_T size = 0;
    if (FAILED(queue->GetMessage(i, nullptr, &size))) continue;
    std::vector<unsigned char> storage(size);
    auto message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
    if (SUCCEEDED(queue->GetMessage(i, message, &size)))
      std::printf("WARP_DEBUG severity=%u id=%u %s\n", message->Severity, message->ID,
        message->pDescription ? message->pDescription : "");
  }
}

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
    for (UINT x = 0; x < width; x++) {
      if (row[x] != color)
        std::fprintf(stderr, "WARP_VIEW_PIXEL subresource=%u size=%ux%u x=%u y=%u expected=%08x actual=%08x\n",
          index, width, height, x, y, color, row[x]);
      CHECK(row[x] == color);
    }
  }
  context->Unmap(staging.Get(), index);
}

int main() {
  D3D11_TEXTURE2D_DESC resource = {};
  resource.Width = resource.Height = 8;
  resource.MipLevels = 2; resource.ArraySize = 2;
  resource.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS; resource.SampleDesc.Count = 1;
  resource.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  D3D10DDIARG_CREATERESOURCE mipResource = {};
  mipResource.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  mipResource.Usage = D3D10_DDI_USAGE_DEFAULT;
  mipResource.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
  mipResource.SampleDesc.Count = 1;
  mipResource.MiscFlags = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP;
  UINT mipFlags = 0;
  CHECK(textureMiscFlags(mipResource, mipFlags) && mipFlags == D3D11_RESOURCE_MISC_GENERATE_MIPS);
  const UINT autoMipFlags = mipFlags;
  auto invalidMip = mipResource; invalidMip.ResourceDimension = D3D10DDIRESOURCE_BUFFER;
  CHECK(!textureMiscFlags(invalidMip, mipFlags) && mipFlags == 0);
  invalidMip = mipResource; invalidMip.BindFlags = D3D10_DDI_BIND_SHADER_RESOURCE;
  CHECK(!textureMiscFlags(invalidMip, mipFlags));
  invalidMip = mipResource; invalidMip.SampleDesc.Count = 4;
  CHECK(!textureMiscFlags(invalidMip, mipFlags));
  invalidMip = mipResource; invalidMip.MapFlags = D3D10_DDI_CPU_ACCESS_WRITE;
  CHECK(!textureMiscFlags(invalidMip, mipFlags));
  invalidMip = mipResource; invalidMip.MiscFlags |= D3D10_DDI_RESOURCE_MISC_SHARED;
  CHECK(!textureMiscFlags(invalidMip, mipFlags));
  // Shared surfaces: one linear image carried by a kernel allocation, never a
  // D3D11 misc flag on the cache, so the reported flags stay empty.
  bool sharedFlag = true;
  auto sharedResource = mipResource;
  sharedResource.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED;
  CHECK(textureMiscFlags(sharedResource, mipFlags, &sharedFlag));
  CHECK(sharedFlag && mipFlags == 0);
  // A caller that passes no out-parameter cannot publish the surface, so it
  // must be refused rather than handed an ordinary unshared resource.
  CHECK(!textureMiscFlags(sharedResource, mipFlags));
  // Generated mips have nowhere to live in a single-image wire format.
  sharedResource.MiscFlags |= D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP;
  CHECK(!textureMiscFlags(sharedResource, mipFlags, &sharedFlag) && !sharedFlag);
  // An unknown misc flag stays rejected alongside a known one.
  sharedResource.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED | 0x40;
  CHECK(!textureMiscFlags(sharedResource, mipFlags, &sharedFlag) && !sharedFlag);
  D3D10DDIARG_CREATESHADERRESOURCEVIEW native = {};
  native.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  native.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  native.Tex2D.MostDetailedMip = 1; native.Tex2D.MipLevels = 1;
  native.Tex2D.FirstArraySlice = 1; native.Tex2D.ArraySize = 1;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
  CHECK(textureShaderView(native, resource, srv));
  CHECK(srv.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY);
  auto automaticMip = resource; automaticMip.MiscFlags = autoMipFlags;
  CHECK(mipGenerationStatus(automaticMip, srv) == S_OK);
  auto missingMipFlag = resource; missingMipFlag.MiscFlags = 0;
  CHECK(mipGenerationStatus(missingMipFlag, srv) == E_FAIL);
  auto invalidMipView = srv; invalidMipView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
  CHECK(mipGenerationStatus(automaticMip, invalidMipView) == E_INVALIDARG);
  invalidMipView = srv; invalidMipView.Texture2DArray.ArraySize = UINT(-1);
  CHECK(mipGenerationStatus(automaticMip, invalidMipView) == E_INVALIDARG);
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
  HRESULT created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
    D3D11_CREATE_DEVICE_DEBUG, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
  if (created == DXGI_ERROR_SDK_COMPONENT_MISSING)
    created = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
      nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
  CHECK(created == S_OK);
  ComPtr<ID3D11InfoQueue> debugQueue;
  device.As(&debugQueue);
  context->ClearState();
  // Validate native reset/replacement semantics against an independent
  // runtime implementation, including a hole that must retain its index.
  const FLOAT nan = std::numeric_limits<FLOAT>::quiet_NaN();
  const D3D10_DDI_VIEWPORT nativeViews[] = {
    {0, 0, 16, 16, 0, 1}, {nan, nan, nan, nan, nan, nan}, {8, 4, 4, 8, 0, 1}};
  auto applyViews = [&](UINT size, const D3D11_VIEWPORT* views) {
    context->RSSetViewports(size, views);
  };
  CHECK(replaceViewports(3, 0, nativeViews, applyViews));
  UINT viewCount = 16;
  D3D11_VIEWPORT actualViews[16] = {};
  context->RSGetViewports(&viewCount, actualViews);
  CHECK(viewCount == 3);
  CHECK(actualViews[0].Width == 16 && actualViews[1].Width == 0 && actualViews[1].Height == 0);
  CHECK(actualViews[2].TopLeftX == 8 && actualViews[2].Width == 4);
  CHECK(!replaceViewports(3, UINT(-1), nativeViews, applyViews));
  CHECK(!replaceViewports(17, 0, nativeViews, applyViews));
  CHECK(!replaceViewports(1, 0, nullptr, applyViews));
  viewCount = 16; context->RSGetViewports(&viewCount, actualViews);
  CHECK(viewCount == 3 && actualViews[2].TopLeftX == 8);
  CHECK(replaceViewports(1, 2, nativeViews, applyViews));
  viewCount = 16; context->RSGetViewports(&viewCount, actualViews);
  CHECK(viewCount == 1 && actualViews[0].Width == 16);
  CHECK(replaceViewports(0, 0, nullptr, applyViews));
  viewCount = 16; context->RSGetViewports(&viewCount, actualViews);
  CHECK(viewCount == 0);
  const D3D10_DDI_PRIMITIVE_TOPOLOGY nativeTopologies[] = {
    D3D10_DDI_PRIMITIVE_TOPOLOGY_UNDEFINED, D3D10_DDI_PRIMITIVE_TOPOLOGY_POINTLIST,
    D3D10_DDI_PRIMITIVE_TOPOLOGY_LINELIST, D3D10_DDI_PRIMITIVE_TOPOLOGY_LINESTRIP,
    D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
    D3D10_DDI_PRIMITIVE_TOPOLOGY_LINELIST_ADJ, D3D10_DDI_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
    D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ, D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ};
  for (auto nativeTopology : nativeTopologies) {
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    CHECK(primitiveTopology(nativeTopology, topology));
    context->IASetPrimitiveTopology(topology);
    D3D11_PRIMITIVE_TOPOLOGY actual;
    context->IAGetPrimitiveTopology(&actual);
    CHECK(actual == topology);
  }
  D3D11_PRIMITIVE_TOPOLOGY unchanged = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
  CHECK(!primitiveTopology(static_cast<D3D10_DDI_PRIMITIVE_TOPOLOGY>(UINT(-1)), unchanged));
  CHECK(unchanged == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
  CHECK(!primitiveTopology(D3D11_DDI_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST, unchanged));
  context->ClearState();
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

  // Keep the mip oracle separate from the typeless clear/sampling case.
  // Request automatic full-chain allocation as documented by CreateTexture2D,
  // then generate only a strict SRV subrange and verify every untouched mip.
  auto generationResource = resource;
  generationResource.Format = native.Format;
  generationResource.MiscFlags = autoMipFlags;
  generationResource.MipLevels = 0;
  UINT generationSupport = 0;
  CHECK(device->CheckFormatSupport(generationResource.Format, &generationSupport) == S_OK);
  CHECK(generationSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN);
  ComPtr<ID3D11Texture2D> generatedTexture;
  D3D11_SUBRESOURCE_DATA fullData[8] = {};
  for (UINT i = 0; i < 8; i++) {
    const UINT width = 8u >> (i % 4);
    fullData[i] = {zeros.data(), width * 4, width * width * 4};
  }
  CHECK(device->CreateTexture2D(&generationResource, fullData, &generatedTexture) == S_OK);
  generatedTexture->GetDesc(&generationResource);
  CHECK(generationResource.MipLevels == 4 && generationResource.ArraySize == 2);
  auto oldMipTarget = nativeTarget; oldMipTarget.Tex2D.FirstArraySlice = 0;
  D3D11_RENDER_TARGET_VIEW_DESC oldMipDesc = {};
  CHECK(textureTargetView(oldMipTarget, generationResource, oldMipDesc));
  ComPtr<ID3D11RenderTargetView> oldMipView;
  CHECK(device->CreateRenderTargetView(generatedTexture.Get(), &oldMipDesc, &oldMipView) == S_OK);
  context->ClearRenderTargetView(oldMipView.Get(), green);
  // WARP on these runners ignores GenerateMips for a nonzero first slice,
  // even in independent API-only controls (CI 34617903693). Use slice0 for
  // this CPU oracle; nonzero-slice DXVK generation still needs GPU proof.
  // Slice1 and out-of-view mips must remain zero, while the old green mip1
  // must become the selected base mip's red value, checked pixel for pixel.
  auto baseTarget = oldMipTarget; baseTarget.Tex2D.MipSlice = 0;
  D3D11_RENDER_TARGET_VIEW_DESC baseDesc = {};
  CHECK(textureTargetView(baseTarget, generationResource, baseDesc));
  ComPtr<ID3D11RenderTargetView> baseView;
  CHECK(device->CreateRenderTargetView(generatedTexture.Get(), &baseDesc, &baseView) == S_OK);
  const FLOAT red[4] = {1, 0, 0, 1};
  context->ClearRenderTargetView(baseView.Get(), red);
  auto generation = native; generation.Tex2D.MostDetailedMip = 0; generation.Tex2D.MipLevels = 2;
  generation.Tex2D.FirstArraySlice = 0;
  D3D11_SHADER_RESOURCE_VIEW_DESC generationDesc = {};
  CHECK(textureShaderView(generation, generationResource, generationDesc));
  CHECK(mipGenerationStatus(generationResource, generationDesc) == S_OK);
  ComPtr<ID3D11ShaderResourceView> generationView;
  CHECK(device->CreateShaderResourceView(generatedTexture.Get(), &generationDesc, &generationView) == S_OK);
  D3D11_TEXTURE2D_DESC actualResource = {}; generatedTexture->GetDesc(&actualResource);
  D3D11_SHADER_RESOURCE_VIEW_DESC actualGenerationView = {}; generationView->GetDesc(&actualGenerationView);
  std::printf("WARP_MIP_INPUT format=%u misc=%u support=%x first_mip=%u mips=%u first_slice=%u slices=%u\n",
    actualResource.Format, actualResource.MiscFlags, generationSupport,
    actualGenerationView.Texture2DArray.MostDetailedMip, actualGenerationView.Texture2DArray.MipLevels,
    actualGenerationView.Texture2DArray.FirstArraySlice, actualGenerationView.Texture2DArray.ArraySize);
  debugMessages(debugQueue.Get());
  if (debugQueue) debugQueue->ClearStoredMessages();
  context->GenerateMips(generationView.Get());
  debugMessages(debugQueue.Get());
  std::puts("WARP_VIEW_STAGE generated-mip-array");
  for (UINT i = 0; i < 8; i++) pixels(device.Get(), context.Get(), generatedTexture.Get(), i,
    i == 0 || i == 1 ? 0xff0000ff : 0);

  // Read relative slice0/mip0 through a view selecting absolute slice1/mip1.
  const char* vertex = "float4 main(uint i:SV_VertexID):SV_Position { return float4((i==1)?3:-1,(i==2)?-3:1,0,1); }";
  const char* pixel = "Texture2DArray<float4> t:register(t0); float4 main(float4 p:SV_Position):SV_Target { return t.Load(int4(int2(p.xy),0,0)); }";
  ComPtr<ID3DBlob> vsCode, psCode, errors;
  CHECK(D3DCompile(vertex, std::strlen(vertex), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsCode, &errors) == S_OK);
  CHECK(D3DCompile(pixel, std::strlen(pixel), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psCode, &errors) == S_OK);
  ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
  CHECK(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &vs) == S_OK);
  CHECK(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &ps) == S_OK);
  auto outputDesc = resource; outputDesc.MiscFlags = 0; outputDesc.Width = outputDesc.Height = 4;
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
  std::puts("WARP_VIEW_STAGE sample-generated-resource");
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
