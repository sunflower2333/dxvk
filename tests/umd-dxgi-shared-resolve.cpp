// SPDX-License-Identifier: MIT
// Exact DXGI1.1 table publication and real typed resource/copy/readback paths.
// Only the embedded renderer is replaced by a controlled WARP fixture.
#include "../src/umd/umd_adapter.h"
#include "../src/umd/umd_api.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_blt_shader.h"
#include <wrl/client.h>
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
static unsigned predicateSnapshots, predicatePixels;
static DWORD caller;
static HRESULT lastError = S_OK, lockResult = S_OK, unlockResult = S_OK, submissionResult = S_OK;
static LUID selected{0x12345678, -43};
static char adapterCookie, deviceCookie, coreCookie, contextCookie;
static std::function<void()> lockAction, submissionAction;
static bool submitting;
static dxvk::umd::RuntimeBackend* constructingRuntime;
static std::unordered_map<ID3D11DeviceContext*, const dxvk::umd::RuntimeBackend*> bridges;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "shared resolve line=%d: %s error=%08lx\n", __LINE__, #value, static_cast<unsigned long>(lastError)); std::abort(); \
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
constexpr unsigned width = 7, height = 5;
static uint32_t expected(unsigned seed, unsigned x, unsigned y) {
  return 0xff000000u | (seed + 3 * x) | ((seed + 7 * y) << 8) | (11 * (x + y) << 16);
}
static std::array<uint32_t, width * height> image(unsigned seed) {
  std::array<uint32_t, width * height> data{};
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) data[y * width + x] = expected(seed, x, y);
  return data;
}
template<typename F>
struct Texture {
  F& fixture; Storage storage; D3D10DDI_HRESOURCE handle; char runtime;
  bool live = true; D3DKMT_HANDLE allocation{};
  Texture(F& f, bool shared, bool staging = false, unsigned seed = 0, D3DKMT_HANDLE opened = 0)
  : fixture(f), storage([&] { typename F::Desc desc{}; return f.table.pfnCalcPrivateResourceSize(f.device, &desc); }()), handle{storage.bytes} {
    lastError = S_OK;
    if (opened) {
      auto info = backings.at(opened).info;
      D3DDDI_OPENALLOCATIONINFO entry{}; entry.hAllocation = opened; entry.pPrivateDriverData = &info; entry.PrivateDriverDataSize = sizeof(info);
      D3D10DDIARG_OPENRESOURCE args{}; args.NumAllocations = 1; args.pOpenAllocationInfo = &entry; args.hKMResource.handle = opened + 1000;
      f.table.pfnOpenResource(f.device, &args, handle, {&runtime}); allocation = opened;
    } else {
      auto initial = image(seed); D3D10_DDIARG_SUBRESOURCE_UP data{initial.data(), width * 4, width * height * 4};
      D3D10DDI_MIPINFO mip{width, height, 1, width, height, 1}; typename F::Desc desc{};
      desc.pMipInfoList = &mip; desc.ResourceDimension = D3D10DDIRESOURCE_TEXTURE2D;
      desc.Usage = staging ? D3D10_DDI_USAGE_STAGING : D3D10_DDI_USAGE_DEFAULT;
      desc.BindFlags = staging ? 0 : D3D10_DDI_BIND_RENDER_TARGET | D3D10_DDI_BIND_SHADER_RESOURCE;
      desc.MapFlags = staging ? D3D10_DDI_CPU_ACCESS_READ : 0; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.MiscFlags = shared ? D3D10_DDI_RESOURCE_MISC_SHARED : 0; desc.SampleDesc.Count = 1;
      desc.MipLevels = desc.ArraySize = 1; if (seed) desc.pInitialDataUP = &data;
      f.table.pfnCreateResource(f.device, &desc, handle, {&runtime});
      for (const auto& entry : backings) if (entry.second.runtime == &runtime) allocation = entry.first;
    }
    CHECK(lastError == S_OK && (!shared || allocation));
  }
  DXGI_DDI_HRESOURCE dxgi() const { return reinterpret_cast<DXGI_DDI_HRESOURCE>(handle.pDrvPrivate); }
  void update(unsigned seed) {
    auto data = image(seed); lastError = S_OK;
    fixture.table.pfnResourceUpdateSubresourceUP(fixture.device, handle, 0, nullptr, data.data(), width * 4, width * height * 4);
    CHECK(lastError == S_OK);
  }
  void retire() { CHECK(live); live = false; fixture.table.pfnDestroyResource(fixture.device, handle); storage.poison(); }
  ~Texture() { if (live) retire(); }
};
static void backingPixels(D3DKMT_HANDLE handle, unsigned seed, bool write) {
  auto& backing = backings.at(handle);
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
    auto at = static_cast<uint8_t*>(backing.address()) + size_t(y) * backing.info.pitch + 4 * x;
    uint32_t word = expected(seed, x, y);
    if (write) std::memcpy(at, &word, 4);
    else { uint32_t actual; std::memcpy(&actual, at, 4); CHECK(actual == word); }
  }
  for (unsigned i = 0; i < 16; ++i) CHECK(backing.data[i] == 0xa5 && backing.data[backing.data.size() - 16 + i] == 0xa5);
  for (unsigned y = 0; y < height; ++y) for (unsigned x = width * 4; x < backing.info.pitch; ++x)
    CHECK(static_cast<uint8_t*>(backing.address())[size_t(y) * backing.info.pitch + x] == 0xa5);
}
static void save(const char* name, const void* data, size_t size) {
  FILE* file = nullptr; CHECK(fopen_s(&file, name, "wb") == 0 && file);
  CHECK(std::fwrite(data, 1, size, file) == size); CHECK(std::fclose(file) == 0);
}
template<typename F>
static void read(F& f, Texture<F>& source, Texture<F>& staging, unsigned seed, unsigned profile, unsigned sample) {
  lastError = S_OK; f.table.pfnResourceCopy(f.device, staging.handle, source.handle); CHECK(lastError == S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE map{};
  f.table.pfnStagingResourceMap(f.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &map);
  CHECK(lastError == S_OK && map.pData && map.RowPitch >= width * 4);
  std::array<uint32_t, width * height> actual{};
  for (unsigned y = 0; y < height; ++y) std::memcpy(actual.data() + y * width,
    static_cast<const uint8_t*>(map.pData) + size_t(y) * map.RowPitch, width * 4);
  const uint32_t metadata[]{width, height, seed, map.RowPitch, map.DepthPitch};
  char name[120]; std::snprintf(name, sizeof(name), "resolve-%u-%u.actual.u32.bin", profile, sample); save(name, actual.data(), sizeof(actual));
  std::snprintf(name, sizeof(name), "resolve-%u-%u.metadata.u32.bin", profile, sample); save(name, metadata, sizeof(metadata));
  f.table.pfnStagingResourceUnmap(f.device, staging.handle, 0);
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) { CHECK(actual[y * width + x] == expected(seed, x, y)); ++pixels; }
  ++snapshots;
}
template<typename Table>
static void profile(unsigned index) {
  using F = Fixture<Table>;
  {
    F old(false);
    CHECK(old.legacy.table.pfnPresent && old.legacy.table.pfnRotateResourceIdentities);
  }
  {
    F owner, reader; Texture<F> shared(owner, true, false, 1), opened(reader, false, false, 0, shared.allocation);
    Texture<F> staging(owner, false, true), readerStaging(reader, false, true), privateTexture(owner, false);
    CHECK(owner.resolve(shared.dxgi()) == S_OK); backingPixels(shared.allocation, 1, false);
    const auto previousLocks = locks, previousSubmissions = submissions;
    CHECK(owner.resolve(shared.dxgi()) == S_OK && locks == previousLocks && submissions == previousSubmissions + 1);
    read(reader, opened, readerStaging, 1, index, 0); CHECK(reader.resolve(opened.dxgi()) == S_OK);
    backingPixels(shared.allocation, 3, true); read(owner, shared, staging, 3, index, 1);
    shared.update(5); lockResult = E_OUTOFMEMORY;
    CHECK(owner.resolve(shared.dxgi()) == E_OUTOFMEMORY); lockResult = S_OK;
    backingPixels(shared.allocation, 3, false); read(owner, shared, staging, 5, index, 2);
    CHECK(owner.resolve(shared.dxgi()) == S_OK); backingPixels(shared.allocation, 5, false);
    read(reader, opened, readerStaging, 5, index, 3);
    CHECK(owner.resolve(privateTexture.dxgi()) == DXGI_DDI_ERR_UNSUPPORTED);
    CHECK(owner.resolve(opened.dxgi()) == E_INVALIDARG);
    Storage unknown(1); unknown.poison(); CHECK(owner.resolve(reinterpret_cast<DXGI_DDI_HRESOURCE>(unknown.bytes)) == E_INVALIDARG);
    shared.update(7); submissionResult = E_OUTOFMEMORY;
    CHECK(owner.resolve(shared.dxgi()) == E_OUTOFMEMORY); submissionResult = S_OK;
    backingPixels(shared.allocation, 7, false); CHECK(owner.resolve(shared.dxgi()) == S_OK);
    shared.update(9); lockResult = S_FALSE;
    const auto previousUnlocks = unlocks;
    CHECK(owner.resolve(shared.dxgi()) == E_FAIL && unlocks == previousUnlocks + 1); lockResult = S_OK;
    backingPixels(shared.allocation, 7, false);
    unlockResult = E_OUTOFMEMORY;
    CHECK(owner.resolve(shared.dxgi()) == E_OUTOFMEMORY); unlockResult = S_OK;
    backingPixels(shared.allocation, 9, false);
    lockAction = [&] {
      CHECK(owner.resolve(shared.dxgi()) == DXGI_ERROR_WAS_STILL_DRAWING);
      DXGI_DDI_HRESOURCE resource = shared.dxgi(); DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES rotation{};
      rotation.hDevice = reinterpret_cast<DXGI_DDI_HDEVICE>(owner.device.pDrvPrivate); rotation.Resources = 1; rotation.pResources = &resource;
      CHECK(owner.dxgi.pfnRotateResourceIdentities(&rotation) == DXGI_ERROR_WAS_STILL_DRAWING);
    };
    submissionAction = [&] { runtimeCaller(); backingPixels(shared.allocation, 9, false); };
    CHECK(owner.resolve(shared.dxgi()) == S_OK); backingPixels(shared.allocation, 9, false);
    CHECK(owner.dxgi.pfnResolveSharedResource(nullptr) == E_INVALIDARG);
    DXGI_DDI_ARG_RESOLVESHAREDRESOURCE bad{};
    CHECK(owner.dxgi.pfnResolveSharedResource(&bad) == E_INVALIDARG);
  }
  CHECK(backings.empty());
  {
    F f; Backing external; external.info.width = width; external.info.height = height;
    external.info.pitch = 40; external.info.size = 40 * height; external.info.format = 3;
    external.data.assign(size_t(external.info.size) + 32, 0xa5); CHECK(backings.emplace(9001, std::move(external)).second);
    {
      Texture<F> opened(f, false, false, 0, 9001), staging(f, false, true);
      backingPixels(9001, 11, true); read(f, opened, staging, 11, index, 4);
      opened.update(13); CHECK(f.resolve(opened.dxgi()) == S_OK); backingPixels(9001, 13, false);
      const auto& retained = backings.at(9001);
      char name[120]; std::snprintf(name, sizeof(name), "resolve-padded-%u.actual.bin", index);
      save(name, retained.data.data(), retained.data.size());
      const uint32_t metadata[]{width, height, retained.info.pitch, uint32_t(retained.info.size)};
      std::snprintf(name, sizeof(name), "resolve-padded-%u.metadata.u32.bin", index); save(name, metadata, sizeof(metadata));
    }
    CHECK(backings.erase(9001) == 1); // Foreign owner, never DeallocateCb.
  }
  {
    F f; Texture<F> shared(f, true, false, 15); const auto handle = shared.allocation;
    lockAction = [&] { const auto before = releases; shared.retire(); CHECK(releases == before && backings.count(handle)); };
    CHECK(f.resolve(shared.dxgi()) == DXGI_ERROR_DEVICE_REMOVED && !backings.count(handle));
  }
  {
    F f; Texture<F> shared(f, true); const auto handle = shared.allocation;
    const auto before = releases;
    submissionAction = [&] {
      runtimeCaller(); shared.retire();
      // Resolve pins this owned surface beyond DestroyResource. DestroyDevice
      // still owes the real DeallocateCb before its callback service closes.
      CHECK(releases == before && backings.count(handle));
      f.retire(); CHECK(releases == before + 1 && !backings.count(handle));
    };
    CHECK(f.resolve(shared.dxgi()) == DXGI_ERROR_DEVICE_REMOVED);
    CHECK(releases == before + 1 && backings.empty());
  }
  CHECK(backings.empty() && !lockAction && !submissionAction && lastError == S_OK);
}
static void predicateSave(const void* address, UINT pitch, UINT depthPitch,
    unsigned seed, unsigned profile, unsigned test) {
  CHECK(address && pitch >= width * 4);
  std::array<uint32_t, width * height> actual{};
  for (unsigned y = 0; y < height; ++y) std::memcpy(actual.data() + y * width,
    static_cast<const uint8_t*>(address) + size_t(y) * pitch, width * 4);
  const uint32_t metadata[]{width, height, test, seed, pitch, depthPitch, 28, profile};
  char name[120]; std::snprintf(name, sizeof(name), "shared-transfer-predicate-%u-%u.actual.u32.bin", profile, test);
  save(name, actual.data(), sizeof(actual));
  std::snprintf(name, sizeof(name), "shared-transfer-predicate-%u-%u.metadata.u32.bin", profile, test);
  save(name, metadata, sizeof(metadata));
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
    const auto wanted = expected(seed, x, y);
    if (actual[y * width + x] != wanted) std::fprintf(stderr,
      "Shared predicate mismatch profile=%u case=%u seed=%u x=%u y=%u actual=%08x expected=%08x\n",
      profile, test, seed, x, y, actual[y * width + x], wanted);
    CHECK(actual[y * width + x] == wanted); ++predicatePixels;
  }
  ++predicateSnapshots;
}
template<typename F>
static void predicateRead(F& f, Texture<F>& source, Texture<F>& staging,
    unsigned seed, unsigned profile, unsigned test) {
  lastError = S_OK; f.table.pfnResourceCopy(f.device, staging.handle, source.handle); CHECK(lastError == S_OK);
  D3D10DDI_MAPPED_SUBRESOURCE mapped{};
  f.table.pfnStagingResourceMap(f.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &mapped);
  CHECK(lastError == S_OK);
  predicateSave(mapped.pData, mapped.RowPitch, mapped.DepthPitch, seed, profile, test);
  f.table.pfnStagingResourceUnmap(f.device, staging.handle, 0); CHECK(lastError == S_OK);
}
static void predicateBacking(D3DKMT_HANDLE allocation, unsigned seed, unsigned profile, unsigned test) {
  backingPixels(allocation, seed, false);
  const auto& backing = backings.at(allocation);
  CHECK(backing.info.pitch == 40 && backing.info.size == 200 && backing.data.size() == 232);
  const uint32_t metadata[]{width, height, test, seed, backing.info.pitch, uint32_t(backing.info.size), 28, profile};
  char name[120]; std::snprintf(name, sizeof(name), "shared-transfer-predicate-%u-%u.actual.bin", profile, test);
  save(name, backing.data.data(), backing.data.size());
  std::snprintf(name, sizeof(name), "shared-transfer-predicate-%u-%u.metadata.u32.bin", profile, test);
  save(name, metadata, sizeof(metadata)); ++predicateSnapshots; predicatePixels += width * height;
}
template<typename Table>
static void predicateProfile(unsigned index) {
  using F = Fixture<Table>; using Microsoft::WRL::ComPtr;
  F f; auto context = f.contextKey; ComPtr<ID3D11Device> backend; context->GetDevice(&backend);
  std::vector<unsigned char> vertex, pixel; CHECK(dxvk::umd::bltShaderContainers(vertex, pixel));
  ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
  CHECK(backend->CreateVertexShader(vertex.data(), vertex.size(), nullptr, &vs) == S_OK);
  CHECK(backend->CreatePixelShader(pixel.data(), pixel.size(), nullptr, &ps) == S_OK);
  const D3D11_VIEWPORT viewport{3, 4, 17, 19, .25f, .75f}; const D3D11_RECT scissor{31, 37, 41, 43};
  D3D11_RASTERIZER_DESC rasterDesc{}; rasterDesc.FillMode = D3D11_FILL_SOLID;
  rasterDesc.CullMode = D3D11_CULL_FRONT; rasterDesc.ScissorEnable = TRUE; rasterDesc.DepthClipEnable = TRUE;
  ComPtr<ID3D11RasterizerState> raster; CHECK(backend->CreateRasterizerState(&rasterDesc, &raster) == S_OK);
  D3D11_TEXTURE2D_DESC appDesc{}; appDesc.Width = appDesc.Height = 4;
  appDesc.MipLevels = appDesc.ArraySize = appDesc.SampleDesc.Count = 1;
  appDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; appDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> target; ComPtr<ID3D11RenderTargetView> view;
  CHECK(backend->CreateTexture2D(&appDesc, nullptr, &target) == S_OK);
  CHECK(backend->CreateRenderTargetView(target.Get(), nullptr, &view) == S_OK);
  context->RSSetViewports(1, &viewport); context->RSSetScissorRects(1, &scissor); context->RSSetState(raster.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
  auto targetView = view.Get(); context->OMSetRenderTargets(1, &targetView, nullptr);
  ComPtr<ID3D11Predicate> predicate; const D3D11_QUERY_DESC queryDesc{D3D11_QUERY_OCCLUSION_PREDICATE, 0};
  CHECK(backend->CreatePredicate(&queryDesc, &predicate) == S_OK);
  context->Begin(predicate.Get()); context->End(predicate.Get()); context->Flush();
  BOOL visible = TRUE; HRESULT result = S_FALSE; const auto started = GetTickCount64();
  while (result == S_FALSE && GetTickCount64() - started < 10000) {
    result = context->GetData(predicate.Get(), &visible, sizeof(visible), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result == S_FALSE) SwitchToThread();
  }
  CHECK(result == S_OK && visible == FALSE);
  auto state = [&] {
    ComPtr<ID3D11Predicate> seenPredicate; BOOL value = TRUE;
    context->GetPredication(&seenPredicate, &value); CHECK(seenPredicate.Get() == predicate.Get() && value == FALSE);
    UINT count = 1; D3D11_VIEWPORT seenViewport{}; context->RSGetViewports(&count, &seenViewport);
    CHECK(count == 1 && !std::memcmp(&seenViewport, &viewport, sizeof(viewport)));
    count = 1; D3D11_RECT seenScissor{}; context->RSGetScissorRects(&count, &seenScissor);
    CHECK(count == 1 && !std::memcmp(&seenScissor, &scissor, sizeof(scissor)));
    ComPtr<ID3D11RasterizerState> seenRaster; context->RSGetState(&seenRaster); CHECK(seenRaster.Get() == raster.Get());
    D3D11_PRIMITIVE_TOPOLOGY topology{}; context->IAGetPrimitiveTopology(&topology);
    CHECK(topology == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ComPtr<ID3D11VertexShader> seenVS; context->VSGetShader(&seenVS, nullptr, nullptr); CHECK(seenVS.Get() == vs.Get());
    ComPtr<ID3D11PixelShader> seenPS; context->PSGetShader(&seenPS, nullptr, nullptr); CHECK(seenPS.Get() == ps.Get());
    ComPtr<ID3D11RenderTargetView> seenView; context->OMGetRenderTargets(1, &seenView, nullptr); CHECK(seenView.Get() == view.Get());
  };
  Backing external; external.info.width = width; external.info.height = height;
  external.info.pitch = 40; external.info.size = 200; external.info.format = 3;
  external.data.assign(232, 0xa5); CHECK(backings.emplace(9002, std::move(external)).second);
  {
    Texture<F> opened(f, false, false, 0, 9002), staging(f, false, true), canary(f, false, false, 3);
    backingPixels(9002, 17, true);
    context->SetPredication(predicate.Get(), FALSE);
    lockResult = E_OUTOFMEMORY; lastError = S_OK;
    f.table.pfnResourceCopy(f.device, canary.handle, opened.handle); CHECK(lastError == E_OUTOFMEMORY);
    lockResult = S_OK; lastError = S_OK;
    f.table.pfnResourceCopy(f.device, canary.handle, opened.handle); CHECK(lastError == S_OK); state();
    // The internal allocation->cache hop must happen, but these application
    // copies remain suppressed. A retry must not memoize the failed download.
    const auto refreshedLocks = locks; backingPixels(9002, 31, true);
    f.table.pfnResourceCopy(f.device, canary.handle, opened.handle); CHECK(lastError == S_OK && locks == refreshedLocks); state();
    context->SetPredication(nullptr, FALSE);
    predicateRead(f, opened, staging, 17, index, 0);
    CHECK(locks == refreshedLocks); // Source observation must use the completed snapshot, not redownload seed31.
    predicateRead(f, canary, staging, 3, index, 1);
    opened.update(19); context->SetPredication(predicate.Get(), FALSE);
    CHECK(f.resolve(opened.dxgi()) == S_OK); state(); predicateBacking(9002, 19, index, 2);
    context->SetPredication(nullptr, FALSE); opened.update(23);
    context->SetPredication(predicate.Get(), FALSE); lockResult = E_OUTOFMEMORY;
    CHECK(f.resolve(opened.dxgi()) == E_OUTOFMEMORY); state(); lockResult = S_OK;
    predicateBacking(9002, 19, index, 3);
    CHECK(f.resolve(opened.dxgi()) == S_OK); state(); predicateBacking(9002, 23, index, 4);
    // Update remains an app operation, and Map remains non-predicated.
    canary.update(29); state(); context->SetPredication(nullptr, FALSE);
    predicateRead(f, canary, staging, 3, index, 5);
    lastError = S_OK; f.table.pfnResourceCopy(f.device, staging.handle, opened.handle); CHECK(lastError == S_OK);
    // The completed ownership handoff invalidated the cache; this clear
    // predicate read refreshes seed23. Use a second dedicated seed17 staging
    // source to keep the Map control independent of later publication seeds.
    Texture<F> mapSource(f, false, false, 17);
    f.table.pfnResourceCopy(f.device, staging.handle, mapSource.handle); CHECK(lastError == S_OK);
    context->SetPredication(predicate.Get(), FALSE);
    D3D10DDI_MAPPED_SUBRESOURCE mapped{};
    f.table.pfnStagingResourceMap(f.device, staging.handle, 0, D3D10_DDI_MAP_READ, 0, &mapped);
    CHECK(lastError == S_OK); predicateSave(mapped.pData, mapped.RowPitch, mapped.DepthPitch, 17, index, 6);
    f.table.pfnStagingResourceUnmap(f.device, staging.handle, 0); CHECK(lastError == S_OK); state();
    context->SetPredication(nullptr, FALSE);
  }
  CHECK(backings.erase(9002) == 1); // The borrowed allocation never owes DeallocateCb.
}
int main() {
  caller = GetCurrentThreadId();
  profile<D3D10DDI_DEVICEFUNCS>(0); profile<D3D10_1DDI_DEVICEFUNCS>(1); profile<D3D11DDI_DEVICEFUNCS>(2);
  predicateProfile<D3D10DDI_DEVICEFUNCS>(0); predicateProfile<D3D10_1DDI_DEVICEFUNCS>(1); predicateProfile<D3D11DDI_DEVICEFUNCS>(2);
  CHECK(predicateSnapshots == 21 && predicatePixels == 735 && backings.empty() && bridges.empty() && lastError == S_OK);
  std::printf("DXGI shared transfer predicate PASS profiles=3 snapshots=21 pixels=735 hardware_admission=0\n");
  CHECK(snapshots == 15 && pixels == 525 && bridges.empty());
  std::printf("DXGI shared resolve PASS checks=%u profiles=3 snapshots=%u pixels=%u hardware_admission=0\n", checks.load(), snapshots, pixels);
}
