// SPDX-License-Identifier: MIT
// Source-linked production volume DDIs against the Microsoft WARP reference.
#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_api.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <fstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, cases, voxels, sampled;
static DWORD caller;
static HRESULT lastError = S_OK;
static ComPtr<ID3D11Device> createdDevice;
static ComPtr<ID3D11DeviceContext> createdContext;
struct FixtureFailure {};
static void check(bool value, unsigned line) {
  ++checks;
  if (!value) { std::fprintf(stderr,"FAIL Texture3D line=%u error=%08lx\n",line,
    static_cast<unsigned long>(lastError)); throw FixtureFailure{}; }
}
#define CHECK(x) check(!!(x), __LINE__)
static void ok() { CHECK(lastError == S_OK); }
static void rejected() { CHECK(FAILED(lastError)); lastError = S_OK; }
static void APIENTRY reportError(D3D10DDI_HRTCORELAYER, HRESULT result) {
  CHECK(GetCurrentThreadId() == caller); lastError = result;
}
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context,
    const dxvk::umd::RuntimeBackend*) noexcept {
  level = dxvk::umd::implementationFeatureLevel(level);
  const HRESULT hr = D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
    &level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if (hr == S_OK) { createdDevice=*device; createdContext=*context; }
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept {
  return E_NOTIMPL;
}
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  context->Flush(); return S_OK;
}
struct Storage {
  SIZE_T size;
  std::unique_ptr<void,decltype(&std::free)> bytes;
  explicit Storage(SIZE_T n) : size(n), bytes(std::calloc(1,n),&std::free) { CHECK(n && bytes); }
  template<typename T> T handle() const { return {bytes.get()}; }
  void poison() { std::memset(bytes.get(),0xcd,size); }
  bool untouched() const {
    const auto p=static_cast<const unsigned char*>(bytes.get());
    return std::all_of(p,p+size,[](unsigned char v){return v==0xcd;});
  }
};
struct Fixture {
  UINT profile;
  UINT readbacks=0;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device=storage.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_DEVICEFUNCS f0{};
  D3D10_1DDI_DEVICEFUNCS f1{};
  D3D11DDI_DEVICEFUNCS f11{};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  ComPtr<ID3D11Device> backend;
  ComPtr<ID3D11DeviceContext> context;
  explicit Fixture(UINT p) : profile(p) {
    LUID luid{}; core10.pfnSetErrorCb=reportError; core11.pfnSetErrorCb=reportError;
    HRESULT hr=E_FAIL;
    if (!profile) hr=VioGpuDxvkCreateDdiTestDevice(&luid,device,{},&core10,&f0);
    else if (profile==1) hr=VioGpuDxvkCreateDdiTestDevice10_1(&luid,device,{},&core10,&f1);
    else hr=VioGpuDxvkCreateDdiTestDevice11(&luid,device,{},&core11,&f11,D3D_FEATURE_LEVEL_11_0);
    CHECK(hr==S_OK); backend=createdDevice; context=createdContext;
    createdDevice.Reset(); createdContext.Reset(); CHECK(backend && context);
  }
  // Every invocation selects a real typed table; there is no table cast.
  template<typename Fn> decltype(auto) call(Fn&& fn) {
    if (!profile) return fn(f0);
    if (profile==1) return fn(f1);
    return fn(f11);
  }
  void create(const D3D10DDIARG_CREATERESOURCE& a,D3D10DDI_HRESOURCE out) {
    if (!profile) f0.pfnCreateResource(device,&a,out,{});
    else if (profile==1) f1.pfnCreateResource(device,&a,out,{});
    else {
      D3D11DDIARG_CREATERESOURCE n{};
      n.pMipInfoList=a.pMipInfoList; n.pInitialDataUP=a.pInitialDataUP;
      n.ResourceDimension=a.ResourceDimension; n.Usage=a.Usage; n.BindFlags=a.BindFlags;
      n.MapFlags=a.MapFlags; n.MiscFlags=a.MiscFlags; n.Format=a.Format;
      n.SampleDesc=a.SampleDesc; n.MipLevels=a.MipLevels; n.ArraySize=a.ArraySize;
      n.pPrimaryDesc=a.pPrimaryDesc; f11.pfnCreateResource(device,&n,out,{});
    }
  }
  void shaderView(const D3D10DDIARG_CREATESHADERRESOURCEVIEW& a,D3D10DDI_HSHADERRESOURCEVIEW out) {
    if (!profile) f0.pfnCreateShaderResourceView(device,&a,out,{});
    else if (profile==1) {
      D3D10_1DDIARG_CREATESHADERRESOURCEVIEW n{}; n.hDrvResource=a.hDrvResource;
      n.Format=a.Format; n.ResourceDimension=a.ResourceDimension; n.Tex3D=a.Tex3D;
      f1.pfnCreateShaderResourceView(device,&n,out,{});
    } else {
      D3D11DDIARG_CREATESHADERRESOURCEVIEW n{}; n.hDrvResource=a.hDrvResource;
      n.Format=a.Format; n.ResourceDimension=a.ResourceDimension; n.Tex3D=a.Tex3D;
      f11.pfnCreateShaderResourceView(device,&n,out,{});
    }
  }
  void targets(const D3D10DDI_HRENDERTARGETVIEW* views,UINT count) {
    if (!profile) f0.pfnSetRenderTargets(device,views,count,0,{});
    else if (profile==1) f1.pfnSetRenderTargets(device,views,count,0,{});
    else f11.pfnSetRenderTargets(device,views,count,0,{},nullptr,nullptr,count,0,count,0);
  }
  ~Fixture() { context->ClearState(); call([&](auto& f){f.pfnDestroyDevice(device);}); ok(); }
};
static void saveShader(UINT profile,const char* name,const void* data,size_t bytes);
struct Description {
  std::vector<D3D10DDI_MIPINFO> mips;
  D3D10DDIARG_CREATERESOURCE args{};
  Description(UINT w,UINT h,UINT d,UINT levels) : mips(levels) {
    for (UINT mip=0;mip<levels;++mip) {
      const UINT x=std::max(1u,w>>mip),y=std::max(1u,h>>mip),z=std::max(1u,d>>mip);
      mips[mip]={x,y,z,(x+15u)&~15u,(y+3u)&~3u,(z+3u)&~3u};
    }
    args.pMipInfoList=mips.data(); args.ResourceDimension=D3D10DDIRESOURCE_TEXTURE3D;
    args.Usage=D3D10_DDI_USAGE_DEFAULT; args.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    args.BindFlags=D3D10_DDI_BIND_RENDER_TARGET|D3D10_DDI_BIND_SHADER_RESOURCE;
    args.SampleDesc.Count=1; args.MipLevels=levels; args.ArraySize=1;
  }
};
using Volume=std::vector<std::vector<UINT>>;
static size_t offset(const D3D10DDI_MIPINFO& m,UINT x,UINT y,UINT z) {
  return (size_t(z)*m.TexelHeight+y)*m.TexelWidth+x;
}
static Volume pixels(const Description& d) {
  Volume out(d.mips.size());
  for (UINT mip=0;mip<d.mips.size();++mip) {
    const auto& m=d.mips[mip]; out[mip].resize(size_t(m.TexelWidth)*m.TexelHeight*m.TexelDepth);
    for (UINT z=0;z<m.TexelDepth;++z) for (UINT y=0;y<m.TexelHeight;++y) for (UINT x=0;x<m.TexelWidth;++x)
      out[mip][offset(m,x,y,z)]=0xff000000u|(mip<<20)|(z<<12)|(y<<6)|x;
  }
  return out;
}
struct Pitched {
  std::vector<std::vector<unsigned char>> bytes;
  std::vector<D3D10_DDIARG_SUBRESOURCE_UP> rows;
  Pitched(const Description& d,const Volume& p) : bytes(p.size()),rows(p.size()) {
    for (UINT i=0;i<p.size();++i) {
      const auto& m=d.mips[i]; auto& row=rows[i];
      row.SysMemPitch=m.TexelWidth*4+12; row.SysMemSlicePitch=row.SysMemPitch*m.TexelHeight+20;
      bytes[i].resize(size_t(row.SysMemSlicePitch)*m.TexelDepth,0xcd); row.pSysMem=bytes[i].data();
      for (UINT z=0;z<m.TexelDepth;++z) for (UINT y=0;y<m.TexelHeight;++y)
        std::memcpy(bytes[i].data()+size_t(z)*row.SysMemSlicePitch+y*row.SysMemPitch,
          p[i].data()+offset(m,0,y,z),m.TexelWidth*4);
    }
  }
};
struct Resource {
  Fixture& owner; Storage storage; D3D10DDI_HRESOURCE handle;
  Resource(Fixture& f,const D3D10DDIARG_CREATERESOURCE& a) : owner(f),
      storage(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,nullptr);})),
      handle(storage.handle<D3D10DDI_HRESOURCE>()) { f.create(a,handle); ok(); }
  ~Resource() { owner.call([&](auto& t){t.pfnDestroyResource(owner.device,handle);}); ok(); }
};
static void readVolume(Fixture& f,Resource& resource,const Description& d,const Volume& expected) {
  const UINT readback=f.readbacks++;
  auto a=d.args; a.Usage=D3D10_DDI_USAGE_STAGING; a.MapFlags=D3D10_DDI_CPU_ACCESS_READ;
  a.BindFlags=a.MiscFlags=0; a.pInitialDataUP=nullptr; Resource staging(f,a);
  f.call([&](auto& t){t.pfnResourceCopy(f.device,staging.handle,resource.handle);}); ok();
  for (UINT mip=0;mip<expected.size();++mip) {
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.call([&](auto& t){t.pfnStagingResourceMap(f.device,staging.handle,mip,D3D10_DDI_MAP_READ,0,&map);}); ok();
    const auto& m=d.mips[mip]; CHECK(map.pData && map.RowPitch>=m.TexelWidth*4);
    if (m.TexelDepth>1) CHECK(map.DepthPitch>=uint64_t(m.TexelHeight-1)*map.RowPitch+m.TexelWidth*4);
    std::vector<UINT> observed(expected[mip].size());
    for (UINT z=0;z<m.TexelDepth;++z) for (UINT y=0;y<m.TexelHeight;++y) for (UINT x=0;x<m.TexelWidth;++x) {
      UINT v; std::memcpy(&v,static_cast<const char*>(map.pData)+size_t(z)*map.DepthPitch+y*map.RowPitch+x*4,4);
      observed[offset(m,x,y,z)]=v;
    }
    const std::string name="readback-"+std::to_string(readback)+"-mip-"+std::to_string(mip);
    const UINT metadata[]={readback,mip,m.TexelWidth,m.TexelHeight,m.TexelDepth,map.RowPitch,map.DepthPitch};
    f.call([&](auto& t){t.pfnStagingResourceUnmap(f.device,staging.handle,mip);}); ok();
    saveShader(f.profile,(name+".actual.u32.bin").c_str(),observed.data(),observed.size()*sizeof(UINT));
    saveShader(f.profile,(name+".expected.u32.bin").c_str(),expected[mip].data(),expected[mip].size()*sizeof(UINT));
    saveShader(f.profile,(name+".dimensions-pitches.u32.bin").c_str(),metadata,sizeof(metadata));
    for (size_t i=0;i<observed.size();++i) {
      ++voxels;
      if (observed[i]!=expected[mip][i]) {
        const size_t x=i%m.TexelWidth,y=(i/m.TexelWidth)%m.TexelHeight;
        const size_t z=i/(size_t(m.TexelWidth)*m.TexelHeight);
        std::fprintf(stderr,"VOLUME_MISMATCH profile=%u readback=%u mip=%u xyz=%zu,%zu,%zu actual=%08x expected=%08x\n",
          f.profile,readback,mip,x,y,z,observed[i],expected[mip][i]);
      }
      CHECK(observed[i]==expected[mip][i]);
    }
  }
}
struct ShaderView {
  Fixture& owner; Storage storage; D3D10DDI_HSHADERRESOURCEVIEW handle;
  ShaderView(Fixture& f,Resource& r,UINT mip,UINT count) : owner(f),
      storage(f.call([&](auto& t){return t.pfnCalcPrivateShaderResourceViewSize(f.device,nullptr);})),
      handle(storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>()) {
    D3D10DDIARG_CREATESHADERRESOURCEVIEW a{}; a.hDrvResource=r.handle;
    a.Format=DXGI_FORMAT_R8G8B8A8_UNORM; a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE3D;
    a.Tex3D={mip,count}; f.shaderView(a,handle); ok();
  }
  ComPtr<ID3D11ShaderResourceView> inspect() {
    owner.call([&](auto& t){t.pfnPsSetShaderResources(owner.device,0,1,&handle);}); ok();
    ComPtr<ID3D11ShaderResourceView> out; owner.context->PSGetShaderResources(0,1,&out); CHECK(out);
    return out;
  }
  ~ShaderView() {
    D3D10DDI_HSHADERRESOURCEVIEW empty{};
    owner.call([&](auto& t){t.pfnPsSetShaderResources(owner.device,0,1,&empty);}); ok();
    owner.call([&](auto& t){t.pfnDestroyShaderResourceView(owner.device,handle);}); ok();
  }
};
struct Target {
  Fixture& owner; Storage storage; D3D10DDI_HRENDERTARGETVIEW handle;
  Target(Fixture& f,Resource& r,UINT mip,UINT first,UINT count) : owner(f),
      storage(f.call([&](auto& t){return t.pfnCalcPrivateRenderTargetViewSize(f.device,nullptr);})),
      handle(storage.handle<D3D10DDI_HRENDERTARGETVIEW>()) {
    D3D10DDIARG_CREATERENDERTARGETVIEW a{}; a.hDrvResource=r.handle; a.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE3D; a.Tex3D={mip,first,count};
    f.call([&](auto& t){t.pfnCreateRenderTargetView(f.device,&a,handle,{});}); ok();
  }
  ~Target() { owner.call([&](auto& t){t.pfnDestroyRenderTargetView(owner.device,handle);}); ok(); }
};

static void saveBytes(const std::string& path,const void* data,size_t bytes) {
  std::ofstream output(path,std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(data),static_cast<std::streamsize>(bytes));
  output.close(); CHECK(!output.fail());
}
static void saveShader(UINT profile,const char* name,const void* data,size_t bytes) {
  saveBytes("volume-"+std::to_string(profile)+"-"+name,data,bytes);
}

// Observe the backend's direct public API on an independently created volume.
// These words never become the production oracle. Save every original level,
// including the terminal 1x1x1 level, before the production comparison runs.
static void observePublicMips(Fixture& f,const Description& d,const Pitched& initial,
    UINT count,const Volume& expected) {
  D3D11_TEXTURE3D_DESC desc{};
  desc.Width=d.mips[0].TexelWidth; desc.Height=d.mips[0].TexelHeight;
  desc.Depth=d.mips[0].TexelDepth; desc.MipLevels=d.args.MipLevels;
  desc.Format=d.args.Format; desc.Usage=D3D11_USAGE_DEFAULT;
  desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
  std::vector<D3D11_SUBRESOURCE_DATA> data(initial.rows.size());
  for (size_t i=0;i<data.size();++i)
    data[i]={initial.rows[i].pSysMem,initial.rows[i].SysMemPitch,initial.rows[i].SysMemSlicePitch};
  ComPtr<ID3D11Texture3D> texture;
  CHECK(f.backend->CreateTexture3D(&desc,data.data(),&texture)==S_OK && texture);
  D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
  viewDesc.Format=desc.Format; viewDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE3D;
  viewDesc.Texture3D={1,count};
  ComPtr<ID3D11ShaderResourceView> view;
  CHECK(f.backend->CreateShaderResourceView(texture.Get(),&viewDesc,&view)==S_OK && view);
  D3D11_SHADER_RESOURCE_VIEW_DESC actualView{}; view->GetDesc(&actualView);
  CHECK(actualView.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE3D);
  CHECK(actualView.Texture3D.MostDetailedMip==1);
  CHECK(actualView.Texture3D.MipLevels==(count==UINT32_MAX ? desc.MipLevels-1 : count));
  f.context->GenerateMips(view.Get());
  desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=desc.MiscFlags=0;
  desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  ComPtr<ID3D11Texture3D> staging;
  CHECK(f.backend->CreateTexture3D(&desc,nullptr,&staging)==S_OK && staging);
  f.context->CopyResource(staging.Get(),texture.Get());
  for (UINT mip=0;mip<desc.MipLevels;++mip) {
    D3D11_MAPPED_SUBRESOURCE map{};
    CHECK(f.context->Map(staging.Get(),mip,D3D11_MAP_READ,0,&map)==S_OK && map.pData);
    const auto& shape=d.mips[mip];
    CHECK(map.RowPitch>=shape.TexelWidth*4);
    if (shape.TexelDepth>1)
      CHECK(map.DepthPitch>=uint64_t(shape.TexelHeight-1)*map.RowPitch+shape.TexelWidth*4);
    std::vector<UINT> observed(expected[mip].size());
    for (UINT z=0;z<shape.TexelDepth;++z) for (UINT y=0;y<shape.TexelHeight;++y)
      for (UINT x=0;x<shape.TexelWidth;++x)
        std::memcpy(&observed[offset(shape,x,y,z)],static_cast<const char*>(map.pData)
          +size_t(z)*map.DepthPitch+y*map.RowPitch+x*4,4);
    const UINT metadata[]={count,mip,shape.TexelWidth,shape.TexelHeight,shape.TexelDepth,
      map.RowPitch,map.DepthPitch,actualView.Texture3D.MostDetailedMip,actualView.Texture3D.MipLevels};
    f.context->Unmap(staging.Get(),mip);
    const std::string name="public-volume-"+std::to_string(f.profile)+"-range-"
      +std::to_string(count)+"-mip-"+std::to_string(mip);
    saveBytes(name+".actual.u32.bin",observed.data(),observed.size()*sizeof(UINT));
    saveBytes(name+".expected.u32.bin",expected[mip].data(),expected[mip].size()*sizeof(UINT));
    saveBytes(name+".dimensions-pitches.u32.bin",metadata,sizeof(metadata));
    size_t mismatches=0;
    for (size_t i=0;i<observed.size();++i) {
      if (observed[i]==expected[mip][i]) continue;
      if (!mismatches) {
        const size_t x=i%shape.TexelWidth,y=(i/shape.TexelWidth)%shape.TexelHeight;
        const size_t z=i/(size_t(shape.TexelWidth)*shape.TexelHeight);
        std::printf("PUBLIC_MIP_OBSERVATION profile=%u count=%u mip=%u first_xyz=%zu,%zu,%zu actual=%08x expected=%08x\n",
          f.profile,count,mip,x,y,z,observed[i],expected[mip][i]);
      }
      ++mismatches;
    }
    std::printf("PUBLIC_MIP_OBSERVATION profile=%u count=%u mip=%u mismatches=%zu words=%zu\n",
      f.profile,count,mip,mismatches,observed.size()); std::fflush(stdout);
  }
}

// Public reference shaders independently sample the production-created and
// production-bound SRV. They do not exercise or replace the UMD shader compiler.
static void sampling(Fixture& f) {
  Description d(9,5,7,1); auto expected=pixels(d); Pitched input(d,expected);
  d.args.pInitialDataUP=input.rows.data(); Resource r(f,d.args); ShaderView view(f,r,0,1);
  const char vs[]= "float4 main(uint id:SV_VertexID):SV_Position { float2 p=float2((id<<1)&2,id&2); return float4(p*float2(2,-2)+float2(-1,1),0,1); }";
  const char ps[]= "Texture3D<float4> t:register(t0); cbuffer Slice:register(b0){uint4 layer;} float4 main(float4 p:SV_Position):SV_Target{return t.Load(int4(int2(p.xy),layer.x,0));}";
  ComPtr<ID3DBlob> vcode,pcode,diagnostics;
  CHECK(D3DCompile(vs,sizeof(vs)-1,"volume-vs",nullptr,nullptr,"main","vs_4_0",0,0,&vcode,&diagnostics)==S_OK);
  diagnostics.Reset();
  CHECK(D3DCompile(ps,sizeof(ps)-1,"volume-ps",nullptr,nullptr,"main","ps_4_0",0,0,&pcode,&diagnostics)==S_OK);
  saveShader(f.profile,"vs.hlsl",vs,sizeof(vs)-1); saveShader(f.profile,"ps.hlsl",ps,sizeof(ps)-1);
  saveShader(f.profile,"vs.dxbc",vcode->GetBufferPointer(),vcode->GetBufferSize());
  saveShader(f.profile,"ps.dxbc",pcode->GetBufferPointer(),pcode->GetBufferSize());
  ComPtr<ID3D11VertexShader> vertex; ComPtr<ID3D11PixelShader> pixel;
  CHECK(f.backend->CreateVertexShader(vcode->GetBufferPointer(),vcode->GetBufferSize(),nullptr,&vertex)==S_OK);
  CHECK(f.backend->CreatePixelShader(pcode->GetBufferPointer(),pcode->GetBufferSize(),nullptr,&pixel)==S_OK);
  D3D11_TEXTURE2D_DESC desc{}; desc.Width=9; desc.Height=5; desc.MipLevels=desc.ArraySize=1;
  desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> output,readback; CHECK(f.backend->CreateTexture2D(&desc,nullptr,&output)==S_OK);
  ComPtr<ID3D11RenderTargetView> target; CHECK(f.backend->CreateRenderTargetView(output.Get(),nullptr,&target)==S_OK);
  desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  CHECK(f.backend->CreateTexture2D(&desc,nullptr,&readback)==S_OK);
  D3D11_BUFFER_DESC cb{}; cb.ByteWidth=16; cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  ComPtr<ID3D11Buffer> constants; CHECK(f.backend->CreateBuffer(&cb,nullptr,&constants)==S_OK);
  D3D11_RASTERIZER_DESC raster{}; raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE; raster.DepthClipEnable=TRUE;
  ComPtr<ID3D11RasterizerState> state; CHECK(f.backend->CreateRasterizerState(&raster,&state)==S_OK);
  D3D11_VIEWPORT viewport{0,0,9,5,0,1}; ID3D11Buffer* rawCb=constants.Get(); ID3D11RenderTargetView* rawTarget=target.Get();
  f.context->IASetInputLayout(nullptr); f.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  f.context->VSSetShader(vertex.Get(),nullptr,0); f.context->PSSetShader(pixel.Get(),nullptr,0);
  f.context->PSSetConstantBuffers(0,1,&rawCb); f.context->RSSetState(state.Get()); f.context->RSSetViewports(1,&viewport);
  f.context->OMSetRenderTargets(1,&rawTarget,nullptr); view.inspect();
  for (UINT z=0;z<7;++z) {
    const UINT slice[4]={z,0,0,0}; f.context->UpdateSubresource(constants.Get(),0,nullptr,slice,0,0);
    f.context->Draw(3,0); f.context->CopyResource(readback.Get(),output.Get());
    D3D11_MAPPED_SUBRESOURCE map{}; CHECK(f.context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)==S_OK && map.pData);
    std::vector<UINT> observed(45),reference(45);
    for (UINT y=0;y<5;++y) for (UINT x=0;x<9;++x) {
      UINT v; std::memcpy(&v,static_cast<const char*>(map.pData)+y*map.RowPitch+x*4,4);
      observed[y*9+x]=v; reference[y*9+x]=expected[0][offset(d.mips[0],x,y,z)];
    }
    const std::string name="sampled-slice-"+std::to_string(z);
    const UINT metadata[]={z,9,5,map.RowPitch};
    f.context->Unmap(readback.Get(),0);
    saveShader(f.profile,(name+".actual.u32.bin").c_str(),observed.data(),observed.size()*sizeof(UINT));
    saveShader(f.profile,(name+".expected.u32.bin").c_str(),reference.data(),reference.size()*sizeof(UINT));
    saveShader(f.profile,(name+".dimensions-pitches.u32.bin").c_str(),metadata,sizeof(metadata));
    for (size_t i=0;i<observed.size();++i) { ++sampled; CHECK(observed[i]==reference[i]); }
  }
  f.context->ClearState(); ++cases;
}

static void initialization(Fixture& f) {
  Description d(9,5,7,4); auto expected=pixels(d); Pitched source(d,expected);
  d.args.Usage=D3D10_DDI_USAGE_IMMUTABLE; d.args.BindFlags=D3D10_DDI_BIND_SHADER_RESOURCE;
  d.args.pInitialDataUP=source.rows.data(); Resource r(f,d.args);
  ShaderView view(f,r,1,UINT32_MAX); auto api=view.inspect();
  D3D11_SHADER_RESOURCE_VIEW_DESC range{}; api->GetDesc(&range);
  CHECK(range.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE3D);
  CHECK(range.Texture3D.MostDetailedMip==1 && range.Texture3D.MipLevels==3);
  ComPtr<ID3D11Resource> resource; api->GetResource(&resource);
  ComPtr<ID3D11Texture3D> texture; CHECK(SUCCEEDED(resource.As(&texture)));
  D3D11_TEXTURE3D_DESC shape{}; texture->GetDesc(&shape);
  CHECK(shape.Width==9 && shape.Height==5 && shape.Depth==7 && shape.MipLevels==4);
  readVolume(f,r,d,expected); ++cases;
}

static void transfers(Fixture& f) {
  Description d(9,5,7,4); const auto original=pixels(d); auto expected=original;
  Pitched input(d,original); d.args.pInitialDataUP=input.rows.data();
  Resource src(f,d.args),dst(f,d.args);
  // Two padded rows per slice and three distinct slices expose depth aliasing.
  Description patch(3,2,3,1); auto values=pixels(patch); Pitched pitched(patch,values);
  D3D10_DDI_BOX box{2,1,2,5,3,5};
  f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,dst.handle,0,&box,
    pitched.rows[0].pSysMem,pitched.rows[0].SysMemPitch,pitched.rows[0].SysMemSlicePitch);}); ok();
  for (UINT z=0;z<3;++z) for (UINT y=0;y<2;++y) for (UINT x=0;x<3;++x)
    expected[0][offset(d.mips[0],x+2,y+1,z+2)]=values[0][offset(patch.mips[0],x,y,z)];
  D3D10_DDI_BOX from{1,1,1,4,3,3};
  f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,dst.handle,0,5,2,4,src.handle,0,&from);}); ok();
  for (UINT z=0;z<2;++z) for (UINT y=0;y<2;++y) for (UINT x=0;x<3;++x)
    expected[0][offset(d.mips[0],x+5,y+2,z+4)]=original[0][offset(d.mips[0],x+1,y+1,z+1)];
  f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,dst.handle,0,0,0,UINT32_MAX,src.handle,0,&from);}); rejected();
  f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,dst.handle,0,&box,pitched.rows[0].pSysMem,11,64);}); rejected();
  f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,dst.handle,0,&box,pitched.rows[0].pSysMem,24,35);}); rejected();
  D3D10_DDI_BOX invalid{0,0,-1,1,1,1};
  f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,dst.handle,0,0,0,0,src.handle,0,&invalid);}); rejected();
  f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,dst.handle,4,nullptr,nullptr,0,0);}); rejected();
  D3D10_DDI_BOX empty{0,0,7,1,1,7};
  f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,dst.handle,0,&empty,nullptr,0,0);}); ok();
  Description different(8,5,7,4); Resource other(f,different.args);
  f.call([&](auto& t){t.pfnResourceCopy(f.device,dst.handle,other.handle);}); rejected();
  f.call([&](auto& t){t.pfnResourceCopy(f.device,dst.handle,dst.handle);}); rejected();
  readVolume(f,dst,d,expected); ++cases;
}

static void dynamic(Fixture& f) {
  Description d(7,3,5,1); d.args.Usage=D3D10_DDI_USAGE_DYNAMIC;
  d.args.MapFlags=D3D10_DDI_CPU_ACCESS_WRITE; d.args.BindFlags=D3D10_DDI_BIND_SHADER_RESOURCE;
  Resource r(f,d.args);
  D3D10DDI_MAPPED_SUBRESOURCE invalid{reinterpret_cast<void*>(uintptr_t(1)),17,19};
  f.call([&](auto& t){t.pfnResourceMap(f.device,r.handle,1,D3D10_DDI_MAP_WRITE_DISCARD,0,&invalid);});
  rejected(); CHECK(!invalid.pData && !invalid.RowPitch && !invalid.DepthPitch);
  for (UINT round=0;round<3;++round) {
    auto expected=pixels(d); for (auto& v:expected[0]) v^=round*0x00010101u;
    D3D10DDI_MAPPED_SUBRESOURCE map{};
    f.call([&](auto& t){t.pfnDynamicResourceMapDiscard(f.device,r.handle,0,D3D10_DDI_MAP_WRITE_DISCARD,0,&map);}); ok();
    CHECK(map.pData && map.RowPitch>=28 && map.DepthPitch>=map.RowPitch*2+28);
    for (UINT z=0;z<5;++z) for (UINT y=0;y<3;++y)
      std::memcpy(static_cast<char*>(map.pData)+size_t(z)*map.DepthPitch+y*map.RowPitch,
        expected[0].data()+offset(d.mips[0],0,y,z),28);
    f.call([&](auto& t){t.pfnDynamicResourceUnmap(f.device,r.handle,0);}); ok();
    readVolume(f,r,d,expected);
  }
  ++cases;
}

static void outputs(Fixture& f) {
  Description d(8,4,8,3); auto expected=pixels(d); Pitched data(d,expected);
  d.args.pInitialDataUP=data.rows.data(); Resource r(f,d.args);
  Target first(f,r,1,0,1),second(f,r,1,1,1),overlap(f,r,1,0,1);
  D3D10DDI_HRENDERTARGETVIEW targets[]{first.handle,second.handle};
  f.targets(targets,2); ok();
  ComPtr<ID3D11RenderTargetView> actual;
  f.context->OMGetRenderTargets(1,&actual,nullptr); CHECK(actual);
  D3D11_RENDER_TARGET_VIEW_DESC view{}; actual->GetDesc(&view);
  CHECK(view.ViewDimension==D3D11_RTV_DIMENSION_TEXTURE3D);
  CHECK(view.Texture3D.MipSlice==1 && view.Texture3D.FirstWSlice==0 && view.Texture3D.WSize==1);
  targets[1]=overlap.handle;
  f.targets(targets,2); rejected();
  FLOAT red[]{1,0,0,1}; f.call([&](auto& t){t.pfnClearRenderTargetView(f.device,second.handle,red);}); ok();
  for (UINT y=0;y<2;++y) for (UINT x=0;x<4;++x) expected[1][offset(d.mips[1],x,y,1)]=0xff0000ffu;
  f.targets(nullptr,0); ok();
  Target remainder(f,r,1,2,UINT32_MAX);
  FLOAT green[]{0,1,0,1}; f.call([&](auto& t){t.pfnClearRenderTargetView(f.device,remainder.handle,green);}); ok();
  for (UINT z=2;z<4;++z) for (UINT y=0;y<2;++y) for (UINT x=0;x<4;++x)
    expected[1][offset(d.mips[1],x,y,z)]=0xff00ff00u;
  readVolume(f,r,d,expected); ++cases;
}

static void mips(Fixture& f) {
  for (UINT count : {UINT32_MAX,2u,1u}) {
    Description d(8,8,8,4); d.args.MiscFlags=D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP;
    auto expected=pixels(d); std::fill(expected[1].begin(),expected[1].end(),0xff00ffffu);
    Pitched data(d,expected); d.args.pInitialDataUP=data.rows.data(); Resource r(f,d.args);
    ShaderView view(f,r,1,count); f.call([&](auto& t){t.pfnGenMips(f.device,view.handle);}); ok();
    const UINT end=count==UINT32_MAX ? 4 : 1+count;
    for (UINT mip=2;mip<end;++mip) std::fill(expected[mip].begin(),expected[mip].end(),0xff00ffffu);
    observePublicMips(f,d,data,count,expected);
    readVolume(f,r,d,expected); ++cases;
  }
}

static void failures(Fixture& f) {
  Description d(9,5,7,4); const auto p=pixels(d); Pitched initial(d,p);
  Storage storage(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,nullptr);}));
  for (UINT kind=0;kind<8;++kind) {
    auto a=d.args; auto mip=d.mips; a.pMipInfoList=mip.data();
    auto data=initial.rows; a.pInitialDataUP=data.data();
    if (kind==0) a.ArraySize=2;
    else if (kind==1) mip[0].TexelDepth=0;
    else if (kind==2) mip[1].TexelHeight=3;
    else if (kind==3) data[0].SysMemPitch=35;
    else if (kind==4) data[0].SysMemSlicePitch=100;
    else if (kind==5) a.BindFlags=D3D10_DDI_BIND_DEPTH_STENCIL;
    else if (kind==6) a.SampleDesc.Count=4;
    else a.MiscFlags=D3D10_DDI_RESOURCE_MISC_SHARED;
    storage.poison(); f.create(a,storage.handle<D3D10DDI_HRESOURCE>()); rejected(); CHECK(storage.untouched());
  }
  Resource r(f,d.args);
  Storage view(f.call([&](auto& t){return t.pfnCalcPrivateRenderTargetViewSize(f.device,nullptr);}));
  D3D10DDIARG_CREATERENDERTARGETVIEW a{}; a.hDrvResource=r.handle; a.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
  a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE3D;
  for (auto range : {D3D10DDIARG_TEX3D_RENDERTARGETVIEW{1,3,1}, {4,0,1}, {0,UINT32_MAX,1}, {0,0,0}}) {
    a.Tex3D=range; view.poison();
    f.call([&](auto& t){t.pfnCreateRenderTargetView(f.device,&a,view.handle<D3D10DDI_HRENDERTARGETVIEW>(),{});});
    rejected(); CHECK(view.untouched());
  }
  Storage srv(f.call([&](auto& t){return t.pfnCalcPrivateShaderResourceViewSize(f.device,nullptr);}));
  D3D10DDIARG_CREATESHADERRESOURCEVIEW sa{}; sa.hDrvResource=r.handle;
  sa.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sa.ResourceDimension=D3D10DDIRESOURCE_TEXTURE3D;
  for (auto range : {D3D10DDIARG_TEX3D_SHADERRESOURCEVIEW{4,1}, {0,0}, {2,3}, {UINT32_MAX,1}}) {
    sa.Tex3D=range; srv.poison(); f.shaderView(sa,srv.handle<D3D10DDI_HSHADERRESOURCEVIEW>());
    rejected(); CHECK(srv.untouched());
  }
  ++cases;
}

int main() {
  caller=GetCurrentThreadId();
  std::puts("BACKEND=WARP\nproduction_volume_DDI=true\nVIOGPU=false\nregistration=false");
  try {
  for (UINT profile=0;profile<3;++profile) {
    Fixture f(profile);
    const struct {const char* name;void (*run)(Fixture&);} tests[]={
      {"padded-initialization",initialization},{"xyz-transfers",transfers},{"dynamic-volume",dynamic},
      {"w-slice-outputs",outputs},{"scoped-volume-mips",mips},{"sampled-volume",sampling},{"atomic-rejections",failures}};
    for (const auto& test:tests) {
      std::printf("BEGIN Texture3D profile=%u case=%s\n",profile,test.name); std::fflush(stdout);
      test.run(f); std::printf("PASS Texture3D case profile=%u case=%s\n",profile,test.name); std::fflush(stdout);
    }
  }
  CHECK(cases==27 && voxels==9138 && sampled==945);
  std::printf("PASS Texture3D\nprofiles=3\ncases=%u\nchecks=%u\nvoxels=%u\nsampled=%u\n",cases,checks,voxels,sampled);
  } catch (const FixtureFailure&) { return 1; }
}
