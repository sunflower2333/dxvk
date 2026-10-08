// SPDX-License-Identifier: MIT
// Actual typed DDI MSAA array copies and independent public WARP references.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_result.h"
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, backendCalls, copies, noops, rejections, snapshots, observedBytes, scenes;
static HRESULT lastError=S_OK, apiResult=S_OK;
static DWORD callerThread;
static UINT profile, sampleCount, scene, phase, control=UINT(-1), subresource=UINT(-1);
static const char* step="startup";
static const LUID expectedLuid{0x57d0a63b,-46};
static ComPtr<ID3D11DeviceContext> createdContext;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"MSAA copy failure line=%d: %s checks=%u callbacks=%u DDI_HRESULT=%08lx API_HRESULT=%08lx step=%s profile=%u samples=%u scene=%u phase=%u control=%u subresource=%u snapshots=%u\n",__LINE__,#x,checks,callbacks,static_cast<unsigned long>(lastError),static_cast<unsigned long>(apiResult),step,profile,sampleCount,scene,phase,control,subresource,snapshots); std::exit(1); } } while (0)
static void ok() { CHECK(lastError==S_OK); }
static void apiOk(HRESULT value) { apiResult=value; CHECK(value==S_OK); }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime,HRESULT value) {
  CHECK(runtime.handle && GetCurrentThreadId()==callerThread && FAILED(value));
  ++callbacks; lastError=value;
}
HRESULT dxvk::umd::createDevice(const LUID& luid,D3D_FEATURE_LEVEL level,
    ID3D11Device** device,ID3D11DeviceContext** context,const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime && !std::memcmp(&luid,&expectedLuid,sizeof(luid))); ++backendCalls;
  level=implementationFeatureLevel(level);
  const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,
    D3D11_SDK_VERSION,device,nullptr,context);
  if (hr==S_OK) createdContext=*context;
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }
struct Storage {
  static constexpr UINT64 canary=0xa51a738a4d97eb63ull;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T size) : words((size+7)/8+2,0) { CHECK(size); words.front()=words.back()=canary; }
  void* data() { return words.data()+1; }
  void guards() const { CHECK(words.front()==canary && words.back()==canary); }
};
struct Fixture {
  const UINT profile;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  struct { D3D10_1DDI_DEVICEFUNCS table{}; UINT64 guard=Storage::canary; } out10;
  struct { D3D11DDI_DEVICEFUNCS table{}; UINT64 guard=Storage::canary; } out11;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Device> backend;
  template<typename Fn> decltype(auto) call(Fn&& fn) {
    if (profile) return fn(out11.table);
    return fn(out10.table);
  }
  explicit Fixture(UINT p) : profile(p) {
    step="device-create"; core10.pfnSetErrorCb=core11.pfnSetErrorCb=error;
    const HRESULT hr=profile ? VioGpuDxvkCreateDdiTestDevice11(&expectedLuid,device,{&core11},&core11,&out11.table,D3D_FEATURE_LEVEL_11_0)
      : VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid,device,{&core10},&core10,&out10.table);
    apiOk(hr); context=createdContext; createdContext.Reset(); CHECK(context);
    context->GetDevice(&backend); CHECK(backend); guards();
    call([&](auto& t) { CHECK(t.pfnResourceCopyRegion && t.pfnResourceConvertRegion && t.pfnResourceResolveSubresource); });
  }
  void guards() const { storage.guards(); CHECK(out10.guard==Storage::canary && out11.guard==Storage::canary); }
  void create(const D3D10DDIARG_CREATERESOURCE& a,D3D10DDI_HRESOURCE out) {
    if (!profile) out10.table.pfnCreateResource(device,&a,out,{});
    else {
      D3D11DDIARG_CREATERESOURCE n{}; n.pMipInfoList=a.pMipInfoList; n.pInitialDataUP=a.pInitialDataUP;
      n.ResourceDimension=a.ResourceDimension; n.Usage=a.Usage; n.BindFlags=a.BindFlags;
      n.MapFlags=a.MapFlags; n.MiscFlags=a.MiscFlags; n.Format=a.Format; n.SampleDesc=a.SampleDesc;
      n.MipLevels=a.MipLevels; n.ArraySize=a.ArraySize;
      out11.table.pfnCreateResource(device,&n,out,{});
    }
  }
  void bind(const D3D10DDI_HRENDERTARGETVIEW* views,UINT count) {
    if (profile) out11.table.pfnSetRenderTargets(device,views,count,0,{},nullptr,nullptr,count,0,count,0);
    else out10.table.pfnSetRenderTargets(device,views,count,0,{});
  }
  ~Fixture() {
    step="device-destroy"; context->ClearState(); context.Reset(); backend.Reset();
    call([&](auto& t) { t.pfnDestroyDevice(device); }); ok(); guards();
  }
};
using Colors=std::array<UINT,3>;
static const Colors sourceColors{{1,2,4}},destinationColors{{3,5,6}};
static std::array<unsigned char,4> color(UINT bits) {
  return {{static_cast<unsigned char>((bits&1) ? 255 : 0),static_cast<unsigned char>((bits&2) ? 255 : 0),
    static_cast<unsigned char>((bits&4) ? 255 : 0),255}};
}
using Descriptor=std::array<UINT,11>;
static Descriptor fields(const D3D11_TEXTURE2D_DESC& d) {
  return {{d.Width,d.Height,d.MipLevels,d.ArraySize,UINT(d.Format),d.SampleDesc.Count,d.SampleDesc.Quality,
    UINT(d.Usage),d.BindFlags,d.CPUAccessFlags,d.MiscFlags}};
}
struct Texture {
  Fixture& f;
  D3D11_TEXTURE2D_DESC desc{};
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  ComPtr<ID3D11Texture2D> reference;
  Descriptor nativeDescriptor{};
  bool inspected=false;
  Texture(Fixture& fixture,UINT samples,DXGI_FORMAT format=DXGI_FORMAT_R8G8B8A8_UNORM,
      UINT width=8,UINT height=4,D3D11_USAGE usage=D3D11_USAGE_DEFAULT,UINT bindings=D3D11_BIND_RENDER_TARGET)
    : f(fixture),storage(f.call([&](auto& t) { return t.pfnCalcPrivateResourceSize(f.device,nullptr); })),handle{storage.data()} {
    step="texture-create"; desc={width,height,1,3,format,{samples,0},usage,bindings,
      usage==D3D11_USAGE_STAGING ? D3D11_CPU_ACCESS_READ : 0u,0};
    D3D10DDI_MIPINFO mip{width,height,1,width,height,1};
    D3D10DDIARG_CREATERESOURCE a{}; a.pMipInfoList=&mip; a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
    a.Usage=static_cast<D3D10_DDI_RESOURCE_USAGE>(usage); a.BindFlags=bindings;
    a.MapFlags=((desc.CPUAccessFlags&D3D11_CPU_ACCESS_READ) ? D3D10_DDI_CPU_ACCESS_READ : 0u)
      | ((desc.CPUAccessFlags&D3D11_CPU_ACCESS_WRITE) ? D3D10_DDI_CPU_ACCESS_WRITE : 0u);
    a.Format=format; a.SampleDesc=desc.SampleDesc; a.MipLevels=1; a.ArraySize=3;
    f.create(a,handle); ok(); storage.guards();
    step="public-texture-create"; apiOk(f.backend->CreateTexture2D(&desc,nullptr,&reference));
    D3D11_TEXTURE2D_DESC actual{}; reference->GetDesc(&actual); CHECK(fields(actual)==fields(desc));
  }
  void clear(const Colors& colors) {
    for (UINT layer=0;layer<3;++layer) {
      subresource=layer; step="native-rtv-create";
      Storage viewStorage(f.call([&](auto& t) { return t.pfnCalcPrivateRenderTargetViewSize(f.device,nullptr); }));
      D3D10DDI_HRENDERTARGETVIEW view{viewStorage.data()};
      D3D10DDIARG_CREATERENDERTARGETVIEW a{}; a.hDrvResource=handle; a.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
      a.Format=desc.Format; a.Tex2D.FirstArraySlice=layer; a.Tex2D.ArraySize=1;
      f.call([&](auto& t) { t.pfnCreateRenderTargetView(f.device,&a,view,{}); }); ok();
      const auto bytes=color(colors[layer]); FLOAT rgba[]{bytes[0]/255.f,bytes[1]/255.f,bytes[2]/255.f,1.f};
      step="native-clear"; f.call([&](auto& t) { t.pfnClearRenderTargetView(f.device,view,rgba); }); ok();
      f.bind(&view,1); ok();
      ComPtr<ID3D11RenderTargetView> actualView; f.context->OMGetRenderTargets(1,&actualView,nullptr); CHECK(actualView);
      D3D11_RENDER_TARGET_VIEW_DESC actualViewDesc{}; actualView->GetDesc(&actualViewDesc);
      CHECK(actualViewDesc.Format==desc.Format && actualViewDesc.ViewDimension==(desc.SampleDesc.Count>1
        ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY));
      if (desc.SampleDesc.Count>1) CHECK(actualViewDesc.Texture2DMSArray.FirstArraySlice==layer && actualViewDesc.Texture2DMSArray.ArraySize==1);
      else CHECK(actualViewDesc.Texture2DArray.FirstArraySlice==layer && actualViewDesc.Texture2DArray.ArraySize==1);
      ComPtr<ID3D11Resource> actualResource; actualView->GetResource(&actualResource);
      ComPtr<ID3D11Texture2D> actualTexture; apiOk(actualResource.As(&actualTexture));
      D3D11_TEXTURE2D_DESC actual{}; actualTexture->GetDesc(&actual); nativeDescriptor=fields(actual);
      CHECK(nativeDescriptor==fields(desc)); inspected=true;
      f.bind(nullptr,0); ok();
      actualTexture.Reset(); actualResource.Reset(); actualView.Reset();
      step="native-rtv-destroy"; f.call([&](auto& t) { t.pfnDestroyRenderTargetView(f.device,view); }); ok(); viewStorage.guards();
      D3D11_RENDER_TARGET_VIEW_DESC publicDesc{}; publicDesc.Format=desc.Format;
      if (desc.SampleDesc.Count>1) { publicDesc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY; publicDesc.Texture2DMSArray={layer,1}; }
      else { publicDesc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY; publicDesc.Texture2DArray={0,layer,1}; }
      ComPtr<ID3D11RenderTargetView> publicView; step="public-rtv-create";
      apiOk(f.backend->CreateRenderTargetView(reference.Get(),&publicDesc,&publicView));
      step="public-clear"; f.context->ClearRenderTargetView(publicView.Get(),rgba);
    }
  }
  ~Texture() {
    step="texture-destroy"; reference.Reset(); f.call([&](auto& t) { t.pfnDestroyResource(f.device,handle); }); ok(); storage.guards();
  }
};
struct Readback {
  std::vector<unsigned char> native,publicBytes;
  std::array<UINT,3> nativeRows{},nativeDepths{},publicRows{},publicDepths{};
};
static Readback read(Texture& source,const Colors& expected) {
  CHECK(source.inspected); auto& f=source.f; const auto& d=source.desc;
  Texture resolved(f,1,d.Format,d.Width,d.Height,D3D11_USAGE_DEFAULT,0);
  Texture staging(f,1,d.Format,d.Width,d.Height,D3D11_USAGE_STAGING,0);
  for (UINT layer=0;layer<3;++layer) {
    subresource=layer; step="native-resolve";
    f.call([&](auto& t) { t.pfnResourceResolveSubresource(f.device,resolved.handle,layer,source.handle,layer,d.Format); }); ok();
    step="public-resolve"; f.context->ResolveSubresource(resolved.reference.Get(),layer,source.reference.Get(),layer,d.Format);
  }
  step="native-stage-copy"; f.call([&](auto& t) { t.pfnResourceCopy(f.device,staging.handle,resolved.handle); }); ok();
  step="public-stage-copy"; f.context->CopyResource(staging.reference.Get(),resolved.reference.Get());
  Readback result;
  for (UINT role=0;role<2;++role) for (UINT layer=0;layer<3;++layer) {
    subresource=layer; D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!role) {
      step="native-stage-map"; D3D10DDI_MAPPED_SUBRESOURCE n{};
      f.call([&](auto& t) { t.pfnStagingResourceMap(f.device,staging.handle,layer,D3D10_DDI_MAP_READ,0,&n); }); ok();
      mapped={n.pData,n.RowPitch,n.DepthPitch}; result.nativeRows[layer]=n.RowPitch; result.nativeDepths[layer]=n.DepthPitch;
    } else {
      step="public-stage-map"; apiOk(f.context->Map(staging.reference.Get(),layer,D3D11_MAP_READ,0,&mapped));
      result.publicRows[layer]=mapped.RowPitch; result.publicDepths[layer]=mapped.DepthPitch;
    }
    CHECK(mapped.pData && mapped.RowPitch>=d.Width*4); step="literal-pixel-compare";
    auto& output=role ? result.publicBytes : result.native;
    const auto pixel=color(expected[layer]);
    for (UINT y=0;y<d.Height;++y) {
      const auto* row=static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch;
      for (UINT x=0;x<d.Width;++x) CHECK(!std::memcmp(row+size_t(x)*4,pixel.data(),4));
      output.insert(output.end(),row,row+d.Width*4);
    }
    if (!role) { step="native-stage-unmap"; f.call([&](auto& t) { t.pfnStagingResourceUnmap(f.device,staging.handle,layer); }); ok(); }
    else { step="public-stage-unmap"; f.context->Unmap(staging.reference.Get(),layer); }
  }
  CHECK(result.native==result.publicBytes && result.native.size()==384); return result;
}
template<typename T,size_t N> static void array(std::ofstream& output,const std::array<T,N>& values) {
  output<<'['; for (size_t i=0;i<N;++i) { if (i) output<<','; output<<values[i]; } output<<']';
}
static void save(UINT id,const char* role,const std::vector<unsigned char>& bytes) {
  char name[80]; CHECK(std::snprintf(name,sizeof(name),"msaa-copy-%03u-%s.bin",id,role)>0);
  std::ofstream file(name,std::ios::binary); CHECK(file.is_open());
  file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())); file.close(); CHECK(!file.fail());
}
static void observe(Texture& texture,const Colors& expected,UINT observedPhase,UINT observedControl=UINT(-1)) {
  phase=observedPhase; control=observedControl;
  const UINT before=callbacks,id=snapshots; auto raw=read(texture,expected); CHECK(callbacks==before);
  save(id,"native",raw.native); save(id,"public",raw.publicBytes);
  D3D11_TEXTURE2D_DESC publicDesc{}; texture.reference->GetDesc(&publicDesc);
  char name[80]; CHECK(std::snprintf(name,sizeof(name),"msaa-copy-%03u-metadata.json",id)>0);
  std::ofstream output(name,std::ios::binary); CHECK(output.is_open());
  output<<"{\"id\":"<<id<<",\"profile\":"<<profile<<",\"scene\":"<<scene<<",\"phase\":"<<phase
    <<",\"control\":"<<control<<",\"callbacks\":"<<callbacks<<",\"copies\":"<<copies<<",\"noops\":"<<noops
    <<",\"rejections\":"<<rejections<<",\"native_descriptor\":"; array(output,texture.nativeDescriptor);
  output<<",\"public_descriptor\":"; array(output,fields(publicDesc));
  output<<",\"native_row_pitches\":"; array(output,raw.nativeRows); output<<",\"public_row_pitches\":"; array(output,raw.publicRows);
  output<<",\"native_depth_pitches\":"; array(output,raw.nativeDepths); output<<",\"public_depth_pitches\":"; array(output,raw.publicDepths);
  output<<",\"bytes_each_role\":"<<raw.native.size()<<",\"hardware_admission\":false,\"registration\":false}\n";
  output.close(); CHECK(!output.fail()); ++snapshots; observedBytes+=static_cast<UINT>(raw.native.size());
}
static void positive(Fixture& f,UINT samples,UINT operation,bool sameResource,bool cast) {
  sampleCount=samples; scene=scenes++; phase=0; control=UINT(-1);
  Texture source(f,samples),destination(f,samples,cast ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
  source.clear(sourceColors); destination.clear(destinationColors); observe(destination,destinationColors,0);
  const UINT target=sameResource ? 0 : 1;
  const auto handle=sameResource ? destination.handle : source.handle;
  step="native-full-copy";
  f.call([&](auto& t) {
    if (operation) t.pfnResourceConvertRegion(f.device,destination.handle,target,0,0,0,handle,2,nullptr);
    else t.pfnResourceCopyRegion(f.device,destination.handle,target,0,0,0,handle,2,nullptr);
  }); ok(); ++copies;
  step="public-full-copy";
  f.context->CopySubresourceRegion(destination.reference.Get(),target,0,0,0,
    sameResource ? destination.reference.Get() : source.reference.Get(),2,nullptr);
  Colors expected=destinationColors; expected[target]=sameResource ? destinationColors[2] : sourceColors[2];
  observe(destination,expected,1); observe(source,sourceColors,2);
}
static void negatives(Fixture& f,Fixture& foreign,UINT samples) {
  sampleCount=samples; scene=scenes++; phase=3; control=UINT(-1);
  Texture source(f,samples),destination(f,samples); source.clear(sourceColors); destination.clear(destinationColors);
  Texture single(f,1),differentSamples(f,samples==2 ? 4 : 2),wide(f,samples,DXGI_FORMAT_R8G8B8A8_UNORM,16),
    narrow(f,samples,DXGI_FORMAT_R8G8B8A8_UNORM,4),tall(f,samples,DXGI_FORMAT_R8G8B8A8_UNORM,8,8),
    shortTexture(f,samples,DXGI_FORMAT_R8G8B8A8_UNORM,8,2),otherFormat(f,samples,DXGI_FORMAT_R32_FLOAT),
    other(foreign,samples),depth(f,samples,DXGI_FORMAT_D32_FLOAT,8,4,D3D11_USAGE_DEFAULT,D3D11_BIND_DEPTH_STENCIL);
  auto reject=[&](auto&& fn) {
    ++control; const UINT currentControl=control,before=callbacks; step="native-rejection"; fn();
    CHECK(callbacks==before+1 && lastError==dxvk::umd::ddiResult(E_INVALIDARG)); lastError=S_OK; ++rejections;
    observe(destination,destinationColors,3,currentControl);
  };
  auto copy=[&](D3D10DDI_HRESOURCE from,UINT fromIndex,UINT toIndex,UINT x,UINT y,UINT z,const D3D10_DDI_BOX* box) {
    f.call([&](auto& t) { t.pfnResourceCopyRegion(f.device,destination.handle,toIndex,x,y,z,from,fromIndex,box); });
  };
  D3D10_DDI_BOX full{0,0,0,8,4,1},partial{0,0,0,4,2,1},negative{-1,0,0,8,4,1};
  reject([&] { copy(source.handle,0,0,0,0,0,&full); });
  reject([&] { copy(source.handle,0,0,0,0,0,&partial); });
  reject([&] { copy(source.handle,0,0,1,0,0,nullptr); });
  reject([&] { copy(source.handle,0,0,0,1,0,nullptr); });
  reject([&] { copy(source.handle,0,0,0,0,1,nullptr); });
  reject([&] { copy(source.handle,3,0,0,0,0,nullptr); });
  reject([&] { copy(source.handle,0,3,0,0,0,nullptr); });
  reject([&] { copy(destination.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(single.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(differentSamples.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(wide.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(narrow.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(tall.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(shortTexture.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(otherFormat.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy(other.handle,0,0,0,0,0,nullptr); });
  reject([&] { copy({},0,0,0,0,0,nullptr); });
  reject([&] { f.call([&](auto& t) { t.pfnResourceConvertRegion(f.device,{},0,0,0,0,source.handle,0,nullptr); }); });
  reject([&] { copy(source.handle,0,0,0,0,0,&negative); });
  const std::array<UINT,32> bytes{};
  reject([&] { f.call([&](auto& t) { t.pfnResourceUpdateSubresourceUP(f.device,destination.handle,0,nullptr,bytes.data(),32,128); }); });
  reject([&] { copy(depth.handle,0,0,0,0,0,nullptr); });
  const std::array<D3D10_DDI_BOX,6> empty{{{2,0,0,1,1,1},{0,2,0,1,1,1},{0,0,2,1,1,1},
    {1,0,0,1,1,1},{0,1,0,1,1,1},{0,0,1,1,1,1}}};
  for (const auto& box:empty) {
    ++control; const UINT currentControl=control,before=callbacks; step="native-empty-noop";
    f.call([&](auto& t) { t.pfnResourceConvertRegion(f.device,destination.handle,0,0,0,0,source.handle,0,&box); }); ok();
    CHECK(callbacks==before); ++noops; observe(destination,destinationColors,4,currentControl);
  }
}
int main() {
  callerThread=GetCurrentThreadId();
  for (profile=0;profile<2;++profile) {
    Fixture f(profile),foreign(profile);
    for (UINT samples : {2u,4u}) for (UINT operation=0;operation<2;++operation)
      for (UINT same=0;same<2;++same) for (UINT cast=0;cast<2;++cast)
        positive(f,samples,operation,same!=0,cast!=0);
    for (UINT samples : {2u,4u}) negatives(f,foreign,samples);
  }
  CHECK(backendCalls==4 && scenes==36 && copies==32 && rejections==84 && callbacks==84 && noops==24
    && snapshots==204 && observedBytes==78336);
  std::printf("MSAA color copy PASS checks=%u profiles=2 scenes=36 copies=32 snapshots=204 bytes=78336 rejections=84 noops=24 raw_files=612 hardware_admission=0\n",checks);
}
