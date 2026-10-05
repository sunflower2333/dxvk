// SPDX-License-Identifier: MIT
// Exercise the published production DXGI rotation DDI on a test-only WARP
// backend, including real views, readback and controlled kernel allocations.
#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_allocation.h"
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

static unsigned checks, locks, releases;
static DWORD caller;
static HRESULT lastError = S_OK, lockResult = S_OK;
static D3DKMT_HANDLE presented;
static char deviceCookie, contextCookie, dxgiCookie;
static LUID luid = {17, 0};
#define CHECK(x) do { ++checks; if (!(x)) { \
  std::fprintf(stderr, "rotation line=%d check=%s error=%08lx\n", __LINE__, #x, \
    static_cast<unsigned long>(lastError)); std::abort(); } } while (0)
struct Backing {
  HANDLE runtime;
  D3DKMT_HANDLE allocation;
  std::vector<uint32_t> pixels;
  bool live = true;
};
static std::vector<Backing> backings;
static std::unordered_map<HANDLE,D3DKMT_HANDLE> runtimeBacking;
static std::unordered_map<DXGI_DDI_HRESOURCE,HANDLE> runtimeResources;
static std::unordered_set<HANDLE> retiredResources;
static std::function<void()> lockAction;
static void runtimeCaller() { CHECK(GetCurrentThreadId() == caller); }
static HRESULT APIENTRY query(HANDLE, const D3DDDICB_QUERYADAPTERINFO* args) {
  runtimeCaller(); CHECK(args && args->PrivateDriverDataSize == 160);
  auto bytes = static_cast<unsigned char*>(args->pPrivateDriverData);
  auto set = [bytes](unsigned at, uint64_t value, unsigned length) {
    for (unsigned i = 0; i < length; ++i) bytes[at+i] = uint8_t(value >> (8*i));
  };
  set(0, 0x504d5644, 4); set(8, 128, 4); set(16, 3, 8); set(24, 19, 8);
  set(128, 0x44494c56, 4); set(132, 1, 4); set(136, 32, 4); set(140, 1, 4); set(152, 1, 4);
  std::memcpy(bytes+144, &luid, sizeof(luid)); return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  runtimeCaller(); CHECK(device == &deviceCookie && args->hResource);
  CHECK(args->NumAllocations == 1 && args->pAllocationInfo);
  dxvk::umd::AllocationInfo info;
  CHECK(args->pAllocationInfo->PrivateDriverDataSize == sizeof(info));
  std::memcpy(&info, args->pAllocationInfo->pPrivateDriverData, sizeof(info));
  const auto id = D3DKMT_HANDLE(100 + backings.size());
  backings.push_back({args->hResource, id, std::vector<uint32_t>(size_t(info.size/4))});
  CHECK(runtimeBacking.emplace(args->hResource,id).second);
  args->pAllocationInfo->hAllocation = id; args->hKMResource = id + 1000;
  return S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  runtimeCaller(); CHECK(device == &deviceCookie && args->hResource);
  CHECK(!args->NumAllocations && !args->HandleList);
  CHECK(retiredResources.erase(args->hResource) == 1);
  // Runtime resource handles remain stable across rotation. The runtime owns
  // their kernel association, not the driver's private allocation wrapper.
  const auto association = runtimeBacking.find(args->hResource);
  CHECK(association != runtimeBacking.end());
  bool found = false;
  for (auto& backing : backings) if (backing.allocation == association->second) {
    CHECK(backing.live); backing.live = false; found = true;
  }
  CHECK(found); runtimeBacking.erase(association); ++releases; return S_OK;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  runtimeCaller(); CHECK(device == &deviceCookie && args->Flags.LockEntire);
  CHECK(!args->Flags.Discard && !args->Flags.IgnoreSync); ++locks;
  if (FAILED(lockResult)) return lockResult;
  if (lockAction) std::exchange(lockAction, {})();
  for (auto& backing : backings) if (backing.allocation == args->hAllocation) {
    CHECK(backing.live); args->pData = backing.pixels.data(); return S_OK;
  }
  CHECK(false); return E_FAIL;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  runtimeCaller(); CHECK(device == &deviceCookie && args->NumAllocations == 1); return S_OK;
}
static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
  runtimeCaller(); CHECK(device == &deviceCookie); args->hContext = &contextCookie; return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
  runtimeCaller(); CHECK(device == &deviceCookie && args->hContext == &contextCookie); return S_OK;
}
static HRESULT APIENTRY present(HANDLE device, DXGIDDICB_PRESENT* args) {
  runtimeCaller(); CHECK(device == &deviceCookie && args->pDXGIContext == &dxgiCookie);
  presented = args->hSrcAllocation; return S_OK;
}
static void APIENTRY error(D3D10DDI_HRTCORELAYER, HRESULT hr) {
  runtimeCaller(); lastError = hr;
}
HRESULT dxvk::umd::createDevice(const LUID&, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend*) noexcept {
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept {
  return E_NOTIMPL;
}
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  context->Flush(); return S_OK;
}
struct Storage {
  std::unique_ptr<void, decltype(&std::free)> bytes;
  explicit Storage(SIZE_T size) : bytes(std::calloc(1,size), &std::free) { CHECK(size && bytes); }
  template<typename T> T handle() const { return {bytes.get()}; }
};
struct Fixture {
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device = storage.handle<D3D10DDI_HDEVICE>();
  D3D10DDI_DEVICEFUNCS f{};
  DXGI_DDI_BASE_FUNCTIONS dxgi{};
  // The runtime owns this callback table for the device lifetime. Keep the
  // fixture's table alive instead of passing a constructor-local temporary.
  DXGI_DDI_BASE_CALLBACKS callbacks{};
  Fixture() {
    D3D10DDI_CORELAYER_DEVICECALLBACKS core{}; core.pfnSetErrorCb = error;
    D3DDDI_DEVICECALLBACKS kernel{};
    kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
    kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
    kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
    callbacks.pfnPresentCb = present;
    D3D10DDIARG_CREATEDEVICE args{};
    args.hDrvDevice = device; args.hRTDevice.handle = &deviceCookie;
    args.pUMCallbacks = &core; args.pKTCallbacks = &kernel; args.pDeviceFuncs = &f;
    args.DXGIBaseDDI.pDXGIBaseCallbacks = &callbacks;
    args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgi;
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->luid = luid; identity->runtime.handle = &deviceCookie; identity->query = query;
    identity->generation = 19; identity->capabilities = 3;
    CHECK(dxvk::umd::createAdapterDevice(identity, &args) == S_OK);
    CHECK(dxgi.pfnRotateResourceIdentities && dxgi.pfnPresent);
  }
  ~Fixture() { f.pfnDestroyDevice(device); }
  HRESULT rotate(const std::vector<DXGI_DDI_HRESOURCE>& resources) {
    DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES args{};
    args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate);
    args.Resources = UINT(resources.size()); args.pResources = resources.data();
    const HRESULT hr = dxgi.pfnRotateResourceIdentities(&args);
    if (hr == S_OK && resources.size() > 1 && runtimeResources.count(resources.front())) {
      // The Microsoft runtime rotates its resource-to-allocation association
      // after the DDI succeeds. Model that independently for DeallocateCb.
      const auto first = runtimeBacking.at(runtimeResources.at(resources.front()));
      for (size_t i=0; i+1<resources.size(); ++i)
        runtimeBacking.at(runtimeResources.at(resources[i])) =
          runtimeBacking.at(runtimeResources.at(resources[i+1]));
      runtimeBacking.at(runtimeResources.at(resources.back())) = first;
    }
    return hr;
  }
};
struct Texture {
  Fixture& owner;
  Storage storage;
  D3D10DDI_HRESOURCE handle;
  char runtime;
  bool live = true;
  Texture(Fixture& fixture, const D3D10DDIARG_CREATERESOURCE& desc)
  : owner(fixture), storage(fixture.f.pfnCalcPrivateResourceSize(fixture.device, &desc)),
    handle(storage.handle<D3D10DDI_HRESOURCE>()) {
    fixture.f.pfnCreateResource(fixture.device, &desc, handle, {&runtime}); CHECK(lastError == S_OK);
    if (runtimeBacking.count(&runtime)) runtimeResources.emplace(dxgi(),&runtime);
  }
  void retire() {
    CHECK(live); live = false;
    if (runtimeBacking.count(&runtime)) CHECK(retiredResources.insert(&runtime).second);
    owner.f.pfnDestroyResource(owner.device,handle);
    runtimeResources.erase(dxgi());
  }
  ~Texture() { if (live) retire(); }
  DXGI_DDI_HRESOURCE dxgi() const { return reinterpret_cast<DXGI_DDI_HRESOURCE>(handle.pDrvPrivate); }
};
static void readPixels(Fixture& fixture, Texture& source, Texture& staging, uint32_t expected) {
  fixture.f.pfnResourceCopy(fixture.device, staging.handle, source.handle); CHECK(lastError == S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE map{};
  fixture.f.pfnStagingResourceMap(fixture.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &map);
  CHECK(lastError == S_OK && map.pData);
  for (unsigned y=0; y<2; ++y) for (unsigned x=0; x<2; ++x)
    CHECK(reinterpret_cast<const uint32_t*>(static_cast<const char*>(map.pData)+y*map.RowPitch)[x] == expected);
  fixture.f.pfnStagingResourceUnmap(fixture.device, staging.handle, 0);
}
static void chainChecks(Fixture& fixture, bool shared, bool presentable) {
  D3D10DDI_MIPINFO mip{2,2,1,2,2,1};
  D3D10DDIARG_CREATERESOURCE desc{};
  desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
  desc.Usage = D3D10_DDI_USAGE_DEFAULT; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1; desc.MipLevels = 1; desc.ArraySize = 1;
  desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE
    | (presentable ? D3D10_DDI_BIND_PRESENT : 0);
  desc.MiscFlags = shared ? D3D10_DDI_RESOURCE_MISC_SHARED : 0;
  constexpr uint32_t colors[] = {0xff0000ff, 0xff00ff00, 0xffff0000};
  std::array<std::unique_ptr<Texture>,3> textures;
  std::vector<DXGI_DDI_HRESOURCE> resources;
  const size_t firstBacking = backings.size();
  for (unsigned i=0; i<3; ++i) {
    uint32_t pixels[] = {colors[i],colors[i],colors[i],colors[i]};
    D3D10_DDIARG_SUBRESOURCE_UP initial{pixels,8,16}; desc.pInitialDataUP = &initial;
    textures[i] = std::make_unique<Texture>(fixture,desc); resources.push_back(textures[i]->dxgi());
  }
  desc.pInitialDataUP = nullptr; desc.MiscFlags = 0;
  desc.Usage = D3D10_DDI_USAGE_STAGING; desc.BindFlags = 0; desc.MapFlags = D3D10_DDI_CPU_ACCESS_READ;
  Texture staging(fixture,desc);
  CHECK(fixture.rotate({}) == S_OK);
  CHECK(fixture.rotate({resources[0]}) == S_OK);
  CHECK(fixture.rotate({resources[0],resources[0]}) == E_INVALIDARG);
  CHECK(fixture.rotate({resources[0],0}) == E_INVALIDARG);
  CHECK(fixture.rotate({resources[0],staging.dxgi()}) == DXGI_DDI_ERR_UNSUPPORTED);
  CHECK(fixture.rotate({resources[0],DXGI_DDI_HRESOURCE(1)}) == E_INVALIDARG);
  if (shared) {
    // A publish failure leaves identities unchanged and the dirty cache
    // intact. The successful retry must publish each original frame once.
    lockResult = DXGI_ERROR_WAS_STILL_DRAWING;
    CHECK(fixture.rotate(resources) == DXGI_ERROR_WAS_STILL_DRAWING);
    lockResult = S_OK;
    lockAction = [&] { CHECK(fixture.rotate(resources) == DXGI_ERROR_WAS_STILL_DRAWING); };
  }
  for (unsigned round=1; round<=6; ++round) {
    CHECK(fixture.rotate(resources) == S_OK);
    for (unsigned i=0; i<3; ++i) {
      readPixels(fixture,*textures[i],staging,colors[(i+round)%3]);
      if (presentable) {
        DXGI_DDI_ARG_PRESENT args{};
        args.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(fixture.device.pDrvPrivate);
        args.hSurfaceToPresent = resources[i]; args.pDXGIContext = &dxgiCookie; args.Flags.Blt = 1;
        CHECK(fixture.dxgi.pfnPresent(&args) == S_OK);
        CHECK(presented == backings[firstBacking+(i+round)%3].allocation);
      }
    }
  }
  // A view created before rotation must still target the same private texture
  // object, whose contents/ownership now represent the rotated buffer.
  D3D10DDIARG_CREATERENDERTARGETVIEW view{};
  view.hDrvResource = textures[0]->handle; view.Format = desc.Format;
  view.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; view.Tex2D.ArraySize = 1;
  Storage viewStorage(fixture.f.pfnCalcPrivateRenderTargetViewSize(fixture.device,&view));
  auto target = viewStorage.handle<D3D10DDI_HRENDERTARGETVIEW>();
  fixture.f.pfnCreateRenderTargetView(fixture.device,&view,target,{}); CHECK(lastError == S_OK);
  CHECK(fixture.rotate(resources) == S_OK);
  FLOAT white[] = {1,1,1,1};
  fixture.f.pfnClearRenderTargetView(fixture.device,target,white); CHECK(lastError == S_OK);
  readPixels(fixture,*textures[0],staging,0xffffffff);
  fixture.f.pfnFlush(fixture.device); CHECK(lastError == S_OK);
  if (shared) {
    for (auto pixel : backings[firstBacking+1].pixels) CHECK(pixel == 0xffffffff);
    for (auto pixel : backings[firstBacking].pixels) CHECK(pixel == colors[0]);
  }
  fixture.f.pfnDestroyRenderTargetView(fixture.device,target);
}
static void retirementCheck(Fixture& fixture) {
  D3D10DDI_MIPINFO mip{2,2,1,2,2,1};
  uint32_t pixels[] = {0xff0000ff,0xff0000ff,0xff0000ff,0xff0000ff};
  D3D10_DDIARG_SUBRESOURCE_UP initial{pixels,8,16};
  D3D10DDIARG_CREATERESOURCE desc{};
  desc.pMipInfoList = &mip; desc.pInitialDataUP = &initial;
  desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; desc.Usage = D3D10_DDI_USAGE_DEFAULT;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
  desc.MipLevels = 1; desc.ArraySize = 1; desc.MiscFlags = D3D10_DDI_RESOURCE_MISC_SHARED;
  desc.BindFlags = D3D10_DDI_BIND_RENDER_TARGET;
  Texture first(fixture,desc), second(fixture,desc);
  lockAction = [&] {
    second.retire();
    // Runtime storage may be overwritten immediately after DestroyResource.
    std::memset(second.storage.bytes.get(),0xcc,
      fixture.f.pfnCalcPrivateResourceSize(fixture.device,&desc));
  };
  CHECK(fixture.rotate({first.dxgi(),second.dxgi()}) == DXGI_ERROR_DEVICE_REMOVED);
}
int main() {
  caller = GetCurrentThreadId();
  Fixture fixture;
  CHECK(fixture.dxgi.pfnRotateResourceIdentities(nullptr) == E_INVALIDARG);
  DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES bad{};
  CHECK(fixture.dxgi.pfnRotateResourceIdentities(&bad) == E_INVALIDARG);
  bad.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(fixture.device.pDrvPrivate); bad.Resources = 2;
  CHECK(fixture.dxgi.pfnRotateResourceIdentities(&bad) == E_INVALIDARG);
  chainChecks(fixture,false,false); chainChecks(fixture,false,true); chainChecks(fixture,true,false);
  CHECK(releases == 6 && lastError == S_OK);
  retirementCheck(fixture);
  CHECK(releases == 8 && runtimeBacking.empty() && runtimeResources.empty()
    && retiredResources.empty() && lastError == S_OK);
  std::printf("PASS native DXGI rotation: %u checks, %u synchronized locks; WARP only, admission closed\n",checks,locks);
}
