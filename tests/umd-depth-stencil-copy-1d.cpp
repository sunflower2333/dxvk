// SPDX-License-Identifier: MIT
// Real historical DDIs with separate public-WARP Texture1D copy controls.
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
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"Depth stencil 1D copy failure line=%d expression=%s phase=%s profile=%u family=%u case=%u ddi=%08lx public=%08lx\n",__LINE__,#x,phase,profile,family,caseIndex,static_cast<unsigned long>(lastError),static_cast<unsigned long>(apiResult)); std::exit(1); } } while(0)
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
enum class Role { Depth, Staging, Color, Immutable };
static DXGI_FORMAT depthFormat(UINT f) { return f?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D16_UNORM; }
static DXGI_FORMAT storageFormat(UINT f) { return f?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_R16_TYPELESS; }
static Image initial(UINT f,UINT seed) {
  Image image(4);const UINT stride=f?4u:2u;
  for(UINT sub=0;sub<4;++sub) {
    const UINT w=sub%2?3u:7u;auto& out=image[sub];out.resize(w*stride);
    for(UINT x=0;x<w;++x) {
      if(!f) {
        const unsigned short depth=static_cast<unsigned short>(0x2000u+seed*64+sub*31+x*17);
        std::memcpy(out.data()+x*2,&depth,2);
      } else {
        const float depth=0.25f+float((seed+sub*3+x)&127u)/256.0f;
        std::memcpy(out.data()+x*4,&depth,4);
      }
    }
  }
  return image;
}
struct Texture {
  Fixture& f;const UINT kind,seed,stride;const Role role;const DXGI_FORMAT format;
  Storage memory;D3D10DDI_HRESOURCE handle;ComPtr<ID3D11Texture1D> reference;
  Image expected,initialMemory,originalMemory;
  Texture(Fixture& owner,UINT fmt,UINT value,Role use=Role::Depth,bool initialize=true)
    :f(owner),kind(fmt),seed(value),stride(fmt?4u:2u),role(use),format(use==Role::Depth?depthFormat(fmt):storageFormat(fmt)),
     memory(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,nullptr);})),handle{memory.pointer()},
     expected(initial(fmt,value)),initialMemory(expected) {
    phase="native-resource-create";std::array<D3D10DDI_MIPINFO,2> mip{{{7,1,1,7,1,1},{3,1,1,3,1,1}}};
    std::array<D3D10_DDIARG_SUBRESOURCE_UP,4> data{};std::array<D3D11_SUBRESOURCE_DATA,4> publicData{};
    for(UINT sub=0;sub<4;++sub) {
      const UINT row=(sub%2?3u:7u)*stride,pitch=row+16;
      initialMemory[sub].assign(row+32,0xa5);std::memcpy(initialMemory[sub].data()+16,expected[sub].data(),row);
      data[sub]={initialMemory[sub].data()+16,pitch,pitch};publicData[sub]={initialMemory[sub].data()+16,pitch,pitch};
    }
    originalMemory=initialMemory;
    D3D10DDIARG_CREATERESOURCE a{};a.pMipInfoList=mip.data();a.pInitialDataUP=initialize?data.data():nullptr;
    a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE1D;a.Format=format;a.SampleDesc.Count=1;a.MipLevels=2;a.ArraySize=2;
    a.Usage=use==Role::Staging?D3D10_DDI_USAGE_STAGING:use==Role::Immutable?D3D10_DDI_USAGE_IMMUTABLE:D3D10_DDI_USAGE_DEFAULT;
    a.BindFlags=use==Role::Depth?D3D10_DDI_BIND_DEPTH_STENCIL:use==Role::Staging?0:D3D10_DDI_BIND_SHADER_RESOURCE;
    a.MapFlags=use==Role::Staging?D3D10_DDI_CPU_ACCESS_READ:0;
    f.create(a,handle);ok();memory.check();CHECK(initialMemory==originalMemory);
    phase="public-resource-create";const D3D11_TEXTURE1D_DESC desc{7,2,2,format,static_cast<D3D11_USAGE>(a.Usage),a.BindFlags,use==Role::Staging?D3D11_CPU_ACCESS_READ:0u,0};
    api(f.backend->CreateTexture1D(&desc,initialize?publicData.data():nullptr,&reference));
  }
  ~Texture() { CHECK(initialMemory==originalMemory);reference.Reset();f.call([&](auto& t){t.pfnDestroyResource(f.device,handle);});ok();memory.check(); }
};
static void retain(const char* name,const void* data,size_t size) {
  CHECK(size<=MAXDWORD);HANDLE file=CreateFileA(name,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  CHECK(file!=INVALID_HANDLE_VALUE);DWORD done=0;CHECK(WriteFile(file,data,DWORD(size),&done,nullptr)&&done==size);CHECK(CloseHandle(file));
}
static void snapshot(Texture& source,UINT which) {
  phase="snapshot";caseIndex=which;Texture stage(source.f,source.kind,0,Role::Staging,false);
  source.f.call([&](auto& t){t.pfnResourceCopy(source.f.device,stage.handle,source.handle);});ok();
  source.f.context->CopyResource(stage.reference.Get(),source.reference.Get());
  Plane native,publicBytes;std::array<UINT,20> metadata{{7,1,2,2,profile,family,which,UINT(source.format),source.stride,4,source.seed,UINT(storageFormat(source.kind))}};
  for(UINT sub=0;sub<4;++sub) {
    const UINT w=sub%2?3u:7u,row=w*source.stride;D3D10DDI_MAPPED_SUBRESOURCE n{};D3D11_MAPPED_SUBRESOURCE p{};
    source.f.call([&](auto& t){t.pfnStagingResourceMap(source.f.device,stage.handle,sub,D3D10_DDI_MAP_READ,0,&n);});ok();
    api(source.f.context->Map(stage.reference.Get(),sub,D3D11_MAP_READ,0,&p));CHECK(n.pData&&p.pData&&n.RowPitch>=row&&p.RowPitch>=row);
    metadata[12+sub]=n.RowPitch;metadata[16+sub]=p.RowPitch;
    const auto* np=static_cast<const unsigned char*>(n.pData);const auto* pp=static_cast<const unsigned char*>(p.pData);
    CHECK(std::memcmp(np,source.expected[sub].data(),row)==0);CHECK(std::memcmp(pp,source.expected[sub].data(),row)==0);
    native.insert(native.end(),np,np+row);publicBytes.insert(publicBytes.end(),pp,pp+row);
    source.f.call([&](auto& t){t.pfnStagingResourceUnmap(source.f.device,stage.handle,sub);});ok();source.f.context->Unmap(stage.reference.Get(),sub);pixels+=w;
  }
  api(source.f.backend->GetDeviceRemovedReason());char name[160];
  CHECK(std::snprintf(name,sizeof(name),"depth-stencil-copy-1d-%u-%u-%u.native.bin",profile,family,which)>0);retain(name,native.data(),native.size());
  CHECK(std::snprintf(name,sizeof(name),"depth-stencil-copy-1d-%u-%u-%u.public.bin",profile,family,which)>0);retain(name,publicBytes.data(),publicBytes.size());
  CHECK(std::snprintf(name,sizeof(name),"depth-stencil-copy-1d-%u-%u-%u.metadata.u32.bin",profile,family,which)>0);retain(name,metadata.data(),sizeof(metadata));
  ++snapshots;bytes+=UINT(native.size());CHECK(source.initialMemory==source.originalMemory);
}
static void copy(Texture& dst,UINT dstSub,Texture& src,UINT srcSub) {
  phase="native-region-copy";dst.f.call([&](auto& t){t.pfnResourceCopyRegion(dst.f.device,dst.handle,dstSub,0,0,0,src.handle,srcSub,nullptr);});ok();
  phase="public-region-copy";dst.f.context->CopySubresourceRegion(dst.reference.Get(),dstSub,0,0,0,src.reference.Get(),srcSub,nullptr);
  api(dst.f.backend->GetDeviceRemovedReason());dst.expected[dstSub]=src.expected[srcSub];++copies;
}
template<class Function>static void reject(Function&& fn) {
  phase="invalid-DDI-control";CHECK(lastError==S_OK);const UINT before=callbacks;fn();CHECK(lastError==E_INVALIDARG&&callbacks==before+1);lastError=S_OK;++rejections;
}
static void scene(Fixture& f) {
  Texture src(f,family,17),dst(f,family,93,profile==0?Role::Staging:Role::Depth),other(f,1-family,67,Role::Staging),immutable(f,family,107,Role::Immutable);
  snapshot(src,0);snapshot(dst,1);copy(dst,2,src,0);snapshot(dst,2);copy(dst,3,src,1);snapshot(dst,3);
  const D3D10_DDI_BOX full{0,0,0,7,1,1},part{0,0,0,3,1,1};
  auto call=[&](Texture& d,UINT ds,UINT x,UINT y,UINT z,Texture& s,UINT ss,const D3D10_DDI_BOX* box) {
    f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,d.handle,ds,x,y,z,s.handle,ss,box);});
  };
  reject([&]{call(dst,0,0,0,0,src,0,&full);});reject([&]{call(dst,0,0,0,0,src,0,&part);});
  reject([&]{call(dst,0,1,0,0,src,0,nullptr);});reject([&]{call(dst,0,0,1,0,src,0,nullptr);});reject([&]{call(dst,0,0,0,1,src,0,nullptr);});
  reject([&]{call(dst,0,0,0,0,src,4,nullptr);});reject([&]{call(dst,4,0,0,0,src,0,nullptr);});reject([&]{call(src,0,0,0,0,src,0,nullptr);});
  reject([&]{call(other,0,0,0,0,src,0,nullptr);});reject([&]{call(dst,1,0,0,0,src,0,nullptr);});reject([&]{call(immutable,0,0,0,0,src,0,nullptr);});
  for(const D3D10_DDI_BOX& empty:{D3D10_DDI_BOX{0,0,0,0,1,1},D3D10_DDI_BOX{4,0,0,3,1,1}}) {
    const UINT before=callbacks;call(dst,0,UINT(-1),0,0,src,UINT(-1),&empty);ok();CHECK(callbacks==before);++noops;
  }
  snapshot(dst,4);snapshot(other,6);snapshot(immutable,7);
  if(profile==0) { Texture depthDestination(f,family,131);reject([&]{call(depthDestination,2,0,0,0,src,0,nullptr);});snapshot(depthDestination,8); }
  else copy(src,3,src,1);
  snapshot(src,5);
  // R16/R32 typeless color resources have ordinary boxed-copy semantics.
  // Belonging to a depth-capable family must not classify them as DS-bound.
  Texture colorSource(f,family,71,Role::Color),colorDestination(f,family,47,Role::Color);snapshot(colorDestination,9);
  const D3D10_DDI_BOX colorBox{1,0,0,4,1,1};const D3D11_BOX publicBox{1,0,0,4,1,1};
  call(colorDestination,2,2,0,0,colorSource,0,&colorBox);ok();
  f.context->CopySubresourceRegion(colorDestination.reference.Get(),2,2,0,0,colorSource.reference.Get(),0,&publicBox);api(f.backend->GetDeviceRemovedReason());
  std::memcpy(colorDestination.expected[2].data()+2*colorDestination.stride,colorSource.expected[0].data()+colorSource.stride,3*colorSource.stride);++copies;
  snapshot(colorDestination,10);snapshot(colorSource,11);
}
int main() {
  caller=GetCurrentThreadId();for(profile=0;profile<3;++profile) { Fixture f;for(family=0;family<2;++family)scene(f); }
  CHECK(copies==22&&rejections==68&&noops==12&&snapshots==68&&pixels==1360&&bytes==4080);
  std::printf("Depth stencil 1D regional copy PASS checks=%u profiles=3 families=2 copies=22 rejections=68 noops=12 snapshots=68 pixels=1360 bytes_each_native_public=4080 raw_files=204 hardware_admission=0\n",checks);
}
