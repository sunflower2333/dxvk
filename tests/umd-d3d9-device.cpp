#include "../src/umd/umd_d3d9_adapter.h"
#include "../src/umd/umd_d3d9_backend.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_runtime_service.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <climits>

static std::atomic<unsigned> checks{0};
#define CHECK(c) do { const auto n = ++checks; if (!(c)) { \
  std::fprintf(stderr, "D3D9 device check %u line %d: %s\n", n, __LINE__, #c); std::exit(1); } } while (0)

template<typename T> static std::array<uint8_t, sizeof(T)> snapshot(const T& value) {
  std::array<uint8_t, sizeof(T)> result;
  std::memcpy(result.data(), &value, result.size());
  return result;
}
static void put(void* ptr, unsigned offset, uint64_t value, unsigned length) {
  auto bytes = static_cast<uint8_t*>(ptr);
  for (unsigned i = 0; i < length; i++) bytes[offset+i] = uint8_t(value >> (8*i));
}
static uint64_t read(const void* ptr, unsigned offset, unsigned length) {
  auto bytes = static_cast<const uint8_t*>(ptr);
  uint64_t value = 0;
  for (unsigned i = 0; i < length; i++) value |= uint64_t(bytes[offset+i]) << (8*i);
  return value;
}

struct Fixture {
  char adapterCookie = 0, deviceCookie = 0, contextCookie = 0;
  DWORD caller = GetCurrentThreadId();
  LUID luid{0x13579024, -11};
  uint64_t generation = 73, capabilities = 3;
  HANDLE adapter = nullptr, device = nullptr;
  D3DDDI_ADAPTERFUNCS adapterFuncs = {};
  D3DDDI_DEVICEFUNCS table = {}, alternateTable = {};
  D3DDDI_DEVICECALLBACKS input = {};
  D3DDDIARG_CREATEDEVICE create = {};
  std::array<uint8_t, 4096> commands = {}, pixels = {};
  D3DDDI_ALLOCATIONLIST list[64] = {};
  D3DDDI_PATCHLOCATIONLIST patches[64] = {};
  std::vector<char> cleanup;
  unsigned queries = 0, contexts = 0, contextCloses = 0;
  unsigned allocations = 0, deallocations = 0, locks = 0, unlocks = 0;
  std::atomic<unsigned> backends{0}, backendCloses{0}, backendFlushes{0};
  HRESULT queryResult = S_OK, backendResult = S_OK, flushResult = S_OK, destroyResult = S_OK;
  bool allocationFailure = false, contextFailure = false, nullBackend = false;
  bool throwAllocation = false, throwOther = false, mapped = false, allocated = false;
  bool callbacksValid = true, adapterValid = true;
  std::function<void()> queryHook, contextHook, allocationHook, destroyHook;
  std::function<void()> backendHook;
  std::function<void()> surfaceHook;
  unsigned surfaceCreates = 0, surfaceCloses = 0, surfaceLocks = 0, surfaceUnlocks = 0;
  unsigned surfaceAttempts = 0, failSurfaceAttempt = 0, clears = 0, copies = 0;
  HRESULT surfaceResult = S_OK, surfaceUnlockResult = S_OK;
  bool nullSurface = false, badMapping = false;
  bool lastComputeRects = false, teardownDiscard = false;
  std::vector<dxvk::umd::D3D9SurfaceDesc> descriptions;
  std::vector<RECT> clearRects;
  void runtime() const { CHECK(callbacksValid && GetCurrentThreadId() == caller); }
};
static Fixture* f;

static HRESULT APIENTRY query(HANDLE adapter, const D3DDDICB_QUERYADAPTERINFO* args) {
  f->runtime();
  CHECK(f->adapterValid);
  CHECK(adapter == &f->adapterCookie && args && args->PrivateDriverDataSize == 160);
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (unsigned i = 0; i < 160; i++) CHECK(bytes[i] == 0);
  ++f->queries;
  if (f->queryHook) { auto hook = std::move(f->queryHook); hook(); }
  put(bytes, 0, 0x504d5644, 4); put(bytes, 8, 128, 4);
  put(bytes, 16, f->capabilities, 8); put(bytes, 24, f->generation, 8);
  put(bytes, 128, 0x44494c56, 4); put(bytes, 132, 1, 4); put(bytes, 136, 32, 4);
  put(bytes, 140, 1, 4); std::memcpy(bytes+144, &f->luid, sizeof(LUID)); put(bytes, 152, 1, 4);
  return f->queryResult;
}
static HRESULT APIENTRY createContext(HANDLE device, D3DDDICB_CREATECONTEXT* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && args->EngineAffinity == 1);
  CHECK(args->PrivateDriverDataSize == 32 && read(args->pPrivateDriverData, 16, 8) == f->generation);
  ++f->contexts;
  if (f->contextFailure) return E_OUTOFMEMORY;
  args->hContext = &f->contextCookie;
  args->pCommandBuffer = f->commands.data(); args->CommandBufferSize = UINT(f->commands.size());
  args->pAllocationList = f->list; args->AllocationListSize = 64;
  args->pPatchLocationList = f->patches; args->PatchLocationListSize = 64;
  if (f->contextHook) { auto hook = std::move(f->contextHook); hook(); }
  return S_OK;
}
static HRESULT APIENTRY destroyContext(HANDLE device, const D3DDDICB_DESTROYCONTEXT* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && args->hContext == &f->contextCookie);
  CHECK(!f->allocated && !f->mapped);
  f->cleanup.push_back('C'); ++f->contextCloses;
  if (f->destroyHook) { auto hook = std::move(f->destroyHook); hook(); }
  return f->destroyResult;
}
static HRESULT APIENTRY escape(HANDLE adapter, const D3DDDICB_ESCAPE* args) {
  f->runtime();
  CHECK(f->adapterValid);
  CHECK(adapter == &f->adapterCookie && args && args->hDevice == &f->deviceCookie);
  CHECK(args->hContext == &f->contextCookie && read(args->pPrivateDriverData, 24, 8) == f->generation);
  void* bytes = args->pPrivateDriverData;
  if (args->PrivateDriverDataSize == 64) {
    CHECK(read(bytes, 16, 4) == 1);
    put(bytes, 32, 0x100000000ull, 8); put(bytes, 40, 0x1000000, 8);
    put(bytes, 48, f->generation, 8); put(bytes, 56, 17, 4); put(bytes, 60, 19, 4);
  } else {
    CHECK(args->PrivateDriverDataSize == 56 && read(bytes, 16, 4) == 2);
    put(bytes, 32, 0, 8); put(bytes, 40, f->generation, 8); put(bytes, 48, 17, 4);
  }
  return S_OK;
}
static HRESULT APIENTRY allocate(HANDLE device, D3DDDICB_ALLOCATE* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && !args->hResource && args->NumAllocations == 1);
  CHECK(args->pAllocationInfo && args->pAllocationInfo->PrivateDriverDataSize == sizeof(dxvk::umd::AllocationInfo));
  dxvk::umd::AllocationInfo info;
  std::memcpy(&info, args->pAllocationInfo->pPrivateDriverData, sizeof(info));
  CHECK(info.contextId == 17 && info.resetGeneration == f->generation && info.size == 4096);
  ++f->allocations;
  if (f->allocationFailure) return E_OUTOFMEMORY;
  CHECK(!f->allocated); f->allocated = true;
  args->pAllocationInfo->hAllocation = 31;
  if (f->allocationHook) { auto hook = std::move(f->allocationHook); hook(); }
  return S_OK;
}
static HRESULT APIENTRY deallocate(HANDLE device, const D3DDDICB_DEALLOCATE* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && !args->hResource && args->NumAllocations == 1);
  CHECK(args->HandleList && *args->HandleList == 31 && f->allocated && !f->mapped);
  ++f->deallocations; f->allocated = false; f->cleanup.push_back('A'); return S_OK;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && args->hAllocation == 31 && f->allocated && !f->mapped);
  ++f->locks; f->mapped = true; args->pData = f->pixels.data(); return S_OK;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && args->NumAllocations == 1 && *args->phAllocations == 31);
  CHECK(f->mapped); ++f->unlocks; f->mapped = false; f->cleanup.push_back('U'); return S_OK;
}
static HRESULT APIENTRY render(HANDLE, D3DDDICB_RENDER*) { CHECK(false); return E_FAIL; }

// Substitute only the renderer for this CPU fixture. The actual typed adapter,
// device lifetime, callback pump and RuntimeGpu allocation/context code run.
struct dxvk::umd::D3D9Backend::State {
  RuntimeBackend bridge;
  mwd_allocation allocation = {};
  D3D9SurfaceResource* target = nullptr;
};
struct dxvk::umd::D3D9SurfaceResource::State {
  D3D9SurfaceDesc desc;
  std::vector<uint8_t> bytes;
  RECT area = {};
  bool locked = false;
};
dxvk::umd::D3D9SurfaceResource::D3D9SurfaceResource() : m_state(std::make_unique<State>()) { }
dxvk::umd::D3D9SurfaceResource::~D3D9SurfaceResource() {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(!m_state->locked);
  ++f->surfaceCloses;
}
dxvk::umd::D3D9Backend::D3D9Backend() : m_state(std::make_unique<State>()) { }
dxvk::umd::D3D9Backend::~D3D9Backend() {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(f->surfaceCreates == f->surfaceCloses && !m_state->target);
  ++f->backendCloses;
  if (!f->adapterValid) {
    uint32_t fence = 99;
    CHECK(m_state->bridge.create.callbacks->completed(m_state->bridge.create.owner, &fence)
      == DXGI_ERROR_DEVICE_REMOVED && fence == 0);
  }
  // Leave mapped backing owned by RuntimeGpu, to verify terminal cleanup
  // unlocks it before deallocation and destroys the context last.
  m_state.reset();
}
IDirect3DDevice9Ex* dxvk::umd::D3D9Backend::device() const noexcept { return nullptr; }
HRESULT dxvk::umd::D3D9Backend::create(const AdapterLuid& luid, const RuntimeBackend* runtime,
                                    std::unique_ptr<D3D9Backend>& output) noexcept {
  output.reset();
  CHECK(GetCurrentThreadId() != f->caller && runtime && runtime->owner);
  CHECK(!std::memcmp(luid.data(), &f->luid, luid.size()));
  ++f->backends;
  try {
    if (f->throwAllocation) throw std::bad_alloc();
    if (f->throwOther) throw 1;
    if (f->backendResult != S_OK) return f->backendResult;
    if (f->nullBackend) return S_OK;
    auto backend = std::make_unique<D3D9Backend>();
    backend->m_state->bridge = *runtime;
    const auto cb = runtime->create.callbacks;
    const auto owner = runtime->create.owner;
    mwd_context_info context = {};
    HRESULT hr = cb->context(owner, &context);
    if (FAILED(hr)) return hr;
    hr = cb->allocate(owner, 4096, 4096, context.va_start, 6,
                      &backend->m_state->allocation);
    if (FAILED(hr)) return hr;
    void* pixels = nullptr; uint32_t pitch = 0;
    hr = cb->map(owner, backend->m_state->allocation.token, &pixels, &pitch);
    if (FAILED(hr)) return hr;
    CHECK(pixels == f->pixels.data());
    if (f->backendHook) { auto hook = std::move(f->backendHook); hook(); }
    output = std::move(backend);
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
HRESULT dxvk::umd::D3D9Backend::flush() noexcept {
  CHECK(GetCurrentThreadId() != f->caller); ++f->backendFlushes;
  if (f->flushResult != S_OK) return f->flushResult;
  return m_state->bridge.create.callbacks->status(m_state->bridge.create.owner);
}

HRESULT dxvk::umd::D3D9Backend::createSurface(const D3D9SurfaceDesc& desc,
    std::unique_ptr<D3D9SurfaceResource>& output) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->surfaceAttempts;
  if (f->surfaceAttempts == f->failSurfaceAttempt) return E_OUTOFMEMORY;
  if (f->surfaceResult != S_OK) return f->surfaceResult;
  if (f->nullSurface) return S_OK;
  auto resource = std::make_unique<D3D9SurfaceResource>();
  ++f->surfaceCreates;
  resource->m_state->desc = desc;
  resource->m_state->bytes.resize(size_t(desc.width) * desc.height * 4);
  f->descriptions.push_back(desc);
  if (f->surfaceHook) { auto hook = std::move(f->surfaceHook); hook(); }
  output = std::move(resource);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setRenderTarget(D3D9SurfaceResource* target) {
  CHECK(GetCurrentThreadId() != f->caller);
  m_state->target = target;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::clear(D3DCOLOR color, UINT count, const RECT* rects, bool computeRects) {
  CHECK(GetCurrentThreadId() != f->caller && m_state->target);
  ++f->clears;
  f->lastComputeRects = computeRects;
  f->clearRects.clear();
  if (count) f->clearRects.assign(rects, rects + count);
  if (!count && !computeRects) return S_OK;
  auto& state = *m_state->target->m_state;
  for (size_t i = 0; i < state.bytes.size(); i += 4) std::memcpy(state.bytes.data() + i, &color, 4);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::copySurface(D3D9SurfaceResource& destination, const RECT& destinationRect,
    D3D9SurfaceResource& source, const RECT& sourceRect) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->copies;
  auto& dst = *destination.m_state;
  auto& src = *source.m_state;
  if (dst.desc.systemMemory && src.desc.systemMemory) return flush();
  const size_t bytes = size_t(sourceRect.right - sourceRect.left) * 4;
  for (LONG row = 0; row < sourceRect.bottom - sourceRect.top; ++row) {
    auto from = src.bytes.data() + (size_t(row + sourceRect.top) * src.desc.width + sourceRect.left) * 4;
    auto to = dst.bytes.data() + (size_t(row + destinationRect.top) * dst.desc.width + destinationRect.left) * 4;
    std::memcpy(to, from, bytes);
    if (dst.desc.systemData)
      std::memcpy(static_cast<uint8_t*>(dst.desc.systemData) + size_t(row + destinationRect.top)
        * dst.desc.systemPitch + size_t(destinationRect.left) * 4, from, bytes);
  }
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::lockSurface(D3D9SurfaceResource& resource, const RECT* area,
    DWORD, D3DLOCKED_RECT& output) {
  CHECK(GetCurrentThreadId() != f->caller);
  auto& state = *resource.m_state;
  CHECK(!state.locked);
  state.locked = true;
  state.area = area ? *area : RECT{0, 0, LONG(state.desc.width), LONG(state.desc.height)};
  ++f->surfaceLocks;
  output.Pitch = f->badMapping ? 0 : INT(state.desc.systemData ? state.desc.systemPitch : state.desc.width * 4);
  auto data = state.desc.systemData ? static_cast<uint8_t*>(state.desc.systemData) : state.bytes.data();
  output.pBits = f->badMapping ? nullptr : data + size_t(state.area.top) * UINT(output.Pitch) + size_t(state.area.left) * 4;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::unlockSurface(D3D9SurfaceResource& resource, bool upload) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->surfaceUnlockResult != S_OK) return f->surfaceUnlockResult;
  CHECK(resource.m_state->locked);
  resource.m_state->locked = false;
  ++f->surfaceUnlocks;
  if (!upload) f->teardownDiscard = true;
  return S_OK;
}

static void initialize(Fixture& fixture) {
  f = &fixture;
  D3DDDI_ADAPTERCALLBACKS callbacks = {}; callbacks.pfnQueryAdapterInfoCb = query;
  D3DDDIARG_OPENADAPTER args = {};
  args.hAdapter = &f->adapterCookie; args.Interface = 9;
  args.pAdapterCallbacks = &callbacks; args.pAdapterFuncs = &f->adapterFuncs;
  CHECK(VioGpuDxvkOpenAdapter9ForTest(&args) == S_OK); f->adapter = args.hAdapter;
  auto& cb = f->input;
  cb.pfnAllocateCb = allocate; cb.pfnDeallocateCb = deallocate;
  cb.pfnLockCb = lock; cb.pfnUnlockCb = unlock;
  cb.pfnCreateContextCb = createContext; cb.pfnDestroyContextCb = destroyContext;
  cb.pfnEscapeCb = escape; cb.pfnRenderCb = render;
  std::memset(&f->table, 0xa5, sizeof(f->table));
  auto& create = f->create;
  create.hDevice = &f->deviceCookie; create.Interface = 9; create.Version = 0xffffffff;
  create.pCallbacks = &cb; create.pDeviceFuncs = &f->table;
  // Obsolete fields are deliberate invalid addresses, never used as backing.
  create.pCommandBuffer = reinterpret_cast<void*>(UINT_PTR(1)); create.CommandBufferSize = 7;
  create.pAllocationList = reinterpret_cast<D3DDDI_ALLOCATIONLIST*>(UINT_PTR(2)); create.AllocationListSize = 3;
  create.pPatchLocationList = reinterpret_cast<D3DDDI_PATCHLOCATIONLIST*>(UINT_PTR(4)); create.PatchLocationListSize = 5;
  create.CommandBuffer = 0xcafef00d;
}
static void unchangedCreate(HRESULT expected) {
  const auto args = snapshot(f->create);
  const auto table = snapshot(f->table);
  CHECK(f->adapterFuncs.pfnCreateDevice(f->adapter, &f->create) == expected);
  CHECK(snapshot(f->create) == args && snapshot(f->table) == table);
}
static void createDevice() {
  auto expected = f->create;
  CHECK(f->adapterFuncs.pfnCreateDevice(f->adapter, &f->create) == S_OK);
  f->device = f->create.hDevice;
  CHECK(f->device && f->device != &f->deviceCookie);
  expected.hDevice = f->device; CHECK(snapshot(f->create) == snapshot(expected));
  CHECK(f->table.pfnFlush && f->table.pfnDestroyDevice);
  auto expectedTable = D3DDDI_DEVICEFUNCS{};
  expectedTable.pfnFlush = f->table.pfnFlush; expectedTable.pfnDestroyDevice = f->table.pfnDestroyDevice;
  expectedTable.pfnCreateResource = f->table.pfnCreateResource;
  expectedTable.pfnDestroyResource = f->table.pfnDestroyResource;
  expectedTable.pfnSetRenderTarget = f->table.pfnSetRenderTarget;
  expectedTable.pfnClear = f->table.pfnClear;
  expectedTable.pfnBlt = f->table.pfnBlt;
  expectedTable.pfnLock = f->table.pfnLock;
  expectedTable.pfnUnlock = f->table.pfnUnlock;
  CHECK(snapshot(f->table) == snapshot(expectedTable));
}
static void closeDevice(HRESULT expected = S_OK) {
  CHECK(f->table.pfnDestroyDevice(f->device) == expected);
  CHECK(!f->allocated && !f->mapped && f->contextCloses == f->contexts);
  CHECK(f->allocations == f->deallocations && f->locks == f->unlocks);
  const std::vector<char> order{'U','A','C'}; CHECK(f->cleanup == order);
  f->callbacksValid = false;
  CHECK(f->table.pfnDestroyDevice(f->device) == E_INVALIDARG);
  CHECK(f->table.pfnFlush(f->device) == E_INVALIDARG);
  f->callbacksValid = true;
}
static void closeAdapter() {
  CHECK(f->adapterFuncs.pfnCloseAdapter(f->adapter) == S_OK);
  f->adapterValid = false;
}

static void ownedServiceStartup() {
  // A completion worker may reach the bridge before the first pump starts,
  // or just after it returns. Both requests must wait for a legal caller.
  for (const bool firstPump : {true, false}) {
    dxvk::umd::RuntimeService service(true);
    if (!firstPump) service.run([] { });
    const DWORD caller = GetCurrentThreadId();
    std::mutex mutex;
    std::condition_variable changed;
    bool started = false, done = false;
    HRESULT result = E_FAIL;
    std::thread worker([&] {
      {
        std::lock_guard<std::mutex> lock(mutex);
        started = true;
        changed.notify_all();
      }
      result = service.invoke([&] {
        CHECK(GetCurrentThreadId() == caller);
        return S_OK;
      });
      std::lock_guard<std::mutex> lock(mutex);
      done = true;
      changed.notify_all();
    });
    {
      std::unique_lock<std::mutex> lock(mutex);
      changed.wait(lock, [&] { return started; });
    }
    service.run([&] {
      std::unique_lock<std::mutex> lock(mutex);
      changed.wait(lock, [&] { return done; });
    });
    worker.join();
    CHECK(result == S_OK);
    service.close();
    CHECK(service.invoke([] { CHECK(false); return S_OK; }) == DXGI_ERROR_DEVICE_REMOVED);
  }
  // Non-owning synchronous probes still fail outside an active DDI pump.
  dxvk::umd::RuntimeService synchronous;
  CHECK(synchronous.invoke([] { CHECK(false); return S_OK; }) == DXGI_ERROR_UNSUPPORTED);
}

static D3DDDIARG_CREATERESOURCE resourceArgs(HANDLE cookie, D3DDDI_SURFACEINFO* info,
                                           UINT count, bool target = false) {
  D3DDDIARG_CREATERESOURCE args = {};
  args.hResource = cookie; args.pSurfList = info; args.SurfCount = count;
  args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_A8R8G8B8);
  args.Pool = target ? D3DDDIPOOL_LOCALVIDMEM : D3DDDIPOOL_SYSTEMMEM;
  args.Flags.RenderTarget = target;
  // Poison reserved fields, which cannot control this nontexture/nonprimary.
  args.MipLevels = args.Fvf = args.VidPnSourceId = UINT_MAX;
  args.RefreshRate.Numerator = args.RefreshRate.Denominator = UINT_MAX;
  args.Rotation = static_cast<D3DDDI_ROTATION>(UINT_MAX);
  if (!target) { args.MultisampleType = static_cast<D3DDDIMULTISAMPLE_TYPE>(UINT_MAX); args.MultisampleQuality = UINT_MAX; }
  return args;
}

static void resourceContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnCreateResource && f->table.pfnDestroyResource && f->table.pfnSetRenderTarget
      && f->table.pfnClear && f->table.pfnBlt && f->table.pfnLock && f->table.pfnUnlock);
    char targetCookie, systemCookie;
    D3DDDI_SURFACEINFO info[2] = {{8,4,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}, {4,3,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}};
    auto args = resourceArgs(&targetCookie, info, 2, true);
    auto before = snapshot(args);
    f->failSurfaceAttempt = 2;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_OUTOFMEMORY);
    CHECK(snapshot(args) == before && f->surfaceCreates == 1 && f->surfaceCloses == 1);
    f->failSurfaceAttempt = 0;
    f->surfaceResult = S_FALSE;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_FAIL && snapshot(args) == before);
    f->surfaceResult = S_OK; f->nullSurface = true;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_FAIL && snapshot(args) == before);
    f->nullSurface = false;
    f->queryHook = [&] {
      CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyResource(f->device, nullptr) == D3DERR_WASSTILLDRAWING);
      info[0].Width = 999; args.Flags.Texture = 1;
      args.hResource = reinterpret_cast<HANDLE>(UINT_PTR(0xdead));
    };
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE target = args.hResource;
    CHECK(target != &targetCookie && target != f->device);
    CHECK(f->descriptions.size() == 3 && f->descriptions[1].width == 8 && f->descriptions[2].width == 4);
    info[0].Width = 8;
    args = resourceArgs(&targetCookie, info, 2, true);
    before = snapshot(args);
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG && snapshot(args) == before);
    for (const UINT invalid : {UINT(2),UINT(0x800),UINT(0x8000),UINT(0x10000),UINT(0x80000)}) {
      args.Flags.Value = invalid;
      before = snapshot(args);
      CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG && snapshot(args) == before);
    }
    D3DDDIARG_SETRENDERTARGET bind = {0,target,2};
    CHECK(f->table.pfnSetRenderTarget(f->device, &bind) == E_INVALIDARG);
    bind.SubResourceIndex = 0;
    CHECK(f->table.pfnSetRenderTarget(f->device, &bind) == S_OK);
    D3DDDIARG_CLEAR fill = {}; fill.Flags = D3DCLEAR_TARGET | 8; fill.FillColor = 0xff123456;
    CHECK(f->table.pfnClear(f->device, &fill, 0, reinterpret_cast<const RECT*>(UINT_PTR(1))) == S_OK);
    CHECK(f->lastComputeRects && f->clearRects.empty());
    fill.Flags = D3DCLEAR_TARGET;
    CHECK(f->table.pfnClear(f->device, &fill, 0, reinterpret_cast<const RECT*>(UINT_PTR(1))) == S_OK);
    CHECK(!f->lastComputeRects && f->clearRects.empty());
    RECT area = {1,1,6,3}; const RECT original = area;
    f->queryHook = [&] { area = {-100,-100,900,900}; fill.FillColor = 0; };
    CHECK(f->table.pfnClear(f->device, &fill, 1, &area) == S_OK);
    CHECK(!f->lastComputeRects && f->clearRects.size() == 1
      && !std::memcmp(&f->clearRects[0], &original, sizeof(RECT)));
    fill.FillColor = 0xff123456;
    CHECK(f->table.pfnClear(f->device, &fill, 1, &area) == E_INVALIDARG);
    fill.Flags |= 8;
    CHECK(f->table.pfnClear(f->device, &fill, 1, &area) == S_OK && f->lastComputeRects);
    fill.Flags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER;
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_INVALIDARG);

    args = resourceArgs(&systemCookie, info, 1);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE system = args.hResource;
    bind.hRenderTarget = system;
    CHECK(f->table.pfnSetRenderTarget(f->device, &bind) == E_INVALIDARG);
    D3DDDIARG_BLT copy = {}; copy.hSrcResource = target; copy.hDstResource = system;
    copy.SrcRect = copy.DstRect = {0,0,8,4};
    CHECK(f->table.pfnBlt(f->device, &copy) == S_OK && f->copies == 1);
    copy.SrcSubResourceIndex = 2;
    CHECK(f->table.pfnBlt(f->device, &copy) == E_INVALIDARG); copy.SrcSubResourceIndex = 0;
    copy.DstRect.right = 9;
    CHECK(f->table.pfnBlt(f->device, &copy) == E_INVALIDARG); copy.DstRect.right = 8;
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = system;
    mapping.pSurfData = reinterpret_cast<void*>(UINT_PTR(0x1234)); mapping.Pitch = mapping.SlicePitch = UINT_MAX;
    for (const UINT invalid : {UINT(3),UINT(4),UINT(8),UINT(0x10),UINT(0x40),UINT(0x80),UINT(0x100),UINT(0x400)}) {
      mapping.Flags.Value = invalid;
      const auto prior = snapshot(mapping);
      CHECK(f->table.pfnLock(f->device, &mapping) == E_INVALIDARG && snapshot(mapping) == prior);
    }
    mapping.Flags.Value = 1;
    f->badMapping = true;
    const auto prior = snapshot(mapping);
    CHECK(f->table.pfnLock(f->device, &mapping) == E_FAIL && snapshot(mapping) == prior);
    f->badMapping = false;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && mapping.pSurfData && mapping.Pitch == 32 && !mapping.SlicePitch);
    UINT color; std::memcpy(&color, mapping.pSurfData, sizeof(color)); CHECK(color == 0xff123456);
    CHECK(f->table.pfnLock(f->device, &mapping) == E_INVALIDARG);
    CHECK(f->table.pfnBlt(f->device, &copy) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyResource(f->device, system) == E_INVALIDARG);
    D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = system;
    unmap.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_INVALIDARG); unmap.Flags.Value = 0;
    f->surfaceUnlockResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_OUTOFMEMORY);
    f->surfaceUnlockResult = S_OK;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_INVALIDARG);
    f->flushResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDestroyResource(f->device, target) == E_OUTOFMEMORY);
    f->flushResult = S_OK;
    CHECK(f->table.pfnDestroyResource(f->device, target) == S_OK);
    CHECK(f->table.pfnDestroyResource(f->device, target) == E_INVALIDARG);
    bind = {0,target,0};
    CHECK(f->table.pfnSetRenderTarget(f->device, &bind) == E_INVALIDARG);
    bind.hRenderTarget = nullptr;
    CHECK(f->table.pfnSetRenderTarget(f->device, &bind) == S_OK);
    CHECK(f->table.pfnDestroyResource(f->device, system) == S_OK);
    CHECK(f->surfaceCreates == f->surfaceCloses && f->surfaceLocks == f->surfaceUnlocks);
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie;
    std::array<uint8_t, 48> backing = {};
    D3DDDI_SURFACEINFO info = {2,2,UINT_MAX,backing.data(),24,UINT_MAX};
    auto args = resourceArgs(&cookie, &info, 1);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE resource = args.hResource;
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = resource;
    CHECK(f->table.pfnLock(f->device, &mapping) == E_INVALIDARG);
    mapping.Flags.NotifyOnly = mapping.Flags.AreaValid = 1; mapping.Area = {1,1,2,2};
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
    CHECK(mapping.pSurfData == backing.data() + 28 && mapping.Pitch == 24);
    // Device destruction discards a still-held CPU view without reading the
    // runtime's external storage, then releases all private resources first.
    closeDevice();
    CHECK(f->teardownDiscard && f->surfaceCreates == f->surfaceCloses && f->surfaceLocks == f->surfaceUnlocks);
    CHECK(f->table.pfnDestroyResource(f->device, resource) == E_INVALIDARG);
    closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie;
    D3DDDI_SURFACEINFO info = {2,2,1,nullptr,0,0};
    auto args = resourceArgs(&cookie, &info, 1);
    const auto before = snapshot(args);
    f->surfaceHook = [] { ++f->generation; };
    CHECK(f->table.pfnCreateResource(f->device, &args) == D3DERR_DEVICELOST);
    CHECK(snapshot(args) == before && f->surfaceCreates == f->surfaceCloses);
    CHECK(f->table.pfnCreateResource(f->device, &args) == D3DERR_DEVICELOST);
    closeDevice(); closeAdapter();
  }
}

int main() {
  ownedServiceStartup();
  resourceContracts();
  {
    Fixture fixture; initialize(fixture);
    // A missing mandatory callback rejects before backend construction.
    const auto original = f->input;
    for (unsigned field = 0; field < 8; field++) {
      f->input = original;
      if (field == 0) f->input.pfnAllocateCb = nullptr;
      if (field == 1) f->input.pfnDeallocateCb = nullptr;
      if (field == 2) f->input.pfnLockCb = nullptr;
      if (field == 3) f->input.pfnUnlockCb = nullptr;
      if (field == 4) f->input.pfnCreateContextCb = nullptr;
      if (field == 5) f->input.pfnDestroyContextCb = nullptr;
      if (field == 6) f->input.pfnEscapeCb = nullptr;
      if (field == 7) f->input.pfnRenderCb = nullptr;
      unchangedCreate(E_INVALIDARG);
    }
    CHECK(f->backends == 0); f->input = original;
    for (const HRESULT hr : {S_FALSE, E_FAIL, E_OUTOFMEMORY, DXGI_ERROR_UNSUPPORTED,
        DXGI_ERROR_DEVICE_REMOVED, DXGI_ERROR_DEVICE_RESET, DXGI_ERROR_WAS_STILL_DRAWING}) {
      f->backendResult = hr;
      unchangedCreate(hr == S_FALSE ? E_FAIL : hr == DXGI_ERROR_UNSUPPORTED ? D3DERR_NOTAVAILABLE
        : hr == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING
        : hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ? D3DERR_DEVICELOST : hr);
    }
    f->backendResult = S_OK; f->nullBackend = true; unchangedCreate(E_FAIL); f->nullBackend = false;
    f->throwAllocation = true; unchangedCreate(E_OUTOFMEMORY); f->throwAllocation = false;
    f->throwOther = true; unchangedCreate(E_FAIL); f->throwOther = false;
    f->contextFailure = true; unchangedCreate(E_OUTOFMEMORY); f->contextFailure = false;
    CHECK(f->contexts == 1 && f->contextCloses == 0);
    f->allocationFailure = true; unchangedCreate(E_OUTOFMEMORY); f->allocationFailure = false;
    CHECK(f->contexts == 2 && f->contextCloses == 1 && !f->allocated);
    closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnFlush(f->device) == S_OK);
    f->create.hDevice = &f->deviceCookie; unchangedCreate(E_INVALIDARG);
    f->create.hDevice = f->device;
    f->queryHook = [] {
      CHECK(f->table.pfnFlush(f->device) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
      std::thread concurrent([] {
        CHECK(f->table.pfnFlush(f->device) == D3DERR_WASSTILLDRAWING);
        CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
      });
      concurrent.join();
    };
    CHECK(f->table.pfnFlush(f->device) == S_OK);
    f->destroyHook = [] {
      CHECK(f->table.pfnDestroyDevice(f->device) == E_INVALIDARG);
      CHECK(f->table.pfnFlush(f->device) == E_INVALIDARG);
      f->create.hDevice = &f->deviceCookie; unchangedCreate(E_INVALIDARG);
    };
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture);
    f->contextHook = [] { unchangedCreate(E_INVALIDARG); };
    createDevice(); closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture);
    const auto originalTable = snapshot(f->table);
    f->queryHook = [] {
      f->input = {};
      f->create.hDevice = reinterpret_cast<HANDLE>(UINT_PTR(0xdead));
      f->create.pCallbacks = nullptr; f->create.pDeviceFuncs = &f->alternateTable;
    };
    CHECK(f->adapterFuncs.pfnCreateDevice(f->adapter, &f->create) == S_OK);
    f->device = f->create.hDevice;
    CHECK(snapshot(f->table) != originalTable && f->table.pfnDestroyDevice && f->table.pfnFlush);
    CHECK(snapshot(f->alternateTable) == snapshot(D3DDDI_DEVICEFUNCS{}));
    CHECK(f->table.pfnFlush(f->device) == S_OK); closeDevice(); closeAdapter();
  }
  for (const bool close : {false, true}) {
    Fixture fixture; initialize(fixture);
    f->backendHook = [close] {
      // Schedule this on the post-construction identity callback.
      f->queryHook = [close] { if (close) closeAdapter(); else ++f->generation; };
    };
    unchangedCreate(D3DERR_DEVICELOST);
    CHECK(!f->allocated && !f->mapped && f->backendCloses == 1 && f->contextCloses == 1);
    const std::vector<char> order{'U','A','C'}; CHECK(f->cleanup == order);
    if (!close) closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    closeAdapter();
    const auto queries = f->queries;
    CHECK(f->table.pfnFlush(f->device) == D3DERR_DEVICELOST);
    CHECK(f->queries == queries); closeDevice();
  }
  {
    Fixture fixture; initialize(fixture);
    f->contextHook = [] { closeAdapter(); };
    unchangedCreate(D3DERR_DEVICELOST);
    CHECK(f->contexts == 1 && f->contextCloses == 1 && !f->allocated);
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    ++f->generation;
    CHECK(f->table.pfnFlush(f->device) == D3DERR_DEVICELOST);
    const auto queries = f->queries;
    --f->generation; CHECK(f->table.pfnFlush(f->device) == D3DERR_DEVICELOST);
    CHECK(f->queries == queries); closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    f->flushResult = S_FALSE; CHECK(f->table.pfnFlush(f->device) == E_FAIL);
    f->flushResult = E_OUTOFMEMORY; CHECK(f->table.pfnFlush(f->device) == E_OUTOFMEMORY);
    f->flushResult = S_OK; CHECK(f->table.pfnFlush(f->device) == S_OK);
    f->destroyResult = S_FALSE; closeDevice(E_FAIL); closeAdapter();
  }
  HANDLE stale = nullptr;
  for (unsigned i = 0; i < 32; i++) {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->device != stale);
    if (stale) { CHECK(f->table.pfnFlush(stale) == E_INVALIDARG); CHECK(f->table.pfnDestroyDevice(stale) == E_INVALIDARG); }
    stale = f->device; closeDevice(); closeAdapter();
  }
  std::printf("native D3D9 device PASS checks=%u; controlled backend, no GPU rendering or runtime admission\n", checks.load());
}
