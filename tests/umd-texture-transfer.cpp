// SPDX-License-Identifier: MIT
// Actual production native DDIs, with an explicit test-only CPU WARP backend.
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_ddi.h"
#include "../src/umd/umd_texture1d.h"
#include "../src/umd/umd_transfer_format.h"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

static unsigned checks, byteChecks;
static HRESULT lastError = S_OK;
static DWORD callerThread;
// Report native callback errors at the first failed independent assertion.
static void check(bool result, unsigned line) {
  ++checks;
  if (!result) {
    std::fprintf(stderr, "FAIL texture transfer line=%u error=%08lx\n",
      line, static_cast<unsigned long>(lastError));
    std::exit(1);
  }
}
#define CHECK(c) check(!!(c), __LINE__)
// Native runtime callbacks may not migrate to the worker running WARP.
static void APIENTRY reportError(D3D10DDI_HRTCORELAYER, HRESULT result) {
  CHECK(GetCurrentThreadId() == callerThread);
  lastError = result;
}
// These replacements are linked only into this executable, never the UMD DLL.
HRESULT dxvk::umd::createDevice(
    const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context,
    const dxvk::umd::RuntimeBackend*) noexcept {
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
}
// No staging-busy shortcut is used to replace a real synchronized Map.
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept {
  return E_NOTIMPL;
}
// Flush only this test's software backend, with no target-GPU completion claim.
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  context->Flush(); return S_OK;
}
struct Storage {
  std::unique_ptr<void, decltype(&std::free)> bytes;
  // Use independent runtime-owned storage for every published native object.
  explicit Storage(SIZE_T size) : bytes(std::calloc(1,size), &std::free) { CHECK(size && bytes); }
  // Preserve the real WDK handle types at the native ABI boundary.
  template<typename T> T handle() const { return {bytes.get()}; }
};
struct Fixture {
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device = storage.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_DEVICEFUNCS f{};
  // Exercise the production test-admission path without driver registration.
  Fixture() {
    LUID luid{};
    D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = reportError;
    CHECK(VioGpuDxvkCreateDdiTestDevice(&luid, device, {}, &core, &f) == S_OK);
  }
  // All child resources are destroyed before private device storage is freed.
  ~Fixture() { f.pfnDestroyDevice(device); }
};
struct Texture {
  Fixture& fixture;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  // Create the real embedded texture through the published DDI table.
  Texture(Fixture& owner, const D3D10DDIARG_CREATERESOURCE& desc)
    : fixture(owner), storage(owner.f.pfnCalcPrivateResourceSize(owner.device, &desc)),
      handle(storage.handle<D3D10DDI_HRESOURCE>()) {
    owner.f.pfnCreateResource(owner.device, &desc, handle, {}); CHECK(lastError == S_OK);
  }
  // Exercise production retirement before releasing runtime private bytes.
  ~Texture() { fixture.f.pfnDestroyResource(fixture.device, handle); }
};
// Consume only explicitly expected rejections; do not conceal previous errors.
static void rejected() { CHECK(lastError == E_INVALIDARG); lastError = S_OK; }

// Check the previous Texture1D translation using actual SDK/WDK types.
static void descriptorChecks() {
  D3D11_TEXTURE1D_DESC desc{};
  desc.Width=8; desc.MipLevels=4; desc.ArraySize=2;
  desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  D3D10DDIARG_CREATESHADERRESOURCEVIEW native{};
  native.ResourceDimension=D3D10DDIRESOURCE_TEXTURE1D;
  native.Format=DXGI_FORMAT_R8_UNORM;
  native.Tex1D.MostDetailedMip=1; native.Tex1D.MipLevels=UINT32_MAX;
  native.Tex1D.FirstArraySlice=1; native.Tex1D.ArraySize=UINT32_MAX;
  D3D11_SHADER_RESOURCE_VIEW_DESC view{};
  CHECK(dxvk::umd::textureShaderView(native,desc,view));
  CHECK(view.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE1DARRAY);
  CHECK(view.Texture1DArray.MipLevels==3 && view.Texture1DArray.ArraySize==1);
  native.Tex1D.FirstArraySlice=UINT32_MAX;
  CHECK(!dxvk::umd::textureShaderView(native,desc,view));
  for (auto format : {DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_NV12,
      DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R1_UNORM, DXGI_FORMAT_R8G8_B8G8_UNORM})
    CHECK(dxvk::umd::transferTexelBytes(format)==0);
}

// Execute initial data, partial upload/copy and all mip/array readbacks on WARP.
static void texture1D(Fixture& fixture, DXGI_FORMAT format, UINT bpp) {
  constexpr UINT mipCount=4, layers=2, count=mipCount*layers;
  auto& f=fixture.f; const auto device=fixture.device;
  D3D10DDI_MIPINFO mips[mipCount]{};
  D3D10_DDIARG_SUBRESOURCE_UP initial[count]{};
  std::array<std::vector<unsigned char>,count> expected;
  for (UINT mip=0; mip<mipCount; ++mip) {
    const UINT width=8u>>mip;
    mips[mip]={width,1,1,width,1,1};
  }
  for (UINT i=0; i<count; ++i) {
    expected[i].resize((8u>>(i%mipCount))*bpp);
    for (size_t n=0; n<expected[i].size(); ++n) expected[i][n]=static_cast<unsigned char>(17*i+n);
    initial[i]={expected[i].data(),UINT(expected[i].size()),0};
  }
  D3D10DDIARG_CREATERESOURCE desc{};
  desc.pMipInfoList=mips; desc.pInitialDataUP=initial;
  desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE1D; desc.Usage=D3D10_DDI_USAGE_DEFAULT;
  desc.Format=format; desc.SampleDesc.Count=1; desc.MipLevels=mipCount; desc.ArraySize=layers;
  Texture source(fixture,desc), destination(fixture,desc);
  std::vector<unsigned char> update(2*bpp,0x6d);
  D3D10_DDI_BOX box{1,0,0,3,1,1};
  f.pfnResourceUpdateSubresourceUP(device,destination.handle,5,&box,update.data(),0,0);
  CHECK(lastError==S_OK);
  std::copy(update.begin(),update.end(),expected[5].begin()+bpp);
  box={0,0,0,1,1,1};
  f.pfnResourceCopyRegion(device,destination.handle,5,0,0,0,source.handle,1,&box);
  CHECK(lastError==S_OK);
  std::copy_n(expected[1].begin(),bpp,expected[5].begin());
  box={0,0,0,0,1,1};
  f.pfnResourceUpdateSubresourceUP(device,destination.handle,5,&box,nullptr,0,0);
  CHECK(lastError==S_OK);
  box={0,0,0,5,1,1};
  f.pfnResourceUpdateSubresourceUP(device,destination.handle,5,&box,update.data(),0,0); rejected();
  f.pfnResourceCopyRegion(device,destination.handle,5,4,0,0,source.handle,1,nullptr); rejected();
  f.pfnResourceUpdateSubresourceUP(device,destination.handle,UINT32_MAX,nullptr,update.data(),0,0); rejected();
  desc.pInitialDataUP=nullptr; desc.Usage=D3D10_DDI_USAGE_STAGING;
  desc.MapFlags=D3D10_DDI_CPU_ACCESS_READ;
  Texture staging(fixture,desc);
  f.pfnResourceCopy(device,staging.handle,destination.handle); CHECK(lastError==S_OK);
  for (UINT i=0; i<count; ++i) {
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    f.pfnStagingResourceMap(device,staging.handle,i,D3D10_DDI_MAP_READ,0,&mapped);
    CHECK(lastError==S_OK && mapped.pData);
    const auto data=static_cast<const unsigned char*>(mapped.pData);
    for (size_t n=0; n<expected[i].size(); ++n) { ++byteChecks; CHECK(data[n]==expected[i][n]); }
    f.pfnStagingResourceUnmap(device,staging.handle,i); CHECK(lastError==S_OK);
  }
  std::printf("PASS native Texture1D format=%u texelBytes=%u subresources=%u\n",UINT(format),bpp,count);
}

// Test non-RGBA8 2D rows, undersized pitch and no-op/failed-update preservation.
static void texture2D(Fixture& fixture) {
  constexpr UINT width=4,height=3,bpp=2,pitch=12;
  std::array<unsigned char,pitch*height> source{};
  for (UINT y=0;y<height;++y)
    for (UINT x=0;x<width*bpp;++x) source[y*pitch+x]=static_cast<unsigned char>(y*31+x);
  D3D10DDI_MIPINFO mip{width,height,1,width,height,1};
  D3D10DDIARG_CREATERESOURCE desc{};
  desc.pMipInfoList=&mip; desc.ResourceDimension=D3D10DDIRESOURCE_TEXTURE2D;
  desc.Usage=D3D10_DDI_USAGE_DEFAULT; desc.Format=DXGI_FORMAT_R16_UINT;
  desc.SampleDesc.Count=1; desc.MipLevels=desc.ArraySize=1;
  Texture texture(fixture,desc);
  auto& f=fixture.f; const auto device=fixture.device;
  f.pfnResourceUpdateSubresourceUP(device,texture.handle,0,nullptr,source.data(),pitch,0); CHECK(lastError==S_OK);
  f.pfnResourceUpdateSubresourceUP(device,texture.handle,0,nullptr,source.data(),7,0); rejected();
  // This pointer is never dereferenced: the production arithmetic guard must
  // reject a footprint which crosses the caller's address-space boundary.
  f.pfnResourceUpdateSubresourceUP(device,texture.handle,0,nullptr,
    reinterpret_cast<const void*>(UINTPTR_MAX-3),pitch,0); rejected();
  desc.Usage=D3D10_DDI_USAGE_STAGING; desc.MapFlags=D3D10_DDI_CPU_ACCESS_READ;
  Texture staging(fixture,desc);
  f.pfnResourceCopy(device,staging.handle,texture.handle); CHECK(lastError==S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.pfnStagingResourceMap(device,staging.handle,0,D3D10_DDI_MAP_READ,0,&mapped);
  CHECK(lastError==S_OK && mapped.pData);
  for (UINT y=0;y<height;++y)
    for (UINT x=0;x<width*bpp;++x) {
      ++byteChecks;
      CHECK(static_cast<const unsigned char*>(mapped.pData)[y*mapped.RowPitch+x]==source[y*pitch+x]);
    }
  f.pfnStagingResourceUnmap(device,staging.handle,0); CHECK(lastError==S_OK);
  std::puts("PASS native Texture2D: typed rows, pitch and pointer-wrap guards");
}

// CPU reference execution never enables the native VIOGPU runtime or INF.
int main() {
  callerThread=GetCurrentThreadId();
  descriptorChecks();
  {
    Fixture fixture;
    for (const auto entry : {std::pair{DXGI_FORMAT_R8_UNORM,1u},
        std::pair{DXGI_FORMAT_R16_UINT,2u},std::pair{DXGI_FORMAT_R32_UINT,4u},
        std::pair{DXGI_FORMAT_R32G32_UINT,8u},std::pair{DXGI_FORMAT_R32G32B32A32_UINT,16u}}) {
      CHECK(dxvk::umd::transferTexelBytes(entry.first)==entry.second);
      texture1D(fixture,entry.first,entry.second);
    }
    texture2D(fixture);
  }
  CHECK(lastError==S_OK);
  std::printf("PASS native texture transfers: checks=%u byte-checks=%u\n",checks,byteChecks);
  std::puts("BACKEND=WARP; VIOGPU_GPU_ACCEPTANCE=NOT_RUN; INSTALLATION=NONE");
}
