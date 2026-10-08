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
static UINT profile, kind, sourceFormat, destinationFormat, operation, control = UINT(-1), subresource = UINT(-1);
static const LUID expectedLuid{0x187bb593,-78};
static ComPtr<ID3D11DeviceContext> createdContext;
static std::ofstream manifest;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"Resource copy cast failure line=%d: %s checks=%u callbacks=%u HRESULT=%08lx api_HRESULT=%08lx phase=%s profile=%u kind=%u source_format=%u destination_format=%u operation=%u control=%u subresource=%u observations=%u rejections=%u\n",__LINE__,#x,checks,callbacks,static_cast<unsigned long>(lastError),static_cast<unsigned long>(apiResult),phase,profile,kind,sourceFormat,destinationFormat,operation,control,subresource,readbacks,rejections); std::exit(1); } } while (0)
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
struct Description {
  UINT kind,width,height,depth,mips,arrays,texel;
  DXGI_FORMAT format;
  std::vector<D3D10DDI_MIPINFO> shapes;
  explicit Description(UINT k,DXGI_FORMAT fmt,UINT w=8,UINT levels=3,UINT layers=2)
    : kind(k),width(k ? w : 128),height(k>1 ? 4 : 1),depth(k==3 ? 4 : 1),
      mips(k ? levels : 1),arrays(k && k!=3 ? layers : 1),texel(k ? dxvk::umd::transferTexelBytes(fmt) : 1),
      format(k ? fmt : DXGI_FORMAT_UNKNOWN),shapes(mips) {
    CHECK(texel && mips);
    for (UINT mip=0;mip<mips;++mip) {
      const UINT x=std::max(1u,width>>mip),y=std::max(1u,height>>mip),z=std::max(1u,depth>>mip);
      shapes[mip]={x,y,z,x,y,z};
    }
  }
  UINT count() const { return mips*arrays; }
};
using Pixels = std::vector<std::vector<unsigned char>>;
static Pixels initial(const Description& d,UINT seed) {
  Pixels result(d.count());
  for (UINT sub=0;sub<d.count();++sub) {
    const auto& s=d.shapes[sub%d.mips]; auto& bytes=result[sub];
    for (UINT z=0;z<s.TexelDepth;++z) for (UINT y=0;y<s.TexelHeight;++y)
      for (UINT x=0;x<s.TexelWidth;++x) for (UINT lane=0;lane<d.texel;++lane)
        bytes.push_back(static_cast<unsigned char>(seed+sub*37+z*23+y*11+x*3+lane*19));
  }
  if (seed==0x17 && d.texel==4 && result[0].size()>=16) {
    const std::array<UINT,4> bits{{0x3f800000u,0x80000000u,0x7fc12345u,0x00000001u}};
    std::memcpy(result[0].data(),bits.data(),sizeof(bits));
  }
  return result;
}
struct Resource {
  Fixture& f;
  const Description desc;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  ComPtr<ID3D11Resource> reference;
  Resource(Fixture& owner,const Description& d,const Pixels* pixels,bool staging=false,bool immutable=false)
    : f(owner),desc(d),storage(f.call([&](auto& t) { return t.pfnCalcPrivateResourceSize(f.device,nullptr); })),handle{storage.data()} {
    Phase step(staging ? "staging-create" : immutable ? "immutable-create" : "resource-create");
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> native(d.count());
    std::vector<D3D11_SUBRESOURCE_DATA> api(d.count());
    if (pixels) for (UINT sub=0;sub<d.count();++sub) {
      const auto& s=d.shapes[sub%d.mips]; const UINT pitch=s.TexelWidth*d.texel;
      native[sub]={const_cast<unsigned char*>((*pixels)[sub].data()),pitch,pitch*s.TexelHeight};
      api[sub]={(*pixels)[sub].data(),pitch,pitch*s.TexelHeight};
    }
    const D3D11_USAGE usage=staging ? D3D11_USAGE_STAGING : immutable ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    const UINT bindings=staging ? 0u : immutable ? (d.kind ? D3D11_BIND_SHADER_RESOURCE : D3D11_BIND_VERTEX_BUFFER) : 0u;
    D3D10DDIARG_CREATERESOURCE args{}; args.pMipInfoList=d.shapes.data(); args.pInitialDataUP=pixels ? native.data() : nullptr;
    args.ResourceDimension=d.kind==0 ? D3D10DDIRESOURCE_BUFFER : d.kind==1 ? D3D10DDIRESOURCE_TEXTURE1D : d.kind==2 ? D3D10DDIRESOURCE_TEXTURE2D : D3D10DDIRESOURCE_TEXTURE3D;
    args.Usage=static_cast<D3D10_DDI_RESOURCE_USAGE>(usage); args.BindFlags=bindings;
    args.MapFlags=staging ? D3D10_DDI_CPU_ACCESS_READ : 0; args.Format=d.format;
    args.SampleDesc.Count=1; args.MipLevels=d.mips; args.ArraySize=d.arrays;
    f.create(args,handle); ok(); storage.guards();
    const UINT cpu=staging ? D3D11_CPU_ACCESS_READ : 0;
    const auto data=pixels ? api.data() : nullptr;
    phase="public-resource-create";
    if (!d.kind) {
      D3D11_BUFFER_DESC a{d.width,usage,bindings,cpu,0,0}; ComPtr<ID3D11Buffer> resource;
      apiOk(f.backend->CreateBuffer(&a,data,&resource)); reference=resource;
    } else if (d.kind==1) {
      D3D11_TEXTURE1D_DESC a{d.width,d.mips,d.arrays,d.format,usage,bindings,cpu,0}; ComPtr<ID3D11Texture1D> resource;
      apiOk(f.backend->CreateTexture1D(&a,data,&resource)); reference=resource;
    } else if (d.kind==2) {
      D3D11_TEXTURE2D_DESC a{d.width,d.height,d.mips,d.arrays,d.format,{1,0},usage,bindings,cpu,0}; ComPtr<ID3D11Texture2D> resource;
      apiOk(f.backend->CreateTexture2D(&a,data,&resource)); reference=resource;
    } else {
      D3D11_TEXTURE3D_DESC a{d.width,d.height,d.depth,d.mips,d.format,usage,bindings,cpu,0}; ComPtr<ID3D11Texture3D> resource;
      apiOk(f.backend->CreateTexture3D(&a,data,&resource)); reference=resource;
    }
  }
  ~Resource() { Phase step("resource-destroy"); reference.Reset(); f.call([&](auto& t) { t.pfnDestroyResource(f.device,handle); }); ok(); storage.guards(); }
};
static void save(UINT id,const char* suffix,const std::vector<unsigned char>& bytes) {
  char name[96]; CHECK(std::snprintf(name,sizeof(name),"copy-cast-%03u-%s.bin",id,suffix)>0);
  std::ofstream output(name,std::ios::binary); CHECK(output.is_open());
  output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
  output.close(); CHECK(!output.fail());
}
static std::vector<unsigned char> read(Resource& source,bool native,const Pixels& expected) {
  Phase step(native ? "native-readback" : "public-readback");
  auto& f=source.f; const auto& d=source.desc;
  Resource stage(f,d,nullptr,true);
  phase=native ? "native-stage-copy" : "public-stage-copy";
  if (native) { f.call([&](auto& t) { t.pfnResourceCopy(f.device,stage.handle,source.handle); }); ok(); }
  else f.context->CopyResource(stage.reference.Get(),source.reference.Get());
  std::vector<unsigned char> result;
  for (UINT sub=0;sub<d.count();++sub) {
    subresource=sub;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    phase=native ? "native-stage-map" : "public-stage-map";
    if (native) {
      D3D10DDI_MAPPED_SUBRESOURCE n{};
      f.call([&](auto& t) { t.pfnStagingResourceMap(f.device,stage.handle,sub,D3D10_DDI_MAP_READ,0,&n); }); ok();
      mapped={n.pData,n.RowPitch,n.DepthPitch};
    } else apiOk(f.context->Map(stage.reference.Get(),sub,D3D11_MAP_READ,0,&mapped));
    phase=native ? "native-readback-bytes" : "public-readback-bytes";
    CHECK(mapped.pData); const auto& s=d.shapes[sub%d.mips];
    const UINT row=s.TexelWidth*d.texel;
    CHECK(!d.kind || mapped.RowPitch>=row);
    CHECK(d.kind!=3 || mapped.DepthPitch>=mapped.RowPitch*s.TexelHeight);
    for (UINT z=0;z<s.TexelDepth;++z) for (UINT y=0;y<s.TexelHeight;++y) {
      const auto* bytes=static_cast<const unsigned char*>(mapped.pData)+size_t(z)*mapped.DepthPitch+size_t(y)*mapped.RowPitch;
      const auto* oracle=expected[sub].data()+(size_t(z)*s.TexelHeight+y)*row;
      CHECK(!std::memcmp(bytes,oracle,row));
      result.insert(result.end(),bytes,bytes+row);
    }
    phase=native ? "native-stage-unmap" : "public-stage-unmap";
    if (native) { f.call([&](auto& t) { t.pfnStagingResourceUnmap(f.device,stage.handle,sub); }); ok(); }
    else f.context->Unmap(stage.reference.Get(),sub);
  }
  return result;
}
static void observe(Resource& destination,const Pixels& expected,DXGI_FORMAT sourceFormat,UINT operation,UINT seed=0xa3) {
  const UINT id=readbacks++; auto native=read(destination,true,expected),api=read(destination,false,expected);
  CHECK(native==api); save(id,"native",native); save(id,"public",api);
  observedBytes+=static_cast<UINT>(native.size()); const auto& d=destination.desc;
  manifest<<id<<' '<<destination.f.profile<<' '<<d.kind<<' '<<UINT(sourceFormat)<<' '<<UINT(d.format)<<' '
    <<operation<<' '<<d.width<<' '<<d.height<<' '<<d.depth<<' '<<d.mips<<' '<<d.arrays<<' '<<d.texel<<' '<<seed<<' '<<native.size()<<'\n';
  manifest.flush(); CHECK(manifest.good());
}
static void perform(Fixture& f,UINT kind,DXGI_FORMAT srcFormat,DXGI_FORMAT dstFormat,UINT operation) {
  Phase step("positive-copy"); ::profile=f.profile; ::kind=kind; sourceFormat=UINT(srcFormat);
  destinationFormat=UINT(dstFormat); ::operation=operation; control=UINT(-1);
  Description sourceDesc(kind,srcFormat),destDesc(kind,dstFormat);
  auto original=initial(sourceDesc,0x17),expected=initial(destDesc,0xa3);
  Resource source(f,sourceDesc,&original),destination(f,destDesc,&expected);
  if (operation<2) {
    expected=original;
    f.call([&](auto& t) {
      if (operation) t.pfnResourceConvert(f.device,destination.handle,source.handle);
      else t.pfnResourceCopy(f.device,destination.handle,source.handle);
    }); ok(); f.context->CopyResource(destination.reference.Get(),source.reference.Get());
  } else {
    const UINT srcSub=kind ? 1 : 0,dstSub=kind && kind!=3 ? 3 : 0;
    const UINT x=kind ? 3 : 80,y=kind>1 ? 1 : 0,z=kind==3 ? 1 : 0;
    D3D10_DDI_BOX box{kind ? 1 : 16,0,0,kind ? 3 : 48,kind>1 ? 2 : 1,kind==3 ? 2 : 1};
    f.call([&](auto& t) {
      if (operation==3) t.pfnResourceConvertRegion(f.device,destination.handle,dstSub,x,y,z,source.handle,srcSub,&box);
      else t.pfnResourceCopyRegion(f.device,destination.handle,dstSub,x,y,z,source.handle,srcSub,&box);
    }); ok();
    const D3D11_BOX apiBox{UINT(box.left),UINT(box.top),UINT(box.front),UINT(box.right),UINT(box.bottom),UINT(box.back)};
    f.context->CopySubresourceRegion(destination.reference.Get(),dstSub,x,y,z,source.reference.Get(),srcSub,&apiBox);
    const auto& src=sourceDesc.shapes[srcSub%sourceDesc.mips]; const auto& dst=destDesc.shapes[dstSub%destDesc.mips];
    for (UINT dz=0;dz<UINT(box.back-box.front);++dz) for (UINT dy=0;dy<UINT(box.bottom-box.top);++dy)
      for (UINT dx=0;dx<UINT(box.right-box.left);++dx) {
        const size_t from=((size_t(dz)*src.TexelHeight+dy)*src.TexelWidth+UINT(box.left)+dx)*sourceDesc.texel;
        const size_t to=((size_t(z+dz)*dst.TexelHeight+y+dy)*dst.TexelWidth+x+dx)*destDesc.texel;
        std::memcpy(expected[dstSub].data()+to,original[srcSub].data()+from,sourceDesc.texel);
      }
  }
  observe(destination,expected,srcFormat,operation);
  CHECK(read(source,true,original)==read(source,false,original));
}
static void negativeControls(Fixture& f,Fixture& foreign,UINT kind) {
  Phase step("negative-control"); ::profile=f.profile; ::kind=kind; sourceFormat=destinationFormat=UINT(DXGI_FORMAT_R32_UINT);
  operation=4; control=UINT(-1);
  Description d(kind,DXGI_FORMAT_R32_UINT); auto expected=initial(d,0xa3),original=initial(d,0x17);
  Resource destination(f,d,&expected),source(f,d,&original),other(foreign,d,&original);
  auto reject=[&](auto&& fn) {
    ++control;
    const UINT before=callbacks; fn();
    CHECK(callbacks==before+1 && lastError==dxvk::umd::ddiResult(E_INVALIDARG)); lastError=S_OK; ++rejections;
    CHECK(read(destination,true,expected)==read(destination,false,expected));
  };
  reject([&] { f.call([&](auto& t) { t.pfnResourceCopy(f.device,destination.handle,destination.handle); }); });
  reject([&] { f.call([&](auto& t) { t.pfnResourceConvert(f.device,destination.handle,other.handle); }); });
  reject([&] { f.call([&](auto& t) { t.pfnResourceConvert(f.device,destination.handle,{}); }); });
  Resource immutable(f,d,&expected,false,true);
  reject([&] { f.call([&](auto& t) { t.pfnResourceConvert(f.device,immutable.handle,source.handle); }); });
  CHECK(read(immutable,true,expected)==read(immutable,false,expected));
  if (kind) {
    Description mismatch(kind,DXGI_FORMAT_R8G8B8A8_UINT); auto badInitial=initial(mismatch,0x17);
    Resource bad(f,mismatch,&badInitial);
    reject([&] { f.call([&](auto& t) { t.pfnResourceConvert(f.device,destination.handle,bad.handle); }); });
    reject([&] { f.call([&](auto& t) { t.pfnResourceConvertRegion(f.device,destination.handle,0,0,0,0,bad.handle,0,nullptr); }); });
    Description width(kind,DXGI_FORMAT_R32_UINT,16); auto widthData=initial(width,0x17); Resource wide(f,width,&widthData);
    reject([&] { f.call([&](auto& t) { t.pfnResourceCopy(f.device,destination.handle,wide.handle); }); });
    Description levels(kind,DXGI_FORMAT_R32_UINT,8,2); auto levelData=initial(levels,0x17); Resource level(f,levels,&levelData);
    reject([&] { f.call([&](auto& t) { t.pfnResourceConvert(f.device,destination.handle,level.handle); }); });
    if (kind!=3) {
      Description layers(kind,DXGI_FORMAT_R32_UINT,8,3,3); auto layerData=initial(layers,0x17); Resource layer(f,layers,&layerData);
      reject([&] { f.call([&](auto& t) { t.pfnResourceCopy(f.device,destination.handle,layer.handle); }); });
    }
  }
  reject([&] { f.call([&](auto& t) { t.pfnResourceConvertRegion(f.device,destination.handle,d.count(),0,0,0,source.handle,0,nullptr); }); });
  reject([&] { f.call([&](auto& t) { t.pfnResourceCopyRegion(f.device,destination.handle,0,0,0,0,source.handle,d.count(),nullptr); }); });
  reject([&] { f.call([&](auto& t) { t.pfnResourceCopyRegion(f.device,destination.handle,0,UINT(-1),0,0,source.handle,0,nullptr); }); });
  D3D10_DDI_BOX invalid{-1,0,0,1,1,1};
  reject([&] { f.call([&](auto& t) { t.pfnResourceConvertRegion(f.device,destination.handle,0,0,0,0,source.handle,0,&invalid); }); });
  const std::array<D3D10_DDI_BOX,6> empty{{{2,0,0,1,1,1},{0,2,0,1,1,1},{0,0,2,1,1,1},
    {1,0,0,1,1,1},{0,1,0,1,1,1},{0,0,1,1,1,1}}};
  for (const auto& box:empty) {
    ++control; Phase emptyStep("empty-native");
    const UINT before=callbacks;
    f.call([&](auto& t) { t.pfnResourceConvertRegion(f.device,destination.handle,0,0,0,0,source.handle,0,&box); }); ok();
    CHECK(callbacks==before);
    // Exercise the six literal equal/reversed DDI boxes above. The public
    // control uses a bounded equal-axis empty box: WARP can consume a
    // reversed unsigned extent instead of treating it as the documented
    // no-op, removing the reference device before the next staging create.
    const D3D11_BOX publicBox{0,0,0,0,1,1};
    phase="empty-public";
    f.context->CopySubresourceRegion(destination.reference.Get(),0,0,0,0,source.reference.Get(),0,&publicBox);
    CHECK(read(destination,true,expected)==read(destination,false,expected));
  }
  observe(destination,expected,d.format,4);
}
int main() {
  callerThread=GetCurrentThreadId(); manifest.open("copy-cast-manifest.txt",std::ios::binary); CHECK(manifest.is_open());
  const std::array<std::array<DXGI_FORMAT,2>,12> pairs{{
    {DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32_FLOAT},{DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_SINT},
    {DXGI_FORMAT_R32_TYPELESS,DXGI_FORMAT_R32_UINT},{DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R9G9B9E5_SHAREDEXP},
    {DXGI_FORMAT_R9G9B9E5_SHAREDEXP,DXGI_FORMAT_R32_SINT},{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
    {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R8G8B8A8_TYPELESS},{DXGI_FORMAT_R8G8B8A8_UINT,DXGI_FORMAT_R8G8B8A8_SNORM},
    {DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R16G16_UINT},{DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT},
    {DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32G32B32A32_SINT},{DXGI_FORMAT_R8_SINT,DXGI_FORMAT_R8_UNORM}}};
  for (UINT profile=0;profile<2;++profile) {
    Fixture f(profile),foreign(profile);
    for (UINT kind=1;kind<=3;++kind) {
      for (const auto& pair:pairs) for (UINT operation=0;operation<4;++operation) perform(f,kind,pair[0],pair[1],operation);
      if (kind==2) for (UINT operation=0;operation<4;++operation) {
        perform(f,kind,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,operation);
        perform(f,kind,DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,DXGI_FORMAT_B8G8R8X8_TYPELESS,operation);
      }
      negativeControls(f,foreign,kind);
    }
    for (UINT operation=0;operation<4;++operation) perform(f,0,DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_UNKNOWN,operation);
    negativeControls(f,foreign,0);
  }
  manifest.close(); CHECK(!manifest.fail());
  CHECK(backendCalls==4 && readbacks==320 && rejections==92 && callbacks==rejections);
  std::printf("Resource copy cast passed: %u checks, %u observations, %u bytes each native/public, %u rejections\n",checks,readbacks,observedBytes,rejections);
}
