// SPDX-License-Identifier: MIT
// Independent public-WARP source writer and literal dataset. Reads only in
// this fixture; the production helper performs the resolve entirely on GPU.
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "../src/umd/umd_blt.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using Microsoft::WRL::ComPtr;
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr,"encoded native failure line=%u expression=%s\n",unsigned(__LINE__),#value); std::abort(); } } while (0)
static unsigned images, pixels;
static uint32_t pattern(unsigned pixel, unsigned sample) {
  return ((pixel + sample * 53) & 255) | (((pixel * 17 + sample * 31) & 255) << 8)
    | (((255 - pixel + sample * 67) & 255) << 16) | (((pixel * 7 + sample * 43) & 255) << 24);
}
static void save(const char* name, const void* data, size_t size) {
  CHECK(GetFileAttributesA(name)==INVALID_FILE_ATTRIBUTES); FILE* file=nullptr;
  CHECK(fopen_s(&file,name,"wb")==0 && file);CHECK(std::fwrite(data,1,size,file)==size);CHECK(std::fclose(file)==0);
}
static ComPtr<ID3D11Texture2D> texture(ID3D11Device* device, DXGI_FORMAT format, UINT samples, UINT bind,
    const D3D11_SUBRESOURCE_DATA* initial=nullptr) {
  D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=16;desc.MipLevels=desc.ArraySize=1;
  desc.Format=format;desc.SampleDesc.Count=samples;desc.BindFlags=bind;
  ComPtr<ID3D11Texture2D> image;CHECK(device->CreateTexture2D(&desc,initial,&image)==S_OK && image);return image;
}
struct Pipeline {
  ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11Buffer> vertices;
  ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11SamplerState> sampler;
  explicit Pipeline(ID3D11Device* device) {
    std::vector<unsigned char> vertex,pixel;CHECK(dxvk::umd::bltShaderContainers(vertex,pixel));
    CHECK(device->CreateVertexShader(vertex.data(),vertex.size(),nullptr,&vs)==S_OK);
    const D3D11_INPUT_ELEMENT_DESC elements[]{
      {dxvk::umd::inputRegisterSemantic,0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
      {dxvk::umd::inputRegisterSemantic,1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
    CHECK(device->CreateInputLayout(elements,2,vertex.data(),vertex.size(),&layout)==S_OK);
    const auto points=dxvk::umd::bltVertices(1);D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth=sizeof(points);buffer.Usage=D3D11_USAGE_IMMUTABLE;buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    const D3D11_SUBRESOURCE_DATA data{points.data(),0,0};CHECK(device->CreateBuffer(&buffer,&data,&vertices)==S_OK);
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
    CHECK(device->CreateRasterizerState(&rs,&raster)==S_OK);
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxAnisotropy=1;
    CHECK(device->CreateSamplerState(&sd,&sampler)==S_OK);
  }
  void draw(ID3D11DeviceContext* context,ID3D11PixelShader* ps,ID3D11ShaderResourceView* source,ID3D11RenderTargetView* target,UINT mask=~0u) {
    context->ClearState();context->IASetInputLayout(layout.Get());ID3D11Buffer* buffers[]{vertices.Get()};const UINT stride=sizeof(dxvk::umd::BltVertex),offset=0;
    context->IASetVertexBuffers(0,1,buffers,&stride,&offset);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps,nullptr,0);context->PSSetShaderResources(0,1,&source);
    ID3D11SamplerState* samplers[]{sampler.Get()};context->PSSetSamplers(0,1,samplers);
    context->OMSetRenderTargets(1,&target,nullptr);context->OMSetBlendState(nullptr,nullptr,mask);context->RSSetState(raster.Get());
    const D3D11_VIEWPORT viewport{0,0,16,16,0,1};context->RSSetViewports(1,&viewport);context->Draw(3,0);context->ClearState();
  }
};
static void snapshot(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* image,unsigned family,unsigned which,DXGI_FORMAT sourceFormat) {
  D3D11_TEXTURE2D_DESC desc{};image->GetDesc(&desc);CHECK(desc.Width==16 && desc.Height==16 && desc.SampleDesc.Count==1);
  desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture2D> staging;CHECK(device->CreateTexture2D(&desc,nullptr,&staging)==S_OK);
  context->CopyResource(staging.Get(),image);D3D11_MAPPED_SUBRESOURCE map{};
  CHECK(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)==S_OK && map.pData && map.RowPitch>=64);
  std::array<uint32_t,256> raw{};for(unsigned y=0;y<16;++y)std::memcpy(raw.data()+y*16,static_cast<const uint8_t*>(map.pData)+size_t(y)*map.RowPitch,64);
  const uint32_t metadata[]{which,16,16,4,unsigned(sourceFormat),unsigned(desc.Format),map.RowPitch,0xa000};context->Unmap(staging.Get(),0);
  char name[100];std::snprintf(name,sizeof(name),"encoded-resolve-native-%u-%u.actual.bin",family,which);save(name,raw.data(),sizeof(raw));
  std::snprintf(name,sizeof(name),"encoded-resolve-native-%u-%u.metadata.bin",family,which);save(name,metadata,sizeof(metadata));
  ++images;pixels+=256;CHECK(device->GetDeviceRemovedReason()==S_OK);
}
int main() {
  const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_10_0;D3D_FEATURE_LEVEL selected{};
  ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
  CHECK(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,&selected,&context)==S_OK && selected==level);
  for(UINT count : {2u,4u,8u,16u,32u}) {
    std::vector<unsigned char> code;CHECK(dxvk::umd::encodedResolveShaderContainer(count,code));
    ComPtr<ID3D11PixelShader> shader;CHECK(device->CreatePixelShader(code.data(),code.size(),nullptr,&shader)==S_OK && shader);
  }
  Pipeline pipeline(device.Get());std::vector<unsigned char> vertex,passCode;CHECK(dxvk::umd::bltShaderContainers(vertex,passCode));
  ComPtr<ID3D11PixelShader> pass;CHECK(device->CreatePixelShader(passCode.data(),passCode.size(),nullptr,&pass)==S_OK);
  for(unsigned family=0;family<2;++family) {
    const DXGI_FORMAT sourceFormat=family ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const DXGI_FORMAT readFormat=family ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    auto source=texture(device.Get(),sourceFormat,4,D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE);
    D3D11_RENDER_TARGET_VIEW_DESC view{};view.Format=sourceFormat;view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DMS;
    ComPtr<ID3D11RenderTargetView> sourceRtv;CHECK(device->CreateRenderTargetView(source.Get(),&view,&sourceRtv)==S_OK);
    for(unsigned sample=0;sample<4;++sample) {
      std::array<uint32_t,256> data{};for(unsigned pixel=0;pixel<256;++pixel) {
        const uint32_t bgra=pattern(pixel,sample);data[pixel]=family ? (bgra&0xff00ff00)|((bgra&255)<<16)|((bgra>>16)&255) : bgra;
      }
      const D3D11_SUBRESOURCE_DATA initial{data.data(),64,1024};
      auto image=texture(device.Get(),DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,1,D3D11_BIND_SHADER_RESOURCE,&initial);
      ComPtr<ID3D11ShaderResourceView> srv;CHECK(device->CreateShaderResourceView(image.Get(),nullptr,&srv)==S_OK);
      pipeline.draw(context.Get(),pass.Get(),srv.Get(),sourceRtv.Get(),1u<<sample);
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=sourceFormat;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DMS;
    ComPtr<ID3D11ShaderResourceView> sourceSrv;CHECK(device->CreateShaderResourceView(source.Get(),&sd,&sourceSrv)==S_OK);
    // Four literal source controls read each physical sample separately, using
    // the same reconstruction program but independent fixed writer byte data.
    for(unsigned sample=0;sample<4;++sample) {
      std::vector<uint32_t> tokens;CHECK(dxvk::umd::encodedResolveTokens(4,tokens));
      for(size_t offset=2;offset<tokens.size();offset+=(tokens[offset]>>24)&127)
        if((tokens[offset]&0x7ff)==46)tokens[offset+8]=sample;
      constexpr dxvk::umd::ShaderSignatureEntry position{1,0,3,dxvk::umd::ShaderScalar::Float32},color{0,0,15,dxvk::umd::ShaderScalar::Float32};
      std::vector<unsigned char> code;CHECK(dxvk::umd::buildShaderContainer(dxvk::umd::ShaderStage::Pixel,tokens.data(),tokens.size(),&position,1,&color,1,code));
      ComPtr<ID3D11PixelShader> shader;CHECK(device->CreatePixelShader(code.data(),code.size(),nullptr,&shader)==S_OK);
      auto output=texture(device.Get(),readFormat,1,D3D11_BIND_RENDER_TARGET);
      ComPtr<ID3D11RenderTargetView> rtv;CHECK(device->CreateRenderTargetView(output.Get(),nullptr,&rtv)==S_OK);
      pipeline.draw(context.Get(),shader.Get(),sourceSrv.Get(),rtv.Get());snapshot(device.Get(),context.Get(),output.Get(),family,sample,sourceFormat);
    }
    auto target=texture(device.Get(),sourceFormat,1,D3D11_BIND_RENDER_TARGET);
    D3D11_TEXTURE2D_DESC src{},dst{};source->GetDesc(&src);target->GetDesc(&dst);dxvk::umd::BltPlan plan;
    CHECK(dxvk::umd::bltPlan(src,dst,0,0,0,0,16,16,1,1,plan)==S_OK);
    CHECK(dxvk::umd::bltTexture2D(device.Get(),context.Get(),source.Get(),target.Get(),0,0,0,0,1,plan,[]{return true;})==S_OK);
    snapshot(device.Get(),context.Get(),target.Get(),family,4,sourceFormat);
    ComPtr<ID3D11RenderTargetView> targetRtv;CHECK(device->CreateRenderTargetView(target.Get(),nullptr,&targetRtv)==S_OK);
    const FLOAT sentinel[]{1,0,1,1};context->ClearRenderTargetView(targetRtv.Get(),sentinel);
    ID3D11RenderTargetView* bound[]{targetRtv.Get()};context->OMSetRenderTargets(1,bound,nullptr);
    auto noSrv=texture(device.Get(),sourceFormat,4,D3D11_BIND_RENDER_TARGET);
    CHECK(dxvk::umd::bltTexture2D(device.Get(),context.Get(),noSrv.Get(),target.Get(),0,0,0,0,1,plan,[]{return true;})==DXGI_ERROR_UNSUPPORTED);
    CHECK(dxvk::umd::bltTexture2D(device.Get(),context.Get(),source.Get(),target.Get(),1,0,0,0,1,plan,[]{return true;})==DXGI_ERROR_UNSUPPORTED);
    auto bad=plan;bad.sourceWidth=15;
    CHECK(dxvk::umd::bltTexture2D(device.Get(),context.Get(),source.Get(),target.Get(),0,0,0,0,1,bad,[]{return true;})==DXGI_ERROR_UNSUPPORTED);
    ComPtr<ID3D11RenderTargetView> after;context->OMGetRenderTargets(1,&after,nullptr);CHECK(after.Get()==targetRtv.Get());
    context->ClearState();snapshot(device.Get(),context.Get(),target.Get(),family,5,sourceFormat);
  }
  CHECK(images==12 && pixels==3072);std::printf("Encoded resolve native PASS profiles=1 families=2 shaders=5 samples=4 images=12 pixels=3072 rejections=6 hardware_admission=0\n");
}
