// SPDX-License-Identifier: MIT
// Production D3D10/11 DDIs with a test-only CPU WARP backend. Raw compressed
// bytes are checked against independently updated public D3D11 resources.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_transfer_format.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>
using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, uploads, noops, rejects, snapshots, readBytes;
static DWORD callerThread;
static HRESULT lastError = S_OK;
static ComPtr<ID3D11DeviceContext> createdContext;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"BC update line=%d expression=%s HRESULT=%08lx\n",__LINE__,#x,static_cast<unsigned long>(lastError)); std::exit(1); } } while (0)
static void ok() { CHECK(lastError == S_OK); }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT result) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && result == E_INVALIDARG);
  ++callbacks; lastError = result;
}
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime);
  const HRESULT hr = D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
    &level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if (hr == S_OK) createdContext = *context;
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }
struct Storage {
  static constexpr UINT64 canary = 0x10ab274961fea538ull;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T bytes) : words((bytes+7)/8+2,0) { CHECK(bytes); words.front()=words.back()=canary; }
  void* data() { return words.data()+1; }
  void guards() const { CHECK(words.front()==canary && words.back()==canary); }
};
struct Fixture {
  UINT profile;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  D3D10DDI_DEVICEFUNCS t10{};
  D3D11DDI_DEVICEFUNCS t11{};
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Device> backend;
  explicit Fixture(UINT version) : profile(version) {
    LUID luid{}; core10.pfnSetErrorCb=error; core11.pfnSetErrorCb=error;
    if (profile==10) CHECK(VioGpuDxvkCreateDdiTestDevice(&luid,device,{&core10},&core10,&t10)==S_OK);
    else CHECK(VioGpuDxvkCreateDdiTestDevice11(&luid,device,{&core11},&core11,&t11,D3D_FEATURE_LEVEL_11_0)==S_OK);
    context=createdContext; createdContext.Reset(); CHECK(context);
    context->GetDevice(&backend); CHECK(backend);
  }
  template<typename F> void call(F fn) { if (profile==10) fn(t10); else fn(t11); }
  SIZE_T resourceSize() { return profile==10?t10.pfnCalcPrivateResourceSize(device,nullptr):t11.pfnCalcPrivateResourceSize(device,nullptr); }
  ~Fixture() { context.Reset(); backend.Reset(); call([&](auto& t){t.pfnDestroyDevice(device);}); ok(); storage.guards(); }
};
static unsigned char pattern(UINT format,UINT profile,UINT cube,UINT sub,UINT step,UINT y,UINT x) {
  return static_cast<unsigned char>(17*format+41*profile+13*cube+29*sub+53*step+7*y+x);
}
struct Texture {
  Fixture& f; Storage storage;
  D3D10DDI_HRESOURCE handle;
  ComPtr<ID3D11Texture2D> reference;
  UINT kind, width, height, mips=5, layers, blockBytes;
  DXGI_FORMAT format;
  std::vector<std::vector<unsigned char>> expected;
  template<typename T> void describe(T& args, D3D10DDI_MIPINFO* shapes,
      D3D10_DDIARG_SUBRESOURCE_UP* initial,bool staging) {
    args.pMipInfoList=shapes; args.pInitialDataUP=staging?nullptr:initial;
    args.ResourceDimension=kind?D3D10DDIRESOURCE_TEXTURECUBE:D3D10DDIRESOURCE_TEXTURE2D;
    args.Usage=staging?D3D10_DDI_USAGE_STAGING:D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags=staging?0:D3D10_DDI_BIND_SHADER_RESOURCE;
    args.MapFlags=staging?D3D10_DDI_CPU_ACCESS_READ:0;
    args.Format=format; args.SampleDesc.Count=1; args.MipLevels=mips; args.ArraySize=layers;
  }
  Texture(Fixture& owner,DXGI_FORMAT fmt,UINT bytes,UINT cube,bool staging=false)
  : f(owner),storage(owner.resourceSize()),handle{storage.data()},
    kind(cube),width(cube?16u:24u),height(16),layers(cube?6u:2u),blockBytes(bytes),format(fmt),expected(mips*layers) {
    std::array<D3D10DDI_MIPINFO,5> shapes{};
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial(expected.size());
    std::vector<D3D11_SUBRESOURCE_DATA> publicInitial(expected.size());
    for (UINT mip=0;mip<mips;++mip) {
      const UINT w=std::max(1u,width>>mip),h=std::max(1u,height>>mip);
      shapes[mip]={w,h,1,(w+3u)&~3u,(h+3u)&~3u,1};
      for (UINT layer=0;layer<layers;++layer) {
        const UINT sub=mip+layer*mips,row=((w+3)/4)*bytes,rows=(h+3)/4;
        auto& data=expected[sub]; data.resize(row*rows);
        for (UINT y=0;y<rows;++y) for (UINT x=0;x<row;++x) data[y*row+x]=pattern(UINT(fmt),f.profile,kind,sub,0,y,x);
        initial[sub]={data.data(),row,row*rows}; publicInitial[sub]={data.data(),row,row*rows};
      }
    }
    if (f.profile==10) {
      D3D10DDIARG_CREATERESOURCE args{}; describe(args,shapes.data(),initial.data(),staging);
      f.t10.pfnCreateResource(f.device,&args,handle,{});
    } else {
      D3D11DDIARG_CREATERESOURCE args{}; describe(args,shapes.data(),initial.data(),staging);
      f.t11.pfnCreateResource(f.device,&args,handle,{});
    }
    ok(); storage.guards();
    D3D11_TEXTURE2D_DESC desc{width,height,mips,layers,fmt,{1,0},staging?D3D11_USAGE_STAGING:D3D11_USAGE_DEFAULT,
      staging?0u:D3D11_BIND_SHADER_RESOURCE,staging?D3D11_CPU_ACCESS_READ:0u,kind?D3D11_RESOURCE_MISC_TEXTURECUBE:0u};
    CHECK(f.backend->CreateTexture2D(&desc,staging?nullptr:publicInitial.data(),&reference)==S_OK);
  }
  ~Texture() { reference.Reset(); f.call([&](auto& t){t.pfnDestroyResource(f.device,handle);}); ok(); storage.guards(); }
  void update(UINT sub,const D3D10_DDI_BOX* input,UINT step) {
    const UINT mip=sub%mips,w=std::max(1u,width>>mip),h=std::max(1u,height>>mip);
    const D3D10_DDI_BOX box=input?*input:D3D10_DDI_BOX{0,0,0,INT(w),INT(h),1};
    const UINT row=UINT((box.right-box.left+3)/4)*blockBytes,rows=UINT((box.bottom-box.top+3)/4);
    const UINT pitch=row+blockBytes+3;
    std::vector<unsigned char> source(pitch*rows,0xea);
    for (UINT y=0;y<rows;++y) for (UINT x=0;x<row;++x) source[y*pitch+x]=pattern(UINT(format),f.profile,kind,sub,step,y,x);
    f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,handle,sub,input,source.data(),pitch,0);}); ok(); ++uploads;
    const D3D11_BOX publicBox{UINT(box.left),UINT(box.top),UINT(box.front),UINT(box.right),UINT(box.bottom),UINT(box.back)};
    f.context->UpdateSubresource(reference.Get(),sub,input?&publicBox:nullptr,source.data(),pitch,0);
    const UINT fullRow=((w+3)/4)*blockBytes;
    for (UINT y=0;y<rows;++y)
      std::copy_n(source.data()+y*pitch,row,expected[sub].data()+(UINT(box.top)/4+y)*fullRow+UINT(box.left)/4*blockBytes);
  }
};
static void save(const std::string& name,const void* data,size_t bytes) {
  std::ofstream output(name,std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(data),static_cast<std::streamsize>(bytes)); output.close(); CHECK(!output.fail());
}
static void snapshot(Texture& texture,Texture& staging,UINT phase) {
  auto& f=texture.f;
  f.call([&](auto& t){t.pfnResourceCopy(f.device,staging.handle,texture.handle);}); ok();
  f.context->CopyResource(staging.reference.Get(),texture.reference.Get());
  for (UINT sub=0;sub<texture.mips*texture.layers;++sub) {
    const UINT mip=sub%texture.mips,w=std::max(1u,texture.width>>mip),h=std::max(1u,texture.height>>mip);
    const UINT row=((w+3)/4)*texture.blockBytes,rows=(h+3)/4;
    D3D10DDI_MAPPED_SUBRESOURCE native{}; D3D11_MAPPED_SUBRESOURCE reference{};
    f.call([&](auto& t){t.pfnStagingResourceMap(f.device,staging.handle,sub,D3D10_DDI_MAP_READ,0,&native);});
    ok(); CHECK(native.pData && native.RowPitch>=row);
    CHECK(f.context->Map(staging.reference.Get(),sub,D3D11_MAP_READ,0,&reference)==S_OK);
    CHECK(reference.pData && reference.RowPitch>=row);
    for (UINT y=0;y<rows;++y) for (UINT x=0;x<row;++x) {
      CHECK(static_cast<const unsigned char*>(native.pData)[y*native.RowPitch+x]==texture.expected[sub][y*row+x]);
      CHECK(static_cast<const unsigned char*>(reference.pData)[y*reference.RowPitch+x]==texture.expected[sub][y*row+x]);
      readBytes+=2;
    }
    const std::string name="bc-update-"+std::to_string(f.profile)+"-"+std::to_string(UINT(texture.format))
      +"-"+std::to_string(texture.kind)+"-"+std::to_string(phase)+"-"+std::to_string(sub);
    const size_t nativeSpan=size_t(rows-1)*native.RowPitch+row,publicSpan=size_t(rows-1)*reference.RowPitch+row;
    save(name+".native.bin",native.pData,nativeSpan); save(name+".public.bin",reference.pData,publicSpan);
    std::ofstream layout(name+".layout.json"); CHECK(layout.is_open());
    layout<<"{\"profile\":"<<f.profile<<",\"format\":"<<UINT(texture.format)<<",\"cube\":"<<texture.kind
      <<",\"phase\":"<<phase<<",\"subresource\":"<<sub<<",\"width\":"<<w<<",\"height\":"<<h
      <<",\"blockBytes\":"<<texture.blockBytes<<",\"rowBytes\":"<<row<<",\"rows\":"<<rows
      <<",\"nativePitch\":"<<native.RowPitch<<",\"publicPitch\":"<<reference.RowPitch<<"}\n";
    layout.close(); CHECK(!layout.fail());
    f.context->Unmap(staging.reference.Get(),sub);
    f.call([&](auto& t){t.pfnStagingResourceUnmap(f.device,staging.handle,sub);}); ok(); ++snapshots;
  }
}
static void malformed(Texture& texture,Texture& staging) {
  auto& f=texture.f; std::array<unsigned char,512> source{};
  auto reject=[&](Texture& target,UINT sub,const D3D10_DDI_BOX* box,const void* data,UINT pitch) {
    const auto before=target.storage.words; const UINT count=callbacks;
    f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,target.handle,sub,box,data,pitch,0);});
    CHECK(lastError==E_INVALIDARG && callbacks==count+1); lastError=S_OK; ++rejects;
    CHECK(target.storage.words==before); target.storage.guards();
  };
  for (const D3D10_DDI_BOX box : {D3D10_DDI_BOX{1,0,0,8,8,1},{0,1,0,8,8,1},{0,0,0,6,8,1},
      {0,0,0,8,6,1},{-1,0,0,8,8,1},{0,0,0,INT(texture.width+4),8,1},{0,0,0,8,8,2}})
    reject(texture,0,&box,source.data(),128);
  reject(texture,0,nullptr,source.data(),((texture.width+3)/4)*texture.blockBytes-1);
  reject(texture,0,nullptr,nullptr,128);
  reject(texture,0,nullptr,reinterpret_cast<const void*>(UINTPTR_MAX-3),128);
  reject(texture,UINT32_MAX,nullptr,source.data(),128);
  reject(staging,0,nullptr,source.data(),128);
  for (const D3D10_DDI_BOX box : {D3D10_DDI_BOX{0,0,0,0,8,1},{9,0,0,8,8,1},{0,9,0,8,8,1},{0,0,2,8,8,1}}) {
    const UINT count=callbacks;
    f.call([&](auto& t){t.pfnResourceUpdateSubresourceUP(f.device,texture.handle,0,&box,nullptr,0,0);});
    ok(); CHECK(callbacks==count); ++noops;
  }
}
static void exercise(Fixture& f,DXGI_FORMAT format,UINT bytes,UINT cube) {
  Texture texture(f,format,bytes,cube),staging(f,format,bytes,cube,true);
  for (UINT sub=0;sub<texture.mips*texture.layers;++sub) texture.update(sub,nullptr,1);
  const UINT layer=texture.layers-1;
  const D3D10_DDI_BOX patch{4,4,0,12,12,1}; texture.update(layer*5,&patch,2);
  if (!cube) { const D3D10_DDI_BOX edge{4,0,0,8,4,1}; texture.update(layer*5+2,&edge,3); }
  const D3D10_DDI_BOX edge{0,0,0,4,4,1}; texture.update(layer*5+3,&edge,4);
  texture.update(layer*5+4,nullptr,5);
  snapshot(texture,staging,0);
  malformed(texture,staging);
  snapshot(texture,staging,1);
}
int main() {
  callerThread=GetCurrentThreadId();
  static_assert(std::is_same_v<decltype(D3D10DDI_DEVICEFUNCS::pfnResourceUpdateSubresourceUP),PFND3D10DDI_RESOURCEUPDATESUBRESOURCEUP>);
  static_assert(std::is_same_v<decltype(D3D11DDI_DEVICEFUNCS::pfnResourceUpdateSubresourceUP),PFND3D10DDI_RESOURCEUPDATESUBRESOURCEUP>);
  for (UINT profile : {10u,11u}) {
    Fixture f(profile);
    for (UINT value=UINT(DXGI_FORMAT_BC1_TYPELESS);value<=UINT(DXGI_FORMAT_BC5_SNORM);++value) {
      const auto format=static_cast<DXGI_FORMAT>(value); const UINT bytes=(value<=72 || (value>=79 && value<=81))?8u:16u;
      CHECK(dxvk::umd::transferBlockBytes(format)==bytes && dxvk::umd::transferTexelBytes(format)==0);
      exercise(f,format,bytes,0);
    }
    exercise(f,DXGI_FORMAT_BC1_UNORM,8,1); exercise(f,DXGI_FORMAT_BC5_SNORM,16,1);
  }
  for (auto format : {DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_NV12,DXGI_FORMAT_BC6H_UF16,DXGI_FORMAT_BC7_UNORM})
    CHECK(dxvk::umd::transferBlockBytes(format)==0);
  CHECK(callbacks==rejects && lastError==S_OK);
  std::printf("PASS native BC1-5 updates: uploads=%u noops=%u rejects=%u snapshots=%u bytes=%u checks=%u\n",uploads,noops,rejects,snapshots,readBytes,checks);
  std::puts("BACKEND=WARP; VIOGPU_GPU_ACCEPTANCE=NOT_RUN; INSTALLATION=NONE");
}
