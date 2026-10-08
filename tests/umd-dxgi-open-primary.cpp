// SPDX-License-Identifier: MIT
// Typed production OpenResource/Resolve/Blt with an externally owned primary.
// WARP replaces only the embedded renderer. Callback ownership is controlled;
// this fixture makes no hardware or ordinary Microsoft-runtime admission claim.
#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_allocation.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wrl/client.h>

static constexpr unsigned width=7, height=5, pitch=40, bytes=pitch*height;
static std::atomic<unsigned> checks{0};
static unsigned images,pixels,acquisitions,releases,releaseAttempts,locks,unlocks,renders,contexts,contextCloses;
static unsigned runtimeTerminalReleases,runtimeTerminalMapClosures;
static unsigned errorCallbacks,failedOpenFrames;
static unsigned sharedPresentImages,sharedPresentPixels,sharedPresents,sharedModes,sharedNegativeCases;
static bool sharedPresentChecks=false;
static D3DKMT_HANDLE expectedPresentHandle;
static std::function<void()> presentAction,modeAction;
static Microsoft::WRL::ComPtr<ID3D11Device> publicDevice;
static Microsoft::WRL::ComPtr<ID3D11DeviceContext> publicContext;
static DWORD caller;
static char adapterCookie,deviceCookie,coreCookie,contextCookie;
static LUID selected{0x13572468,-73};
static bool runtimeValid=true;
static HRESULT lastError=S_OK,allocateResult=S_OK,releaseResult=S_OK,unlockResult=S_OK;
static unsigned malformedAllocation;
static std::function<void()> allocateBefore,allocateAfter,deallocateAction,lockAction,unlockAction,renderAction;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr,"opened primary line=%d: %s error=%08lx\n",__LINE__,#value,static_cast<unsigned long>(lastError)); std::abort(); \
} } while (0)
static void callback() { CHECK(runtimeValid && GetCurrentThreadId()==caller); }
struct Backing {
  dxvk::umd::AllocationInfo info;
  void* page;
  bool internal=false,mapped=false;
  HANDLE resource=nullptr;
  uint8_t* data() const { return static_cast<uint8_t*>(page)+16; }
};
static std::unordered_map<D3DKMT_HANDLE,Backing> backing;
static std::vector<void*> retiredPages;
static D3DKMT_HANDLE nextHandle=501,borrowedHandle;
static std::array<uint8_t,4096> commands[2];
static D3DDDI_ALLOCATIONLIST allocationLists[2][8];
static D3DDDI_PATCHLOCATIONLIST patchLists[2][8];
static unsigned buffer;
static size_t internalCount() {
  size_t count=0;for (const auto& entry:backing) count+=entry.second.internal;return count;
}
static uint32_t color(unsigned seed,unsigned x,unsigned y) {
  return 0xff000000u | ((17*seed+3*x+5*y)&255) | (((29*seed+7*x+11*y)&255)<<8)
    | (((43*seed+13*x+19*y)&255)<<16);
}
static std::array<uint32_t,width*height> image(unsigned seed) {
  std::array<uint32_t,width*height> result{};
  for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) result[y*width+x]=color(seed,x,y);
  return result;
}
static void guards(const Backing& value) {
  for (unsigned i=0;i<16;++i) CHECK(static_cast<uint8_t*>(value.page)[i]==0xa5);
  for (size_t i=16+value.info.size;i<4096;++i) CHECK(static_cast<uint8_t*>(value.page)[i]==0xa5);
  for (unsigned y=0;y<value.info.height;++y) for (unsigned x=value.info.width*4;x<value.info.pitch;++x)
    CHECK(value.data()[y*value.info.pitch+x]==0xa5);
}
static void kernelPixels(D3DKMT_HANDLE handle,unsigned seed,bool write) {
  auto& value=backing.at(handle);
  for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
    auto at=value.data()+y*pitch+4*x;auto expected=color(seed,x,y);
    if (write) std::memcpy(at,&expected,4);
    else {
      uint32_t actual;std::memcpy(&actual,at,4);
      if (actual!=expected) std::fprintf(stderr,
        "Opened primary kernel pixel failure format=%u seed=%u x=%u y=%u actual=%08x expected=%08x\n",
        value.info.format,seed,x,y,actual,expected);
      CHECK(actual==expected);
    }
  }
  guards(value);
}
static void retirePage(D3DKMT_HANDLE handle) {
  auto entry=backing.find(handle);CHECK(entry!=backing.end());guards(entry->second);
  DWORD old;CHECK(VirtualProtect(entry->second.page,4096,PAGE_NOACCESS,&old));
  retiredPages.push_back(entry->second.page);backing.erase(entry);
}
struct Foreign {
  D3DKMT_HANDLE handle=nextHandle++;
  dxvk::umd::AllocationInfo info;
  explicit Foreign(unsigned format=3) {
    info.flags=1;info.width=width;info.height=height;info.pitch=pitch;info.size=bytes;info.format=format;
    info.refreshNumerator=60000;info.refreshDenominator=1001;
    void* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(page);
    std::memset(page,0xa5,4096);CHECK(backing.emplace(handle,Backing{info,page,false,false}).second);
    borrowedHandle=handle;kernelPixels(handle,1,true);
  }
  ~Foreign() { CHECK(backing.count(handle) && !backing.at(handle).internal && !backing.at(handle).mapped);retirePage(handle); }
};
static HRESULT APIENTRY query(HANDLE adapter,const D3DDDICB_QUERYADAPTERINFO* request) {
  callback();CHECK(adapter==&adapterCookie && request && request->PrivateDriverDataSize==160);
  auto output=static_cast<uint8_t*>(request->pPrivateDriverData);
  for (unsigned i=0;i<160;++i) CHECK(output[i]==0);
  auto put=[&](unsigned at,uint64_t value,unsigned count) { for (unsigned i=0;i<count;++i) output[at+i]=uint8_t(value>>(i*8)); };
  put(0,0x504d5644,4);put(8,128,4);put(16,3,8);put(24,31,8);
  put(128,0x44494c56,4);put(132,1,4);put(136,32,4);put(140,1,4);put(152,1,4);
  std::memcpy(output+144,&selected,sizeof(selected));return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device,D3DDDICB_ALLOCATE* request) {
  callback();CHECK(device==&deviceCookie && request && !request->hKMResource);
  const bool internal=!request->hResource;
  CHECK(!request->pPrivateDriverData && !request->PrivateDriverDataSize && request->NumAllocations==1 && request->pAllocationInfo);
  auto& slot=request->pAllocationInfo[0];CHECK(!slot.hAllocation && !slot.Flags.Value && !slot.VidPnSourceId);
  CHECK(slot.pPrivateDriverData && slot.PrivateDriverDataSize==80);
  dxvk::umd::AllocationInfo info;std::memcpy(&info,slot.pPrivateDriverData,80);
  CHECK(info.magic==0x504d5644 && !info.version && info.headerSize==80 && !info.reserved);
  CHECK(info.flags==2 && info.width==width && info.height==height);
  CHECK(info.pitch==(internal?pitch:width*4) && info.size==uint64_t(info.pitch)*height);
  CHECK(info.alignment==4096 && !info.requestedIova && !info.resetGeneration && !info.contextId);
  CHECK(info.format>=1 && info.format<=3 && !info.refreshNumerator && !info.refreshDenominator);
  if (allocateBefore) std::exchange(allocateBefore,{})();
  if (FAILED(allocateResult)) return allocateResult;
  if (malformedAllocation==1) return S_OK;
  if (malformedAllocation==2) { slot.hAllocation=borrowedHandle;return S_OK; }
  void* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(page);
  std::memset(page,0xa5,4096);
  auto handle=nextHandle++;CHECK(backing.emplace(handle,Backing{info,page,true,false,request->hResource}).second);
  slot.hAllocation=handle;++acquisitions;
  if (!internal) request->hKMResource=handle+1000;
  std::memset(slot.pPrivateDriverData,0xff,80); // Retained pitch must survive callback mutation.
  if (malformedAllocation==3) request->NumAllocations=2;
  if (malformedAllocation==4) request->hResource=&coreCookie;
  if (malformedAllocation==5) request->hKMResource=987654;
  if (allocateAfter) std::exchange(allocateAfter,{})();
  return allocateResult;
}
static HRESULT APIENTRY deallocate(HANDLE device,const D3DDDICB_DEALLOCATE* request) {
  callback();CHECK(device==&deviceCookie && request);
  D3DKMT_HANDLE handle=0;
  if (request->hResource) {
    CHECK(!request->NumAllocations && !request->HandleList);
    for (const auto& entry:backing) if (entry.second.internal && entry.second.resource==request->hResource) {
      CHECK(!handle);handle=entry.first;
    }
  } else {
    CHECK(request->NumAllocations==1 && request->HandleList);
    handle=*request->HandleList;
    CHECK(backing.count(handle) && !backing.at(handle).resource);
  }
  CHECK(handle && backing.count(handle) && backing.at(handle).internal && handle!=borrowedHandle);
  CHECK(!backing.at(handle).mapped);++releaseAttempts;
  const auto result=releaseResult;
  if (SUCCEEDED(result)) { retirePage(handle);++releases; }
  if (deallocateAction) std::exchange(deallocateAction,{})();
  return result;
}
static HRESULT APIENTRY lock(HANDLE device,D3DDDICB_LOCK* request) {
  callback();CHECK(device==&deviceCookie && request && backing.count(request->hAllocation));
  auto& value=backing.at(request->hAllocation);CHECK(value.internal && value.info.flags==2 && !value.mapped);
  CHECK(request->Flags.LockEntire && !request->Flags.Discard && !request->Flags.IgnoreSync);
  value.mapped=true;request->pData=value.data();++locks;
  if (lockAction) std::exchange(lockAction,{})();
  return S_OK;
}
static HRESULT APIENTRY unlock(HANDLE device,const D3DDDICB_UNLOCK* request) {
  callback();CHECK(device==&deviceCookie && request && request->NumAllocations==1 && request->phAllocations);
  auto handle=*request->phAllocations;CHECK(backing.count(handle) && backing.at(handle).internal && backing.at(handle).mapped);
  const auto result=unlockResult;
  if (SUCCEEDED(result)) { backing.at(handle).mapped=false;++unlocks; }
  if (unlockAction) std::exchange(unlockAction,{})();
  return result;
}
static HRESULT APIENTRY createContext(HANDLE device,D3DDDICB_CREATECONTEXT* request) {
  callback();CHECK(device==&deviceCookie && request && !request->NodeOrdinal && request->EngineAffinity==1 && !request->Flags.Value);
  CHECK(!request->pPrivateDriverData && !request->PrivateDriverDataSize);
  ++contexts;buffer=0;request->hContext=&contextCookie;
  request->pCommandBuffer=commands[0].data();request->CommandBufferSize=4096;
  request->pAllocationList=allocationLists[0];request->AllocationListSize=8;
  request->pPatchLocationList=patchLists[0];request->PatchLocationListSize=8;return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE device,const D3DDDICB_DESTROYCONTEXT* request) {
  callback();CHECK(device==&deviceCookie && request && request->hContext==&contextCookie);++contextCloses;return S_OK;
}
static HRESULT APIENTRY render(HANDLE device,D3DDDICB_RENDER* request) {
  callback();CHECK(device==&deviceCookie && request && request->hContext==&contextCookie);
  CHECK(request->CommandLength==64 && !request->CommandOffset && request->NumAllocations==2 && !request->NumPatchLocations && !request->Flags.Value);
  std::array<uint32_t,16> words{};std::memcpy(words.data(),commands[buffer].data(),64);
  CHECK((words==std::array<uint32_t,16>{0x504d5644,0,64,0,2,0,width,height,0,0,0,0,0,0,0,0}));
  const auto& source=allocationLists[buffer][0];const auto& destination=allocationLists[buffer][1];
  CHECK(source.hAllocation!=destination.hAllocation && !source.WriteOperation && destination.WriteOperation && !source.Reserved && !destination.Reserved);
  CHECK(backing.count(source.hAllocation) && backing.count(destination.hAllocation));
  const auto& from=backing.at(source.hAllocation);auto& to=backing.at(destination.hAllocation);
  CHECK(from.internal!=to.internal && !from.mapped && !to.mapped && from.info.pitch==pitch && to.info.pitch==pitch);
  for (unsigned y=0;y<height;++y) std::memcpy(to.data()+y*pitch,from.data()+y*pitch,width*4);
  ++renders;buffer^=1;
  request->pNewCommandBuffer=commands[buffer].data();request->NewCommandBufferSize=4096;
  request->pNewAllocationList=allocationLists[buffer];request->NewAllocationListSize=8;
  request->pNewPatchLocationList=patchLists[buffer];request->NewPatchLocationListSize=8;
  if (renderAction) std::exchange(renderAction,{})();return S_OK;
}
static HRESULT APIENTRY present(HANDLE device,DXGIDDICB_PRESENT* request) {
  callback();CHECK(sharedPresentChecks && device==&deviceCookie && request);
  CHECK(request->hSrcAllocation==expectedPresentHandle && backing.count(expectedPresentHandle));
  CHECK(request->hContext==&contextCookie && request->pDXGIContext==&adapterCookie && !request->hDstAllocation);
  CHECK(!backing.at(expectedPresentHandle).mapped);++sharedPresents;
  if (presentAction) std::exchange(presentAction,{})();return S_OK;
}
static HRESULT APIENTRY mode(HANDLE device,D3DDDICB_SETDISPLAYMODE* request) {
  callback();CHECK(sharedPresentChecks && device==&deviceCookie && request);
  CHECK(request->hPrimaryAllocation==expectedPresentHandle && expectedPresentHandle==borrowedHandle);
  CHECK(backing.count(expectedPresentHandle) && !backing.at(expectedPresentHandle).internal
    && backing.at(expectedPresentHandle).info.flags==1 && !request->PrivateDriverFormatAttribute);
  ++sharedModes;if (modeAction) std::exchange(modeAction,{})();return S_OK;
}
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime,HRESULT result) { callback();CHECK(runtime.handle==&coreCookie && FAILED(result));lastError=result;++errorCallbacks; }
static void runtimeTerminalCleanup() {
  CHECK(!runtimeValid && GetCurrentThreadId()==caller && internalCount()==1);
  D3DKMT_HANDLE handle=0;for (auto& entry:backing) if (entry.second.internal) {
    handle=entry.first;if (entry.second.mapped) { entry.second.mapped=false;++runtimeTerminalMapClosures; }
  }
  CHECK(handle);retirePage(handle);++runtimeTerminalReleases; // Never counted as a UMD callback.
}
HRESULT dxvk::umd::createDevice(const LUID& luid,D3D_FEATURE_LEVEL level,
    ID3D11Device** device,ID3D11DeviceContext** context,const RuntimeBackend*) noexcept {
  CHECK(!std::memcmp(&luid,&selected,sizeof(luid)));
  const HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,device,nullptr,context);
  if (hr==S_OK && sharedPresentChecks) { publicDevice=*device;publicContext=*context; }
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*,ID3D11Resource*,BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept { context->Flush();return S_OK; }
struct Storage {
  void* data;
  explicit Storage(SIZE_T size):data(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)) { CHECK(size && data); }
  void poison() { DWORD previous;CHECK(VirtualProtect(data,1,PAGE_NOACCESS,&previous)); }
  ~Storage() { CHECK(VirtualFree(data,0,MEM_RELEASE)); }
};
static D3DDDI_DEVICECALLBACKS kernelCallbacks() {
  D3DDDI_DEVICECALLBACKS value{};value.pfnAllocateCb=allocate;value.pfnDeallocateCb=deallocate;
  value.pfnLockCb=lock;value.pfnUnlockCb=unlock;value.pfnCreateContextCb=createContext;
  value.pfnDestroyContextCb=destroyContext;value.pfnRenderCb=render;value.pfnSetDisplayModeCb=mode;return value;
}
template<typename Table>
struct Fixture {
  using Desc=std::conditional_t<std::is_same_v<Table,D3D11DDI_DEVICEFUNCS>,D3D11DDIARG_CREATERESOURCE,D3D10DDIARG_CREATERESOURCE>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};D3D10DDI_HDEVICE device{storage.data};Table table{};
  DXGI1_1_DDI_BASE_FUNCTIONS dxgi{};DXGI_DDI_BASE_CALLBACKS callbacks{};
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{};D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  bool live=true;
  Fixture() {
    runtimeValid=true;lastError=S_OK;callbacks.pfnPresentCb=present;core10.pfnSetErrorCb=error;core11.pfnSetErrorCb=error;
    const auto kernel=kernelCallbacks();D3D10DDIARG_CREATEDEVICE args{};
    if constexpr (std::is_same_v<Table,D3D10DDI_DEVICEFUNCS>) {
      args.Interface=D3D10_0_DDI_INTERFACE_VERSION;args.Version=D3D10_0_DDI_BUILD_VERSION<<16;
      args.pDeviceFuncs=&table;args.pUMCallbacks=&core10;
    } else if constexpr (std::is_same_v<Table,D3D10_1DDI_DEVICEFUNCS>) {
      args.Interface=D3D10_1_DDI_INTERFACE_VERSION;args.Version=D3D10_1_DDI_BUILD_VERSION<<16;
      args.p10_1DeviceFuncs=&table;args.pUMCallbacks=&core10;
    } else {
      args.Interface=D3D11_0_DDI_INTERFACE_VERSION;args.Version=D3D11_0_DDI_BUILD_VERSION<<16;
      args.Flags=D3D11DDI_3DPIPELINELEVEL_11_0<<D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
      args.p11DeviceFuncs=&table;args.p11UMCallbacks=&core11;
    }
    args.Version|=std::is_same_v<Table,D3D11DDI_DEVICEFUNCS>?DXGI_RESOLVE_SHARED_RESOURCE:(VISTA_GOLD_PRODUCT_VER|DXGI_RESOLVE_SHARED_RESOURCE);
    CHECK(dxvk::umd::nativeDxgiUses1_1(args.Interface,args.Version));
    args.hDrvDevice=device;args.hRTDevice.handle=&deviceCookie;args.hRTCoreLayer.handle=&coreCookie;args.pKTCallbacks=&kernel;
    args.DXGIBaseDDI.pDXGIBaseCallbacks=&callbacks;args.DXGIBaseDDI.pDXGIDDIBaseFunctions2=&dxgi;
    auto identity=std::make_shared<dxvk::umd::AdapterIdentity>();identity->luid=selected;identity->runtime=&adapterCookie;
    identity->query=query;identity->generation=31;identity->capabilities=3;
    CHECK(dxvk::umd::createAdapterDevice(identity,&args)==S_OK && dxgi.pfnResolveSharedResource && dxgi.pfnBlt);
  }
  HRESULT resolve(DXGI_DDI_HRESOURCE resource) {
    DXGI_DDI_ARG_RESOLVESHAREDRESOURCE args{};args.hDevice=reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate);args.hResource=resource;
    return dxgi.pfnResolveSharedResource(&args);
  }
  void retire() { CHECK(live);live=false;table.pfnDestroyDevice(device);storage.poison();runtimeValid=false; }
  ~Fixture() { if (live) retire(); }
};
template<typename F>
struct Texture {
  F& fixture;Storage storage;D3D10DDI_HRESOURCE handle;char runtime;
  bool live=true;
  Texture(F& f,DXGI_FORMAT format,bool staging=false,bool presentable=false,Foreign* foreign=nullptr,bool sharing=false)
  :fixture(f),storage(f.table.pfnCalcPrivateResourceSize(f.device,nullptr)),handle{storage.data} {
    lastError=S_OK;
    if (foreign) {
      auto info=foreign->info;D3DDDI_OPENALLOCATIONINFO entry{};entry.hAllocation=foreign->handle;
      entry.pPrivateDriverData=&info;entry.PrivateDriverDataSize=80;
      D3D10DDIARG_OPENRESOURCE args{};args.NumAllocations=1;args.pOpenAllocationInfo=&entry;args.hKMResource.handle=foreign->handle+1000;
      CHECK(f.table.pfnCalcPrivateOpenedResourceSize(f.device,&args)==f.table.pfnCalcPrivateResourceSize(f.device,nullptr));
      f.table.pfnOpenResource(f.device,&args,handle,{&runtime});
    } else {
      D3D10DDI_MIPINFO mip{width,height,1,width,height,1};typename F::Desc desc{};
      desc.pMipInfoList=&mip;desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;desc.Format=format;
      desc.Usage=staging?D3D10_DDI_USAGE_STAGING:D3D10_DDI_USAGE_DEFAULT;
      desc.BindFlags=staging?0:D3D10_DDI_BIND_RENDER_TARGET|D3D10_DDI_BIND_SHADER_RESOURCE;
      if (presentable) desc.BindFlags|=D3D10_DDI_BIND_PRESENT;
      if (sharing) desc.MiscFlags=D3D10_DDI_RESOURCE_MISC_SHARED;
      desc.MapFlags=staging?D3D10_DDI_CPU_ACCESS_READ:0;desc.SampleDesc.Count=1;desc.MipLevels=desc.ArraySize=1;
      f.table.pfnCreateResource(f.device,&desc,handle,{&runtime});
    }
    if (lastError != S_OK) std::fprintf(stderr,
      "Opened primary resource failure format=%u foreign=%u staging=%u presentable=%u error=%08lx\n",
      static_cast<unsigned>(format), unsigned(foreign != nullptr), unsigned(staging), unsigned(presentable),
      static_cast<unsigned long>(lastError));
    CHECK(lastError==S_OK);
  }
  DXGI_DDI_HRESOURCE dxgi() const { return reinterpret_cast<DXGI_DDI_HRESOURCE>(handle.pDrvPrivate); }
  void update(unsigned seed) {
    auto values=image(seed);lastError=S_OK;
    fixture.table.pfnResourceUpdateSubresourceUP(fixture.device,handle,0,nullptr,values.data(),width*4,width*height*4);
    CHECK(lastError==S_OK);
  }
  void retire() { CHECK(live);live=false;fixture.table.pfnDestroyResource(fixture.device,handle);storage.poison(); }
  ~Texture() { if (live) retire(); }
};
static void save(const char* name,const void* data,size_t size) {
  FILE* file=nullptr;CHECK(fopen_s(&file,name,"wb")==0 && file);CHECK(std::fwrite(data,1,size,file)==size);CHECK(std::fclose(file)==0);
}
template<typename F>
static void read(F& f,Texture<F>& source,Texture<F>& staging,unsigned seed,unsigned profile,unsigned format,unsigned snapshot) {
  lastError=S_OK;f.table.pfnResourceCopy(f.device,staging.handle,source.handle);CHECK(lastError==S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.table.pfnStagingResourceMap(f.device,staging.handle,0,D3D10_DDI_MAP_READ,0,&mapped);
  CHECK(lastError==S_OK && mapped.pData && mapped.RowPitch>=width*4);
  std::array<uint32_t,width*height> actual{};
  for (unsigned y=0;y<height;++y) std::memcpy(actual.data()+y*width,static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch,width*4);
  uint32_t metadata[]{width,height,format,seed,mapped.RowPitch,mapped.DepthPitch,pitch,bytes};
  char name[120];std::snprintf(name,sizeof(name),"open-primary-%u-%u-%u.actual.u32.bin",profile,format,snapshot);save(name,actual.data(),sizeof(actual));
  std::snprintf(name,sizeof(name),"open-primary-%u-%u-%u.metadata.u32.bin",profile,format,snapshot);save(name,metadata,sizeof(metadata));
  f.table.pfnStagingResourceUnmap(f.device,staging.handle,0);CHECK(lastError==S_OK);
  for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) { CHECK(actual[y*width+x]==color(seed,x,y));++pixels; }
  ++images;
}
template<typename Table>
static void profile(unsigned index) {
  using F=Fixture<Table>;
  for (unsigned format=1;format<=3;++format) {
    Foreign foreign(format);const auto oldAcquisitions=acquisitions,oldReleases=releases;
    {
      F f;auto dxgiFormat=dxvk::umd::allocationFormat(format);
      Texture<F> opened(f,dxgiFormat,false,false,&foreign),staging(f,dxgiFormat,true),source(f,dxgiFormat,false,true);
      CHECK(acquisitions==oldAcquisitions+2); // Open staging + independent source publication allocation.
      read(f,opened,staging,1,index,format,0);
      kernelPixels(foreign.handle,2,true);f.table.pfnFlush(f.device);CHECK(lastError==S_OK);
      read(f,opened,staging,2,index,format,1);
      opened.update(3);CHECK(f.resolve(opened.dxgi())==S_OK);kernelPixels(foreign.handle,3,false);
      read(f,opened,staging,3,index,format,2);
      source.update(4);DXGI_DDI_ARG_BLT blt{};blt.hDevice=reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);
      blt.hSrcResource=source.dxgi();blt.hDstResource=opened.dxgi();blt.DstRight=width;blt.DstBottom=height;
      blt.Rotate=DXGI_DDI_MODE_ROTATION_IDENTITY;blt.Flags.Present=1;
      CHECK(f.dxgi.pfnBlt(&blt)==S_OK);kernelPixels(foreign.handle,4,false);
      read(f,opened,staging,4,index,format,3);
      auto& retained=backing.at(foreign.handle);
      char name[96];std::snprintf(name,sizeof(name),"open-primary-padded-%u-%u.actual.bin",index,format);save(name,retained.page,16+bytes+16);
      const uint32_t metadata[]{width,height,format,4,pitch,bytes};
      std::snprintf(name,sizeof(name),"open-primary-padded-%u-%u.metadata.u32.bin",index,format);save(name,metadata,sizeof(metadata));
      CHECK(f.resolve(reinterpret_cast<DXGI_DDI_HRESOURCE>(source.handle.pDrvPrivate))==DXGI_DDI_ERR_UNSUPPORTED);
    }
    CHECK(backing.count(foreign.handle) && internalCount()==0 && releases==oldReleases+2);
    kernelPixels(foreign.handle,4,false);
  }
}
template<typename Table>
static void openFrames() {
  Foreign foreign;
  Fixture<Table> f;
  for (unsigned failure=0;failure<26;++failure) {
    const SIZE_T privateBytes=f.table.pfnCalcPrivateResourceSize(f.device,nullptr);
    Storage output(privateBytes);D3D10DDI_HRESOURCE handle{output.data};char runtime;
    std::vector<uint8_t> untouched(privateBytes,0x6d);std::memcpy(output.data,untouched.data(),privateBytes);
    auto info=foreign.info;D3DDDI_OPENALLOCATIONINFO entry{};entry.hAllocation=foreign.handle;
    entry.pPrivateDriverData=&info;entry.PrivateDriverDataSize=80;
    D3D10DDIARG_OPENRESOURCE args{};args.NumAllocations=1;args.pOpenAllocationInfo=&entry;args.hKMResource.handle=foreign.handle+1000;
    HRESULT expected=E_INVALIDARG;
    switch (failure) {
      case 0: break; // Null top-level request.
      case 1: args.pOpenAllocationInfo=nullptr;break;
      case 2: args.NumAllocations=0;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 3: args.NumAllocations=2;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 4: entry.hAllocation=0;break;
      case 5: entry.pPrivateDriverData=nullptr;break;
      case 6: entry.PrivateDriverDataSize=79;break;
      case 7: entry.PrivateDriverDataSize=81;break;
      case 8: args.hKMResource.handle=0;break;
      case 9: info.magic=0;break;
      case 10: info.version=1;break;
      case 11: info.headerSize=79;break;
      case 12: info.reserved=1;break;
      case 13: info.width=0;break;
      case 14: info.height=0;break;
      case 15: info.pitch=width*4-1;break;
      case 16: info.size=bytes-1;break;
      case 17: info.refreshNumerator=0;break;
      case 18: info.refreshDenominator=0;break;
      case 19: info.format=4;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 20: info.contextId=1;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 21: info.requestedIova=4096;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 22: info.size=uint64_t(1)<<32;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 23: info.alignment=8192;expected=DXGI_ERROR_UNSUPPORTED;break;
      case 24: info.pitch|=1;break;
      case 25: info.flags=3;expected=DXGI_ERROR_UNSUPPORTED;break;
    }
    const auto oldAcquisitions=acquisitions,oldReleases=releases,oldErrors=errorCallbacks;
    lastError=S_OK;f.table.pfnOpenResource(f.device,failure?&args:nullptr,handle,{&runtime});
    CHECK(lastError==expected && errorCallbacks==oldErrors+1);
    CHECK(acquisitions==oldAcquisitions && releases==oldReleases && internalCount()==0);
    CHECK(std::memcmp(output.data,untouched.data(),privateBytes)==0);
    f.table.pfnDestroyResource(f.device,handle);CHECK(errorCallbacks==oldErrors+1);
    // The rejected reservation must not prevent a valid open at the same address.
    info=foreign.info;entry.hAllocation=foreign.handle;entry.pPrivateDriverData=&info;entry.PrivateDriverDataSize=80;
    args.NumAllocations=1;args.pOpenAllocationInfo=&entry;args.hKMResource.handle=foreign.handle+1000;
    lastError=S_OK;f.table.pfnOpenResource(f.device,&args,handle,{&runtime});CHECK(lastError==S_OK);
    f.table.pfnDestroyResource(f.device,handle);CHECK(acquisitions==oldAcquisitions+1 && releases==oldReleases+1 && internalCount()==0);
    kernelPixels(foreign.handle,1,false);++failedOpenFrames;
  }
}
template<typename Table>
static void openRetirement() {
  for (bool acquired:{false,true}) {
    Foreign foreign;Fixture<Table> f;
    Storage output(f.table.pfnCalcPrivateResourceSize(f.device,nullptr));D3D10DDI_HRESOURCE handle{output.data};char runtime;
    auto info=foreign.info;D3DDDI_OPENALLOCATIONINFO entry{};entry.hAllocation=foreign.handle;
    entry.pPrivateDriverData=&info;entry.PrivateDriverDataSize=80;
    D3D10DDIARG_OPENRESOURCE args{};args.NumAllocations=1;args.pOpenAllocationInfo=&entry;args.hKMResource.handle=foreign.handle+1000;
    const auto oldAcquisitions=acquisitions,oldReleases=releases,oldErrors=errorCallbacks;
    if (acquired) allocateAfter=[&] { f.retire();output.poison(); };
    else allocateBefore=[&] { f.retire();output.poison();allocateResult=DXGI_ERROR_DEVICE_REMOVED; };
    f.table.pfnOpenResource(f.device,&args,handle,{&runtime});
    CHECK(!f.live && lastError==S_OK && errorCallbacks==oldErrors && internalCount()==0);
    CHECK(acquisitions==oldAcquisitions+unsigned(acquired) && releases==oldReleases+unsigned(acquired));
    kernelPixels(foreign.handle,1,false);allocateResult=S_OK;
  }
}
struct Direct {
  dxvk::umd::RuntimeMemory memory;
  std::shared_ptr<dxvk::umd::RuntimeService> service=std::make_shared<dxvk::umd::RuntimeService>();
  dxvk::umd::RuntimeService::Scope scope{service.get()};
  DXGI_DDI_BASE_CALLBACKS dxgi{};
  bool live=true;
  Direct() { runtimeValid=true;dxgi.pfnPresentCb=present;memory.initialize(&deviceCookie,kernelCallbacks(),&dxgi,{},service); }
  HRESULT close() { CHECK(live);live=false;const auto hr=memory.closeDeviceAllocations();service->close();runtimeValid=false;return hr; }
  ~Direct() { if (live) CHECK(close()==S_OK); }
};
static void ownership() {
  Foreign foreign;
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;auto values=image(11);
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    CHECK(f.memory.upload(allocation,values.data(),width*4)==S_OK);kernelPixels(foreign.handle,11,false);
    kernelPixels(foreign.handle,12,true);std::array<uint32_t,width*height> observed{};
    CHECK(f.memory.download(allocation,observed.data(),width*4)==S_OK);
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) CHECK(observed[y*width+x]==color(12,x,y));
    CHECK(allocation.release()==S_OK && internalCount()==0 && f.close()==S_OK);
  }
  for (unsigned malformed=1;malformed<=5;++malformed) {
    Direct f;dxvk::umd::RuntimeAllocation allocation;malformedAllocation=malformed;
    const auto old=releases;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==E_FAIL);
    CHECK(!allocation.handle() && internalCount()==0 && releases==old+(malformed>=3?1u:0u));malformedAllocation=0;
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;allocateResult=E_OUTOFMEMORY;const auto old=releases;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==E_OUTOFMEMORY);
    CHECK(!allocation.handle() && releases==old && internalCount()==0);allocateResult=S_OK;
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;allocateResult=S_FALSE;const auto old=releases;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==E_FAIL);
    CHECK(!allocation.handle() && releases==old+1 && internalCount()==0);allocateResult=S_OK;
  }
  for (bool moved:{false,true}) {
    Direct f;dxvk::umd::RuntimeAllocation allocation;std::optional<dxvk::umd::RuntimeAllocation> next;
    if (moved) allocateAfter=[&] { next.emplace(std::move(allocation)); };
    else allocateBefore=[&] { CHECK(allocation.release()==S_OK); };
    const auto old=releases;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==DXGI_ERROR_DEVICE_REMOVED);
    CHECK(!allocation.handle() && internalCount()==0 && releases==old+1);
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;allocateBefore=[&] { CHECK(allocation.release()==S_OK); };
    releaseResult=E_OUTOFMEMORY;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==E_OUTOFMEMORY);
    CHECK(!allocation.handle() && internalCount()==1);releaseResult=S_OK;CHECK(f.close()==S_OK && internalCount()==0);
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;std::optional<dxvk::umd::RuntimeAllocation> moved;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    releaseResult=E_OUTOFMEMORY;auto values=image(5);const auto beforeLocks=locks,beforeRenders=renders;
    deallocateAction=[&] {
      CHECK(allocation.release()==DXGI_ERROR_WAS_STILL_DRAWING);
      CHECK(f.memory.upload(allocation,values.data(),width*4)==DXGI_ERROR_WAS_STILL_DRAWING);
      moved.emplace(std::move(allocation));
    };
    CHECK(allocation.release()==E_OUTOFMEMORY && !allocation.handle() && moved && moved->handle()==foreign.handle);
    CHECK(internalCount()==1 && locks==beforeLocks && renders==beforeRenders);
    releaseResult=S_OK;CHECK(moved->release()==S_OK && internalCount()==0);
  }
  {
    Direct f;auto allocation=std::make_unique<dxvk::umd::RuntimeAllocation>();
    CHECK(f.memory.adoptPrimary(*allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    releaseResult=E_OUTOFMEMORY;allocation.reset();CHECK(internalCount()==1);
    releaseResult=S_OK;CHECK(f.close()==S_OK && internalCount()==0);
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation,other;auto values=image(6);
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    CHECK(f.memory.adoptPrimary(other,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    unlockResult=E_OUTOFMEMORY;CHECK(f.memory.upload(allocation,values.data(),width*4)==E_OUTOFMEMORY);
    const auto beforeLocks=locks,beforeRenders=renders;
    CHECK(f.memory.upload(allocation,values.data(),width*4)==E_OUTOFMEMORY && locks==beforeLocks && renders==beforeRenders);
    CHECK(!allocation.canRotateWith(other));unlockResult=S_OK;
    CHECK(allocation.release()==S_OK && other.release()==S_OK && internalCount()==0);
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;auto values=image(7);
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    lockAction=[&] { CHECK(allocation.release()==S_OK); };
    CHECK(f.memory.upload(allocation,values.data(),width*4)==DXGI_ERROR_DEVICE_REMOVED && internalCount()==0);
  }
  for (bool after:{false,true}) {
    Direct f;dxvk::umd::RuntimeAllocation allocation;
    const auto old=releases;
    if (after) allocateAfter=[&] { CHECK(f.close()==S_OK); };
    else allocateBefore=[&] { CHECK(f.close()==S_OK);allocateResult=DXGI_ERROR_DEVICE_REMOVED; };
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==DXGI_ERROR_DEVICE_REMOVED);
    CHECK(!allocation.handle() && internalCount()==0 && releases==old+(after?1u:0u));allocateResult=S_OK;
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;auto values=image(8);
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    lockAction=[&] { CHECK(f.close()==S_OK); };
    CHECK(f.memory.upload(allocation,values.data(),width*4)==DXGI_ERROR_DEVICE_REMOVED && internalCount()==0);
  }
  for (const HRESULT result:{S_OK,E_OUTOFMEMORY}) {
    Direct f;dxvk::umd::RuntimeAllocation allocation;auto values=image(9);
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    unlockResult=result;unlockAction=[&] { CHECK(f.close()==DXGI_ERROR_WAS_STILL_DRAWING); };
    CHECK(f.memory.upload(allocation,values.data(),width*4)==DXGI_ERROR_DEVICE_REMOVED && internalCount()==1);
    runtimeTerminalCleanup();unlockResult=S_OK;
  }
  {
    Direct f;dxvk::umd::RuntimeAllocation allocation;auto values=image(10);
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    unlockResult=E_OUTOFMEMORY;CHECK(f.memory.upload(allocation,values.data(),width*4)==E_OUTOFMEMORY);
    CHECK(f.close()==E_OUTOFMEMORY && internalCount()==1);runtimeTerminalCleanup();unlockResult=S_OK;
  }
  for (const HRESULT result:{S_OK,E_OUTOFMEMORY}) {
    Direct f;dxvk::umd::RuntimeAllocation allocation;
    CHECK(f.memory.adoptPrimary(allocation,foreign.handle,foreign.handle+1000,foreign.info)==S_OK);
    releaseResult=result;deallocateAction=[&] { CHECK(f.close()==DXGI_ERROR_WAS_STILL_DRAWING); };
    CHECK(allocation.release()==DXGI_ERROR_DEVICE_REMOVED);
    if (FAILED(result)) { CHECK(internalCount()==1);runtimeTerminalCleanup(); }
    else CHECK(internalCount()==0);
    releaseResult=S_OK;
  }
  CHECK(internalCount()==0 && backing.count(foreign.handle));
}
template<typename F>
static HRESULT sharedPresent(F& f,Texture<F>& texture,bool displayMode=false) {
  if (displayMode) {
    DXGI_DDI_ARG_SETDISPLAYMODE args{};args.hDevice=reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);
    args.hResource=texture.dxgi();return f.dxgi.pfnSetDisplayMode(&args);
  }
  DXGI_DDI_ARG_PRESENT args{};args.hDevice=reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);
  args.hSurfaceToPresent=texture.dxgi();args.Flags.Blt=1;args.pDXGIContext=&adapterCookie;
  return f.dxgi.pfnPresent(&args);
}
template<typename F>
static void sharedPresentRead(F& f,Texture<F>& source,Texture<F>& staging,
    unsigned profile,unsigned kind,unsigned format,unsigned snapshot,unsigned seed) {
  std::array<uint32_t,width*height> native{},publicWords{},kernel{};
  lastError=S_OK;f.table.pfnResourceCopy(f.device,staging.handle,source.handle);CHECK(lastError==S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.table.pfnStagingResourceMap(f.device,staging.handle,0,D3D10_DDI_MAP_READ,0,&mapped);
  CHECK(lastError==S_OK && mapped.pData && mapped.RowPitch>=width*4);
  const auto nativePitch=mapped.RowPitch;
  for (unsigned y=0;y<height;++y) std::memcpy(native.data()+y*width,static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch,width*4);
  f.table.pfnStagingResourceUnmap(f.device,staging.handle,0);CHECK(lastError==S_OK);
  // An independent public API resource uses the same original caller words.
  // Retain both actual readbacks, not the generated expected array.
  const auto values=image(seed);D3D11_TEXTURE2D_DESC desc{};
  desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
  desc.Format=dxvk::umd::allocationFormat(format);desc.SampleDesc.Count=1;
  desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
  D3D11_SUBRESOURCE_DATA initial{values.data(),width*4,width*height*4};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> publicSource,publicStaging;
  CHECK(publicDevice && publicContext && publicDevice->CreateTexture2D(&desc,&initial,&publicSource)==S_OK);
  desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  CHECK(publicDevice->CreateTexture2D(&desc,nullptr,&publicStaging)==S_OK);
  publicContext->CopyResource(publicStaging.Get(),publicSource.Get());
  D3D11_MAPPED_SUBRESOURCE publicMap{};CHECK(publicContext->Map(publicStaging.Get(),0,D3D11_MAP_READ,0,&publicMap)==S_OK);
  CHECK(publicMap.pData && publicMap.RowPitch>=width*4);const auto publicPitch=publicMap.RowPitch;
  for (unsigned y=0;y<height;++y) std::memcpy(publicWords.data()+y*width,static_cast<uint8_t*>(publicMap.pData)+y*publicMap.RowPitch,width*4);
  publicContext->Unmap(publicStaging.Get(),0);
  const auto& allocation=backing.at(expectedPresentHandle);guards(allocation);
  for (unsigned y=0;y<height;++y) std::memcpy(kernel.data()+y*width,allocation.data()+y*allocation.info.pitch,width*4);
  const uint32_t metadata[]{width,height,format,seed,nativePitch,publicPitch,allocation.info.pitch,
    uint32_t(allocation.info.size),expectedPresentHandle,allocation.info.flags};
  char name[160];
  auto write=[&](const char* suffix,const void* data,size_t size) {
    std::snprintf(name,sizeof(name),"shared-present-%u-%u-%u-%u.%s.bin",profile,kind,format,snapshot,suffix);save(name,data,size);
  };
  write("native.u32",native.data(),sizeof(native));write("public.u32",publicWords.data(),sizeof(publicWords));
  write("kernel.u32",kernel.data(),sizeof(kernel));write("metadata.u32",metadata,sizeof(metadata));
  write("allocation",&allocation.info,sizeof(allocation.info));
  for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
    const auto at=y*width+x,expected=color(seed,x,y);
    CHECK(native[at]==expected && publicWords[at]==expected && kernel[at]==expected);++sharedPresentPixels;
  }
  ++sharedPresentImages;
}
template<typename Table>
static void sharedPresentProfile(unsigned index) {
  using F=Fixture<Table>;
  for (unsigned format=1;format<=3;++format) {
    Foreign foreign(format);const auto acquired=acquisitions,released=releases;
    {
      F f;const auto dxgiFormat=dxvk::umd::allocationFormat(format);
      Texture<F> opened(f,dxgiFormat,false,false,&foreign),staging(f,dxgiFormat,true);
      expectedPresentHandle=foreign.handle;
      CHECK(acquisitions==acquired+1 && internalCount()==1);
      CHECK(sharedPresent(f,opened)==S_OK);
      sharedPresentRead(f,opened,staging,index,0,format,0,1);
      kernelPixels(foreign.handle,2,true);
      CHECK(sharedPresent(f,opened)==S_OK);
      sharedPresentRead(f,opened,staging,index,0,format,1,2);
      opened.update(3);CHECK(sharedPresent(f,opened,true)==S_OK);
      sharedPresentRead(f,opened,staging,index,0,format,2,3);
      kernelPixels(foreign.handle,7,true);CHECK(sharedPresent(f,opened,true)==S_OK);
      sharedPresentRead(f,opened,staging,index,0,format,3,7);
      opened.update(3);const auto before=sharedPresents;
      lockAction=[&] {
        CHECK(sharedPresent(f,opened)==DXGI_ERROR_WAS_STILL_DRAWING);
        CHECK(f.resolve(opened.dxgi())==DXGI_ERROR_WAS_STILL_DRAWING);sharedNegativeCases+=2;
      };
      CHECK(sharedPresent(f,opened)==S_OK && sharedPresents==before+1);
    }
    CHECK(acquisitions==acquired+1 && releases==released+1 && internalCount()==0 && backing.count(foreign.handle));
  }
  for (unsigned format:{1u,3u}) {
    const auto acquired=acquisitions,released=releases;
    F f;const auto dxgiFormat=dxvk::umd::allocationFormat(format);
    Texture<F> source(f,dxgiFormat,false,true,nullptr,true),staging(f,dxgiFormat,true);
    CHECK(acquisitions==acquired+1 && internalCount()==1);
    expectedPresentHandle=0;
    for (const auto& entry:backing) if (entry.second.resource==&source.runtime) {
      CHECK(!expectedPresentHandle);expectedPresentHandle=entry.first;
    }
    CHECK(expectedPresentHandle && backing.at(expectedPresentHandle).info.flags==2);
    D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc{};viewDesc.hDrvResource=source.handle;
    viewDesc.Format=dxgiFormat;viewDesc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
    viewDesc.Tex2D.ArraySize=1;
    Storage viewStorage(f.table.pfnCalcPrivateRenderTargetViewSize(f.device,&viewDesc));
    D3D10DDI_HRENDERTARGETVIEW view{viewStorage.data};
    f.table.pfnCreateRenderTargetView(f.device,&viewDesc,view,{});CHECK(lastError==S_OK);
    source.update(4);CHECK(sharedPresent(f,source)==S_OK);
    sharedPresentRead(f,source,staging,index,1,format,0,4);
    auto& allocation=backing.at(expectedPresentHandle);
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
      const auto value=color(5,x,y);std::memcpy(allocation.data()+y*allocation.info.pitch+x*4,&value,4);
    }
    CHECK(sharedPresent(f,source)==S_OK);
    sharedPresentRead(f,source,staging,index,1,format,1,5);
    source.retire();CHECK(releases==released && internalCount()==1);
    CHECK(sharedPresent(f,source)==E_INVALIDARG);++sharedNegativeCases;
    f.table.pfnDestroyRenderTargetView(f.device,view);viewStorage.poison();
    CHECK(releases==released+1 && internalCount()==0);
  }
  // Invalid shape rejection must happen before acquiring a second backing.
  for (unsigned failure=0;failure<6;++failure) {
    F f;D3D10DDI_MIPINFO mip{width,height,1,width,height,1};typename F::Desc desc{};
    desc.pMipInfoList=&mip;desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D10_DDI_USAGE_DEFAULT;
    desc.BindFlags=D3D10_DDI_BIND_PRESENT|D3D10_DDI_BIND_RENDER_TARGET;
    desc.MiscFlags=D3D10_DDI_RESOURCE_MISC_SHARED;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    DXGI_DDI_PRIMARY_DESC primary{};
    switch (failure) {
      case 0: desc.SampleDesc.Count=4;break;
      case 1: desc.MapFlags=D3D10_DDI_CPU_ACCESS_READ;break;
      case 2: desc.ArraySize=2;break;
      case 3: desc.MipLevels=2;break;
      case 4: desc.Format=DXGI_FORMAT_B8G8R8X8_UNORM;break;
      case 5:
        primary.ModeDesc.Width=width;primary.ModeDesc.Height=height;primary.ModeDesc.Format=desc.Format;
        primary.ModeDesc.RefreshRate={60000,1001};
        primary.ModeDesc.ScanlineOrdering=DXGI_DDI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        primary.ModeDesc.Rotation=DXGI_DDI_MODE_ROTATION_IDENTITY;
        primary.ModeDesc.Scaling=DXGI_DDI_MODE_SCALING_UNSPECIFIED;
        desc.pPrimaryDesc=&primary;break;
    }
    Storage storage(f.table.pfnCalcPrivateResourceSize(f.device,&desc));std::memset(storage.data,0x6d,128);
    const auto acquired=acquisitions;lastError=S_OK;char runtime;
    f.table.pfnCreateResource(f.device,&desc,{storage.data},{&runtime});
    CHECK(lastError==DXGI_ERROR_UNSUPPORTED && acquisitions==acquired && internalCount()==0);
    for (unsigned i=0;i<128;++i) CHECK(static_cast<uint8_t*>(storage.data)[i]==0x6d);
    ++sharedNegativeCases;
  }
  for (bool terminal:{false,true}) {
    Foreign foreign;F f;Texture<F> opened(f,DXGI_FORMAT_R8G8B8A8_UNORM,false,false,&foreign);
    expectedPresentHandle=foreign.handle;opened.update(6);const auto before=sharedPresents;
    lockAction=[&] { opened.retire();if (terminal) f.retire(); };
    CHECK(sharedPresent(f,opened)==DXGI_ERROR_DEVICE_REMOVED && sharedPresents==before);
    CHECK(internalCount()==0 && backing.count(foreign.handle));++sharedNegativeCases;
  }
  for (bool displayMode:{false,true}) for (bool terminal:{false,true}) {
    Foreign foreign;F f;Texture<F> opened(f,DXGI_FORMAT_R8G8B8A8_UNORM,false,false,&foreign);
    expectedPresentHandle=foreign.handle;opened.update(6);
    auto retire=[&] { opened.retire();if (terminal) f.retire(); };
    if (displayMode) modeAction=retire;else presentAction=retire;
    CHECK(sharedPresent(f,opened,displayMode)==DXGI_ERROR_DEVICE_REMOVED);
    CHECK(internalCount()==0 && backing.count(foreign.handle));++sharedNegativeCases;
  }
  publicContext.Reset();publicDevice.Reset();
}
int main() {
  caller=GetCurrentThreadId();profile<D3D10DDI_DEVICEFUNCS>(0);profile<D3D10_1DDI_DEVICEFUNCS>(1);profile<D3D11DDI_DEVICEFUNCS>(2);
  openFrames<D3D10DDI_DEVICEFUNCS>();openFrames<D3D10_1DDI_DEVICEFUNCS>();openFrames<D3D11DDI_DEVICEFUNCS>();
  openRetirement<D3D10DDI_DEVICEFUNCS>();openRetirement<D3D10_1DDI_DEVICEFUNCS>();openRetirement<D3D11DDI_DEVICEFUNCS>();
  ownership();
  CHECK(images==36 && pixels==1260 && backing.empty() && contexts==contextCloses);
  CHECK(locks==unlocks+runtimeTerminalMapClosures && acquisitions==releases+runtimeTerminalReleases);
  CHECK(runtimeTerminalReleases==4 && runtimeTerminalMapClosures==2);
  CHECK(errorCallbacks==78 && failedOpenFrames==78);
  CHECK(!allocateBefore && !allocateAfter && !deallocateAction && !lockAction && !unlockAction && !renderAction);
  const auto openedPrimaryChecks=checks.load(),openedPrimaryCallbacks=errorCallbacks;
  sharedPresentChecks=true;
  sharedPresentProfile<D3D10DDI_DEVICEFUNCS>(0);sharedPresentProfile<D3D10_1DDI_DEVICEFUNCS>(1);sharedPresentProfile<D3D11DDI_DEVICEFUNCS>(2);
  CHECK(sharedPresentImages==48 && sharedPresentPixels==1680 && sharedPresents==45 && sharedModes==24 && sharedNegativeCases==60);
  CHECK(backing.empty() && contexts==contextCloses && locks==unlocks+runtimeTerminalMapClosures
    && acquisitions==releases+runtimeTerminalReleases);
  CHECK(!presentAction && !modeAction && !lockAction);
  for (auto page:retiredPages) CHECK(VirtualFree(page,0,MEM_RELEASE));
  std::printf("DXGI opened primary PASS checks=%u profiles=3 formats=3 images=%u pixels=%u failures=%u callbacks=%u runtime_terminal_releases=%u runtime_terminal_maps=%u hardware_admission=0\n",
    openedPrimaryChecks,images,pixels,failedOpenFrames,openedPrimaryCallbacks,runtimeTerminalReleases,runtimeTerminalMapClosures);
  std::printf("DXGI shared present PASS profiles=3 images=%u pixels=%u presents=%u modes=%u negatives=%u raw_files=240 hardware_admission=0\n",
    sharedPresentImages,sharedPresentPixels,sharedPresents,sharedModes,sharedNegativeCases);
}
