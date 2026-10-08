// SPDX-License-Identifier: MIT
// Exact typed DXGI Blt tables and production GPU-only copy/rotation/scaling.
// Only the embedded renderer is replaced by a controlled WARP fixture.
#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_blt.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

static std::atomic<unsigned> checks{0};
static unsigned snapshots, pixels, locks, unlocks, submissions, releases;
static unsigned identitySnapshots, identityPixels;
static DWORD caller;
static HRESULT lastError = S_OK, lockResult = S_OK, unlockResult = S_OK, submissionResult = S_OK;
static LUID selected{0x12345678, -43};
static char adapterCookie, deviceCookie, coreCookie, contextCookie;
static std::function<void()> lockAction, submissionAction;
static bool submitting;
static dxvk::umd::RuntimeBackend* constructingRuntime;
static std::unordered_map<ID3D11DeviceContext*, const dxvk::umd::RuntimeBackend*> bridges;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "DXGI Blt line=%d: %s error=%08lx\n", __LINE__, #value, static_cast<unsigned long>(lastError)); std::abort(); \
} } while (0)
static void runtimeCaller() { CHECK(GetCurrentThreadId() == caller); }
struct Backing {
  HANDLE runtime{};
  dxvk::umd::AllocationInfo info{};
  std::vector<uint8_t> data;
  void* address() { return data.data() + 16; }
};
static std::unordered_map<D3DKMT_HANDLE, Backing> backings;
static D3DKMT_HANDLE nextHandle = 100;
static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* request) {
  runtimeCaller(); CHECK(runtime == &adapterCookie && request && request->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(request->pPrivateDriverData);
  for (unsigned i = 0; i < 160; ++i) CHECK(bytes[i] == 0);
  auto set = [&](unsigned offset, uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes[offset + i] = uint8_t(value >> (8 * i));
  };
  set(0, 0x504d5644, 4); set(8, 128, 4); set(16, 3, 8); set(24, 19, 8);
  set(128, 0x44494c56, 4); set(132, 1, 4); set(136, 32, 4); set(140, 1, 4); set(152, 1, 4);
  std::memcpy(bytes + 144, &selected, sizeof(selected));
  if (submitting && submissionAction) std::exchange(submissionAction, {})();
  return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* request) {
  runtimeCaller(); CHECK(device == &deviceCookie && request && request->hResource);
  CHECK(request->NumAllocations == 1 && request->pAllocationInfo);
  Backing backing; backing.runtime = request->hResource;
  CHECK(request->pAllocationInfo->PrivateDriverDataSize == sizeof(backing.info));
  std::memcpy(&backing.info, request->pAllocationInfo->pPrivateDriverData, sizeof(backing.info));
  backing.data.assign(size_t(backing.info.size) + 32, 0xa5);
  const auto handle = nextHandle++; CHECK(backings.emplace(handle, std::move(backing)).second);
  request->pAllocationInfo->hAllocation = handle; request->hKMResource = handle + 1000; return S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* request) {
  runtimeCaller(); CHECK(device == &deviceCookie && request && request->hResource);
  auto found = backings.end();
  for (auto entry = backings.begin(); entry != backings.end(); ++entry)
    if (entry->second.runtime == request->hResource) found = entry;
  CHECK(found != backings.end()); backings.erase(found); ++releases; return S_OK;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* request) {
  runtimeCaller(); CHECK(device == &deviceCookie && request && backings.count(request->hAllocation)); ++locks;
  CHECK(request->Flags.LockEntire && !request->Flags.Discard && !request->Flags.IgnoreSync);
  CHECK(bool(request->Flags.ReadOnly) != bool(request->Flags.WriteOnly));
  if (lockAction) std::exchange(lockAction, {})();
  CHECK(backings.count(request->hAllocation)); request->pData = backings.at(request->hAllocation).address();
  return lockResult;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* request) {
  runtimeCaller(); CHECK(device == &deviceCookie && request && request->NumAllocations == 1);
  CHECK(request->phAllocations && backings.count(request->phAllocations[0])); ++unlocks; return unlockResult;
}
static HRESULT APIENTRY createContext(HANDLE, D3DDDICB_CREATECONTEXT* request) { request->hContext = &contextCookie; return S_OK; }
static HRESULT APIENTRY destroyContext(HANDLE, const D3DDDICB_DESTROYCONTEXT*) { return S_OK; }
static HRESULT APIENTRY present(HANDLE, DXGIDDICB_PRESENT* request) { CHECK(!request); return E_FAIL; }
static void APIENTRY error(D3D10DDI_HRTCORELAYER runtime, HRESULT hr) {
  runtimeCaller(); CHECK(runtime.handle == &coreCookie); lastError = hr;
}
HRESULT dxvk::umd::createDevice(const LUID& luid, D3D_FEATURE_LEVEL level,
    ID3D11Device** device, ID3D11DeviceContext** context, const RuntimeBackend* runtime) noexcept {
  CHECK(!std::memcmp(&luid, &selected, sizeof(luid)));
  CHECK(runtime && constructingRuntime);
  *constructingRuntime = *runtime;
  const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hr == S_OK) CHECK(bridges.emplace(*context, constructingRuntime).second);
  return hr;
}
HRESULT dxvk::umd::isStagingResourceBusy(ID3D11DeviceContext*, ID3D11Resource*, BOOL*) noexcept { return E_NOTIMPL; }
HRESULT dxvk::umd::flushRuntimeSubmission(ID3D11DeviceContext* context) noexcept {
  ++submissions;
  CHECK(GetCurrentThreadId() != caller && bridges.count(context));
  // This real RuntimeGpu entry pumps QueryAdapterInfo on the original DDI
  // caller. A synthetic backend must not invent runtime callbacks on its worker.
  auto bridge = *bridges.at(context); submitting = true;
  const HRESULT hr = bridge.create.callbacks->status(bridge.create.owner);
  submitting = false;
  if (hr != S_OK) return hr;
  context->Flush(); return submissionResult;
}
struct Storage {
  void* bytes;
  explicit Storage(SIZE_T size) : bytes(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) { CHECK(size && bytes); }
  void poison() { DWORD previous; CHECK(VirtualProtect(bytes, 1, PAGE_NOACCESS, &previous)); }
  ~Storage() { CHECK(VirtualFree(bytes, 0, MEM_RELEASE)); }
};
template<typename Table>
struct Fixture {
  using Desc = std::conditional_t<std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>, D3D11DDIARG_CREATERESOURCE, D3D10DDIARG_CREATERESOURCE>;
  Storage storage{VioGpuDxvkPrivateDeviceSize()};
  D3D10DDI_HDEVICE device{storage.bytes}; Table table{};
  DXGI1_1_DDI_BASE_FUNCTIONS dxgi{}; DXGI_DDI_BASE_CALLBACKS dxgiCallbacks{};
  struct LegacyTable { DXGI_DDI_BASE_FUNCTIONS table{}; uintptr_t guards[2]{0x13579u, 0x24680u}; } legacy;
  D3D10DDI_CORELAYER_DEVICECALLBACKS core10{}; D3D11DDI_CORELAYER_DEVICECALLBACKS core11{};
  dxvk::umd::RuntimeBackend bridge;
  ID3D11DeviceContext* contextKey = nullptr;
  bool live = true;
  explicit Fixture(bool dxgi11 = true) {
    core10.pfnSetErrorCb = error; core11.pfnSetErrorCb = error;
    D3DDDI_DEVICECALLBACKS kernel{}; kernel.pfnAllocateCb = allocate; kernel.pfnDeallocateCb = deallocate;
    kernel.pfnLockCb = lock; kernel.pfnUnlockCb = unlock;
    kernel.pfnCreateContextCb = createContext; kernel.pfnDestroyContextCb = destroyContext;
    dxgiCallbacks.pfnPresentCb = present;
    D3D10DDIARG_CREATEDEVICE args{};
    if constexpr (std::is_same_v<Table, D3D10DDI_DEVICEFUNCS>) {
      args.Interface = D3D10_0_DDI_INTERFACE_VERSION; args.Version = D3D10_0_DDI_BUILD_VERSION << 16;
      args.pDeviceFuncs = &table; args.pUMCallbacks = &core10;
    } else if constexpr (std::is_same_v<Table, D3D10_1DDI_DEVICEFUNCS>) {
      args.Interface = D3D10_1_DDI_INTERFACE_VERSION; args.Version = D3D10_1_DDI_BUILD_VERSION << 16;
      args.p10_1DeviceFuncs = &table; args.pUMCallbacks = &core10;
    } else {
      args.Interface = D3D11_0_DDI_INTERFACE_VERSION; args.Version = D3D11_0_DDI_BUILD_VERSION << 16;
      args.Flags = D3D11DDI_3DPIPELINELEVEL_11_0 << D3D11DDI_CREATEDEVICE_FLAG_3DPIPELINESUPPORT_SHIFT;
      args.p11DeviceFuncs = &table; args.p11UMCallbacks = &core11;
    }
    if (dxgi11) args.Version |= std::is_same_v<Table, D3D11DDI_DEVICEFUNCS>
      ? DXGI_RESOLVE_SHARED_RESOURCE : (VISTA_GOLD_PRODUCT_VER | DXGI_RESOLVE_SHARED_RESOURCE);
    CHECK(dxvk::umd::nativeDxgiUses1_1(args.Interface, args.Version) == dxgi11);
    args.hDrvDevice = device; args.hRTDevice.handle = &deviceCookie; args.hRTCoreLayer.handle = &coreCookie;
    args.pKTCallbacks = &kernel; args.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgiCallbacks;
    if (dxgi11) args.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = &dxgi;
    else args.DXGIBaseDDI.pDXGIDDIBaseFunctions = &legacy.table;
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->luid = selected; identity->runtime = &adapterCookie; identity->query = query;
    identity->generation = 19; identity->capabilities = 3;
    constructingRuntime = &bridge;
    CHECK(dxvk::umd::createAdapterDevice(identity, &args) == S_OK);
    CHECK(dxgi11 ? bool(dxgi.pfnResolveSharedResource)
      : legacy.guards[0] == 0x13579u && legacy.guards[1] == 0x24680u && !dxgi.pfnResolveSharedResource);
    constructingRuntime = nullptr;
    for (const auto& entry : bridges) if (entry.second == &bridge) contextKey = entry.first;
    CHECK(contextKey);
    if (dxgi11) {
      CHECK(dxgi.pfnBlt != nullptr);
      CHECK(dxgi.pfnBlt(nullptr) == E_INVALIDARG);
    } else {
      CHECK(legacy.table.pfnBlt != nullptr);
      CHECK(legacy.table.pfnBlt(nullptr) == E_INVALIDARG);
    }
  }
  HRESULT blt(DXGI_DDI_ARG_BLT request) {
    request.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate);
    return dxgi.pfnBlt(&request);
  }
  HRESULT resolve(DXGI_DDI_HRESOURCE resource) {
    DXGI_DDI_ARG_RESOLVESHAREDRESOURCE request{};
    request.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(device.pDrvPrivate); request.hResource = resource;
    return dxgi.pfnResolveSharedResource(&request);
  }
  void retire() {
    CHECK(live); live = false; table.pfnDestroyDevice(device); storage.poison();
    CHECK(bridges.erase(contextKey) == 1); bridge = {};
  }
  ~Fixture() { if (live) retire(); }
};
static uint32_t color(unsigned x, unsigned y) {
  return 0xff000000u | (8 + 8 * x) | ((16 + 16 * y) << 8) | ((32 + 8 * (x + y)) << 16);
}
static uint32_t swapRedBlue(uint32_t word) {
  return (word & 0xff00ff00u) | ((word & 255) << 16) | ((word >> 16) & 255);
}
static void save(const char* name, const void* data, size_t size) {
  FILE* file = nullptr; CHECK(fopen_s(&file, name, "wb") == 0 && file);
  CHECK(std::fwrite(data, 1, size, file) == size); CHECK(std::fclose(file) == 0);
}
template<typename F>
struct Texture {
  F& fixture; Storage storage;
  D3D10DDI_HRESOURCE handle{storage.bytes};
  char runtime{}; bool live = true;
  UINT width, height, mips, arrays, samples; DXGI_FORMAT format;
  D3DKMT_HANDLE allocation = 0;
  Texture(F& f, UINT w, UINT h, DXGI_FORMAT fmt, UINT bind, UINT misc = 0,
      bool gradient = false, UINT count = 1, UINT mipCount = 1, UINT arrayCount = 1,
      bool staging = false)
  : fixture(f), storage(f.table.pfnCalcPrivateResourceSize(f.device, nullptr)),
    width(w), height(h), mips(mipCount), arrays(arrayCount), samples(count), format(fmt) {
    std::vector<D3D10DDI_MIPINFO> shapes(mips);
    std::vector<std::vector<uint32_t>> words(size_t(mips) * arrays);
    std::vector<D3D10_DDIARG_SUBRESOURCE_UP> initial(words.size());
    for (UINT mip = 0; mip < mips; ++mip) {
      const UINT mw = std::max(1u, width >> mip), mh = std::max(1u, height >> mip);
      shapes[mip] = {mw, mh, 1, mw, mh, 1};
      for (UINT slice = 0; slice < arrays; ++slice) {
        auto& pixelsForMip = words[size_t(slice) * mips + mip]; pixelsForMip.resize(size_t(mw) * mh);
        for (UINT y = 0; y < mh; ++y) for (UINT x = 0; x < mw; ++x)
          pixelsForMip[size_t(y) * mw + x] = gradient ? color(x, y) : 0xff281008;
        initial[size_t(slice) * mips + mip] = {pixelsForMip.data(), mw * 4, mw * mh * 4};
      }
    }
    typename F::Desc args{}; args.pMipInfoList = shapes.data();
    args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    args.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
    args.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0;
    args.BindFlags = bind; args.MiscFlags = misc; args.Format = format; args.SampleDesc.Count = samples;
    args.MipLevels = mips; args.ArraySize = arrays;
    if (!staging && samples == 1) args.pInitialDataUP = initial.data();
    f.table.pfnCreateResource(f.device, &args, handle, {&runtime}); CHECK(lastError == S_OK);
    for (const auto& entry : backings) if (entry.second.runtime == &runtime) allocation = entry.first;
  }
  DXGI_DDI_HRESOURCE dxgi() const { return reinterpret_cast<DXGI_DDI_HRESOURCE>(handle.pDrvPrivate); }
  void retire() { CHECK(live); live = false; fixture.table.pfnDestroyResource(fixture.device, handle); storage.poison(); }
  ~Texture() { if (live) retire(); }
};
template<typename F>
static void clearMultisample(F& f, Texture<F>& texture) {
  Storage storage(f.table.pfnCalcPrivateRenderTargetViewSize(f.device, nullptr));
  D3D10DDI_HRENDERTARGETVIEW view{storage.bytes};
  D3D10DDIARG_CREATERENDERTARGETVIEW args{}; args.hDrvResource = texture.handle;
  args.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D; args.Format = texture.format;
  args.Tex2D.ArraySize = 1;
  f.table.pfnCreateRenderTargetView(f.device, &args, view, {}); CHECK(lastError == S_OK);
  float red[]{1, 0, 0, 1}; f.table.pfnClearRenderTargetView(f.device, view, red); CHECK(lastError == S_OK);
  f.table.pfnDestroyRenderTargetView(f.device, view);
}
static uint32_t expected(unsigned test, unsigned x, unsigned y, unsigned dw, unsigned dh) {
  const unsigned left = test == 8 ? 2 : test == 9 ? 0 : 1;
  const unsigned top = test == 9 ? 0 : 1;
  const unsigned rw = test == 1 || test == 3 || test == 8 || test == 9 ? 4 : test == 4 ? 3 : test == 5 ? 12 : 6;
  const unsigned rh = test == 1 || test == 3 ? 6 : test == 4 ? 2 : test == 5 ? 8 : 4;
  CHECK(dw && dh);
  if (x < left || y < top || x >= left + rw || y >= top + rh) return 0xff281008;
  x -= left; y -= top;
  if (test == 8) return 0xffff0000; // Resolved red into BGRA, all four flags at once.
  if (test == 1) return color(5 - y, x);
  if (test == 2) return color(5 - x, 3 - y);
  if (test == 3) return color(y, 3 - x);
  if (test == 4) return 0xff000000u | (12 + 16 * x) | ((24 + 32 * y) << 8) | ((40 + 16 * (x + y)) << 16);
  if (test == 5) {
    const unsigned r = x == 0 ? 8 : x == 11 ? 48 : 6 + 4 * x;
    const unsigned g = y == 0 ? 16 : y == 7 ? 64 : 12 + 8 * y;
    const unsigned b = 32 + (r - 8) + (g - 16) / 2;
    return 0xff000000u | r | (g << 8) | (b << 16);
  }
  if (test == 9) return 0xff000000u | (10 + 12 * x) | ((16 + 16 * y) << 8) | ((34 + 12 * x + 8 * y) << 16);
  const uint32_t word = color(x, y);
  return test == 6 ? swapRedBlue(word) : word;
}
template<typename F>
static void read(F& f, Texture<F>& texture, UINT subresource, unsigned profile, unsigned test) {
  Texture<F> staging(f, texture.width, texture.height, texture.format, 0, 0, false, 1, texture.mips, texture.arrays, true);
  f.table.pfnResourceCopy(f.device, staging.handle, texture.handle); CHECK(lastError == S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.table.pfnStagingResourceMap(f.device, staging.handle, subresource, D3D10_DDI_MAP_READ, 0, &mapped);
  const UINT width = std::max(1u, texture.width >> (subresource % texture.mips));
  const UINT height = std::max(1u, texture.height >> (subresource % texture.mips));
  CHECK(lastError == S_OK && mapped.pData && mapped.RowPitch >= width * 4);
  std::vector<uint32_t> actual(size_t(width) * height);
  for (UINT y = 0; y < height; ++y) std::memcpy(actual.data() + size_t(y) * width,
    static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, width * 4);
  const uint32_t metadata[]{width, height, test, subresource, uint32_t(texture.format), mapped.RowPitch};
  char name[96]; std::snprintf(name, sizeof(name), "blt-%u-%u.actual.u32.bin", profile, test); save(name, actual.data(), actual.size() * 4);
  std::snprintf(name, sizeof(name), "blt-%u-%u.metadata.u32.bin", profile, test); save(name, metadata, sizeof(metadata));
  f.table.pfnStagingResourceUnmap(f.device, staging.handle, subresource); CHECK(lastError == S_OK);
  for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
    const auto wanted = expected(test, x, y, width, height);
    if (actual[size_t(y) * width + x] != wanted) std::fprintf(stderr,
      "Blt mismatch profile=%u test=%u x=%u y=%u actual=%08x expected=%08x\n", profile, test, x, y, actual[size_t(y) * width + x], wanted);
    CHECK(actual[size_t(y) * width + x] == wanted); ++pixels;
  }
  ++snapshots;
}
template<typename Table>
static void profile(unsigned index) {
  using F = Fixture<Table>;
  { F legacy(false); CHECK(legacy.legacy.guards[0] == 0x13579u && legacy.legacy.guards[1] == 0x24680u); }
  F f;
  Texture<F> source(f, 6, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT, 0, true);
  Texture<F> srgb(f, 6, 4, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT, 0, true);
  Texture<F> msaa(f, 4, 2, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT, 0, false, 4);
  clearMultisample(f, msaa);
  // Independent application state is retained through Blt, including state
  // which differs from the UMD bookkeeping and is irrelevant to its blit.
  auto context = f.contextKey;
  const D3D11_VIEWPORT originalViewport{3, 4, 17, 19, .25f, .75f};
  context->RSSetViewports(1, &originalViewport); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  std::vector<unsigned char> vsBytes, psBytes; CHECK(dxvk::umd::bltShaderContainers(vsBytes, psBytes));
  Microsoft::WRL::ComPtr<ID3D11Device> backend; context->GetDevice(&backend);
  Microsoft::WRL::ComPtr<ID3D11VertexShader> originalVS;
  CHECK(backend->CreateVertexShader(vsBytes.data(), vsBytes.size(), nullptr, &originalVS) == S_OK);
  context->VSSetShader(originalVS.Get(), nullptr, 0);
  for (unsigned test = 0; test < 10; ++test) {
    const UINT width = test == 1 || test == 3 ? 6 : test == 4 ? 5 : test == 5 ? 14 : 8;
    const UINT height = test == 1 || test == 3 ? 8 : test == 4 ? 4 : test == 5 ? 10 : test == 9 ? 8 : 6;
    const auto format = test == 6 || test == 8 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    Texture<F> destination(f, width, height, format, D3D10_DDI_BIND_RENDER_TARGET,
      test == 8 ? D3D10_DDI_RESOURCE_MISC_SHARED : 0, false, 1, test == 9 ? 2 : 1, test == 9 ? 2 : 1);
    DXGI_DDI_ARG_BLT args{}; args.hSrcResource = test == 7 ? srgb.dxgi() : test == 8 ? msaa.dxgi() : source.dxgi();
    args.hDstResource = destination.dxgi(); args.DstSubresource = test == 9 ? 3 : 0;
    args.DstLeft = test == 8 ? 2 : test == 9 ? 0 : 1; args.DstTop = test == 9 ? 0 : 1;
    args.DstRight = args.DstLeft + (test == 1 || test == 3 || test == 8 || test == 9 ? 4 : test == 4 ? 3 : test == 5 ? 12 : 6);
    args.DstBottom = args.DstTop + (test == 1 || test == 3 ? 6 : test == 4 ? 2 : test == 5 ? 8 : 4);
    args.Rotate = DXGI_DDI_MODE_ROTATION(test == 1 ? 2 : test == 2 ? 3 : test == 3 ? 4 : test == 8 ? 2 : 1);
    args.Flags.Value = test == 8 ? 15 : ((test >= 4 && test <= 5) || test == 9) ? 4 : 2;
    CHECK(f.blt(args) == S_OK && lastError == S_OK);
    UINT count = 1; D3D11_VIEWPORT actualViewport{}; context->RSGetViewports(&count, &actualViewport);
    CHECK(count == 1 && !std::memcmp(&actualViewport, &originalViewport, sizeof(originalViewport)));
    D3D11_PRIMITIVE_TOPOLOGY topology{}; context->IAGetPrimitiveTopology(&topology); CHECK(topology == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    Microsoft::WRL::ComPtr<ID3D11VertexShader> currentVS; context->VSGetShader(&currentVS, nullptr, nullptr); CHECK(currentVS.Get() == originalVS.Get());
    read(f, destination, args.DstSubresource, index, test);
    if (test == 9) {
      Texture<F> staging(f, width, height, format, 0, 0, false, 1, 2, 2, true);
      f.table.pfnResourceCopy(f.device, staging.handle, destination.handle); CHECK(lastError == S_OK);
      for (UINT subresource = 0; subresource < 3; ++subresource) {
        D3D10DDI_MAPPED_SUBRESOURCE map{};
        f.table.pfnStagingResourceMap(f.device, staging.handle, subresource, D3D10_DDI_MAP_READ, 0, &map);
        const UINT w = width >> (subresource % 2), h = height >> (subresource % 2);
        CHECK(lastError == S_OK && map.pData && map.RowPitch >= w * 4);
        for (UINT y = 0; y < h; ++y) for (UINT x = 0; x < w; ++x) {
          uint32_t word; std::memcpy(&word, static_cast<const uint8_t*>(map.pData) + size_t(y) * map.RowPitch + x * 4, 4);
          CHECK(word == 0xff281008);
        }
        f.table.pfnStagingResourceUnmap(f.device, staging.handle, subresource);
      }
    }
    if (test == 8) {
      auto& backing = backings.at(destination.allocation);
      for (unsigned i = 0; i < 16; ++i) CHECK(backing.data[i] == 0xa5 && backing.data[backing.data.size() - 16 + i] == 0xa5);
      for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
        uint32_t actual; std::memcpy(&actual, static_cast<const uint8_t*>(backing.address()) + size_t(y) * backing.info.pitch + 4 * x, 4);
        CHECK(actual == expected(test, x, y, width, height));
      }
      char name[96]; std::snprintf(name, sizeof(name), "blt-present-%u.actual.bin", index); save(name, backing.data.data(), backing.data.size());
      const uint32_t metadata[]{width, height, backing.info.pitch, uint32_t(backing.info.size)};
      std::snprintf(name, sizeof(name), "blt-present-%u.metadata.u32.bin", index); save(name, metadata, sizeof(metadata));
    }
  }
  {
    Texture<F> dst(f, 8, 6, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_DDI_BIND_RENDER_TARGET);
    DXGI_DDI_ARG_BLT args{}; args.hSrcResource = source.dxgi(); args.hDstResource = dst.dxgi();
    args.DstRight = 6; args.DstBottom = 4; args.Rotate = DXGI_DDI_MODE_ROTATION_IDENTITY; args.Flags.Convert = 1;
    for (unsigned test = 0; test < 8; ++test) {
      auto bad = args;
      if (test == 0) bad.Flags.Value |= 16;
      if (test == 1) bad.Rotate = DXGI_DDI_MODE_ROTATION_UNSPECIFIED;
      if (test == 2) bad.SrcSubresource = 1;
      if (test == 3) bad.DstSubresource = 1;
      if (test == 4) bad.DstRight = 0;
      if (test == 5) bad.DstRight = 9;
      if (test == 6) bad.DstRight = 7;
      if (test == 7) bad.Flags.Resolve = 1;
      CHECK(f.blt(bad) == E_INVALIDARG);
    }
    auto unsupported = args; unsupported.Flags.Present = 1; CHECK(f.blt(unsupported) == DXGI_DDI_ERR_UNSUPPORTED);
    unsupported.hSrcResource = dst.dxgi(); CHECK(f.blt(unsupported) == DXGI_DDI_ERR_UNSUPPORTED);
    Storage unknown(1); unknown.poison(); auto foreign = args;
    foreign.hDstResource = reinterpret_cast<DXGI_DDI_HRESOURCE>(unknown.bytes); CHECK(f.blt(foreign) == E_INVALIDARG);
    // No rejected request changes any destination word.
    Texture<F> staging(f, 8, 6, dst.format, 0, 0, false, 1, 1, 1, true);
    f.table.pfnResourceCopy(f.device, staging.handle, dst.handle); CHECK(lastError == S_OK);
    D3D10DDI_MAPPED_SUBRESOURCE map{}; f.table.pfnStagingResourceMap(f.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &map);
    CHECK(lastError == S_OK && map.pData && map.RowPitch >= 32);
    for (UINT y = 0; y < 6; ++y) for (UINT x = 0; x < 8; ++x) {
      uint32_t word; std::memcpy(&word, static_cast<const uint8_t*>(map.pData) + size_t(y) * map.RowPitch + x * 4, 4); CHECK(word == 0xff281008);
    }
    f.table.pfnStagingResourceUnmap(f.device, staging.handle, 0);
  }
  originalVS.Reset(); backend.Reset();
  {
    D3D10DDI_MIPINFO mip{6, 4, 1, 6, 4, 1}; typename F::Desc desc{};
    desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.Usage = D3D10_DDI_USAGE_DEFAULT;
    desc.BindFlags = D3D10_DDI_BIND_PRESENT | D3D10_DDI_BIND_RENDER_TARGET;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    Storage bytes(f.table.pfnCalcPrivateResourceSize(f.device, &desc)), primary(1); primary.poison();
    desc.pPrimaryDesc = static_cast<DXGI_DDI_PRIMARY_DESC*>(primary.bytes);
    const auto before = backings.size(); char runtime;
    f.table.pfnCreateResource(f.device, &desc, {bytes.bytes}, {&runtime});
    CHECK(lastError == DXGI_DDI_ERR_UNSUPPORTED && backings.size() == before && !*static_cast<uint8_t*>(bytes.bytes)); lastError = S_OK;
    desc.pPrimaryDesc = nullptr; desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    f.table.pfnCreateResource(f.device, &desc, {bytes.bytes}, {&runtime});
    CHECK(lastError == DXGI_ERROR_UNSUPPORTED && backings.size() == before && !*static_cast<uint8_t*>(bytes.bytes)); lastError = S_OK;
  }
  {
    Texture<F> dst(f, 6, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_DDI_BIND_RENDER_TARGET, D3D10_DDI_RESOURCE_MISC_SHARED);
    DXGI_DDI_ARG_BLT args{}; args.hSrcResource = source.dxgi(); args.hDstResource = dst.dxgi();
    args.DstRight = 6; args.DstBottom = 4; args.Rotate = DXGI_DDI_MODE_ROTATION_IDENTITY; args.Flags.Value = 10;
    lockResult = E_OUTOFMEMORY; CHECK(f.blt(args) == E_OUTOFMEMORY); lockResult = S_OK;
    lockAction = [&] {
      runtimeCaller(); CHECK(f.blt(args) == DXGI_ERROR_WAS_STILL_DRAWING);
      CHECK(f.resolve(dst.dxgi()) == DXGI_ERROR_WAS_STILL_DRAWING);
      dst.retire();
    };
    CHECK(f.blt(args) == DXGI_ERROR_DEVICE_REMOVED);
  }
}
template<typename Table>
static void terminalRetirement() {
  using F = Fixture<Table>;
  F f;
  Texture<F> source(f, 6, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT, 0, true);
  Texture<F> destination(f, 6, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D10_DDI_BIND_RENDER_TARGET, D3D10_DDI_RESOURCE_MISC_SHARED);
  DXGI_DDI_ARG_BLT args{}; args.hSrcResource = source.dxgi(); args.hDstResource = destination.dxgi();
  args.DstRight = 6; args.DstBottom = 4; args.Rotate = DXGI_DDI_MODE_ROTATION_IDENTITY; args.Flags.Value = 10;
  const auto allocation = destination.allocation;
  submissionAction = [&] {
    runtimeCaller(); source.retire(); destination.retire();
    // Blt pins this genuine owned destination after DestroyResource. Terminal
    // DestroyDevice still owes Deallocate before its callback owner is retired.
    CHECK(backings.count(allocation)); f.retire(); CHECK(!backings.count(allocation));
  };
  CHECK(f.blt(args) == DXGI_ERROR_DEVICE_REMOVED && backings.empty());
}
static uint32_t identityColor(unsigned x, unsigned y, unsigned seed) {
  return ((64 + seed * 11 + x * 3 + y * 5) & 255) << 24
    | ((seed * 13 + x * 17 + y * 7) & 255)
    | (((seed * 19 + x * 5 + y * 23) & 255) << 8)
    | (((seed * 29 + x * 11 + y * 13) & 255) << 16);
}
static uint32_t identityExpected(unsigned test, unsigned subresource, unsigned x, unsigned y) {
  if (test == 2 || test == 5) return identityColor(x, y, 4);
  if ((test == 0 || (test == 1 && subresource == 3))
      && x >= 2 && y >= 1 && x < 9 && y < 6) return identityColor(x - 2, y - 1, 4);
  return 0xff281008;
}
static void identitySave(const void* address, UINT pitch, UINT width, UINT height,
    DXGI_FORMAT format, unsigned profile, unsigned formatIndex, unsigned test, unsigned subresource) {
  CHECK(address && pitch >= width * 4);
  std::vector<uint32_t> actual(size_t(width) * height);
  for (UINT y = 0; y < height; ++y) std::memcpy(actual.data() + size_t(y) * width,
    static_cast<const uint8_t*>(address) + size_t(y) * pitch, width * 4);
  const uint32_t metadata[]{width, height, test, subresource, uint32_t(format), pitch, 4, profile};
  char name[128]; std::snprintf(name, sizeof(name), "identity-blt-%u-%u-%u-%u.actual.u32.bin",
    profile, formatIndex, test, subresource); save(name, actual.data(), actual.size() * 4);
  std::snprintf(name, sizeof(name), "identity-blt-%u-%u-%u-%u.metadata.u32.bin",
    profile, formatIndex, test, subresource); save(name, metadata, sizeof(metadata));
  for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
    const auto wanted = identityExpected(test, subresource, x, y);
    if (actual[size_t(y) * width + x] != wanted) std::fprintf(stderr,
      "Identity Blt mismatch profile=%u format=%u seed=4 case=%u sub=%u x=%u y=%u actual=%08x expected=%08x\n",
      profile, unsigned(format), test, subresource, x, y, actual[size_t(y) * width + x], wanted);
    CHECK(actual[size_t(y) * width + x] == wanted); ++identityPixels;
  }
  ++identitySnapshots;
}
template<typename F>
static void identityRead(F& f, Texture<F>& texture, UINT subresource, unsigned profile,
    unsigned formatIndex, unsigned test) {
  Texture<F> staging(f, texture.width, texture.height, texture.format, 0, 0, false, 1,
    texture.mips, texture.arrays, true);
  f.table.pfnResourceCopy(f.device, staging.handle, texture.handle); CHECK(lastError == S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.table.pfnStagingResourceMap(f.device, staging.handle, subresource, D3D10_DDI_MAP_READ, 0, &mapped);
  CHECK(lastError == S_OK);
  identitySave(mapped.pData, mapped.RowPitch, std::max(1u, texture.width >> (subresource % texture.mips)),
    std::max(1u, texture.height >> (subresource % texture.mips)), texture.format, profile, formatIndex, test, subresource);
  f.table.pfnStagingResourceUnmap(f.device, staging.handle, subresource); CHECK(lastError == S_OK);
}
template<typename Table>
static void identityProfile(unsigned index) {
  using F = Fixture<Table>;
  using Microsoft::WRL::ComPtr;
  F f; auto context = f.contextKey; ComPtr<ID3D11Device> backend; context->GetDevice(&backend);
  std::vector<unsigned char> vertex, pixel; CHECK(dxvk::umd::bltShaderContainers(vertex, pixel));
  ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
  CHECK(backend->CreateVertexShader(vertex.data(), vertex.size(), nullptr, &vs) == S_OK);
  CHECK(backend->CreatePixelShader(pixel.data(), pixel.size(), nullptr, &ps) == S_OK);
  D3D11_RASTERIZER_DESC rasterDesc{}; rasterDesc.FillMode = D3D11_FILL_SOLID;
  rasterDesc.CullMode = D3D11_CULL_FRONT; rasterDesc.ScissorEnable = TRUE; rasterDesc.DepthClipEnable = TRUE;
  ComPtr<ID3D11RasterizerState> raster; CHECK(backend->CreateRasterizerState(&rasterDesc, &raster) == S_OK);
  D3D11_TEXTURE2D_DESC appDesc{}; appDesc.Width = appDesc.Height = 4;
  appDesc.MipLevels = appDesc.ArraySize = appDesc.SampleDesc.Count = 1;
  appDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; appDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> appTarget; ComPtr<ID3D11RenderTargetView> appView;
  CHECK(backend->CreateTexture2D(&appDesc, nullptr, &appTarget) == S_OK);
  CHECK(backend->CreateRenderTargetView(appTarget.Get(), nullptr, &appView) == S_OK);
  const D3D11_VIEWPORT viewport{3, 4, 17, 19, .25f, .75f}; const D3D11_RECT scissor{31, 37, 41, 43};
  context->RSSetViewports(1, &viewport); context->RSSetScissorRects(1, &scissor); context->RSSetState(raster.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
  auto target = appView.Get(); context->OMSetRenderTargets(1, &target, nullptr);
  ComPtr<ID3D11Predicate> predicate;
  const D3D11_QUERY_DESC queryDesc{D3D11_QUERY_OCCLUSION_PREDICATE, 0};
  CHECK(backend->CreatePredicate(&queryDesc, &predicate) == S_OK);
  context->Begin(predicate.Get()); context->End(predicate.Get()); context->Flush();
  BOOL visible = TRUE; HRESULT result = S_FALSE; const auto started = GetTickCount64();
  while (result == S_FALSE && GetTickCount64() - started < 10000) {
    result = context->GetData(predicate.Get(), &visible, sizeof(visible), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result == S_FALSE) SwitchToThread();
  }
  CHECK(result == S_OK && visible == FALSE);
  auto state = [&] {
    ComPtr<ID3D11Predicate> currentPredicate; BOOL value = TRUE;
    context->GetPredication(&currentPredicate, &value); CHECK(currentPredicate.Get() == predicate.Get() && value == FALSE);
    UINT count = 1; D3D11_VIEWPORT seenViewport{}; context->RSGetViewports(&count, &seenViewport);
    CHECK(count == 1 && !std::memcmp(&seenViewport, &viewport, sizeof(viewport)));
    count = 1; D3D11_RECT seenScissor{}; context->RSGetScissorRects(&count, &seenScissor);
    CHECK(count == 1 && !std::memcmp(&seenScissor, &scissor, sizeof(scissor)));
    ComPtr<ID3D11RasterizerState> seenRaster; context->RSGetState(&seenRaster); CHECK(seenRaster.Get() == raster.Get());
    D3D11_PRIMITIVE_TOPOLOGY topology{}; context->IAGetPrimitiveTopology(&topology);
    CHECK(topology == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ComPtr<ID3D11VertexShader> seenVS; context->VSGetShader(&seenVS, nullptr, nullptr); CHECK(seenVS.Get() == vs.Get());
    ComPtr<ID3D11PixelShader> seenPS; context->PSGetShader(&seenPS, nullptr, nullptr); CHECK(seenPS.Get() == ps.Get());
    ComPtr<ID3D11RenderTargetView> seenView; context->OMGetRenderTargets(1, &seenView, nullptr);
    CHECK(seenView.Get() == appView.Get());
  };
  const DXGI_FORMAT formats[]{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM};
  for (unsigned formatIndex = 0; formatIndex < 4; ++formatIndex) {
    const auto format = formats[formatIndex];
    Texture<F> source(f, 7, 5, format, D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_PRESENT);
    std::array<uint32_t, 35> words{}, sentinel{}; sentinel.fill(0xff281008);
    for (UINT y = 0; y < 5; ++y) for (UINT x = 0; x < 7; ++x) words[y * 7 + x] = identityColor(x, y, 4);
    f.table.pfnResourceUpdateSubresourceUP(f.device, source.handle, 0, nullptr, words.data(), 28, 140);
    CHECK(lastError == S_OK);
    // This public-context canary proves the issued, non-hint predicate really
    // suppresses ordinary copies, before and after the production typed Blt.
    D3D11_TEXTURE2D_DESC probeDesc{}; probeDesc.Width = 7; probeDesc.Height = 5;
    probeDesc.MipLevels = probeDesc.ArraySize = probeDesc.SampleDesc.Count = 1; probeDesc.Format = format;
    D3D11_SUBRESOURCE_DATA nativeSource{words.data(), 28, 140}, nativeSentinel{sentinel.data(), 28, 140};
    ComPtr<ID3D11Texture2D> controlSource, controlDestination;
    CHECK(backend->CreateTexture2D(&probeDesc, &nativeSource, &controlSource) == S_OK);
    CHECK(backend->CreateTexture2D(&probeDesc, &nativeSentinel, &controlDestination) == S_OK);
    Texture<F> destination(f, 11, 8, format, D3D10_DDI_BIND_RENDER_TARGET);
    DXGI_DDI_ARG_BLT args{}; args.hSrcResource = source.dxgi(); args.hDstResource = destination.dxgi();
    args.DstLeft = 2; args.DstTop = 1; args.DstRight = 9; args.DstBottom = 6;
    args.Rotate = DXGI_DDI_MODE_ROTATION_IDENTITY; args.Flags.Convert = 1;
    context->SetPredication(predicate.Get(), FALSE);
    context->CopyResource(controlDestination.Get(), controlSource.Get());
    CHECK(f.blt(args) == S_OK && lastError == S_OK); state();
    context->CopyResource(controlDestination.Get(), controlSource.Get());
    context->SetPredication(nullptr, FALSE); identityRead(f, destination, 0, index, formatIndex, 0);
    probeDesc.Usage = D3D11_USAGE_STAGING; probeDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> controlStaging; CHECK(backend->CreateTexture2D(&probeDesc, nullptr, &controlStaging) == S_OK);
    context->CopyResource(controlStaging.Get(), controlDestination.Get());
    D3D11_MAPPED_SUBRESOURCE map{}; CHECK(context->Map(controlStaging.Get(), 0, D3D11_MAP_READ, 0, &map) == S_OK);
    identitySave(map.pData, map.RowPitch, 7, 5, format, index, formatIndex, 4, 0); context->Unmap(controlStaging.Get(), 0);
    Texture<F> array(f, 22, 16, format, D3D10_DDI_BIND_RENDER_TARGET, 0, false, 1, 2, 2);
    args.hDstResource = array.dxgi(); args.DstSubresource = 3;
    context->SetPredication(predicate.Get(), FALSE); CHECK(f.blt(args) == S_OK); state();
    context->SetPredication(nullptr, FALSE);
    for (UINT subresource = 0; subresource < 4; ++subresource) identityRead(f, array, subresource, index, formatIndex, 1);
    args.hDstResource = source.dxgi(); args.DstSubresource = 0;
    args.DstLeft = args.DstTop = 0; args.DstRight = 7; args.DstBottom = 5;
    context->SetPredication(predicate.Get(), FALSE); CHECK(f.blt(args) == S_OK); state();
    context->SetPredication(nullptr, FALSE); identityRead(f, source, 0, index, formatIndex, 2);
    Texture<F> rejected(f, 11, 8, format, D3D10_DDI_BIND_RENDER_TARGET);
    args.hDstResource = rejected.dxgi(); args.DstLeft = 2; args.DstTop = 1; args.DstRight = 9; args.DstBottom = 6;
    for (unsigned negative = 0; negative < 6; ++negative) {
      auto bad = args;
      if (negative == 0) bad.Flags.Value |= 16;
      if (negative == 1) bad.Rotate = DXGI_DDI_MODE_ROTATION_UNSPECIFIED;
      if (negative == 2) bad.SrcSubresource = 1;
      if (negative == 3) bad.DstSubresource = 1;
      if (negative == 4) bad.DstRight = 10;
      if (negative == 5) bad.Flags.Resolve = 1;
      CHECK(f.blt(bad) == E_INVALIDARG);
    }
    auto unsupported = args; unsupported.Flags.Present = 1; CHECK(f.blt(unsupported) == DXGI_DDI_ERR_UNSUPPORTED);
    identityRead(f, rejected, 0, index, formatIndex, 3);
    // The runtime Present-source contract currently requires one mip/slice.
    // Exercise selected-source mip and same-native-image aliasing directly in
    // the production GPU helper, rather than claim broader DDI admission.
    D3D11_TEXTURE2D_DESC aliasDesc{}; aliasDesc.Width = 22; aliasDesc.Height = 16;
    aliasDesc.MipLevels = 2; aliasDesc.ArraySize = 2; aliasDesc.SampleDesc.Count = 1;
    aliasDesc.Format = format; aliasDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    std::array<std::vector<uint32_t>, 4> aliasWords;
    std::array<D3D11_SUBRESOURCE_DATA, 4> aliasInitial{};
    for (UINT subresource = 0; subresource < 4; ++subresource) {
      const UINT width = 22 >> (subresource % 2), height = 16 >> (subresource % 2);
      auto& image = aliasWords[subresource]; image.resize(size_t(width) * height, 0xff281008);
      if (subresource == 1) for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x)
        image[size_t(y) * width + x] = identityColor(x, y, 4);
      aliasInitial[subresource] = {image.data(), width * 4, width * height * 4};
    }
    ComPtr<ID3D11Texture2D> alias; CHECK(backend->CreateTexture2D(&aliasDesc, aliasInitial.data(), &alias) == S_OK);
    dxvk::umd::BltPlan aliasPlan;
    CHECK(dxvk::umd::bltPlan(aliasDesc, aliasDesc, 1, 3, 0, 0, 11, 8, 2, 1, aliasPlan) == S_OK && aliasPlan.directCopy);
    context->SetPredication(predicate.Get(), FALSE);
    CHECK(dxvk::umd::bltTexture2D(backend.Get(), context, alias.Get(), alias.Get(), 1, 3, 0, 0, 1,
      aliasPlan, [] { return true; }) == S_OK); state(); context->SetPredication(nullptr, FALSE);
    aliasDesc.BindFlags = 0; aliasDesc.Usage = D3D11_USAGE_STAGING; aliasDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> aliasStaging; CHECK(backend->CreateTexture2D(&aliasDesc, nullptr, &aliasStaging) == S_OK);
    context->CopyResource(aliasStaging.Get(), alias.Get());
    for (UINT subresource : {1u, 3u}) {
      CHECK(context->Map(aliasStaging.Get(), subresource, D3D11_MAP_READ, 0, &map) == S_OK);
      identitySave(map.pData, map.RowPitch, 11, 8, format, index, formatIndex, 5, subresource);
      context->Unmap(aliasStaging.Get(), subresource);
    }
  }
}
static void identityBltPolicy() {
  D3D11_TEXTURE2D_DESC source{}; source.Width = 7; source.Height = 5;
  source.MipLevels = source.ArraySize = source.SampleDesc.Count = 1;
  source.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; source.BindFlags = D3D11_BIND_RENDER_TARGET;
  auto destination = source; dxvk::umd::BltPlan plan;
  for (UINT flags : {0u, 2u, 8u, 10u}) {
    CHECK(dxvk::umd::bltPlan(source, destination, 0, 0, 0, 0, 7, 5, flags, 1, plan) == S_OK && plan.directCopy);
  }
  CHECK(dxvk::umd::bltPlan(source, destination, 0, 0, 0, 0, 7, 5, 4, 1, plan) == S_OK && !plan.directCopy);
  CHECK(dxvk::umd::bltPlan(source, destination, 0, 0, 0, 0, 7, 5, 0, 3, plan) == S_OK && !plan.directCopy);
  destination.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  CHECK(dxvk::umd::bltPlan(source, destination, 0, 0, 0, 0, 7, 5, 2, 1, plan) == S_OK && !plan.directCopy);
}
static void x8BltPolicy() {
  D3D11_TEXTURE2D_DESC source{};
  source.Width = 7; source.Height = 5;
  source.MipLevels = source.ArraySize = source.SampleDesc.Count = 1;
  source.Format = DXGI_FORMAT_B8G8R8X8_UNORM;
  source.Usage = D3D11_USAGE_DEFAULT; source.BindFlags = D3D11_BIND_RENDER_TARGET;
  auto destination = source;
  dxvk::umd::BltPlan plan;
  CHECK(dxvk::umd::bltLinearFormat(source.Format) == source.Format);
  CHECK(dxvk::umd::bltPlan(source, destination, 0, 0, 0, 0, 7, 5, 0, 1, plan) == S_OK);
  CHECK(plan.sourceFormat == DXGI_FORMAT_B8G8R8X8_UNORM
    && plan.destinationFormat == DXGI_FORMAT_B8G8R8X8_UNORM
    && plan.sourceWidth == 7 && plan.sourceHeight == 5 && plan.width == 7 && plan.height == 5 && !plan.resolve);
  DXGI_DDI_ARG_BLT_FLAGS present{}; present.Present = 1;
  CHECK(present.Value == 8);
  CHECK(dxvk::umd::bltPlan(source, destination, 0, 0, 0, 0, 7, 5, present.Value, 1, plan) == S_OK);
  CHECK(plan.sourceFormat == DXGI_FORMAT_B8G8R8X8_UNORM
    && plan.destinationFormat == DXGI_FORMAT_B8G8R8X8_UNORM && !plan.resolve);
  // Rejected plans must not publish even partially changed format/extent data.
  const auto original = plan;
  for (unsigned test = 0; test < 10; ++test) {
    auto src = source, dst = destination;
    UINT flags = 0, right = 7, rotation = 1;
    HRESULT expected = DXGI_ERROR_UNSUPPORTED;
    if (test == 0) { src.SampleDesc.Count = 4; flags = 1; }
    if (test == 1) { src.Format = DXGI_FORMAT_B8G8R8A8_UNORM; flags = 2; }
    if (test == 2) { dst.Format = DXGI_FORMAT_B8G8R8A8_UNORM; flags = 2; }
    if (test == 3) src.Format = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
    if (test == 4) dst.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (test == 5) dst.BindFlags = 0;
    if (test == 6) { flags = 16; expected = E_INVALIDARG; }
    if (test == 7) { rotation = 0; expected = E_INVALIDARG; }
    if (test == 8) { right = 8; expected = E_INVALIDARG; }
    if (test == 9) { flags = 1; expected = E_INVALIDARG; }
    CHECK(dxvk::umd::bltPlan(src, dst, 0, 0, 0, 0, right, 5, flags, rotation, plan) == expected);
    CHECK(plan.sourceWidth == original.sourceWidth && plan.sourceHeight == original.sourceHeight
      && plan.width == original.width && plan.height == original.height
      && plan.sourceFormat == original.sourceFormat && plan.destinationFormat == original.destinationFormat
      && plan.resolve == original.resolve);
  }
}
int main() {
  caller = GetCurrentThreadId();
  x8BltPolicy();
  identityBltPolicy();
  std::vector<unsigned char> vertex, pixel;
  CHECK(dxvk::umd::bltShaderContainers(vertex, pixel));
  save("blt-internal-vs.dxbc", vertex.data(), vertex.size());
  save("blt-internal-ps.dxbc", pixel.data(), pixel.size());
  profile<D3D10DDI_DEVICEFUNCS>(0); profile<D3D10_1DDI_DEVICEFUNCS>(1); profile<D3D11DDI_DEVICEFUNCS>(2);
  terminalRetirement<D3D10DDI_DEVICEFUNCS>(); terminalRetirement<D3D10_1DDI_DEVICEFUNCS>(); terminalRetirement<D3D11DDI_DEVICEFUNCS>();
  identityProfile<D3D10DDI_DEVICEFUNCS>(0); identityProfile<D3D10_1DDI_DEVICEFUNCS>(1); identityProfile<D3D11DDI_DEVICEFUNCS>(2);
  CHECK(identitySnapshots == 120 && identityPixels == 15624);
  std::printf("DXGI identity Blt PASS profiles=3 formats=4 snapshots=120 pixels=15624 hardware_admission=0\n");
  CHECK(snapshots == 30 && pixels == 1536 && backings.empty() && bridges.empty() && lastError == S_OK);
  std::printf("DXGI Blt PASS checks=%u profiles=3 snapshots=30 pixels=1536 hardware_admission=0\n", checks.load());
}
