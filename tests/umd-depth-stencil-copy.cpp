// SPDX-License-Identifier: MIT
// Actual historical DDIs and a separate public WARP resource pair. No GPU
// adapter/ordinary-system-runtime admission is inferred from this fixture.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_result.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>
using Microsoft::WRL::ComPtr;
static UINT checks, callbacks, copies, rejections, noops, snapshots, pixels, bytes;
static UINT profile, family, caseIndex;
static const char* phase="startup";
static HRESULT lastError=S_OK, apiResult=S_OK;
static DWORD caller;
static ComPtr<ID3D11DeviceContext> created;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"Depth stencil copy failure line=%d expression=%s phase=%s profile=%u family=%u case=%u ddi=%08lx public=%08lx\n",__LINE__,#x,phase,profile,family,caseIndex,static_cast<unsigned long>(lastError),static_cast<unsigned long>(apiResult)); std::exit(1); } } while(0)
static void ok() { CHECK(lastError==S_OK); }
static void api(HRESULT hr) { apiResult=hr; CHECK(hr==S_OK); }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime,HRESULT hr) {
  CHECK(runtime.handle&&GetCurrentThreadId()==caller&&FAILED(hr)); ++callbacks;lastError=hr;
}
HRESULT dxvk::umd::createDevice(const LUID&,D3D_FEATURE_LEVEL logical,ID3D11Device** device,
    ID3D11DeviceContext** context,const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime&&logical==(profile==1?D3D_FEATURE_LEVEL_10_1:D3D_FEATURE_LEVEL_10_0));
  const D3D_FEATURE_LEVEL actual=D3D_FEATURE_LEVEL_11_0;
  const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&actual,1,D3D11_SDK_VERSION,device,nullptr,context);
  if(hr==S_OK)created=*context; return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush();return S_OK; }
struct Storage {
  static constexpr UINT64 guard=0x73649215baced037ull;
  std::vector<UINT64> data;
  explicit Storage(SIZE_T size):data((size+7)/8+2,0) { CHECK(size);data.front()=data.back()=guard; }
  void* pointer() { return data.data()+1; }
  void check() const { CHECK(data.front()==guard&&data.back()==guard); }
};
struct Fixture {
  Storage memory{VioGpuDxvkPrivateDeviceSize()};D3D10DDI_HDEVICE device{memory.pointer()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  D3D10DDI_DEVICEFUNCS t0{};D3D10_1DDI_DEVICEFUNCS t1{};D3D11DDI_DEVICEFUNCS t11{};
  ComPtr<ID3D11Device> backend;ComPtr<ID3D11DeviceContext> context;
  template<class Function> decltype(auto) call(Function&& f) {
    if(profile==0)return f(t0); if(profile==1)return f(t1); return f(t11);
  }
  Fixture() {
    phase="native-device-create";core10.pfnSetErrorCb=core11.pfnSetErrorCb=error;LUID luid{};HRESULT hr;
    if(profile==0)hr=VioGpuDxvkCreateDdiTestDevice(&luid,device,{&core10},&core10,&t0);
    else if(profile==1)hr=VioGpuDxvkCreateDdiTestDevice10_1(&luid,device,{&core10},&core10,&t1);
    // D3D11 permits DS destinations even at FL10_0. That differs from the
    // D3D10.0 table at the same logical feature level and must not be inferred
    // from the broader WARP implementation feature level used underneath.
    else hr=VioGpuDxvkCreateDdiTestDevice11(&luid,device,{&core11},&core11,&t11,D3D_FEATURE_LEVEL_10_0);
    api(hr);CHECK(created);context=created;created.Reset();context->GetDevice(&backend);CHECK(backend);ok();
  }
  void create(const D3D10DDIARG_CREATERESOURCE& a,D3D10DDI_HRESOURCE handle) {
    if(profile!=2)call([&](auto& t) { if constexpr(!std::is_same_v<std::decay_t<decltype(t)>,D3D11DDI_DEVICEFUNCS>)t.pfnCreateResource(device,&a,handle,{}); });
    else {
      D3D11DDIARG_CREATERESOURCE n{};n.pMipInfoList=a.pMipInfoList;n.pInitialDataUP=a.pInitialDataUP;
      n.ResourceDimension=a.ResourceDimension;n.Usage=a.Usage;n.BindFlags=a.BindFlags;n.MapFlags=a.MapFlags;
      n.Format=a.Format;n.SampleDesc=a.SampleDesc;n.MipLevels=a.MipLevels;n.ArraySize=a.ArraySize;
      t11.pfnCreateResource(device,&n,handle,{});
    }
  }
  ~Fixture() { context->ClearState();context.Reset();backend.Reset();call([&](auto& t){t.pfnDestroyDevice(device);});ok();memory.check(); }
};
using Plane=std::vector<unsigned char>;
using Image=std::vector<Plane>;
static DXGI_FORMAT format(UINT f) { return f?DXGI_FORMAT_R32G8X24_TYPELESS:DXGI_FORMAT_R24G8_TYPELESS; }
static Image initial(UINT f,UINT seed,UINT levels=2,UINT arrays=2) {
  Image image(levels*arrays);const UINT stride=f?8u:4u;
  for(UINT sub=0;sub<levels*arrays;++sub) {
    const UINT w=std::max(1u,7u>>(sub%levels)),h=std::max(1u,5u>>(sub%levels));
    auto& out=image[sub];out.resize(w*h*stride);
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x) {
      const UINT stencil=(seed+sub*13+x*7+y*11)&255u;
      if(!f) {
        const UINT packed=(0x200000u+seed*1024+sub*512+x*17+y*31)|(stencil<<24);
        std::memcpy(out.data()+(y*w+x)*4,&packed,4);
      } else {
        const float depth=0.25f+float((seed+sub*3+x+y*5)&127u)/256.0f;
        std::memcpy(out.data()+(y*w+x)*8,&depth,4);
        const UINT packedStencil=stencil; // retain all 64 bits, including zero X24.
        std::memcpy(out.data()+(y*w+x)*8+4,&packedStencil,4);
      }
    }
  }
  return image;
}
struct Texture {
  Fixture& f;const UINT kind,seed,levels,arrays,stride,samples;const bool staging;
  Storage memory;D3D10DDI_HRESOURCE handle;ComPtr<ID3D11Texture2D> reference;
  Image expected,initialMemory,originalMemory;
  Texture(Fixture& owner,UINT fmt,UINT value,bool stage=false,bool immutable=false,UINT count=1,bool initialize=true)
    : f(owner),kind(fmt),seed(value),levels(count==1?2u:1u),arrays(count==1?2u:1u),stride(fmt?8u:4u),samples(count),staging(stage),
      memory(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,nullptr);})),handle{memory.pointer()},expected(initial(fmt,value,levels,arrays)),initialMemory(expected) {
    phase="native-resource-create";std::vector<D3D10DDI_MIPINFO> mip(levels);
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> data(levels*arrays);std::vector<D3D11_SUBRESOURCE_DATA> publicData(levels*arrays);
    for(UINT level=0;level<levels;++level) {
      const UINT w=std::max(1u,7u>>level),h=std::max(1u,5u>>level);mip[level]={w,h,1,w,h,1};
      for(UINT layer=0;layer<arrays;++layer) {
        const UINT sub=level+layer*levels,row=w*stride,pitch=row+16;
        initialMemory[sub].assign(size_t(pitch)*h+16,0xa5);
        for(UINT y=0;y<h;++y)std::memcpy(initialMemory[sub].data()+size_t(y)*pitch,expected[sub].data()+size_t(y)*row,row);
        data[sub]={initialMemory[sub].data(),pitch,pitch*h};publicData[sub]={initialMemory[sub].data(),pitch,pitch*h};
      }
    }
    originalMemory=initialMemory;const bool supplied=initialize&&count==1;
    D3D10DDIARG_CREATERESOURCE a{};a.pMipInfoList=mip.data();a.pInitialDataUP=supplied?data.data():nullptr;
    a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;a.Format=format(kind);a.SampleDesc.Count=count;
    a.Usage=stage?D3D10_DDI_USAGE_STAGING:immutable?D3D10_DDI_USAGE_IMMUTABLE:D3D10_DDI_USAGE_DEFAULT;
    a.BindFlags=stage?0:immutable?D3D10_DDI_BIND_SHADER_RESOURCE:D3D10_DDI_BIND_DEPTH_STENCIL;
    a.MapFlags=stage?D3D10_DDI_CPU_ACCESS_READ:0;a.MipLevels=levels;a.ArraySize=arrays;
    f.create(a,handle);ok();memory.check();CHECK(initialMemory==originalMemory);
    phase="public-resource-create";
    const D3D11_TEXTURE2D_DESC desc{7,5,levels,arrays,format(kind),{count,0},static_cast<D3D11_USAGE>(a.Usage),
      a.BindFlags,stage?D3D11_CPU_ACCESS_READ:0u,0};
    api(f.backend->CreateTexture2D(&desc,supplied?publicData.data():nullptr,&reference));
  }
  ~Texture() { CHECK(initialMemory==originalMemory);reference.Reset();f.call([&](auto& t){t.pfnDestroyResource(f.device,handle);});ok();memory.check(); }
};
static void retain(const char* name,const void* data,size_t size) {
  CHECK(size<=MAXDWORD);HANDLE file=CreateFileA(name,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  CHECK(file!=INVALID_HANDLE_VALUE);DWORD done=0;CHECK(WriteFile(file,data,DWORD(size),&done,nullptr)&&done==size);CHECK(CloseHandle(file));
}
static void snapshot(Texture& source,UINT which) {
  phase="snapshot";caseIndex=which;CHECK(source.samples==1&&source.levels==2&&source.arrays==2);
  Texture stage(source.f,source.kind,0,true,false,1,false);
  source.f.call([&](auto& t){t.pfnResourceCopy(source.f.device,stage.handle,source.handle);});ok();
  source.f.context->CopyResource(stage.reference.Get(),source.reference.Get());
  Plane native,publicBytes;std::array<UINT,20> metadata{{7,5,2,2,profile,family,which,UINT(format(source.kind)),source.stride,4,source.seed,0}};
  for(UINT sub=0;sub<4;++sub) {
    const UINT w=std::max(1u,7u>>(sub%2)),h=std::max(1u,5u>>(sub%2)),row=w*source.stride;
    D3D10DDI_MAPPED_SUBRESOURCE n{};D3D11_MAPPED_SUBRESOURCE p{};
    source.f.call([&](auto& t){t.pfnStagingResourceMap(source.f.device,stage.handle,sub,D3D10_DDI_MAP_READ,0,&n);});ok();
    api(source.f.context->Map(stage.reference.Get(),sub,D3D11_MAP_READ,0,&p));
    CHECK(n.pData&&p.pData&&n.RowPitch>=row&&p.RowPitch>=row);metadata[12+sub]=n.RowPitch;metadata[16+sub]=p.RowPitch;
    for(UINT y=0;y<h;++y) {
      const auto* np=static_cast<const unsigned char*>(n.pData)+size_t(y)*n.RowPitch;
      const auto* pp=static_cast<const unsigned char*>(p.pData)+size_t(y)*p.RowPitch;
      CHECK(std::memcmp(np,source.expected[sub].data()+size_t(y)*row,row)==0);
      CHECK(std::memcmp(pp,source.expected[sub].data()+size_t(y)*row,row)==0);
      native.insert(native.end(),np,np+row);publicBytes.insert(publicBytes.end(),pp,pp+row);
    }
    source.f.call([&](auto& t){t.pfnStagingResourceUnmap(source.f.device,stage.handle,sub);});ok();
    source.f.context->Unmap(stage.reference.Get(),sub);pixels+=w*h;
  }
  api(source.f.backend->GetDeviceRemovedReason());CHECK(native==publicBytes);char name[160];
  CHECK(std::snprintf(name,sizeof(name),"depth-stencil-copy-%u-%u-%u.native.bin",profile,family,which)>0);retain(name,native.data(),native.size());
  CHECK(std::snprintf(name,sizeof(name),"depth-stencil-copy-%u-%u-%u.public.bin",profile,family,which)>0);retain(name,publicBytes.data(),publicBytes.size());
  CHECK(std::snprintf(name,sizeof(name),"depth-stencil-copy-%u-%u-%u.metadata.u32.bin",profile,family,which)>0);retain(name,metadata.data(),sizeof(metadata));
  ++snapshots;bytes+=UINT(native.size());CHECK(source.initialMemory==source.originalMemory);
}
static void copy(Texture& dst,UINT dstSub,Texture& src,UINT srcSub) {
  phase="native-region-copy";
  dst.f.call([&](auto& t){t.pfnResourceCopyRegion(dst.f.device,dst.handle,dstSub,0,0,0,src.handle,srcSub,nullptr);});ok();
  phase="public-region-copy";dst.f.context->CopySubresourceRegion(dst.reference.Get(),dstSub,0,0,0,src.reference.Get(),srcSub,nullptr);
  api(dst.f.backend->GetDeviceRemovedReason());dst.expected[dstSub]=src.expected[srcSub];++copies;
}
template<class Function>static void reject(Function&& fn) {
  phase="invalid-DDI-control";CHECK(lastError==S_OK);const UINT before=callbacks;fn();
  CHECK(lastError==E_INVALIDARG&&callbacks==before+1);lastError=S_OK;++rejections;
}
static void scene(Fixture& f) {
  Texture src(f,family,17),dst(f,family,93,profile==0),other(f,1-family,67,true),immutable(f,family,107,false,true),msaa(f,family,0,false,false,2,false);
  snapshot(src,0);snapshot(dst,1);copy(dst,2,src,0);snapshot(dst,2);copy(dst,3,src,1);snapshot(dst,3);
  const D3D10_DDI_BOX full{0,0,0,7,5,1},part{0,0,0,3,2,1};
  auto call=[&](Texture& d,UINT ds,UINT x,UINT y,UINT z,Texture& s,UINT ss,const D3D10_DDI_BOX* box) {
    f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,d.handle,ds,x,y,z,s.handle,ss,box);});
  };
  reject([&]{call(dst,0,0,0,0,src,0,&full);});reject([&]{call(dst,0,0,0,0,src,0,&part);});
  reject([&]{call(dst,0,1,0,0,src,0,nullptr);});reject([&]{call(dst,0,0,1,0,src,0,nullptr);});reject([&]{call(dst,0,0,0,1,src,0,nullptr);});
  reject([&]{call(dst,0,0,0,0,src,4,nullptr);});reject([&]{call(dst,4,0,0,0,src,0,nullptr);});
  reject([&]{call(src,0,0,0,0,src,0,nullptr);});reject([&]{call(other,0,0,0,0,src,0,nullptr);});
  reject([&]{call(dst,0,0,0,0,msaa,0,nullptr);});reject([&]{call(dst,1,0,0,0,src,0,nullptr);});
  reject([&]{call(immutable,0,0,0,0,src,0,nullptr);});
  for(const D3D10_DDI_BOX& empty:{D3D10_DDI_BOX{0,0,0,0,5,1},D3D10_DDI_BOX{4,0,0,3,5,1}}) {
    const UINT before=callbacks;call(dst,0,UINT(-1),0,0,src,UINT(-1),&empty);ok();CHECK(callbacks==before);++noops;
  }
  snapshot(dst,4);snapshot(other,6);snapshot(immutable,7);
  if(profile==0) { Texture depthDestination(f,family,131);reject([&]{call(depthDestination,2,0,0,0,src,0,nullptr);});snapshot(depthDestination,8); }
  else copy(src,3,src,1); // same actual resource, different mip/array subresource.
  snapshot(src,5);
}
int main() {
  caller=GetCurrentThreadId();
  for(profile=0;profile<3;++profile) { Fixture f;for(family=0;family<2;++family)scene(f); }
  CHECK(copies==16&&rejections==74&&noops==12&&snapshots==50&&pixels==4100&&bytes==24600);
  std::printf("Depth stencil regional copy PASS checks=%u profiles=3 families=2 copies=16 rejections=74 noops=12 snapshots=50 pixels=4100 bytes_each_native_public=24600 raw_files=150 hardware_admission=0\n",checks);
}
