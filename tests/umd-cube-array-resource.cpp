// SPDX-License-Identifier: MIT
// Actual typed production DDI; the linked reference factory uses WARP only.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, cases, texels, failures;
static HRESULT lastError=S_OK;
static DWORD callerThread;
static ComPtr<ID3D11DeviceContext> createdContext;
static void check(bool value,unsigned line) {
  ++checks;
  if (!value) {
    std::fprintf(stderr,"FAIL cube-array resource line=%u error=%08lx\n",line,static_cast<unsigned long>(lastError));
    std::exit(1);
  }
}
#define CHECK(x) check(!!(x),__LINE__)
static void ok() { CHECK(lastError==S_OK); }
static void rejected() { CHECK(lastError==E_INVALIDARG); lastError=S_OK; ++failures; }
static void APIENTRY error(D3D10DDI_HRTCORELAYER,HRESULT result) {
  CHECK(GetCurrentThreadId()==callerThread); lastError=result;
}
HRESULT dxvk::umd::createDevice(const LUID&,D3D_FEATURE_LEVEL level,
    ID3D11Device** device,ID3D11DeviceContext** context,const dxvk::umd::RuntimeBackend*) noexcept {
  level=dxvk::umd::implementationFeatureLevel(level);
  HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
    &level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if (hr==S_OK) createdContext=*context;
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }

struct Storage {
  SIZE_T size;
  std::unique_ptr<void,decltype(&std::free)> data;
  explicit Storage(SIZE_T bytes):size(bytes),data(std::calloc(1,bytes),&std::free) { CHECK(bytes && data); }
  template<typename T> T handle() const { return {data.get()}; }
  void poison() { std::memset(data.get(),0xcd,size); }
  bool untouched() const {
    auto bytes=static_cast<const unsigned char*>(data.get());
    return std::all_of(bytes,bytes+size,[](unsigned char v){return v==0xcd;});
  }
};
struct Fixture {
  bool newer;
  unsigned readbacks=0;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device=storage.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_DEVICEFUNCS f0{};
  D3D10_1DDI_DEVICEFUNCS f1{};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core{};
  ComPtr<ID3D11DeviceContext> context;
  explicit Fixture(bool tenOne):newer(tenOne) {
    static_assert(std::is_same_v<decltype(f1.pfnCreateResource),PFND3D10DDI_CREATERESOURCE>);
    LUID luid{}; core.pfnSetErrorCb=error;
    HRESULT hr=newer ? VioGpuDxvkCreateDdiTestDevice10_1(&luid,device,{},&core,&f1)
      : VioGpuDxvkCreateDdiTestDevice(&luid,device,{},&core,&f0);
    CHECK(hr==S_OK); context=createdContext; createdContext.Reset(); CHECK(context);
  }
  template<typename Fn> decltype(auto) call(Fn&& fn) {
    if (newer) return fn(f1);
    return fn(f0);
  }
  ~Fixture() { context->ClearState(); call([&](auto& f){f.pfnDestroyDevice(device);}); ok(); }
};
struct Description {
  std::vector<D3D10DDI_MIPINFO> mips;
  D3D10DDIARG_CREATERESOURCE args{};
  Description(UINT edge,UINT count,UINT faces):mips(count) {
    for (UINT i=0;i<count;++i) {
      UINT w=std::max(1u,edge>>i);
      mips[i]={w,w,1,(w+15)&~15u,(w+15)&~15u,1};
    }
    args.pMipInfoList=mips.data(); args.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;
    args.Usage=D3D10_DDI_USAGE_DEFAULT; args.BindFlags=D3D10_DDI_BIND_SHADER_RESOURCE;
    args.Format=DXGI_FORMAT_R8G8B8A8_UNORM; args.SampleDesc.Count=1;
    args.MipLevels=count; args.ArraySize=faces;
  }
};
using Pixels=std::vector<std::vector<UINT>>;
static Pixels initial(UINT edge,UINT mips,UINT faces) {
  Pixels out(size_t(mips)*faces);
  for (UINT face=0;face<faces;++face)
    for (UINT mip=0;mip<mips;++mip) {
      const UINT width=std::max(1u,edge>>mip);
      auto& row=out[size_t(face)*mips+mip]; row.resize(size_t(width)*width);
      for (size_t i=0;i<row.size();++i)
        row[i]=0xff000000u|(face<<16)|(mip<<12)|static_cast<UINT>(i+1);
    }
  return out;
}
struct Resource {
  Fixture& owner;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  Resource(Fixture& f,const D3D10DDIARG_CREATERESOURCE& source,Pixels* pixels=nullptr)
    :owner(f),storage(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,&source);})),
     handle(storage.handle<D3D10DDI_HRESOURCE>()) {
    auto args=source; std::vector<D3D10_DDIARG_SUBRESOURCE_UP> upload;
    if (pixels) {
      CHECK(pixels->size()==size_t(args.ArraySize)*args.MipLevels);
      upload.resize(pixels->size());
      for (size_t i=0;i<upload.size();++i) {
        UINT width=args.pMipInfoList[i%args.MipLevels].TexelWidth;
        upload[i]={(*pixels)[i].data(),static_cast<UINT>(width*sizeof(UINT)),
          static_cast<UINT>((*pixels)[i].size()*sizeof(UINT))};
      }
      args.pInitialDataUP=upload.data();
    }
    f.call([&](auto& t){t.pfnCreateResource(f.device,&args,handle,{});}); ok();
  }
  ~Resource() { owner.call([&](auto& t){t.pfnDestroyResource(owner.device,handle);}); ok(); }
};
static void save(const std::string& path,const void* data,size_t bytes) {
  std::ofstream file(path,std::ios::binary); CHECK(file.is_open());
  file.write(static_cast<const char*>(data),static_cast<std::streamsize>(bytes));
  file.close(); CHECK(!file.fail());
}
static void readback(Fixture& f,Resource& source,const Description& d,const Pixels& expected) {
  auto stage=d.args; stage.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
  stage.Usage=D3D10_DDI_USAGE_STAGING; stage.MapFlags=D3D10_DDI_CPU_ACCESS_READ;
  stage.BindFlags=stage.MiscFlags=0; stage.pInitialDataUP=nullptr;
  Resource staging(f,stage);
  f.call([&](auto& t){t.pfnResourceCopy(f.device,staging.handle,source.handle);}); ok();
  const unsigned id=f.readbacks++;
  for (UINT sub=0;sub<expected.size();++sub) {
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    f.call([&](auto& t){t.pfnStagingResourceMap(f.device,staging.handle,sub,D3D10_DDI_MAP_READ,0,&mapped);}); ok();
    UINT edge=d.mips[sub%d.args.MipLevels].TexelWidth; CHECK(mapped.pData && mapped.RowPitch>=edge*sizeof(UINT));
    std::vector<UINT> observed(expected[sub].size());
    for (size_t i=0;i<observed.size();++i)
      std::memcpy(&observed[i],static_cast<const char*>(mapped.pData)+(i/edge)*mapped.RowPitch+(i%edge)*sizeof(UINT),sizeof(UINT));
    const std::string stem="cube-array-p"+std::to_string(f.newer)+"-r"+std::to_string(id)+"-s"+std::to_string(sub);
    const UINT metadata[]={static_cast<UINT>(f.newer),id,sub,sub/d.args.MipLevels,sub%d.args.MipLevels,edge,mapped.RowPitch,mapped.DepthPitch};
    // Retain actual and expected bytes before evaluating the first mismatch.
    save(stem+".metadata",metadata,sizeof(metadata));
    save(stem+".actual",observed.data(),observed.size()*sizeof(UINT));
    save(stem+".expected",expected[sub].data(),expected[sub].size()*sizeof(UINT));
    for (size_t i=0;i<observed.size();++i) {
      if (observed[i]!=expected[sub][i])
        std::fprintf(stderr,"cube-array mismatch profile=%u readback=%u face=%u mip=%u xy=%zu/%zu actual=%08x expected=%08x\n",
          static_cast<UINT>(f.newer),id,sub/d.args.MipLevels,sub%d.args.MipLevels,i%edge,i/edge,observed[i],expected[sub][i]);
      ++texels; CHECK(observed[i]==expected[sub][i]);
    }
    f.call([&](auto& t){t.pfnStagingResourceUnmap(f.device,staging.handle,sub);}); ok();
  }
}
static void inspect(Fixture& f,Resource& r,const Description& d) {
  Storage storage(f.call([&](auto& t){return t.pfnCalcPrivateShaderResourceViewSize(f.device,nullptr);}));
  auto handle=storage.handle<D3D10DDI_HSHADERRESOURCEVIEW>();
  if (f.newer) {
    D3D10_1DDIARG_CREATESHADERRESOURCEVIEW a{};
    a.hDrvResource=r.handle; a.Format=d.args.Format; a.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;
    a.TexCube.MostDetailedMip=1; a.TexCube.MipLevels=2;
    a.TexCube.First2DArrayFace=d.args.ArraySize>6 ? 6u : 0u;
    a.TexCube.NumCubes=d.args.ArraySize/6-(d.args.ArraySize>6 ? 1u : 0u);
    f.f1.pfnCreateShaderResourceView(f.device,&a,handle,{}); ok();
  } else {
    D3D10DDIARG_CREATESHADERRESOURCEVIEW a{};
    a.hDrvResource=r.handle; a.Format=d.args.Format; a.ResourceDimension=D3D10DDIRESOURCE_TEXTURECUBE;
    a.TexCube.MostDetailedMip=1; a.TexCube.MipLevels=2;
    f.f0.pfnCreateShaderResourceView(f.device,&a,handle,{}); ok();
  }
  f.call([&](auto& t){t.pfnPsSetShaderResources(f.device,0,1,&handle);}); ok();
  ComPtr<ID3D11ShaderResourceView> view; f.context->PSGetShaderResources(0,1,&view); CHECK(view);
  D3D11_SHADER_RESOURCE_VIEW_DESC v{}; view->GetDesc(&v);
  if (f.newer) {
    CHECK(v.ViewDimension==D3D11_SRV_DIMENSION_TEXTURECUBEARRAY);
    CHECK(v.TextureCubeArray.First2DArrayFace==(d.args.ArraySize>6 ? 6u : 0u));
    CHECK(v.TextureCubeArray.NumCubes==d.args.ArraySize/6-(d.args.ArraySize>6 ? 1u : 0u));
  } else CHECK(v.ViewDimension==D3D11_SRV_DIMENSION_TEXTURECUBE);
  ComPtr<ID3D11Resource> backend; view->GetResource(&backend);
  ComPtr<ID3D11Texture2D> texture; CHECK(SUCCEEDED(backend.As(&texture)));
  D3D11_TEXTURE2D_DESC shape{}; texture->GetDesc(&shape);
  CHECK(shape.ArraySize==d.args.ArraySize && shape.Width==7 && shape.Height==7 && shape.MipLevels==3);
  CHECK(shape.MiscFlags==D3D11_RESOURCE_MISC_TEXTURECUBE);
  D3D10DDI_HSHADERRESOURCEVIEW empty{};
  f.call([&](auto& t){t.pfnPsSetShaderResources(f.device,0,1,&empty);t.pfnDestroyShaderResourceView(f.device,handle);}); ok();
}
static void initialized(Fixture& f,UINT faces) {
  Description d(7,3,faces); d.args.Usage=D3D10_DDI_USAGE_IMMUTABLE;
  auto expected=initial(7,3,faces); Resource r(f,d.args,&expected);
  inspect(f,r,d); readback(f,r,d,expected); ++cases;
}
static void transfers(Fixture& f) {
  Description d(7,3,18); auto expected=initial(7,3,18); auto untouched=expected;
  Resource src(f,d.args,&untouched),dst(f,d.args,&expected);
  const UINT red[]={0xff0000ffu,0xff0000ffu,0xff0000ffu,0xff0000ffu};
  D3D10_DDI_BOX box{1,1,0,3,3,1};
  f.f1.pfnResourceUpdateSubresourceUP(f.device,dst.handle,19,&box,red,8,16); ok();
  for (UINT y=1;y<3;++y) for (UINT x=1;x<3;++x) expected[19][y*3+x]=red[0];
  D3D10_DDI_BOX from{1,2,0,3,4,1};
  f.f1.pfnResourceCopyRegion(f.device,dst.handle,51,4,3,0,src.handle,0,&from); ok();
  for (UINT y=0;y<2;++y) for (UINT x=0;x<2;++x) expected[51][(y+3)*7+x+4]=untouched[0][(y+2)*7+x+1];
  f.f1.pfnResourceUpdateSubresourceUP(f.device,dst.handle,54,nullptr,red,8,16); rejected();
  D3D10_DDI_BOX outside{6,0,0,8,1,1};
  f.f1.pfnResourceUpdateSubresourceUP(f.device,dst.handle,19,&outside,red,8,16); rejected();
  readback(f,dst,d,expected); ++cases;
}
static void malformed(Fixture& f) {
  Description d(7,3,6);
  Storage storage(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,&d.args);}));
  auto handle=storage.handle<D3D10DDI_HRESOURCE>();
  auto reject=[&](const D3D10DDIARG_CREATERESOURCE& a){
    storage.poison(); f.call([&](auto& t){t.pfnCreateResource(f.device,&a,handle,{});});
    rejected(); CHECK(storage.untouched());
  };
  for (UINT faces : {0u,1u,5u,7u,511u,512u,513u,UINT32_MAX}) { auto a=d.args; a.ArraySize=faces; reject(a); }
  if (!f.newer) for (UINT faces : {12u,18u,510u}) { auto a=d.args; a.ArraySize=faces; reject(a); }
  auto a=d.args; a.SampleDesc.Count=2; reject(a);
  a=d.args; a.SampleDesc.Quality=1; reject(a);
  a=d.args; a.MipLevels=4; reject(a);
  auto old=d.mips[1]; d.mips[1].TexelHeight=4; reject(d.args); d.mips[1]=old;
  if (f.newer) { old=d.mips[0]; d.mips[0].TexelWidth=8193; reject(d.args); d.mips[0]=old; }
  void* inaccessible=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS); CHECK(inaccessible);
  for (UINT faces : {0u,1u,7u,513u,UINT32_MAX}) {
    a=d.args; a.ArraySize=faces; a.pMipInfoList=static_cast<const D3D10DDI_MIPINFO*>(inaccessible);
    a.pInitialDataUP=static_cast<const D3D10_DDIARG_SUBRESOURCE_UP*>(inaccessible); reject(a);
  }
  CHECK(VirtualFree(inaccessible,0,MEM_RELEASE));
  // Reuse exactly the same failed private bytes repeatedly. No failed object
  // requires DestroyResource, and malformed inputs cannot reserve its address.
  for (UINT repeat=0;repeat<8;++repeat) {
    a=d.args; a.ArraySize=f.newer ? 12u : 6u;
    f.call([&](auto& t){t.pfnCreateResource(f.device,&a,handle,{});}); ok();
    f.call([&](auto& t){t.pfnDestroyResource(f.device,handle);}); ok();
    a.ArraySize=7; reject(a);
  }
  ++cases;
}
int main() {
  callerThread=GetCurrentThreadId();
  std::puts("BACKEND=WARP; production_DDI=true; VIOGPU=false; registration=false");
  { Fixture f(false); initialized(f,6); malformed(f); }
  { Fixture f(true); for (UINT faces:{6u,12u,18u}) initialized(f,faces); transfers(f); malformed(f); }
  std::printf("PASS cube-array resource\nprofiles=2\ncases=%u\nchecks=%u\ntexels=%u\nfailures=%u\n",cases,checks,texels,failures);
}
