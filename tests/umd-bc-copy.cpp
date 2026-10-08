// SPDX-License-Identifier: MIT
// Source-linked real DDI and independent public WARP bit-copy readbacks.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_result.h"
#include "../src/umd/umd_transfer_format.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
static unsigned checks, callbacks, backendCalls, readbacks, observedBytes, rejections;
static HRESULT lastError = S_OK;
static HRESULT apiResult = S_OK;
static DWORD callerThread;
static const char* phase = "startup";
static const char* submission = "none";
static HRESULT deviceRemoved = S_OK;
static UINT profile, kind, sourceFormat, destinationFormat, operation, control = UINT(-1), subresource = UINT(-1);
static const LUID expectedLuid{0x187bb593,-78};
static ComPtr<ID3D11DeviceContext> createdContext;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"BC regional copy failure line=%d: %s checks=%u callbacks=%u HRESULT=%08lx api_HRESULT=%08lx phase=%s profile=%u kind=%u source_format=%u destination_format=%u operation=%u control=%u subresource=%u observations=%u rejections=%u submission=%s device_removed_checkpoint=%08lx\n",__LINE__,#x,checks,callbacks,static_cast<unsigned long>(lastError),static_cast<unsigned long>(apiResult),phase,profile,kind,sourceFormat,destinationFormat,operation,control,subresource,readbacks,rejections,submission,static_cast<unsigned long>(deviceRemoved)); std::exit(1); } } while (0)
struct Phase {
  const char* previous = phase;
  const UINT previousSubresource = subresource;
  explicit Phase(const char* name) { phase=name; subresource=UINT(-1); }
  ~Phase() { phase=previous; subresource=previousSubresource; }
};
static void ok() { CHECK(lastError == S_OK); }
static void apiOk(HRESULT result) { apiResult=result; CHECK(result==S_OK); }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime,HRESULT result) {
  CHECK(runtime.handle && GetCurrentThreadId() == callerThread && FAILED(result));
  ++callbacks; lastError = result;
}
HRESULT dxvk::umd::createDevice(const LUID& luid,D3D_FEATURE_LEVEL level,
    ID3D11Device** device,ID3D11DeviceContext** context,const RuntimeBackend* runtime) noexcept {
  CHECK(!runtime && !std::memcmp(&luid,&expectedLuid,sizeof(luid)));
  ++backendCalls; level = implementationFeatureLevel(level);
  const HRESULT result = D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
    &level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if (result == S_OK) createdContext = *context;
  return result;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush(); return S_OK; }
struct Storage {
  static constexpr UINT64 canary = 0x257acf69597851ecull;
  std::vector<UINT64> words;
  explicit Storage(SIZE_T bytes) : words((bytes+7)/8+2,0) { CHECK(bytes); words.front()=words.back()=canary; }
  void* data() { return words.data()+1; }
  void guards() const { CHECK(words.front()==canary && words.back()==canary); }
};
struct Fixture {
  const UINT profile;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.data()};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};
  D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  struct { D3D10_1DDI_DEVICEFUNCS table{}; UINT64 guard = Storage::canary; } out10;
  struct { D3D11DDI_DEVICEFUNCS table{}; UINT64 guard = Storage::canary; } out11;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Device> backend;
  template<typename Fn> decltype(auto) call(Fn&& fn) {
    if (profile) return fn(out11.table);
    return fn(out10.table);
  }
  explicit Fixture(UINT p) : profile(p) {
    Phase step("device-create"); ::profile=p;
    core10.pfnSetErrorCb=core11.pfnSetErrorCb=error;
    const HRESULT result = profile ? VioGpuDxvkCreateDdiTestDevice11(&expectedLuid,device,{&core11},&core11,&out11.table,D3D_FEATURE_LEVEL_11_0)
      : VioGpuDxvkCreateDdiTestDevice10_1(&expectedLuid,device,{&core10},&core10,&out10.table);
    CHECK(result==S_OK); context=createdContext; createdContext.Reset(); CHECK(context);
    context->GetDevice(&backend); CHECK(backend);
    call([&](auto& t) { CHECK(t.pfnResourceCopy && t.pfnResourceCopyRegion && t.pfnResourceConvert && t.pfnResourceConvertRegion); });
    guards();
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
  ~Fixture() {
    context->ClearState(); context.Reset(); backend.Reset();
    call([&](auto& t) { t.pfnDestroyDevice(device); }); ok(); guards();
  }
};
static UINT scenes,copies,noops,resourceSnapshots,pairIndex;
using Planes=std::vector<std::vector<unsigned char>>;
struct Texture {
  Fixture& f;
  UINT cube,width,height=16,mips=5,layers,blockBytes;
  DXGI_FORMAT format;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  ComPtr<ID3D11Texture2D> reference;
  Planes expected,initialBytes,initialOriginal;
  Texture(Fixture& owner,DXGI_FORMAT fmt,UINT cubeKind,UINT seed,bool staging=false,bool immutable=false)
    : f(owner),cube(cubeKind),width(cubeKind?16u:24u),layers(cubeKind?6u:2u),blockBytes(dxvk::umd::transferBlockBytes(fmt)),format(fmt),
      storage(f.call([&](auto& t){return t.pfnCalcPrivateResourceSize(f.device,nullptr);})),handle{storage.data()},
      expected(mips*layers),initialBytes(mips*layers) {
    Phase step(staging?"BC-staging-create":"BC-resource-create"); CHECK(blockBytes==8 || blockBytes==16);
    std::array<D3D10DDI_MIPINFO,5> shapes{};
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> data(mips*layers);
    std::vector<D3D11_SUBRESOURCE_DATA> publicData(mips*layers);
    for (UINT mip=0;mip<mips;++mip) {
      const UINT w=std::max(1u,width>>mip),h=std::max(1u,height>>mip);
      shapes[mip]={w,h,1,w,h,1};
      for (UINT layer=0;layer<layers;++layer) {
        const UINT sub=mip+layer*mips,row=((w+3)/4)*blockBytes,rows=(h+3)/4,pitch=row+7;
        expected[sub].resize(row*rows); initialBytes[sub].assign(size_t(pitch)*rows+16,0xe7);
        for (UINT y=0;y<rows;++y) for (UINT x=0;x<row;++x)
          expected[sub][y*row+x]=static_cast<unsigned char>(seed+f.profile*41+pairIndex*17+cube*13+sub*29+y*7+x);
        for (UINT y=0;y<rows;++y) std::memcpy(initialBytes[sub].data()+y*pitch,expected[sub].data()+y*row,row);
        data[sub]={initialBytes[sub].data(),pitch,pitch*rows};
        publicData[sub]={initialBytes[sub].data(),pitch,pitch*rows};
      }
    }
    initialOriginal=initialBytes;
    D3D10DDIARG_CREATERESOURCE args{}; args.pMipInfoList=shapes.data(); args.pInitialDataUP=staging?nullptr:data.data();
    args.ResourceDimension=cube?D3D10DDIRESOURCE_TEXTURECUBE:D3D10DDIRESOURCE_TEXTURE2D;
    args.Usage=staging?D3D10_DDI_USAGE_STAGING:immutable?D3D10_DDI_USAGE_IMMUTABLE:D3D10_DDI_USAGE_DEFAULT;
    args.BindFlags=staging?0:D3D10_DDI_BIND_SHADER_RESOURCE; args.MapFlags=staging?D3D10_DDI_CPU_ACCESS_READ:0;
    args.Format=format; args.SampleDesc.Count=1; args.MipLevels=mips; args.ArraySize=layers;
    f.create(args,handle); ok(); storage.guards();
    const D3D11_TEXTURE2D_DESC desc{width,height,mips,layers,format,{1,0},static_cast<D3D11_USAGE>(args.Usage),
      args.BindFlags,staging?D3D11_CPU_ACCESS_READ:0u,cube?D3D11_RESOURCE_MISC_TEXTURECUBE:0u};
    apiOk(f.backend->CreateTexture2D(&desc,staging?nullptr:publicData.data(),&reference));
    CHECK(initialBytes==initialOriginal);
  }
  ~Texture() { Phase step("BC-resource-destroy"); CHECK(initialBytes==initialOriginal); reference.Reset();
    f.call([&](auto& t){t.pfnDestroyResource(f.device,handle);}); ok(); storage.guards(); }
};
static void file(const std::string& name,const void* data,size_t size) {
  std::ofstream output(name,std::ios::binary); CHECK(output.is_open());
  output.write(static_cast<const char*>(data),static_cast<std::streamsize>(size)); output.close(); CHECK(!output.fail());
}
static void snapshot(Texture& texture,UINT phaseIndex,const char* role) {
  Phase step("BC-snapshot"); auto& f=texture.f;
  Texture stage(f,texture.format,texture.cube,0,true);
  f.call([&](auto& t){t.pfnResourceCopy(f.device,stage.handle,texture.handle);}); ok();
  f.context->CopyResource(stage.reference.Get(),texture.reference.Get());
  std::vector<unsigned char> nativeRaw,publicRaw; std::vector<UINT> nativePitches,publicPitches;
  for (UINT sub=0;sub<texture.mips*texture.layers;++sub) {
    subresource=sub; const UINT mip=sub%texture.mips,w=std::max(1u,texture.width>>mip),h=std::max(1u,texture.height>>mip);
    const UINT row=((w+3)/4)*texture.blockBytes,rows=(h+3)/4;
    D3D10DDI_MAPPED_SUBRESOURCE n{}; D3D11_MAPPED_SUBRESOURCE p{};
    f.call([&](auto& t){t.pfnStagingResourceMap(f.device,stage.handle,sub,D3D10_DDI_MAP_READ,0,&n);}); ok();
    apiOk(f.context->Map(stage.reference.Get(),sub,D3D11_MAP_READ,0,&p));
    CHECK(n.pData && p.pData && n.RowPitch>=row && p.RowPitch>=row);
    nativePitches.push_back(n.RowPitch); publicPitches.push_back(p.RowPitch);
    for (UINT y=0;y<rows;++y) {
      const auto* observedN=static_cast<const unsigned char*>(n.pData)+size_t(y)*n.RowPitch;
      const auto* observedP=static_cast<const unsigned char*>(p.pData)+size_t(y)*p.RowPitch;
      CHECK(!std::memcmp(observedN,texture.expected[sub].data()+y*row,row));
      CHECK(!std::memcmp(observedP,texture.expected[sub].data()+y*row,row));
      nativeRaw.insert(nativeRaw.end(),observedN,observedN+row); publicRaw.insert(publicRaw.end(),observedP,observedP+row);
    }
    f.context->Unmap(stage.reference.Get(),sub);
    f.call([&](auto& t){t.pfnStagingResourceUnmap(f.device,stage.handle,sub);}); ok(); ++readbacks;
  }
  const std::string stem="bc-copy-"+std::to_string(f.profile)+"-"+std::to_string(pairIndex)+"-"+
    std::to_string(texture.cube)+"-"+std::to_string(phaseIndex)+"-"+role;
  file(stem+".native.bin",nativeRaw.data(),nativeRaw.size()); file(stem+".public.bin",publicRaw.data(),publicRaw.size());
  std::ofstream metadata(stem+".layout.json"); CHECK(metadata.is_open());
  metadata<<"{\"profile\":"<<f.profile<<",\"pair\":"<<pairIndex<<",\"cube\":"<<texture.cube<<",\"phase\":"<<phaseIndex
    <<",\"role\":\""<<role<<"\",\"format\":"<<UINT(texture.format)<<",\"width\":"<<texture.width<<",\"height\":16,\"mips\":5,\"layers\":"<<texture.layers
    <<",\"block_bytes\":"<<texture.blockBytes<<",\"bytes\":"<<nativeRaw.size()<<",\"native_pitches\":[";
  for (UINT i=0;i<nativePitches.size();++i) metadata<<(i?",":"")<<nativePitches[i];
  metadata<<"],\"public_pitches\":["; for (UINT i=0;i<publicPitches.size();++i) metadata<<(i?",":"")<<publicPitches[i];
  metadata<<"],\"hardware_admission\":false,\"registration\":false}\n"; metadata.close(); CHECK(!metadata.fail());
  CHECK(nativeRaw==publicRaw); observedBytes+=UINT(nativeRaw.size()); ++resourceSnapshots;
}
static void region(Texture& destination,Texture& source,UINT dstSub,UINT x,UINT y,UINT srcSub,const D3D10_DDI_BOX* input,bool convert) {
  Phase step("BC-region"); auto& f=destination.f; const UINT mip=srcSub%source.mips;
  const D3D10_DDI_BOX box=input?*input:D3D10_DDI_BOX{0,0,0,INT(std::max(1u,source.width>>mip)),INT(std::max(1u,source.height>>mip)),1};
  submission="BC-native-region"; operation=convert?3:2;
  f.call([&](auto& t){if(convert)t.pfnResourceConvertRegion(f.device,destination.handle,dstSub,x,y,0,source.handle,srcSub,input);
    else t.pfnResourceCopyRegion(f.device,destination.handle,dstSub,x,y,0,source.handle,srcSub,input);}); ok();
  deviceRemoved=f.backend->GetDeviceRemovedReason(); CHECK(deviceRemoved==S_OK);
  submission="BC-public-region";
  const D3D11_BOX apiBox{UINT(box.left),UINT(box.top),0,UINT(box.right),UINT(box.bottom),1};
  f.context->CopySubresourceRegion(destination.reference.Get(),dstSub,x,y,0,source.reference.Get(),srcSub,input?&apiBox:nullptr);
  deviceRemoved=f.backend->GetDeviceRemovedReason(); CHECK(deviceRemoved==S_OK);
  const UINT sourceRow=((std::max(1u,source.width>>mip)+3)/4)*source.blockBytes;
  const UINT destinationRow=((std::max(1u,destination.width>>(dstSub%destination.mips))+3)/4)*destination.blockBytes;
  const UINT rows=UINT(box.bottom-box.top+3)/4,row=UINT(box.right-box.left+3)/4*source.blockBytes;
  const auto sourceBefore=source.expected[srcSub];
  for (UINT dy=0;dy<rows;++dy) std::memcpy(destination.expected[dstSub].data()+(y/4+dy)*destinationRow+x/4*destination.blockBytes,
    sourceBefore.data()+(UINT(box.top)/4+dy)*sourceRow+UINT(box.left)/4*source.blockBytes,row);
  ++copies;
}
static void invalid(Texture& destination,Texture& source,Fixture& foreign) {
  Phase step("BC-invalid"); auto& f=destination.f; operation=4;
  auto reject=[&](auto&& fn) { const UINT before=callbacks; const auto sourceWords=source.storage.words,destinationWords=destination.storage.words;
    fn(); CHECK(callbacks==before+1 && lastError==dxvk::umd::ddiResult(E_INVALIDARG)); lastError=S_OK; ++rejections;
    CHECK(source.storage.words==sourceWords && destination.storage.words==destinationWords); };
  auto invoke=[&](const D3D10_DDI_BOX* box,UINT dstSub,UINT x,UINT y,UINT z,UINT srcSub) {
    f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,destination.handle,dstSub,x,y,z,source.handle,srcSub,box);}); };
  for (const D3D10_DDI_BOX box : {D3D10_DDI_BOX{1,0,0,8,8,1},{0,1,0,8,8,1},{0,0,0,6,8,1},{0,0,0,8,6,1},
      {-4,0,0,4,4,1},{0,0,0,28,4,1},{0,0,0,4,20,1},{0,0,1,4,4,2}})
    reject([&]{invoke(&box,0,0,0,0,0);});
  const D3D10_DDI_BOX box{0,0,0,4,4,1};
  for (const std::array<UINT,3> xyz : {std::array<UINT,3>{2,0,0},{0,2,0},{0,0,1},{UINT(-1),0,0}})
    reject([&]{invoke(&box,0,xyz[0],xyz[1],xyz[2],0);});
  reject([&]{invoke(&box,UINT(-1),0,0,0,0);}); reject([&]{invoke(&box,0,0,0,0,UINT(-1));});
  reject([&]{f.call([&](auto& t){t.pfnResourceConvertRegion(f.device,destination.handle,0,0,0,0,destination.handle,0,&box);});});
  const DXGI_FORMAT other=destination.blockBytes==8?(pairIndex==0?DXGI_FORMAT_BC4_UNORM:DXGI_FORMAT_BC1_UNORM):
    (pairIndex==1?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_BC2_UNORM);
  Texture incompatible(f,other,destination.cube,0x17),immutable(f,destination.format,destination.cube,0xa3,false,true),alien(foreign,source.format,source.cube,0x17);
  reject([&]{f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,destination.handle,0,0,0,0,incompatible.handle,0,&box);});});
  reject([&]{f.call([&](auto& t){t.pfnResourceConvert(f.device,destination.handle,incompatible.handle);});});
  reject([&]{f.call([&](auto& t){t.pfnResourceConvertRegion(f.device,immutable.handle,0,0,0,0,source.handle,0,&box);});});
  reject([&]{f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,destination.handle,0,0,0,0,alien.handle,0,&box);});});
  reject([&]{f.call([&](auto& t){t.pfnResourceCopyRegion(f.device,destination.handle,0,0,0,0,{},0,&box);});});
  for (const D3D10_DDI_BOX empty : {D3D10_DDI_BOX{2,0,0,1,1,1},{0,2,0,1,1,1},{0,0,2,1,1,1},
      {1,0,0,1,1,1},{0,1,0,1,1,1},{0,0,1,1,1,1}}) {
    const UINT before=callbacks; const auto words=destination.storage.words;
    f.call([&](auto& t){t.pfnResourceConvertRegion(f.device,destination.handle,0,0,0,0,source.handle,0,&empty);}); ok();
    CHECK(callbacks==before && destination.storage.words==words); ++noops;
  }
  CHECK(immutable.initialBytes==immutable.initialOriginal);
  snapshot(immutable,4,"immutable");
}
static void exercise(Fixture& f,Fixture& foreign,DXGI_FORMAT srcFmt,DXGI_FORMAT dstFmt,UINT cube) {
  ::profile=f.profile; kind=cube; sourceFormat=UINT(srcFmt); destinationFormat=UINT(dstFmt); ++scenes;
  Texture source(f,srcFmt,cube,0x17);
  for (UINT mode=0;mode<2;++mode) {
    Phase step("BC-whole-copy"); Texture destination(f,dstFmt,cube,0xa3); operation=mode; submission="BC-native-full";
    f.call([&](auto& t){if(mode)t.pfnResourceConvert(f.device,destination.handle,source.handle);
      else t.pfnResourceCopy(f.device,destination.handle,source.handle);}); ok(); ++copies;
    deviceRemoved=f.backend->GetDeviceRemovedReason(); CHECK(deviceRemoved==S_OK);
    submission="BC-public-full"; f.context->CopyResource(destination.reference.Get(),source.reference.Get());
    deviceRemoved=f.backend->GetDeviceRemovedReason(); CHECK(deviceRemoved==S_OK); destination.expected=source.expected;
    snapshot(destination,mode,"destination"); snapshot(source,mode,"source");
  }
  Texture destination(f,dstFmt,cube,0xa3);
  const UINT layer=destination.layers-1;
  const D3D10_DDI_BOX interior{4,4,0,12,12,1}; region(destination,source,layer*5,8,4,0,&interior,false);
  const D3D10_DDI_BOX edge{cube?0:4,0,0,cube?4:6,4,1}; region(destination,source,layer*5+2,cube?0u:4u,0,2,&edge,true);
  region(destination,source,layer*5+3,0,0,3,nullptr,false);
  const D3D10_DDI_BOX tiny{0,0,0,1,1,1}; region(destination,source,layer*5+4,0,0,4,&tiny,true);
  const D3D10_DDI_BOX toInterior{0,0,0,cube?2:3,2,1}; region(destination,source,layer*5,4,12,3,&toInterior,false);
  const D3D10_DDI_BOX oneBlock{0,0,0,4,4,1}; region(destination,destination,layer*5,0,0,0,&oneBlock,true);
  snapshot(destination,2,"destination"); snapshot(source,2,"source");
  invalid(destination,source,foreign); snapshot(destination,3,"destination"); snapshot(source,3,"source");
}
int main() {
  callerThread=GetCurrentThreadId();
  const std::array<std::array<DXGI_FORMAT,2>,5> pairs{{
    {DXGI_FORMAT_BC1_TYPELESS,DXGI_FORMAT_BC1_UNORM_SRGB},{DXGI_FORMAT_BC2_UNORM,DXGI_FORMAT_BC2_TYPELESS},
    {DXGI_FORMAT_BC3_UNORM_SRGB,DXGI_FORMAT_BC3_UNORM},{DXGI_FORMAT_BC4_UNORM,DXGI_FORMAT_BC4_SNORM},
    {DXGI_FORMAT_BC5_SNORM,DXGI_FORMAT_BC5_TYPELESS}}};
  for (UINT p=0;p<2;++p) {
    Fixture f(p),foreign(p);
    for (pairIndex=0;pairIndex<pairs.size();++pairIndex) for (UINT cube=0;cube<2;++cube)
      exercise(f,foreign,pairs[pairIndex][0],pairs[pairIndex][1],cube);
  }
  CHECK(backendCalls==4 && scenes==20 && copies==160 && rejections==400 && noops==120 && resourceSnapshots==180 && readbacks==3600 && observedBytes==237312 && callbacks==rejections);
  std::printf("BC regional copy PASS checks=%u profiles=2 scenes=%u copies=%u rejections=%u noops=%u snapshots=%u subresources=%u bytes=%u raw_files=540 hardware_admission=0\n",
    checks,scenes,copies,rejections,noops,resourceSnapshots,readbacks,observedBytes);
}
