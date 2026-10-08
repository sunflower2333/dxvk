// SPDX-License-Identifier: MIT
// Reuse the original typed callback ownership fixture without changing its
// assertions or running its main. Public WARP readbacks remain pixel controls,
// not proof of ordinary DXGI sharing, scanout or hardware admission.
// Its renamed entry point is never called. Renaming removes C++ main's
// implicit return-zero rule; contain that diagnostic to this unchanged import.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4715)
#endif
#define main inheritedOpenedPrimaryMain
#include "umd-dxgi-open-primary.cpp"
#undef main
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

static unsigned optionalImages, optionalPixels, optionalNegatives;

template<typename F>
struct OptionalShared {
  F& fixture;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  char runtime;
  bool live=true;
  UINT driverFlags=0x6d6d6d6d;
  D3DKMT_HANDLE allocation=0;
  OptionalShared(F& f,DXGI_FORMAT format,OptionalShared* creator=nullptr)
  :fixture(f),storage(f.table.pfnCalcPrivateResourceSize(f.device,nullptr)),handle{storage.data} {
    lastError=S_OK;
    if (creator) {
      allocation=creator->allocation;
      auto info=backing.at(allocation).info;
      D3DDDI_OPENALLOCATIONINFO opened{};
      opened.hAllocation=allocation;opened.pPrivateDriverData=&info;opened.PrivateDriverDataSize=sizeof(info);
      D3D10DDIARG_OPENRESOURCE args{};
      args.NumAllocations=1;args.pOpenAllocationInfo=&opened;args.hKMResource.handle=allocation+1000;
      f.table.pfnOpenResource(f.device,&args,handle,{&runtime});
      driverFlags=creator->driverFlags;
    } else {
      auto words=image(4);
      D3D10_DDIARG_SUBRESOURCE_UP initial{words.data(),width*4,width*height*4};
      D3D10DDI_MIPINFO mip{width,height,1,width,height,1};
      typename F::Desc args{};
      args.pMipInfoList=&mip;args.pInitialDataUP=&initial;
      args.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;args.Format=format;
      args.Usage=D3D10_DDI_USAGE_DEFAULT;
      args.BindFlags=D3D10_DDI_BIND_RENDER_TARGET|D3D10_DDI_BIND_SHADER_RESOURCE|D3D10_DDI_BIND_PRESENT;
      args.MiscFlags=D3D10_DDI_RESOURCE_MISC_SHARED;
      args.MipLevels=args.ArraySize=args.SampleDesc.Count=1;
      DXGI_DDI_PRIMARY_DESC primary{};
      primary.Flags=DXGI_DDI_PRIMARY_OPTIONAL;
      if (format==DXGI_FORMAT_R8G8B8A8_UNORM) primary.Flags|=DXGI_DDI_PRIMARY_NONPREROTATED;
      primary.DriverFlags=driverFlags;
      primary.ModeDesc={width,height,format,{60000,1001},DXGI_DDI_MODE_SCANLINE_ORDER_PROGRESSIVE,
        DXGI_DDI_MODE_ROTATION_IDENTITY,DXGI_DDI_MODE_SCALING_UNSPECIFIED};
      args.pPrimaryDesc=&primary;
      f.table.pfnCreateResource(f.device,&args,handle,{&runtime});
      driverFlags=primary.DriverFlags;
      for (const auto& entry:backing) if (entry.second.resource==&runtime) {
        CHECK(!allocation);allocation=entry.first;
      }
    }
    CHECK(lastError==S_OK && allocation && driverFlags==DXGI_DDI_PRIMARY_DRIVER_FLAG_NO_SCANOUT);
    CHECK(backing.at(allocation).info.flags==2 && backing.at(allocation).info.pitch==width*4);
  }
  DXGI_DDI_HRESOURCE dxgi() const { return reinterpret_cast<DXGI_DDI_HRESOURCE>(handle.pDrvPrivate); }
  void update(unsigned seed) {
    const auto words=image(seed);lastError=S_OK;
    fixture.table.pfnResourceUpdateSubresourceUP(fixture.device,handle,0,nullptr,words.data(),width*4,width*height*4);
    CHECK(lastError==S_OK);
  }
  void retire() { CHECK(live);live=false;fixture.table.pfnDestroyResource(fixture.device,handle);storage.poison(); }
  ~OptionalShared() { if (live) retire(); }
};

template<typename F>
static HRESULT optionalPresent(F& f,OptionalShared<F>& source,bool displayMode=false) {
  if (displayMode) {
    DXGI_DDI_ARG_SETDISPLAYMODE args{};
    args.hDevice=reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);args.hResource=source.dxgi();
    return f.dxgi.pfnSetDisplayMode(&args);
  }
  DXGI_DDI_ARG_PRESENT args{};
  args.hDevice=reinterpret_cast<DXGI_DDI_HDEVICE>(f.device.pDrvPrivate);
  args.hSurfaceToPresent=source.dxgi();args.Flags.Blt=1;args.pDXGIContext=&adapterCookie;
  return f.dxgi.pfnPresent(&args);
}

template<typename F>
static void optionalRead(F& f,OptionalShared<F>& source,Texture<F>& staging,
    unsigned profile,unsigned format,unsigned snapshot,unsigned seed) {
  std::array<uint32_t,width*height> native{},reference{},kernel{};
  lastError=S_OK;f.table.pfnResourceCopy(f.device,staging.handle,source.handle);CHECK(lastError==S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.table.pfnStagingResourceMap(f.device,staging.handle,0,D3D10_DDI_MAP_READ,0,&mapped);
  CHECK(lastError==S_OK && mapped.pData && mapped.RowPitch>=width*4);
  const auto nativePitch=mapped.RowPitch;
  for (unsigned y=0;y<height;++y) std::memcpy(native.data()+y*width,
    static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch,width*4);
  f.table.pfnStagingResourceUnmap(f.device,staging.handle,0);CHECK(lastError==S_OK);
  const auto words=image(seed);
  D3D11_TEXTURE2D_DESC desc{};
  desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
  desc.Format=dxvk::umd::allocationFormat(format);desc.SampleDesc.Count=1;
  desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
  D3D11_SUBRESOURCE_DATA initial{words.data(),width*4,width*height*4};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> original,readback;
  CHECK(publicDevice->CreateTexture2D(&desc,&initial,&original)==S_OK);
  desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  CHECK(publicDevice->CreateTexture2D(&desc,nullptr,&readback)==S_OK);
  publicContext->CopyResource(readback.Get(),original.Get());
  D3D11_MAPPED_SUBRESOURCE publicMap{};
  CHECK(publicContext->Map(readback.Get(),0,D3D11_MAP_READ,0,&publicMap)==S_OK && publicMap.pData);
  const auto publicPitch=publicMap.RowPitch;CHECK(publicPitch>=width*4);
  for (unsigned y=0;y<height;++y) std::memcpy(reference.data()+y*width,
    static_cast<uint8_t*>(publicMap.pData)+y*publicMap.RowPitch,width*4);
  publicContext->Unmap(readback.Get(),0);
  const auto& allocation=backing.at(source.allocation);guards(allocation);
  for (unsigned y=0;y<height;++y) std::memcpy(kernel.data()+y*width,
    allocation.data()+y*allocation.info.pitch,width*4);
  const uint32_t metadata[]{width,height,format,seed,nativePitch,publicPitch,allocation.info.pitch,
    uint32_t(allocation.info.size),source.allocation,allocation.info.flags,source.driverFlags};
  char name[128];auto write=[&](const char* suffix,const void* data,size_t size) {
    std::snprintf(name,sizeof(name),"optional-shared-%u-%u-%u.%s.bin",profile,format,snapshot,suffix);
    save(name,data,size);
  };
  write("native.u32",native.data(),sizeof(native));write("public.u32",reference.data(),sizeof(reference));
  write("kernel.u32",kernel.data(),sizeof(kernel));write("metadata.u32",metadata,sizeof(metadata));
  write("allocation",&allocation.info,sizeof(allocation.info));
  for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
    const auto at=y*width+x,expected=color(seed,x,y);
    CHECK(native[at]==expected && reference[at]==expected && kernel[at]==expected);++optionalPixels;
  }
  ++optionalImages;
}

template<typename Table>
static void optionalProfile(unsigned profile) {
  using F=Fixture<Table>;
  for (unsigned format:{1u,3u}) {
    F f;const auto acquired=acquisitions,released=releases;
    OptionalShared<F> creator(f,dxvk::umd::allocationFormat(format));
    Texture<F> staging(f,dxvk::umd::allocationFormat(format),true);
    CHECK(acquisitions==acquired+1 && internalCount()==1);
    expectedPresentHandle=creator.allocation;
    CHECK(optionalPresent(f,creator)==S_OK);
    optionalRead(f,creator,staging,profile,format,0,4);
    const auto modes=sharedModes;
    CHECK(optionalPresent(f,creator,true)==DXGI_DDI_ERR_UNSUPPORTED && sharedModes==modes);++optionalNegatives;
    {
      OptionalShared<F> opened(f,dxvk::umd::allocationFormat(format),&creator);
      CHECK(acquisitions==acquired+1 && releases==released);
      optionalRead(f,opened,staging,profile,format,1,4);
      opened.update(6);CHECK(f.resolve(opened.dxgi())==S_OK);
      CHECK(optionalPresent(f,creator)==S_OK);
      optionalRead(f,creator,staging,profile,format,2,6);
      creator.update(7);CHECK(f.resolve(creator.dxgi())==S_OK);
      CHECK(f.resolve(opened.dxgi())==S_OK);
      optionalRead(f,opened,staging,profile,format,3,7);
    }
    CHECK(releases==released && internalCount()==1);
    D3D10DDIARG_CREATERENDERTARGETVIEW viewDesc{};
    viewDesc.hDrvResource=creator.handle;viewDesc.Format=dxvk::umd::allocationFormat(format);
    viewDesc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;viewDesc.Tex2D.ArraySize=1;
    Storage viewStorage(f.table.pfnCalcPrivateRenderTargetViewSize(f.device,&viewDesc));
    D3D10DDI_HRENDERTARGETVIEW view{viewStorage.data};
    lastError=S_OK;f.table.pfnCreateRenderTargetView(f.device,&viewDesc,view,{});CHECK(lastError==S_OK);
    creator.retire();CHECK(releases==released && internalCount()==1);
    CHECK(optionalPresent(f,creator)==E_INVALIDARG);++optionalNegatives;
    f.table.pfnDestroyRenderTargetView(f.device,view);viewStorage.poison();
    CHECK(releases==released+1 && internalCount()==0);
  }
  publicContext.Reset();publicDevice.Reset();
}

int main() {
  caller=GetCurrentThreadId();sharedPresentChecks=true;
  optionalProfile<D3D10DDI_DEVICEFUNCS>(0);
  optionalProfile<D3D10_1DDI_DEVICEFUNCS>(1);
  optionalProfile<D3D11DDI_DEVICEFUNCS>(2);
  CHECK(optionalImages==24 && optionalPixels==840 && optionalNegatives==12);
  CHECK(backing.empty() && acquisitions==releases && locks==unlocks && contexts==contextCloses);
  CHECK(!sharedModes && sharedPresents==12);
  for (auto page:retiredPages) CHECK(VirtualFree(page,0,MEM_RELEASE));
  std::printf("DXGI optional shared primary PASS checks=%u profiles=3 formats=2 images=24 pixels=840 negatives=12 raw_files=120 hardware_admission=0\n",checks.load());
}
