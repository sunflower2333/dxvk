// SPDX-License-Identifier: MIT
// Execute production view-shape/alias helpers against real WARP-created views.
#include "../src/umd/umd_output_merger.h"
#include <cstdio>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
using namespace dxvk::umd;
static unsigned checks;
// Abort at a stable assertion rather than count a skipped device as a success.
static void check(bool value, unsigned line) {
  checks++;
  if (!value) { std::fprintf(stderr,"FAIL output views line=%u\n",line); std::exit(1); }
}
#define CHECK(x) check(!!(x),__LINE__)
// Validate actual array, mip, MSAA and depth metadata without opening VIOGPU.
int main() {
  ComPtr<ID3D11Device> device;
  CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
    nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)));
  D3D11_TEXTURE2D_DESC desc{};
  desc.Width=desc.Height=32; desc.ArraySize=4; desc.MipLevels=2;
  desc.SampleDesc.Count=1; desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BindFlags=D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> texture;
  CHECK(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)));
  // Distinct view objects can still address overlapping subresources.
  auto view = [&](UINT mip,UINT first,UINT layers) {
    ComPtr<ID3D11RenderTargetView> result;
    D3D11_RENDER_TARGET_VIEW_DESC r{};
    r.Format=desc.Format; r.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    r.Texture2DArray.MipSlice=mip; r.Texture2DArray.FirstArraySlice=first; r.Texture2DArray.ArraySize=layers;
    CHECK(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),&r,&result)));
    return result;
  };
  auto a=view(1,0,2), b=view(1,2,2), c=view(1,1,2), d=view(0,0,2), duplicate=view(1,0,2);
  OutputView va,vb,vc,vd,ve;
  CHECK(outputView(a.Get(),va)); CHECK(outputView(b.Get(),vb)); CHECK(outputView(c.Get(),vc));
  CHECK(outputView(d.Get(),vd)); CHECK(outputView(duplicate.Get(),ve));
  CHECK(va.shape.width==16 && va.shape.height==16 && va.shape.layers==2);
  CHECK(!overlappingOutputs(va,vb)); CHECK(overlappingOutputs(va,vc));
  CHECK(overlappingOutputs(va,ve)); CHECK(!overlappingOutputs(va,vd));
  auto shape=va.shape;
  CHECK(mergeOutputShape(shape,vb.shape)); CHECK(!mergeOutputShape(shape,vd.shape));
  CHECK(!outputView(nullptr,va));
  OutputShape sentinel{7,7,1,1,0};
  CHECK(!outputShape(desc,32,0,1,sentinel)); CHECK(sentinel.width==7);
  CHECK(!outputShape(desc,0,4,1,sentinel)); CHECK(sentinel.width==7);
  CHECK(!outputShape(desc,0,1,UINT32_MAX,sentinel)); CHECK(sentinel.width==7);
  desc.Width=desc.Height=16; desc.MipLevels=1; desc.ArraySize=2;
  desc.BindFlags=D3D11_BIND_DEPTH_STENCIL; desc.Format=DXGI_FORMAT_D32_FLOAT;
  ComPtr<ID3D11Texture2D> depth;
  CHECK(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&depth)));
  ComPtr<ID3D11DepthStencilView> dsv;
  CHECK(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)));
  OutputShape depthShape;
  CHECK(depthOutputShape(dsv.Get(),depthShape)); CHECK(mergeOutputShape(shape,depthShape));
  CHECK(!depthOutputShape(nullptr,depthShape));
  desc.ArraySize=1; desc.SampleDesc.Count=4;
  desc.BindFlags=D3D11_BIND_RENDER_TARGET; desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
  UINT quality=0; CHECK(SUCCEEDED(device->CheckMultisampleQualityLevels(desc.Format,4,&quality)) && quality);
  ComPtr<ID3D11Texture2D> ms;
  ComPtr<ID3D11RenderTargetView> msview;
  CHECK(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&ms)));
  CHECK(SUCCEEDED(device->CreateRenderTargetView(ms.Get(),nullptr,&msview)));
  OutputView vm; CHECK(outputView(msview.Get(),vm)); CHECK(vm.shape.samples==4 && vm.shape.layers==1);
  OutputShape single{16,16,1,1,0}; CHECK(!mergeOutputShape(single,vm.shape));
  std::printf("PASS output views: %u checks; actual WARP view metadata, no VIOGPU\n",checks);
}
