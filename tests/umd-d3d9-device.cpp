#include "../src/umd/umd_d3d9_adapter.h"
#include "../src/umd/umd_d3d9_backend.h"
#include "../src/umd/umd_allocation.h"
#include "../src/umd/umd_runtime_service.h"
#include "../src/d3d9/d3d9_shader_code.h"
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
#include <limits>
#include <map>

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
  HRESULT surfaceLockResult = S_OK;
  std::vector<DWORD> surfaceLockFlags;
  bool nullSurface = false, badMapping = false;
  bool lastComputeRects = false, teardownDiscard = false;
  HRESULT depthResult = S_OK, clearResult = S_OK;
  unsigned depthSets = 0;
  DWORD clearFlags = 0, clearStencil = 0;
  float clearDepth = 0.0f;
  std::vector<dxvk::umd::D3D9SurfaceDesc> descriptions;
  std::vector<RECT> clearRects;
  unsigned declarationCreates = 0, declarationCloses = 0, declarationSets = 0, draws = 0;
  HRESULT declarationResult = S_OK, stateResult = S_OK, drawResult = S_OK;
  bool nullDeclaration = false;
  std::function<void()> declarationHook;
  std::vector<D3DVERTEXELEMENT9> declarationElements;
  std::vector<uint8_t> drawVertices;
  size_t expectedDrawBytes = 0;
  UINT drawStride = 0, drawCount = 0;
  D3DPRIMITIVETYPE drawType = D3DPT_POINTLIST;
  D3DRENDERSTATETYPE renderState = D3DRS_ZENABLE;
  DWORD renderValue = 0;
  D3DVIEWPORT9 viewport = {0,0,0,0,0.0f,1.0f};
  RECT scissor = {};
  bool scene = false, software = false;
  HRESULT fixedResult = S_OK, lightResult = S_OK, lightEnableResult = S_OK;
  bool throwLight = false, multiplyTransform = false;
  unsigned transformSets = 0, materialSets = 0, lightSets = 0, lightEnableSets = 0;
  D3DTRANSFORMSTATETYPE transformState = D3DTS_WORLD;
  D3DMATRIX transform = {};
  D3DMATERIAL9 material = {};
  unsigned clipPlaneSets = 0;
  UINT clipPlaneIndex = 0;
  std::array<std::array<float,4>,6> clipPlanes = {};
  UINT lightSlot = 0;
  std::vector<D3DLIGHT9> lights;
  std::vector<bool> lightsEnabled;
  unsigned shaderCreates = 0, shaderCloses = 0, shaderSets = 0, constantSets = 0;
  HRESULT shaderResult = S_OK, constantResult = S_OK;
  bool nullShader = false;
  std::function<void()> shaderHook;
  dxvk::umd::D3D9ShaderStage shaderStage = dxvk::umd::D3D9ShaderStage::Vertex;
  std::vector<DWORD> shaderCode;
  std::vector<uint8_t> constantBytes;
  UINT constantFirst = 0, constantCount = 0;
  char constantType = 0;
  unsigned queryCreates = 0, queryCloses = 0, queryIssues = 0, queryReads = 0;
  HRESULT queryCreateResult = S_OK, queryIssueResult = S_OK, queryDataResult = S_OK;
  bool nullQuery = false, queryThrowAllocation = false, queryThrowOther = false;
  bool querySingleByteEvent = false;
  D3DQUERYTYPE queryType = D3DQUERYTYPE_EVENT;
  DWORD queryIssueFlags = 0;
  UINT queryDataBytes = 0;
  std::array<uint8_t, sizeof(D3DDEVINFO_VCACHE)> queryBytes = {};
  std::function<void()> queryCreateHook, queryIssueHook, queryDataHook;
  unsigned textureCreates = 0, textureCloses = 0, textureSets = 0;
  bool nullTexture = false;
  HRESULT textureResult = S_OK, copyResult = S_OK;
  D3DTEXTURESTAGESTATETYPE textureState = D3DTSS_COLOROP;
  D3DSAMPLERSTATETYPE samplerState = D3DSAMP_ADDRESSU;
  UINT textureStage = 0;
  DWORD textureValue = 0;
  bool lastSampler = false;
  std::vector<RECT> copySources, copyDestinations;
  std::vector<UINT> copyWidths;
  std::vector<std::vector<uint8_t>> uploads;
  unsigned bufferCreates = 0, bufferCloses = 0, bufferLocks = 0, bufferUnlocks = 0;
  HRESULT bufferResult = S_OK, bufferUnlockResult = S_OK;
  HRESULT bufferCopyResult = S_OK;
  unsigned bufferCopies = 0;
  UINT bufferDestinationOffset = 0, bufferSourceOffset = 0, bufferCopyBytes = 0;
  std::vector<uint8_t> bufferCopiedData;
  bool nullBuffer = false, badBufferMapping = false;
  std::function<void()> bufferHook;
  std::vector<dxvk::umd::D3D9BufferDesc> bufferDescriptions;
  UINT bufferOffset = 0, bufferBytes = 0, drawStart = 0, drawMinimum = 0, drawVertexCount = 0;
  INT drawBase = 0;
  DWORD bufferFlags = 0;
  struct PresentAllocation {
    dxvk::umd::AllocationInfo info;
    D3DKMT_HANDLE handle = 0;
    bool mapped = false;
    std::vector<uint8_t> guarded;
  };
  std::map<HANDLE, PresentAllocation> presentAllocations;
  char presentContextCookie = 0;
  bool presentContextLive = false, nullPresentAllocation = false;
  bool nullPresentResource = false, nullPresentContext = false, badPresentMapping = false;
  bool shortReadback = false;
  D3DKMT_HANDLE nextPresentAllocation = 71;
  unsigned presentAllocationAttempts = 0, presentAllocationCreates = 0;
  unsigned presentReleaseAttempts = 0, presentReleases = 0;
  unsigned presentLockAttempts = 0, presentLocks = 0, presentUnlocks = 0;
  unsigned presentContextAttempts = 0, presentContexts = 0, presentContextCloses = 0;
  unsigned surfaceReads = 0, presents = 0;
  HRESULT presentAllocateResult = S_OK, presentReleaseResult = S_OK;
  HRESULT presentLockResult = S_OK, presentUnlockResult = S_OK;
  HRESULT presentContextResult = S_OK, presentResult = S_OK, readbackResult = S_OK;
  std::function<void()> presentAllocateHook, presentReleaseHook, presentLockHook;
  std::function<void()> presentUnlockHook, presentContextHook, presentHook, readbackHook;
  std::vector<uint8_t>* rendererPixels = nullptr;
  std::vector<uint8_t> expectedPresentPixels, presentedPixels;
  D3DDDICB_PRESENT lastPresent = {};
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
  if (!args->pPrivateDriverData && !args->PrivateDriverDataSize) {
    ++f->presentContextAttempts;
    CHECK(!f->presentContextLive);
    if (FAILED(f->presentContextResult)) return f->presentContextResult;
    if (!f->nullPresentContext) {
      args->hContext = &f->presentContextCookie;
      f->presentContextLive = true; ++f->presentContexts;
    }
    if (f->presentContextHook) { auto hook = std::move(f->presentContextHook); hook(); }
    return f->presentContextResult;
  }
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
  if (args && args->hContext == &f->presentContextCookie) {
    CHECK(device == &f->deviceCookie && f->presentContextLive);
    f->presentContextLive = false; ++f->presentContextCloses;
    f->cleanup.push_back('P');
    return S_OK;
  }
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
  if (args && args->hResource) {
    CHECK(device == &f->deviceCookie && args->NumAllocations == 1 && args->pAllocationInfo);
    CHECK(!args->PrivateDriverDataSize && !args->pPrivateDriverData && !args->hKMResource);
    CHECK(args->pAllocationInfo->PrivateDriverDataSize == sizeof(dxvk::umd::AllocationInfo));
    dxvk::umd::AllocationInfo info;
    std::memcpy(&info, args->pAllocationInfo->pPrivateDriverData, sizeof(info));
    CHECK(info.magic == 0x504d5644 && !info.version && info.headerSize == 80 && !info.reserved);
    CHECK(info.flags == 2 && !info.contextId && !info.resetGeneration && !info.requestedIova);
    CHECK((info.format == 1 || info.format == 2) && info.width && info.height);
    CHECK(info.pitch == info.width * 4 && info.size == uint64_t(info.pitch) * info.height);
    ++f->presentAllocationAttempts;
    if (FAILED(f->presentAllocateResult)) return f->presentAllocateResult;
    CHECK(!f->presentAllocations.count(args->hResource));
    Fixture::PresentAllocation allocation;
    allocation.info = info; allocation.handle = f->nextPresentAllocation++;
    allocation.guarded.assign(size_t(info.size) + 32, 0xcd);
    args->pAllocationInfo->hAllocation = f->nullPresentAllocation ? 0 : allocation.handle;
    args->hKMResource = f->nullPresentResource ? 0 : allocation.handle + 1000;
    f->presentAllocations.emplace(args->hResource, std::move(allocation));
    ++f->presentAllocationCreates;
    if (f->presentAllocateHook) { auto hook = std::move(f->presentAllocateHook); hook(); }
    return f->presentAllocateResult;
  }
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
  if (args && args->hResource) {
    CHECK(device == &f->deviceCookie && !args->NumAllocations && !args->HandleList);
    auto entry = f->presentAllocations.find(args->hResource);
    CHECK(entry != f->presentAllocations.end() && !entry->second.mapped);
    ++f->presentReleaseAttempts;
    if (f->presentReleaseHook) { auto hook = std::move(f->presentReleaseHook); hook(); }
    if (SUCCEEDED(f->presentReleaseResult)) {
      ++f->presentReleases; f->presentAllocations.erase(entry); f->cleanup.push_back('R');
    }
    return f->presentReleaseResult;
  }
  CHECK(device == &f->deviceCookie && args && !args->hResource && args->NumAllocations == 1);
  CHECK(args->HandleList && *args->HandleList == 31 && f->allocated && !f->mapped);
  ++f->deallocations; f->allocated = false; f->cleanup.push_back('A'); return S_OK;
}
static HRESULT APIENTRY lock(HANDLE device, D3DDDICB_LOCK* args) {
  f->runtime();
  if (args && args->hAllocation != 31) {
    CHECK(device == &f->deviceCookie && args->Flags.LockEntire && args->Flags.WriteOnly);
    CHECK(!args->Flags.ReadOnly && !args->Flags.Discard && !args->Flags.IgnoreSync);
    Fixture::PresentAllocation* allocation = nullptr;
    for (auto& entry : f->presentAllocations)
      if (entry.second.handle == args->hAllocation) allocation = &entry.second;
    CHECK(allocation && !allocation->mapped);
    ++f->presentLockAttempts;
    if (FAILED(f->presentLockResult)) return f->presentLockResult;
    allocation->mapped = true; ++f->presentLocks;
    args->pData = f->badPresentMapping ? nullptr : allocation->guarded.data() + 16;
    if (f->presentLockHook) { auto hook = std::move(f->presentLockHook); hook(); }
    return f->presentLockResult;
  }
  CHECK(device == &f->deviceCookie && args && args->hAllocation == 31 && f->allocated && !f->mapped);
  ++f->locks; f->mapped = true; args->pData = f->pixels.data(); return S_OK;
}
static HRESULT APIENTRY unlock(HANDLE device, const D3DDDICB_UNLOCK* args) {
  f->runtime();
  if (args && args->NumAllocations == 1 && args->phAllocations && *args->phAllocations != 31) {
    CHECK(device == &f->deviceCookie);
    Fixture::PresentAllocation* allocation = nullptr;
    for (auto& entry : f->presentAllocations)
      if (entry.second.handle == *args->phAllocations) allocation = &entry.second;
    CHECK(allocation && allocation->mapped);
    for (unsigned i = 0; i < 16; ++i) {
      CHECK(allocation->guarded[i] == 0xcd);
      CHECK(allocation->guarded[allocation->guarded.size() - 1 - i] == 0xcd);
    }
    allocation->mapped = false; ++f->presentUnlocks;
    if (f->presentUnlockHook) { auto hook = std::move(f->presentUnlockHook); hook(); }
    return f->presentUnlockResult;
  }
  CHECK(device == &f->deviceCookie && args && args->NumAllocations == 1 && *args->phAllocations == 31);
  CHECK(f->mapped); ++f->unlocks; f->mapped = false; f->cleanup.push_back('U'); return S_OK;
}
static HRESULT APIENTRY render(HANDLE, D3DDDICB_RENDER*) { CHECK(false); return E_FAIL; }

static HRESULT APIENTRY presentCallback(HANDLE device, D3DDDICB_PRESENT* args) {
  f->runtime();
  CHECK(device == &f->deviceCookie && args && f->presentContextLive);
  CHECK(args->hContext == &f->presentContextCookie && !args->hDstAllocation);
  auto expected = D3DDDICB_PRESENT{};
  expected.hSrcAllocation = args->hSrcAllocation; expected.hContext = &f->presentContextCookie;
  CHECK(snapshot(*args) == snapshot(expected));
  Fixture::PresentAllocation* allocation = nullptr;
  for (auto& entry : f->presentAllocations)
    if (entry.second.handle == args->hSrcAllocation) allocation = &entry.second;
  CHECK(allocation && !allocation->mapped && args->hSrcAllocation != 31);
  f->presentedPixels.assign(allocation->guarded.begin() + 16, allocation->guarded.end() - 16);
  CHECK(f->presentedPixels == f->expectedPresentPixels);
  f->lastPresent = *args; ++f->presents;
  if (f->presentHook) { auto hook = std::move(f->presentHook); hook(); }
  return f->presentResult;
}

// Substitute only the renderer for this CPU fixture. The actual typed adapter,
// device lifetime, callback pump and RuntimeGpu allocation/context code run.
struct dxvk::umd::D3D9Backend::State {
  RuntimeBackend bridge;
  mwd_allocation allocation = {};
  D3D9SurfaceResource* target = nullptr;
  D3D9SurfaceResource* depth = nullptr;
  D3D9VertexDeclaration* declaration = nullptr;
  std::array<D3D9Shader*, 2> shaders = {};
  std::array<D3D9TextureResource*, 20> textures = {};
  std::array<D3D9BufferResource*, 16> streams = {};
  D3D9BufferResource* indices = nullptr;
};

struct dxvk::umd::D3D9BufferResource::State {
  D3D9BufferDesc desc;
  std::vector<uint8_t> bytes;
  bool locked = false;
  bool readOnly = false;
  UINT offset = 0, length = 0;
  unsigned bindings = 0;
};
dxvk::umd::D3D9BufferResource::D3D9BufferResource() : m_state(std::make_unique<State>()) {
  CHECK(GetCurrentThreadId() != f->caller); ++f->bufferCreates;
}
dxvk::umd::D3D9BufferResource::~D3D9BufferResource() {
  CHECK(GetCurrentThreadId() != f->caller && !m_state->locked && !m_state->bindings);
  ++f->bufferCloses;
}
HRESULT dxvk::umd::D3D9Backend::createBuffer(const D3D9BufferDesc& desc,
    std::unique_ptr<D3D9BufferResource>& output, const void* initialData) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->bufferResult != S_OK) return f->bufferResult;
  if (f->nullBuffer) return S_OK;
  auto buffer = std::make_unique<D3D9BufferResource>();
  buffer->m_state->desc = desc; buffer->m_state->bytes.resize(desc.bytes,0x6d);
  CHECK(bool(initialData) == bool(desc.systemData) && (!desc.systemData || desc.systemMemory));
  if (initialData) std::memcpy(buffer->m_state->bytes.data(), initialData, desc.bytes);
  f->bufferDescriptions.push_back(desc);
  if (f->bufferHook) { auto hook = std::move(f->bufferHook); hook(); }
  output = std::move(buffer); return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::lockBuffer(D3D9BufferResource& buffer, UINT offset, UINT bytes,
    DWORD flags, void*& data) {
  CHECK(GetCurrentThreadId() != f->caller && !buffer.m_state->locked);
  CHECK(bytes && uint64_t(offset) + bytes <= buffer.m_state->bytes.size());
  ++f->bufferLocks; f->bufferOffset = offset; f->bufferBytes = bytes; f->bufferFlags = flags;
  buffer.m_state->locked = true;
  buffer.m_state->offset = offset; buffer.m_state->length = bytes;
  buffer.m_state->readOnly = (flags & D3DLOCK_READONLY) != 0;
  data = f->badBufferMapping ? nullptr : buffer.m_state->bytes.data() + offset;
  if (data && buffer.m_state->desc.systemData) {
    data = static_cast<uint8_t*>(buffer.m_state->desc.systemData) + offset;
    if (buffer.m_state->readOnly) std::memcpy(data, buffer.m_state->bytes.data() + offset, bytes);
  }
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::unlockBuffer(D3D9BufferResource& buffer, const void* upload) {
  CHECK(GetCurrentThreadId() != f->caller && buffer.m_state->locked);
  ++f->bufferUnlocks;
  if (f->bufferUnlockResult != S_OK) return f->bufferUnlockResult;
  if (upload) {
    CHECK(buffer.m_state->desc.systemData && !buffer.m_state->readOnly);
    std::memcpy(buffer.m_state->bytes.data() + buffer.m_state->offset, upload, buffer.m_state->length);
  }
  if (buffer.m_state->desc.systemData && !upload && !buffer.m_state->readOnly)
    f->teardownDiscard = true;
  buffer.m_state->locked = false; return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::copyBuffer(D3D9BufferResource& destination, UINT destinationOffset,
    D3D9BufferResource& source, UINT sourceOffset, UINT bytes, const void* upload) {
  CHECK(GetCurrentThreadId() != f->caller && !destination.m_state->locked && !source.m_state->locked);
  CHECK(bytes && uint64_t(sourceOffset) + bytes <= source.m_state->bytes.size()
    && uint64_t(destinationOffset) + bytes <= destination.m_state->bytes.size());
  CHECK(!source.m_state->desc.systemData || upload);
  ++f->bufferCopies; f->bufferDestinationOffset = destinationOffset;
  f->bufferSourceOffset = sourceOffset; f->bufferCopyBytes = bytes;
  if (f->bufferCopyResult != S_OK) return f->bufferCopyResult;
  const auto data = upload ? static_cast<const uint8_t*>(upload) : source.m_state->bytes.data() + sourceOffset;
  f->bufferCopiedData.assign(data, data + bytes);
  std::memcpy(destination.m_state->bytes.data() + destinationOffset, f->bufferCopiedData.data(), bytes);
  if (destination.m_state->desc.systemData)
    std::memcpy(static_cast<uint8_t*>(destination.m_state->desc.systemData) + destinationOffset,
      f->bufferCopiedData.data(), bytes);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setStreamSource(UINT stream, D3D9BufferResource* buffer,
    UINT offset, UINT stride) {
  CHECK(GetCurrentThreadId() != f->caller && stream < 16);
  CHECK(!buffer || (!buffer->m_state->desc.index && offset < buffer->m_state->desc.bytes && stride));
  if (f->stateResult != S_OK && f->stateResult != D3DERR_DEVICELOST) return f->stateResult;
  if (m_state->streams[stream]) --m_state->streams[stream]->m_state->bindings;
  m_state->streams[stream] = buffer;
  if (buffer) ++buffer->m_state->bindings;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setIndices(D3D9BufferResource* buffer) {
  CHECK(GetCurrentThreadId() != f->caller && (!buffer || buffer->m_state->desc.index));
  if (f->stateResult != S_OK && f->stateResult != D3DERR_DEVICELOST) return f->stateResult;
  if (m_state->indices) --m_state->indices->m_state->bindings;
  m_state->indices = buffer;
  if (buffer) ++buffer->m_state->bindings;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::drawPrimitiveBuffers(D3DPRIMITIVETYPE type, UINT start, UINT count) {
  CHECK(GetCurrentThreadId() != f->caller && m_state->declaration && m_state->target);
  ++f->draws; f->drawType = type; f->drawStart = start; f->drawCount = count;
  return f->drawResult;
}
HRESULT dxvk::umd::D3D9Backend::drawIndexedPrimitive(D3DPRIMITIVETYPE type, INT base, UINT minimum,
    UINT vertices, UINT start, UINT count) {
  CHECK(GetCurrentThreadId() != f->caller && m_state->declaration && m_state->target && m_state->indices);
  ++f->draws; f->drawType = type; f->drawBase = base; f->drawMinimum = minimum;
  f->drawVertexCount = vertices; f->drawStart = start; f->drawCount = count;
  return f->drawResult;
}

struct dxvk::umd::D3D9TextureResource::State {
  unsigned liveLevels = 0, bindings = 0;
};
dxvk::umd::D3D9TextureResource::D3D9TextureResource() : m_state(std::make_unique<State>()) {
  CHECK(GetCurrentThreadId() != f->caller); ++f->textureCreates;
}
dxvk::umd::D3D9TextureResource::~D3D9TextureResource() {
  CHECK(GetCurrentThreadId() != f->caller && !m_state->liveLevels && !m_state->bindings);
  ++f->textureCloses;
}

struct dxvk::umd::D3D9Shader::State {
  D3D9ShaderStage stage = D3D9ShaderStage::Vertex;
};
dxvk::umd::D3D9Shader::D3D9Shader() : m_state(std::make_unique<State>()) {
  CHECK(GetCurrentThreadId() != f->caller); ++f->shaderCreates;
}
dxvk::umd::D3D9Shader::~D3D9Shader() {
  CHECK(GetCurrentThreadId() != f->caller); ++f->shaderCloses;
}

struct dxvk::umd::D3D9QueryResource::State {
  D3DQUERYTYPE type = D3DQUERYTYPE_EVENT;
};
dxvk::umd::D3D9QueryResource::D3D9QueryResource() : m_state(std::make_unique<State>()) {
  CHECK(GetCurrentThreadId() != f->caller); ++f->queryCreates;
}
dxvk::umd::D3D9QueryResource::~D3D9QueryResource() {
  CHECK(GetCurrentThreadId() != f->caller); ++f->queryCloses;
}

struct dxvk::umd::D3D9VertexDeclaration::State { };
dxvk::umd::D3D9VertexDeclaration::D3D9VertexDeclaration() : m_state(std::make_unique<State>()) {
  CHECK(GetCurrentThreadId() != f->caller); ++f->declarationCreates;
}
dxvk::umd::D3D9VertexDeclaration::~D3D9VertexDeclaration() {
  CHECK(GetCurrentThreadId() != f->caller); ++f->declarationCloses;
}
struct dxvk::umd::D3D9SurfaceResource::State {
  D3D9SurfaceDesc desc;
  std::vector<uint8_t> bytes;
  RECT area = {};
  bool locked = false;
  unsigned depthBindings = 0;
  unsigned* textureLevels = nullptr;
};
dxvk::umd::D3D9SurfaceResource::D3D9SurfaceResource() : m_state(std::make_unique<State>()) { }
dxvk::umd::D3D9SurfaceResource::~D3D9SurfaceResource() {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(!m_state->locked && !m_state->depthBindings);
  if (m_state->textureLevels) --*m_state->textureLevels;
  ++f->surfaceCloses;
}
dxvk::umd::D3D9Backend::D3D9Backend() : m_state(std::make_unique<State>()) { }
dxvk::umd::D3D9Backend::~D3D9Backend() {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(f->surfaceCreates == f->surfaceCloses && !m_state->target && !m_state->depth);
  CHECK(f->declarationCreates == f->declarationCloses && !m_state->declaration);
  CHECK(f->shaderCreates == f->shaderCloses && !m_state->shaders[0] && !m_state->shaders[1]);
  CHECK(f->queryCreates == f->queryCloses);
  CHECK(f->textureCreates == f->textureCloses);
  for (const auto texture : m_state->textures) CHECK(!texture);
  CHECK(f->bufferCreates == f->bufferCloses && !m_state->indices);
  for (const auto stream : m_state->streams) CHECK(!stream);
  for (const auto enabled : f->lightsEnabled) CHECK(!enabled);
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

HRESULT dxvk::umd::D3D9Backend::createQuery(D3DQUERYTYPE type,
    std::unique_ptr<D3D9QueryResource>& output) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->queryThrowAllocation) throw std::bad_alloc();
  if (f->queryThrowOther) throw 1;
  if (f->queryCreateResult != S_OK) return f->queryCreateResult;
  if (f->nullQuery) return S_OK;
  auto query = std::make_unique<D3D9QueryResource>();
  query->m_state->type = type; f->queryType = type;
  if (f->queryCreateHook) { auto hook = std::move(f->queryCreateHook); hook(); }
  output = std::move(query);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::issueQuery(D3D9QueryResource&, DWORD flags) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->queryIssues; f->queryIssueFlags = flags;
  if (f->queryIssueHook) { auto hook = std::move(f->queryIssueHook); hook(); }
  return f->queryIssueResult;
}
HRESULT dxvk::umd::D3D9Backend::getQueryData(D3D9QueryResource& query, void* data, UINT bytes) {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK((data && bytes > 0 && bytes <= f->queryBytes.size()) || (!data && !bytes));
  ++f->queryReads; f->queryDataBytes = bytes;
  // Write private output even on pending/failure to catch premature publication.
  if (data) {
    if (query.m_state->type == D3DQUERYTYPE_EVENT && f->querySingleByteEvent)
      *static_cast<uint8_t*>(data) = 1;
    else std::memcpy(data, f->queryBytes.data(), bytes);
  }
  if (f->queryDataHook) { auto hook = std::move(f->queryDataHook); hook(); }
  return f->queryDataResult;
}

HRESULT dxvk::umd::D3D9Backend::createVertexDeclaration(const D3DVERTEXELEMENT9* elements,
    std::unique_ptr<D3D9VertexDeclaration>& output) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->declarationResult != S_OK) return f->declarationResult;
  if (f->nullDeclaration) return S_OK;
  auto declaration = std::make_unique<D3D9VertexDeclaration>();
  f->declarationElements.clear();
  for (auto p = elements; p->Stream != 0xff; ++p) f->declarationElements.push_back(*p);
  if (f->declarationHook) { auto hook = std::move(f->declarationHook); hook(); }
  output = std::move(declaration);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setVertexDeclaration(D3D9VertexDeclaration* declaration) {
  CHECK(GetCurrentThreadId() != f->caller); ++f->declarationSets;
  if (f->stateResult != S_OK) return f->stateResult;
  m_state->declaration = declaration;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::createShader(D3D9ShaderStage stage, const DWORD* code, UINT bytes,
    std::unique_ptr<D3D9Shader>& output) {
  CHECK(GetCurrentThreadId() != f->caller);
  f->shaderStage = stage;
  f->shaderCode.assign(code, code + bytes / sizeof(DWORD));
  if (!f->nullShader) {
    output = std::make_unique<D3D9Shader>();
    output->m_state->stage = stage;
  }
  if (f->shaderHook) { auto hook = std::move(f->shaderHook); hook(); }
  return f->shaderResult;
}
HRESULT dxvk::umd::D3D9Backend::setShader(D3D9ShaderStage stage, D3D9Shader* shader) {
  CHECK(GetCurrentThreadId() != f->caller && (!shader || shader->m_state->stage == stage));
  ++f->shaderSets;
  if (f->stateResult != S_OK) {
    if (f->stateResult == D3DERR_DEVICELOST && !shader) m_state->shaders[size_t(stage)] = nullptr;
    return f->stateResult;
  }
  m_state->shaders[size_t(stage)] = shader;
  return S_OK;
}
static HRESULT captureConstants(dxvk::umd::D3D9ShaderStage stage, UINT first, UINT count,
    const void* values, size_t bytes, char type) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->constantResult != S_OK) return f->constantResult;
  ++f->constantSets;
  f->shaderStage = stage; f->constantFirst = first; f->constantCount = count; f->constantType = type;
  const auto data = static_cast<const uint8_t*>(values);
  f->constantBytes.assign(data, data + bytes);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setShaderConstantF(D3D9ShaderStage stage, UINT first, UINT count, const float* values) {
  return captureConstants(stage, first, count, values, size_t(count) * 16, 'F');
}
HRESULT dxvk::umd::D3D9Backend::setShaderConstantI(D3D9ShaderStage stage, UINT first, UINT count, const INT* values) {
  return captureConstants(stage, first, count, values, size_t(count) * 16, 'I');
}
HRESULT dxvk::umd::D3D9Backend::setShaderConstantB(D3D9ShaderStage stage, UINT first, UINT count, const BOOL* values) {
  return captureConstants(stage, first, count, values, size_t(count) * 4, 'B');
}
HRESULT dxvk::umd::D3D9Backend::setTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX& matrix,
    bool multiply) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->transformSets;
  if (f->fixedResult != S_OK) return f->fixedResult;
  f->transformState = state; f->transform = matrix; f->multiplyTransform = multiply;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setMaterial(const D3DMATERIAL9& material) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->materialSets;
  if (f->fixedResult != S_OK) return f->fixedResult;
  f->material = material; return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setClipPlane(UINT index, const float* plane) {
  CHECK(GetCurrentThreadId() != f->caller && index < f->clipPlanes.size() && plane);
  ++f->clipPlaneSets;
  if (f->fixedResult != S_OK) return f->fixedResult;
  f->clipPlaneIndex = index;
  std::memcpy(f->clipPlanes[index].data(), plane, sizeof(f->clipPlanes[index]));
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setLight(UINT index, const D3DLIGHT9& light) {
  CHECK(GetCurrentThreadId() != f->caller && index <= f->lights.size() && index < 64);
  ++f->lightSets;
  if (f->throwLight) throw std::bad_alloc();
  if (f->lightResult != S_OK) return f->lightResult;
  if (index == f->lights.size()) {
    f->lights.emplace_back(); f->lightsEnabled.push_back(false);
  }
  f->lightSlot = index; f->lights[index] = light;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setLightEnabled(UINT index, bool enable) {
  CHECK(GetCurrentThreadId() != f->caller && index < f->lights.size());
  ++f->lightEnableSets;
  if (f->lightEnableResult != S_OK) return f->lightEnableResult;
  f->lightSlot = index; f->lightsEnabled[index] = enable;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setRenderState(D3DRENDERSTATETYPE state, DWORD value) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->stateResult != S_OK) return f->stateResult;
  f->renderState = state; f->renderValue = value;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setScene(bool capture) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->stateResult != S_OK) return f->stateResult;
  if (f->scene == capture) return D3DERR_INVALIDCALL;
  f->scene = capture; return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setSoftwareVertexProcessing(bool enable) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->stateResult != S_OK) return f->stateResult;
  f->software = enable; return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setViewport(UINT x, UINT y, UINT width, UINT height) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->stateResult != S_OK) return f->stateResult;
  f->viewport.X = x; f->viewport.Y = y; f->viewport.Width = width; f->viewport.Height = height;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setZRange(float minimum, float maximum) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->stateResult != S_OK) return f->stateResult;
  f->viewport.MinZ = minimum; f->viewport.MaxZ = maximum;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::setScissorRect(const RECT& area) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (f->stateResult != S_OK) return f->stateResult;
  f->scissor = area; return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::drawPrimitive(D3DPRIMITIVETYPE type, UINT count,
    const void* vertices, UINT stride) {
  CHECK(GetCurrentThreadId() != f->caller && m_state->target && m_state->declaration);
  ++f->draws; f->drawStride = stride; f->drawCount = count; f->drawType = type;
  const auto bytes = static_cast<const uint8_t*>(vertices);
  f->drawVertices.assign(bytes, bytes + f->expectedDrawBytes);
  return f->drawResult;
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
HRESULT dxvk::umd::D3D9Backend::setDepthStencil(D3D9SurfaceResource* depth) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->depthSets;
  if (f->depthResult != S_OK) return f->depthResult;
  if (depth) CHECK(depth->m_state->desc.depthStencil && !depth->m_state->locked);
  if (m_state->depth) --m_state->depth->m_state->depthBindings;
  m_state->depth = depth;
  if (depth) ++depth->m_state->depthBindings;
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::clear(DWORD flags, D3DCOLOR color, float depth, DWORD stencil,
    UINT count, const RECT* rects, bool computeRects) {
  CHECK(GetCurrentThreadId() != f->caller);
  if (flags & D3DCLEAR_TARGET) CHECK(m_state->target);
  if (flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) CHECK(m_state->depth);
  ++f->clears;
  f->clearFlags = flags; f->clearDepth = depth; f->clearStencil = stencil;
  f->lastComputeRects = computeRects;
  f->clearRects.clear();
  if (count) f->clearRects.assign(rects, rects + count);
  if (f->clearResult != S_OK) return f->clearResult;
  if ((!count && !computeRects) || !(flags & D3DCLEAR_TARGET)) return S_OK;
  auto& state = *m_state->target->m_state;
  for (size_t i = 0; i < state.bytes.size(); i += 4) std::memcpy(state.bytes.data() + i, &color, 4);
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::copySurface(D3D9SurfaceResource& destination, const RECT& destinationRect,
    D3D9SurfaceResource& source, const RECT& sourceRect, const D3D9SurfaceUpload* upload) {
  CHECK(GetCurrentThreadId() != f->caller);
  ++f->copies;
  auto& dst = *destination.m_state;
  auto& src = *source.m_state;
  f->copySources.push_back(sourceRect); f->copyDestinations.push_back(destinationRect);
  f->copyWidths.push_back(src.desc.width);
  f->uploads.emplace_back();
  if (upload) {
    const size_t size = size_t(sourceRect.bottom - sourceRect.top) * upload->pitch;
    const auto data = static_cast<const uint8_t*>(upload->data);
    f->uploads.back().assign(data, data + size);
  }
  if (f->copyResult != S_OK) return f->copyResult;
  if (dst.desc.systemMemory && src.desc.systemMemory) return flush();
  const size_t bytes = size_t(sourceRect.right - sourceRect.left) * 4;
  for (LONG row = 0; row < sourceRect.bottom - sourceRect.top; ++row) {
    auto from = src.bytes.data() + (size_t(row + sourceRect.top) * src.desc.width + sourceRect.left) * 4;
    if (src.desc.systemData) {
      const auto data = upload ? static_cast<const uint8_t*>(upload->data) + size_t(row) * upload->pitch
        : static_cast<const uint8_t*>(src.desc.systemData) + size_t(row + sourceRect.top)
          * src.desc.systemPitch + size_t(sourceRect.left) * 4;
      std::memcpy(from, data, bytes);
    }
    auto to = dst.bytes.data() + (size_t(row + destinationRect.top) * dst.desc.width + destinationRect.left) * 4;
    std::memcpy(to, from, bytes);
    if (dst.desc.systemData)
      std::memcpy(static_cast<uint8_t*>(dst.desc.systemData) + size_t(row + destinationRect.top)
        * dst.desc.systemPitch + size_t(destinationRect.left) * 4, from, bytes);
  }
  return S_OK;
}
HRESULT dxvk::umd::D3D9Backend::createTexture(const D3D9SurfaceDesc* levels, UINT count,
    std::unique_ptr<D3D9TextureResource>& output,
    std::vector<std::unique_ptr<D3D9SurfaceResource>>& surfaces) {
  CHECK(GetCurrentThreadId() != f->caller && count);
  if (f->nullTexture) return S_OK;
  auto texture = std::make_unique<D3D9TextureResource>();
  std::vector<std::unique_ptr<D3D9SurfaceResource>> views;
  for (UINT i = 0; i < count; ++i) {
    std::unique_ptr<D3D9SurfaceResource> level;
    const HRESULT hr = createSurface(levels[i], level);
    if (hr != S_OK) return hr;
    if (level) { level->m_state->textureLevels = &texture->m_state->liveLevels; ++texture->m_state->liveLevels; }
    views.push_back(std::move(level));
  }
  surfaces = std::move(views); output = std::move(texture);
  return f->textureResult;
}
HRESULT dxvk::umd::D3D9Backend::setTexture(UINT stage, D3D9TextureResource* texture) {
  CHECK(GetCurrentThreadId() != f->caller);
  const size_t slot = stage < 16 ? stage : 16 + stage - D3DVERTEXTEXTURESAMPLER0;
  CHECK(slot < m_state->textures.size());
  ++f->textureSets;
  if (f->stateResult != S_OK && !(f->stateResult == D3DERR_DEVICELOST && !texture)) return f->stateResult;
  if (m_state->textures[slot]) --m_state->textures[slot]->m_state->bindings;
  m_state->textures[slot] = texture;
  if (texture) ++texture->m_state->bindings;
  f->textureStage = stage;
  return f->stateResult;
}
HRESULT dxvk::umd::D3D9Backend::setTextureStageState(UINT stage, D3DTEXTURESTAGESTATETYPE state, DWORD value) {
  CHECK(GetCurrentThreadId() != f->caller);
  f->textureStage = stage; f->textureState = state; f->textureValue = value; f->lastSampler = false;
  return f->stateResult;
}
HRESULT dxvk::umd::D3D9Backend::setSamplerState(UINT stage, D3DSAMPLERSTATETYPE state, DWORD value) {
  CHECK(GetCurrentThreadId() != f->caller);
  f->textureStage = stage; f->samplerState = state; f->textureValue = value; f->lastSampler = true;
  return f->stateResult;
}
HRESULT dxvk::umd::D3D9Backend::lockSurface(D3D9SurfaceResource& resource, const RECT* area,
    DWORD flags, D3DLOCKED_RECT& output) {
  CHECK(GetCurrentThreadId() != f->caller);
  auto& state = *resource.m_state;
  CHECK(!state.locked);
  f->surfaceLockFlags.push_back(flags);
  if (f->surfaceLockResult != S_OK) return f->surfaceLockResult;
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

HRESULT dxvk::umd::D3D9Backend::readSurface(D3D9SurfaceResource& resource,
    std::vector<uint8_t>& pixels) {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(resource.m_state->desc.renderTarget && !resource.m_state->locked);
  ++f->surfaceReads;
  if (f->readbackHook) { auto hook = std::move(f->readbackHook); hook(); }
  if (f->readbackResult != S_OK) return f->readbackResult;
  pixels = resource.m_state->bytes;
  f->rendererPixels = &resource.m_state->bytes;
  if (f->shortReadback) pixels.pop_back();
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
  expectedTable.pfnPresent = f->table.pfnPresent;
  expectedTable.pfnCreateResource = f->table.pfnCreateResource;
  expectedTable.pfnDestroyResource = f->table.pfnDestroyResource;
  expectedTable.pfnSetRenderTarget = f->table.pfnSetRenderTarget;
  expectedTable.pfnSetDepthStencil = f->table.pfnSetDepthStencil;
  expectedTable.pfnClear = f->table.pfnClear;
  expectedTable.pfnBlt = f->table.pfnBlt;
  expectedTable.pfnBufBlt = f->table.pfnBufBlt;
  expectedTable.pfnLock = f->table.pfnLock;
  expectedTable.pfnUnlock = f->table.pfnUnlock;
  expectedTable.pfnSetTexture = f->table.pfnSetTexture;
  expectedTable.pfnSetTextureStageState = f->table.pfnSetTextureStageState;
  expectedTable.pfnTexBlt = f->table.pfnTexBlt;
  expectedTable.pfnCreateVertexShaderDecl = f->table.pfnCreateVertexShaderDecl;
  expectedTable.pfnSetVertexShaderDecl = f->table.pfnSetVertexShaderDecl;
  expectedTable.pfnDeleteVertexShaderDecl = f->table.pfnDeleteVertexShaderDecl;
  expectedTable.pfnCreateVertexShaderFunc = f->table.pfnCreateVertexShaderFunc;
  expectedTable.pfnSetVertexShaderFunc = f->table.pfnSetVertexShaderFunc;
  expectedTable.pfnDeleteVertexShaderFunc = f->table.pfnDeleteVertexShaderFunc;
  expectedTable.pfnCreatePixelShader = f->table.pfnCreatePixelShader;
  expectedTable.pfnSetPixelShader = f->table.pfnSetPixelShader;
  expectedTable.pfnDeletePixelShader = f->table.pfnDeletePixelShader;
  expectedTable.pfnSetVertexShaderConst = f->table.pfnSetVertexShaderConst;
  expectedTable.pfnSetPixelShaderConst = f->table.pfnSetPixelShaderConst;
  expectedTable.pfnSetVertexShaderConstI = f->table.pfnSetVertexShaderConstI;
  expectedTable.pfnSetPixelShaderConstI = f->table.pfnSetPixelShaderConstI;
  expectedTable.pfnSetVertexShaderConstB = f->table.pfnSetVertexShaderConstB;
  expectedTable.pfnSetPixelShaderConstB = f->table.pfnSetPixelShaderConstB;
  expectedTable.pfnSetRenderState = f->table.pfnSetRenderState;
  expectedTable.pfnSetTransform = f->table.pfnSetTransform;
  expectedTable.pfnMultiplyTransform = f->table.pfnMultiplyTransform;
  expectedTable.pfnSetMaterial = f->table.pfnSetMaterial;
  expectedTable.pfnSetClipPlane = f->table.pfnSetClipPlane;
  expectedTable.pfnCreateQuery = f->table.pfnCreateQuery;
  expectedTable.pfnIssueQuery = f->table.pfnIssueQuery;
  expectedTable.pfnGetQueryData = f->table.pfnGetQueryData;
  expectedTable.pfnDestroyQuery = f->table.pfnDestroyQuery;
  expectedTable.pfnCreateLight = f->table.pfnCreateLight;
  expectedTable.pfnSetLight = f->table.pfnSetLight;
  expectedTable.pfnDestroyLight = f->table.pfnDestroyLight;
  expectedTable.pfnSetViewport = f->table.pfnSetViewport;
  expectedTable.pfnSetZRange = f->table.pfnSetZRange;
  expectedTable.pfnSetScissorRect = f->table.pfnSetScissorRect;
  expectedTable.pfnSetStreamSourceUm = f->table.pfnSetStreamSourceUm;
  expectedTable.pfnSetStreamSource = f->table.pfnSetStreamSource;
  expectedTable.pfnSetIndices = f->table.pfnSetIndices;
  expectedTable.pfnDrawPrimitive = f->table.pfnDrawPrimitive;
  expectedTable.pfnDrawIndexedPrimitive = f->table.pfnDrawIndexedPrimitive;
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

static void creationFlagContracts() {
  for (const UINT flags : {UINT(0), UINT(1), UINT(2), UINT(3)}) {
    Fixture fixture; initialize(fixture); f->create.Flags.Value = flags;
    createDevice();
    CHECK(f->create.Flags.Value == flags);
    CHECK(f->table.pfnFlush(f->device) == S_OK);
    CHECK(f->contexts == 1 && f->backends == 1);
    closeDevice(); closeAdapter();
  }
  for (UINT bit = 2; bit < 32; ++bit) {
    Fixture fixture; initialize(fixture);
    f->create.Flags.Value = (UINT(1) << bit) | 3u;
    const auto queries = f->queries;
    unchangedCreate(D3DERR_NOTAVAILABLE);
    CHECK(f->queries == queries && !f->contexts && !f->backends && !f->allocations);
    closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); f->create.Flags.Value = 3;
    // The runtime can replace the caller's flags during the identity callback.
    // The supported original permissions must already have been snapshotted.
    f->queryHook = [] { f->create.Flags.Value = UINT_MAX; };
    CHECK(f->adapterFuncs.pfnCreateDevice(f->adapter, &f->create) == S_OK);
    f->device = f->create.hDevice;
    CHECK(f->create.Flags.Value == UINT_MAX && f->table.pfnFlush && f->table.pfnDestroyDevice);
    CHECK(f->table.pfnFlush(f->device) == S_OK);
    closeDevice(); closeAdapter();
  }
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

static D3DDDIARG_CREATERESOURCE depthArgs(HANDLE cookie, D3DDDI_SURFACEINFO* info, D3DFORMAT format) {
  auto args = resourceArgs(cookie, info, 1, true);
  args.Flags.RenderTarget = 0; args.Flags.ZBuffer = 1;
  args.Format = static_cast<D3DDDIFORMAT>(format);
  return args;
}

static void depthContracts() {
  for (const auto format : {D3DFMT_D16, D3DFMT_D24S8}) {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnSetDepthStencil);
    char cookie, targetCookie;
    D3DDDI_SURFACEINFO info = {8,4,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = depthArgs(&cookie, &info, format);
    const auto valid = args;
    auto before = snapshot(args);
    f->surfaceResult = S_FALSE;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_FAIL && snapshot(args) == before);
    f->surfaceResult = S_OK; f->nullSurface = true;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_FAIL && snapshot(args) == before);
    f->nullSurface = false;
    for (unsigned field = 0; field < 8; ++field) {
      args = valid;
      if (field == 0) args.Flags.RenderTarget = 1;
      if (field == 1) args.Flags.Texture = 1;
      if (field == 2) args.SurfCount = 2;
      if (field == 3) args.Pool = D3DDDIPOOL_SYSTEMMEM;
      if (field == 4) args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_A8R8G8B8);
      if (field == 5) args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_D32);
      if (field == 6) args.MultisampleType = D3DDDIMULTISAMPLE_2_SAMPLES;
      if (field == 7) args.MultisampleQuality = 1;
      before = snapshot(args);
      CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG && snapshot(args) == before);
    }
    args = valid; args.Flags.NotLockable = 1;
    f->queryHook = [&] { info.Width = 200; args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_D32); };
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE depth = args.hResource;
    CHECK(f->descriptions.size() == 1 && f->descriptions[0].depthStencil
      && !f->descriptions[0].renderTarget && !f->descriptions[0].systemMemory
      && !f->descriptions[0].lockable && f->descriptions[0].width == 8
      && f->descriptions[0].height == 4 && f->descriptions[0].format == format);
    info.Width = 8;
    D3DDDIARG_SETDEPTHSTENCIL bind = {depth};
    CHECK(f->table.pfnSetDepthStencil(f->device, nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnSetDepthStencil(nullptr, &bind) == E_INVALIDARG);
    bind.hZBuffer = reinterpret_cast<HANDLE>(UINT_PTR(0xcafef00d));
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == E_INVALIDARG);
    bind.hZBuffer = depth;
    f->depthResult = S_FALSE;
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == E_FAIL);
    D3DDDIARG_CLEAR fill = {}; fill.Flags = D3DCLEAR_ZBUFFER | 8; fill.FillDepth = 0.375f;
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_INVALIDARG);
    f->depthResult = S_OK;
    f->queryHook = [&] { bind.hZBuffer = nullptr; };
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == S_OK);
    CHECK(f->table.pfnClear(f->device, &fill, 0, reinterpret_cast<const RECT*>(UINT_PTR(1))) == S_OK);
    CHECK(f->lastComputeRects && f->clearFlags == D3DCLEAR_ZBUFFER && f->clearDepth == 0.375f);
    fill.Flags = D3DCLEAR_ZBUFFER;
    CHECK(f->table.pfnClear(f->device, &fill, 0, reinterpret_cast<const RECT*>(UINT_PTR(1))) == S_OK);
    CHECK(!f->lastComputeRects && f->clearRects.empty());
    const auto validFill = fill;
    for (const float invalid : {-0.1f,1.1f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
      fill.FillDepth = invalid;
      CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_INVALIDARG);
    }
    fill = validFill;
    for (const UINT invalid : {0u,8u,0x10u,0x80000002u}) {
      fill.Flags = invalid;
      CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_INVALIDARG);
    }
    fill = validFill;
    RECT rects[2] = {{1,1,7,4},{0,0,0,0}};
    const auto saved = snapshot(rects);
    f->queryHook = [&] {
      CHECK(f->table.pfnClear(f->device, &fill, 1, reinterpret_cast<const RECT*>(UINT_PTR(1))) == D3DERR_WASSTILLDRAWING);
      rects[0] = {-100,-100,900,900}; fill.FillDepth = 0.875f; fill.Flags = D3DCLEAR_TARGET;
    };
    CHECK(f->table.pfnClear(f->device, &fill, 2, rects) == S_OK);
    CHECK(f->clearFlags == D3DCLEAR_ZBUFFER && f->clearDepth == 0.375f
      && f->clearRects.size() == 2 && !std::memcmp(f->clearRects.data(), saved.data(), sizeof(rects)));
    fill = validFill;
    CHECK(f->table.pfnClear(f->device, &fill, 1, rects) == E_INVALIDARG);
    fill.Flags |= 8;
    CHECK(f->table.pfnClear(f->device, &fill, 1, rects) == S_OK && f->lastComputeRects);
    rects[0] = {2,1,1,3};
    CHECK(f->table.pfnClear(f->device, &fill, 1, rects) == E_INVALIDARG);
    fill.Flags = D3DCLEAR_STENCIL | 8; fill.FillDepth = std::numeric_limits<float>::quiet_NaN(); fill.FillStencil = 7;
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == (format == D3DFMT_D24S8 ? S_OK : E_INVALIDARG));
    fill.FillStencil = 256;
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_INVALIDARG);
    D3DDDIARG_LOCK map = {}; map.hResource = depth;
    const auto mapping = snapshot(map);
    CHECK(f->table.pfnLock(f->device, &map) == E_INVALIDARG && snapshot(map) == mapping);
    D3DDDIARG_BLT copy = {}; copy.hSrcResource = copy.hDstResource = depth;
    copy.SrcRect = copy.DstRect = {0,0,8,4};
    CHECK(f->table.pfnBlt(f->device, &copy) == E_INVALIDARG);
    args = resourceArgs(&targetCookie, &info, 1, true);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE target = args.hResource;
    bind.hZBuffer = target;
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == E_INVALIDARG);
    D3DDDIARG_SETRENDERTARGET rt = {0,depth,0};
    CHECK(f->table.pfnSetRenderTarget(f->device, &rt) == E_INVALIDARG);
    rt.hRenderTarget = target;
    CHECK(f->table.pfnSetRenderTarget(f->device, &rt) == S_OK);
    fill = validFill; fill.Flags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | 8; fill.FillColor = 0xff135724;
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == S_OK
      && f->clearFlags == (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER));
    f->clearResult = S_FALSE;
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_FAIL);
    f->clearResult = S_OK; f->depthResult = DXGI_ERROR_WAS_STILL_DRAWING;
    CHECK(f->table.pfnDestroyResource(f->device, depth) == D3DERR_WASSTILLDRAWING);
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == S_OK);
    f->depthResult = S_OK; f->flushResult = DXGI_ERROR_WAS_STILL_DRAWING;
    CHECK(f->table.pfnDestroyResource(f->device, depth) == D3DERR_WASSTILLDRAWING);
    CHECK(f->table.pfnClear(f->device, &fill, 0, nullptr) == E_INVALIDARG);
    f->flushResult = S_OK;
    bind.hZBuffer = depth;
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == S_OK);
    CHECK(f->table.pfnDestroyResource(f->device, depth) == S_OK);
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyResource(f->device, depth) == E_INVALIDARG);
    args = valid;
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    bind.hZBuffer = args.hResource;
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == S_OK);
    // Close must unbind the remaining depth resource before worker retirement.
    closeAdapter(); closeDevice();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char targetCookie, depthOwners[2];
    D3DDDI_SURFACEINFO info = {8,4,1,nullptr,0,0};
    auto args = resourceArgs(&targetCookie, &info, 1, true);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const D3DDDIARG_SETRENDERTARGET rt = {0,args.hResource,0};
    CHECK(f->table.pfnSetRenderTarget(f->device, &rt) == S_OK);
    HANDLE depthResources[2] = {};
    for (UINT i = 0; i < 2; ++i) {
      info.Width = i ? 4 : 8; info.Height = i ? 2 : 4;
      args = depthArgs(&depthOwners[i], &info, D3DFMT_D16);
      CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
      depthResources[i] = args.hResource;
    }
    const D3DDDIVERTEXELEMENT element = {0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITIONT,0};
    D3DDDIARG_CREATEVERTEXSHADERDECL declaration = {1,nullptr};
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device, &declaration, &element) == S_OK);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device, declaration.ShaderHandle) == S_OK);
    const float vertex[] = {0,0,0.5f,1};
    const D3DDDIARG_SETSTREAMSOURCEUM stream = {0,sizeof(vertex)};
    CHECK(f->table.pfnSetStreamSourceUm(f->device, &stream, vertex) == S_OK);
    f->expectedDrawBytes = sizeof(vertex);
    const D3DDDIARG_DRAWPRIMITIVE primitive = {D3DPT_POINTLIST,0,1};
    D3DDDIARG_SETDEPTHSTENCIL bind = {depthResources[0]};
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device, &primitive, nullptr) == S_OK);
    CHECK(f->draws == 1);
    bind.hZBuffer = depthResources[1];
    CHECK(f->table.pfnSetDepthStencil(f->device, &bind) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device, &primitive, nullptr) == E_INVALIDARG && f->draws == 1);
    CHECK(f->table.pfnDestroyResource(f->device, depthResources[1]) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device, &primitive, nullptr) == S_OK && f->draws == 2);
    closeAdapter(); closeDevice();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie;
    D3DDDI_SURFACEINFO info = {8,4,1,nullptr,0,0};
    auto args = depthArgs(&cookie, &info, D3DFMT_D24S8);
    const auto before = snapshot(args);
    f->surfaceHook = [&] { ++f->generation; };
    CHECK(f->table.pfnCreateResource(f->device, &args) == D3DERR_DEVICELOST && snapshot(args) == before);
    CHECK(f->surfaceCreates == f->surfaceCloses);
    closeAdapter(); closeDevice();
  }
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

static void drawContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnCreateVertexShaderDecl && f->table.pfnSetVertexShaderDecl
      && f->table.pfnDeleteVertexShaderDecl && f->table.pfnSetRenderState
      && f->table.pfnSetViewport && f->table.pfnSetZRange && f->table.pfnSetScissorRect
      && f->table.pfnSetStreamSourceUm && f->table.pfnDrawPrimitive);
    D3DDDIVERTEXELEMENT elements[3] = {{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITIONT,0},
      {0,16,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}, {0xff,0,D3DDECLTYPE_UNUSED,0,0,0}};
    D3DDDIARG_CREATEVERTEXSHADERDECL declaration = {2,reinterpret_cast<HANDLE>(UINT_PTR(0xdead))};
    auto prior = snapshot(declaration);
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,nullptr,elements) == E_INVALIDARG);
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,nullptr) == E_INVALIDARG);
    for (const UINT count : {UINT(0),UINT(65),UINT_MAX}) {
      declaration.NumVertexElements = count; prior = snapshot(declaration);
      CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,elements) == E_INVALIDARG);
      CHECK(snapshot(declaration) == prior);
    }
    declaration.NumVertexElements = 2; prior = snapshot(declaration);
    for (unsigned field = 0; field < 5; ++field) {
      auto invalid = elements[0];
      if (field == 0) invalid.Stream = 16;
      if (field == 1) invalid.Type = D3DDECLTYPE_UNUSED;
      if (field == 2) invalid.Method = D3DDECLMETHOD_CROSSUV;
      if (field == 3) invalid.Usage = D3DDECLUSAGE_SAMPLE + 1;
      if (field == 4) invalid.UsageIndex = 16;
      declaration.NumVertexElements = 1; const auto before = snapshot(declaration);
      const auto queries = f->queries;
      CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,&invalid) == E_INVALIDARG);
      CHECK(snapshot(declaration) == before && f->queries == queries);
    }
    declaration.NumVertexElements = 2;
    f->declarationResult = S_FALSE;
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,elements) == E_FAIL && snapshot(declaration) == prior);
    f->declarationResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,elements) == E_OUTOFMEMORY && snapshot(declaration) == prior);
    f->declarationResult = S_OK; f->nullDeclaration = true;
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,elements) == E_FAIL && snapshot(declaration) == prior);
    f->nullDeclaration = false;
    f->queryHook = [&] {
      elements[1].Offset = 200; declaration.NumVertexElements = UINT_MAX;
      CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
    };
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,elements) == S_OK);
    const HANDLE token = declaration.ShaderHandle;
    CHECK(token != f->device && token != reinterpret_cast<HANDLE>(UINT_PTR(0xdead)));
    CHECK(f->declarationElements.size() == 2 && f->declarationElements[1].Offset == 16);
    elements[1].Offset = 16;
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,token) == S_OK);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,&fixture) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyResource(f->device,token) == E_INVALIDARG);
    char cookie;
    D3DDDI_SURFACEINFO info = {8,8,0,nullptr,0,0};
    auto resource = resourceArgs(&cookie,&info,1,true);
    CHECK(f->table.pfnCreateResource(f->device,&resource) == S_OK);
    CHECK(f->table.pfnDeleteVertexShaderDecl(f->device,resource.hResource) == E_INVALIDARG);
    D3DDDIARG_SETRENDERTARGET target = {0,resource.hResource,0};
    CHECK(f->table.pfnSetRenderTarget(f->device,&target) == S_OK);

    D3DDDIARG_RENDERSTATE state = {D3DDDIRS_CULLMODE,D3DCULL_NONE};
    f->queryHook = [&] { state.State = D3DDDIRS_STENCILENABLE; state.Value = 99; };
    CHECK(f->table.pfnSetRenderState(f->device,&state) == S_OK
      && f->renderState == D3DRS_CULLMODE && f->renderValue == D3DCULL_NONE);
    for (const UINT invalid : {UINT(0),UINT(10),UINT(30),UINT(40),UINT(62+1),UINT(64),UINT(95),UINT(169),UINT(255),UINT_MAX}) {
      state.State = static_cast<D3DDDIRENDERSTATETYPE>(invalid);
      CHECK(f->table.pfnSetRenderState(f->device,&state) == E_INVALIDARG);
    }
    state = {D3DDDIRS_SCENECAPTURE,1};
    CHECK(f->table.pfnSetRenderState(f->device,&state) == S_OK && f->scene);
    CHECK(f->table.pfnSetRenderState(f->device,&state) == D3DERR_INVALIDCALL);
    state.Value = 0;
    CHECK(f->table.pfnSetRenderState(f->device,&state) == S_OK && !f->scene);
    state = {D3DDDIRS_SOFTWAREVERTEXPROCESSING,1};
    CHECK(f->table.pfnSetRenderState(f->device,&state) == S_OK && f->software);
    state.Value = 2; CHECK(f->table.pfnSetRenderState(f->device,&state) == E_INVALIDARG);
    D3DDDIARG_ZRANGE range = {0.2f,0.8f};
    CHECK(f->table.pfnSetZRange(f->device,&range) == S_OK);
    D3DDDIARG_VIEWPORTINFO viewport = {1,2,3,4};
    f->queryHook = [&] { viewport = {0,0,1,1}; range = {0.0f,1.0f}; };
    CHECK(f->table.pfnSetViewport(f->device,&viewport) == S_OK);
    CHECK(f->viewport.X == 1 && f->viewport.Y == 2 && f->viewport.Width == 3 && f->viewport.Height == 4
      && f->viewport.MinZ == 0.2f && f->viewport.MaxZ == 0.8f);
    CHECK(f->table.pfnSetZRange(f->device,&range) == S_OK && f->viewport.X == 1 && f->viewport.Width == 3);
    for (const D3DDDIARG_ZRANGE invalid : {D3DDDIARG_ZRANGE{-1.0f,0.5f},{0.0f,2.0f},{0.8f,0.2f},
        {std::numeric_limits<float>::quiet_NaN(),1.0f},{0.0f,std::numeric_limits<float>::infinity()}})
      CHECK(f->table.pfnSetZRange(f->device,&invalid) == E_INVALIDARG);
    viewport = {UINT_MAX,0,1,1}; CHECK(f->table.pfnSetViewport(f->device,&viewport) == E_INVALIDARG);
    viewport = {0,0,0,1}; CHECK(f->table.pfnSetViewport(f->device,&viewport) == E_INVALIDARG);
    RECT scissor = {-1,2,6,7};
    f->queryHook = [&] { scissor = {99,99,1,1}; };
    CHECK(f->table.pfnSetScissorRect(f->device,&scissor) == S_OK && f->scissor.left == -1 && f->scissor.right == 6);
    CHECK(f->table.pfnSetScissorRect(f->device,&scissor) == E_INVALIDARG);
    scissor = {3,3,3,3}; CHECK(f->table.pfnSetScissorRect(f->device,&scissor) == S_OK);

    std::array<uint8_t,240> vertices;
    for (size_t i = 0; i < vertices.size(); ++i) vertices[i] = uint8_t(i ^ 0x5a);
    D3DDDIARG_SETSTREAMSOURCEUM stream = {0,24};
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,vertices.data()) == S_OK);
    stream.Stream = 1; CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,vertices.data()) == E_INVALIDARG);
    stream.Stream = 0; stream.Stride = 0;
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,vertices.data()) == E_INVALIDARG);
    D3DDDIARG_DRAWPRIMITIVE draw = {D3DPT_TRIANGLESTRIP,1,2};
    f->expectedDrawBytes = 96;
    const std::vector<uint8_t> expected(vertices.begin()+24,vertices.begin()+120);
    f->queryHook = [&] {
      CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDeleteVertexShaderDecl(f->device,token) == D3DERR_WASSTILLDRAWING);
      std::thread concurrent([&] { CHECK(f->table.pfnSetVertexShaderDecl(f->device,nullptr) == D3DERR_WASSTILLDRAWING); });
      concurrent.join();
      vertices.fill(0xe1); draw = {D3DPT_POINTLIST,UINT_MAX,UINT_MAX};
    };
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == S_OK);
    CHECK(f->drawVertices == expected && f->drawStride == 24 && f->drawCount == 2 && f->drawType == D3DPT_TRIANGLESTRIP);
    // The DDI binding survives UP's internal stream-zero reset; a later draw
    // snapshots the current caller bytes rather than the previous upload.
    for (const auto test : {std::pair<D3DPRIMITIVETYPE,UINT>{D3DPT_POINTLIST,2}, {D3DPT_LINELIST,4},
        {D3DPT_LINESTRIP,3},{D3DPT_TRIANGLELIST,6},{D3DPT_TRIANGLESTRIP,4},{D3DPT_TRIANGLEFAN,4}}) {
      draw = {test.first,1,2}; f->expectedDrawBytes = size_t(test.second) * 24;
      CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == S_OK);
      CHECK(f->drawVertices == std::vector<uint8_t>(f->expectedDrawBytes,0xe1)
        && f->drawType == test.first && f->drawCount == 2);
    }
    const auto draws = f->draws;
    draw = {D3DPT_TRIANGLELIST,0,UINT_MAX};
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG);
    draw = {static_cast<D3DPRIMITIVETYPE>(0),0,1};
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG);
    draw = {D3DPT_POINTLIST,0,1};
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,reinterpret_cast<const UINT*>(UINT_PTR(1))) == E_INVALIDARG);
    stream = {0,24};
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,reinterpret_cast<void*>(UINTPTR_MAX-3)) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG && f->draws == draws);
    draw.PrimitiveCount = 0; draw.VStart = UINT_MAX;
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == S_OK && f->draws == draws);
    stream.Stride = UINT_MAX;
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,vertices.data()) == S_OK);
    draw = {D3DPT_POINTLIST,0,1};
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG);
    stream.Stride = 4;
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,vertices.data()) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,nullptr) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG);
    stream.Stride = 24;
    CHECK(f->table.pfnSetStreamSourceUm(f->device,&stream,vertices.data()) == S_OK);
    f->stateResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,nullptr) == E_OUTOFMEMORY);
    f->stateResult = S_OK; f->drawResult = E_OUTOFMEMORY; f->expectedDrawBytes = 24;
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_OUTOFMEMORY);
    f->drawResult = S_OK;
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == S_OK);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = resource.hResource; mapping.Flags.ReadOnly = 1;
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
    const auto calls = f->draws;
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG && f->draws == calls);
    D3DDDIARG_UNLOCK unlockTarget = {}; unlockTarget.hResource = resource.hResource;
    CHECK(f->table.pfnUnlock(f->device,&unlockTarget) == S_OK);
    auto otherStream = elements[0]; otherStream.Stream = 1;
    D3DDDIARG_CREATEVERTEXSHADERDECL otherDeclaration = {1,nullptr};
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&otherDeclaration,&otherStream) == S_OK);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,otherDeclaration.ShaderHandle) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device,&draw,nullptr) == E_INVALIDARG && f->draws == calls);
    CHECK(f->table.pfnDeleteVertexShaderDecl(f->device,otherDeclaration.ShaderHandle) == S_OK);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,token) == S_OK);
    const auto closes = f->declarationCloses;
    f->flushResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDeleteVertexShaderDecl(f->device,token) == E_OUTOFMEMORY && f->declarationCloses == closes);
    f->flushResult = S_OK;
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,token) == S_OK);
    CHECK(f->table.pfnDeleteVertexShaderDecl(f->device,token) == S_OK && f->declarationCloses == closes + 1);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,token) == E_INVALIDARG);
    CHECK(f->table.pfnDeleteVertexShaderDecl(f->device,token) == E_INVALIDARG);
    declaration.NumVertexElements = 3;
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&declaration,elements) == S_OK);
    CHECK(declaration.ShaderHandle != token && f->declarationElements.size() == 2);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device,declaration.ShaderHandle) == S_OK);
    closeDevice(); CHECK(f->declarationCreates == f->declarationCloses); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    D3DDDIVERTEXELEMENT element = {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0};
    D3DDDIARG_CREATEVERTEXSHADERDECL args = {1,reinterpret_cast<HANDLE>(UINT_PTR(0xdead))};
    const auto original = snapshot(args);
    f->declarationHook = [&] { f->queryHook = [&] { ++f->generation; }; };
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&args,&element) == D3DERR_DEVICELOST);
    CHECK(snapshot(args) == original && f->declarationCreates == 1 && f->declarationCloses == 1);
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device,&args,&element) == D3DERR_DEVICELOST);
    closeDevice(); closeAdapter();
  }
}

template<typename Args, typename T, typename Function>
static void constantContract(Function function, bool vertex, char type, UINT limit) {
  Args args{limit - 2, 2};
  std::array<T, 8> values = {};
  for (size_t i = 0; i < sizeof(values); ++i)
    reinterpret_cast<uint8_t*>(values.data())[i] = uint8_t(17 + i * 7);
  const size_t bytes = type == 'B' ? 2 * sizeof(T) : 8 * sizeof(T);
  const auto ptr = reinterpret_cast<const uint8_t*>(values.data());
  const std::vector<uint8_t> expected(ptr, ptr + bytes);
  f->queryHook = [&] {
    args = {UINT_MAX, UINT_MAX};
    values.fill(T{});
    Args nested{0, 1};
    CHECK(function(f->device, &nested, reinterpret_cast<const T*>(UINT_PTR(1))) == D3DERR_WASSTILLDRAWING);
  };
  CHECK(function(f->device, &args, values.data()) == S_OK);
  CHECK(f->constantFirst == limit - 2 && f->constantCount == 2 && f->constantType == type);
  CHECK(f->shaderStage == (vertex ? dxvk::umd::D3D9ShaderStage::Vertex : dxvk::umd::D3D9ShaderStage::Pixel));
  CHECK(f->constantBytes == expected);
  args = {limit, 0};
  const auto sets = f->constantSets;
  CHECK(function(f->device, &args, nullptr) == S_OK && f->constantSets == sets);
  CHECK(function(f->device, nullptr, values.data()) == E_INVALIDARG);
  args = {0, 1}; CHECK(function(f->device, &args, nullptr) == E_INVALIDARG);
  args = {limit, 1}; CHECK(function(f->device, &args, values.data()) == E_INVALIDARG);
  args = {UINT_MAX, 0}; CHECK(function(f->device, &args, values.data()) == E_INVALIDARG);
  args = {1, UINT_MAX}; CHECK(function(f->device, &args, values.data()) == E_INVALIDARG);
  args = {0, 1};
  CHECK(function(f->device, &args, reinterpret_cast<const T*>(UINTPTR_MAX - 1)) == E_INVALIDARG);
  f->constantResult = S_FALSE; CHECK(function(f->device, &args, values.data()) == E_FAIL);
  f->constantResult = E_OUTOFMEMORY; CHECK(function(f->device, &args, values.data()) == E_OUTOFMEMORY);
  f->constantResult = S_OK;
}

static void shaderContracts() {
  // END-like immediate values and comment payloads are data, not terminators.
  // Exercise the same bounded framing check used by the real renderer entry.
  for (DWORD version : {0xfffe0101u,0xfffe0200u,0xfffe0300u,0xffff0101u,0xffff0200u,0xffff0300u}) {
    const bool vertex = (version >> 16) == 0xfffe;
    const bool legacy = ((version >> 8) & 255) == 1;
    const std::vector<DWORD> code{version,0x0002fffe,0x0000ffff,0x00000001,
      legacy ? 0x00000051u : 0x05000051u,0xa00f0000,0x0000ffff,0,0,0,0x0000ffff};
    CHECK(dxvk::validateD3D9ShaderCode(code.data(),code.size() * 4,vertex));
    CHECK(!dxvk::validateD3D9ShaderCode(code.data(),code.size() * 4,!vertex));
    for (size_t words = 0; words < code.size(); ++words)
      CHECK(!dxvk::validateD3D9ShaderCode(code.data(),words * 4,vertex));
    auto trailing = code; trailing.push_back(0);
    CHECK(!dxvk::validateD3D9ShaderCode(trailing.data(),trailing.size() * 4,vertex));
  }
  {
    const DWORD relative[] = {0xfffe0300,0x03000001,0xe00f0000,0xa0e42000,0xb0000000,0x0000ffff};
    const DWORD predicate[] = {0xfffe0300,0x13000001,0xe00f0000,0xb0001000,0x90e40000,0x0000ffff};
    CHECK(dxvk::validateD3D9ShaderCode(relative,sizeof(relative),true));
    CHECK(dxvk::validateD3D9ShaderCode(predicate,sizeof(predicate),true));
    auto bad = std::array<DWORD,6>{{0xfffe0300,0x03000001,0xe00f0000,0xa0e42000,0,0x0000ffff}};
    CHECK(!dxvk::validateD3D9ShaderCode(bad.data(),sizeof(bad),true));
    bad = {{0xfffe0300,0x03000001,0xe00f0000,0xa0e40000,0xb0000000,0x0000ffff}};
    CHECK(!dxvk::validateD3D9ShaderCode(bad.data(),sizeof(bad),true));
  }
  const std::array<UINT, 5> vs{{0xfffe0300u, 0x02000001u, 0xe00f0000u, 0x90e40000u, 0x0000ffffu}};
  const std::array<UINT, 5> ps{{0xffff0300u, 0x02000001u, 0x800f0800u, 0xa0e40000u, 0x0000ffffu}};
  {
    Fixture fixture; initialize(fixture); createDevice();
    auto code = vs;
    D3DDDIARG_CREATEVERTEXSHADERFUNC vertex{UINT(sizeof(code)), reinterpret_cast<HANDLE>(UINT_PTR(0xabc))};
    const auto original = snapshot(vertex);
    const auto queries = f->queries;
    CHECK(f->table.pfnCreateVertexShaderFunc(f->device, nullptr, code.data()) == E_INVALIDARG);
    CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, nullptr) == E_INVALIDARG);
    for (UINT size : {0u, 4u, 6u, 4u * 1024u * 1024u + 4u, UINT_MAX}) {
      vertex.Size = size;
      const auto before = snapshot(vertex);
      CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, code.data()) == E_INVALIDARG);
      CHECK(snapshot(vertex) == before);
    }
    vertex.Size = UINT(sizeof(code));
    CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex,
      reinterpret_cast<const UINT*>(UINTPTR_MAX - 3)) == E_INVALIDARG);
    CHECK(f->queries == queries && !f->shaderCreates);
    for (UINT size = 8; size < sizeof(code); size += 4) {
      vertex.Size = size;
      CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, code.data()) == E_INVALIDARG);
    }
    vertex.Size = UINT(sizeof(code));
    for (auto invalid : {ps, std::array<UINT, 5>{{0xfffe0400u, 0, 0, 0, 0xffff}},
      std::array<UINT, 5>{{0xfffe0300u, 0x03000001u, 0xe00f0000u, 0x90e40000u, 0xffff}},
      std::array<UINT, 5>{{0xfffe0300u, 0x02000001u, 0xe00f0000u, 0x90e40000u, 0}},
      std::array<UINT, 5>{{0xfffe0300u, 0x7ffffffeu, 0, 0, 0xffff}}}) {
      CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, invalid.data()) == E_INVALIDARG);
    }
    CHECK(snapshot(vertex) == original && !f->shaderCreates);
    // No read may cross the explicit code bound, even when END is absent.
    SYSTEM_INFO info = {}; GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    auto allocation = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    CHECK(allocation);
    DWORD prior = 0;
    CHECK(VirtualProtect(allocation + page, page, PAGE_NOACCESS, &prior));
    auto bounded = allocation + page - 16;
    std::memcpy(bounded, code.data(), 16);
    vertex.Size = 16;
    CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, reinterpret_cast<const UINT*>(bounded)) == E_INVALIDARG);
    CHECK(VirtualFree(allocation, 0, MEM_RELEASE));
    vertex.Size = UINT(sizeof(code));
    for (HRESULT failure : {S_FALSE, E_OUTOFMEMORY}) {
      f->shaderResult = failure;
      CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, code.data()) == (failure == S_FALSE ? E_FAIL : failure));
      CHECK(snapshot(vertex) == original && f->shaderCreates == f->shaderCloses);
    }
    f->shaderResult = S_OK; f->nullShader = true;
    CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, code.data()) == E_FAIL && snapshot(vertex) == original);
    f->nullShader = false;
    f->queryHook = [&] {
      vertex.Size = 8;
      code.fill(0);
      CHECK(f->table.pfnSetPixelShader(f->device, nullptr) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex,
        reinterpret_cast<const UINT*>(UINT_PTR(1))) == D3DERR_WASSTILLDRAWING);
      std::thread concurrent([&] { CHECK(f->table.pfnSetVertexShaderFunc(f->device, nullptr) == D3DERR_WASSTILLDRAWING); });
      concurrent.join();
    };
    CHECK(f->table.pfnCreateVertexShaderFunc(f->device, &vertex, code.data()) == S_OK);
    CHECK(f->shaderCode == std::vector<DWORD>(vs.begin(), vs.end()));
    const auto vertexToken = vertex.ShaderHandle;
    CHECK(vertexToken && vertexToken != f->device);
    D3DDDIARG_CREATEPIXELSHADER pixel{UINT(sizeof(ps)), nullptr};
    CHECK(f->table.pfnCreatePixelShader(f->device, &pixel, ps.data()) == S_OK);
    CHECK(pixel.ShaderHandle && pixel.ShaderHandle != vertexToken);
    CHECK(f->table.pfnSetVertexShaderFunc(f->device, vertexToken) == S_OK);
    CHECK(f->table.pfnSetPixelShader(f->device, pixel.ShaderHandle) == S_OK);
    CHECK(f->table.pfnSetVertexShaderFunc(f->device, pixel.ShaderHandle) == E_INVALIDARG);
    CHECK(f->table.pfnSetPixelShader(f->device, vertexToken) == E_INVALIDARG);
    CHECK(f->table.pfnDeleteVertexShaderFunc(f->device, pixel.ShaderHandle) == E_INVALIDARG);
    CHECK(f->table.pfnDeletePixelShader(f->device, vertexToken) == E_INVALIDARG);
    CHECK(f->table.pfnDeleteVertexShaderDecl(f->device, vertexToken) == E_INVALIDARG);
    CHECK(f->table.pfnDeletePixelShader(f->device, f->device) == E_INVALIDARG);
    CHECK(f->table.pfnDeletePixelShader(f->device, nullptr) == E_INVALIDARG);
    constantContract<D3DDDIARG_SETVERTEXSHADERCONST, float>(f->table.pfnSetVertexShaderConst, true, 'F', 256);
    constantContract<D3DDDIARG_SETPIXELSHADERCONST, float>(f->table.pfnSetPixelShaderConst, false, 'F', 224);
    constantContract<D3DDDIARG_SETVERTEXSHADERCONSTI, INT>(f->table.pfnSetVertexShaderConstI, true, 'I', 16);
    constantContract<D3DDDIARG_SETPIXELSHADERCONSTI, INT>(f->table.pfnSetPixelShaderConstI, false, 'I', 16);
    constantContract<D3DDDIARG_SETVERTEXSHADERCONSTB, BOOL>(f->table.pfnSetVertexShaderConstB, true, 'B', 16);
    constantContract<D3DDDIARG_SETPIXELSHADERCONSTB, BOOL>(f->table.pfnSetPixelShaderConstB, false, 'B', 16);
    const auto closes = f->shaderCloses;
    f->stateResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDeleteVertexShaderFunc(f->device, vertexToken) == E_OUTOFMEMORY && f->shaderCloses == closes);
    f->stateResult = S_OK; f->flushResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDeleteVertexShaderFunc(f->device, vertexToken) == E_OUTOFMEMORY && f->shaderCloses == closes);
    CHECK(f->table.pfnSetVertexShaderFunc(f->device, vertexToken) == S_OK);
    f->flushResult = S_OK;
    CHECK(f->table.pfnDeleteVertexShaderFunc(f->device, vertexToken) == S_OK && f->shaderCloses == closes + 1);
    CHECK(f->table.pfnSetVertexShaderFunc(f->device, vertexToken) == E_INVALIDARG);
    CHECK(f->table.pfnDeleteVertexShaderFunc(f->device, vertexToken) == E_INVALIDARG);
    closeDevice(); CHECK(f->shaderCreates == f->shaderCloses); closeAdapter();
  }
  for (bool pixel : {false, true}) {
    Fixture fixture; initialize(fixture); createDevice();
    f->shaderHook = [] { f->queryHook = [] { ++f->generation; }; };
    D3DDDIARG_CREATEVERTEXSHADERFUNC vertex{UINT(sizeof(vs)), reinterpret_cast<HANDLE>(UINT_PTR(0x123))};
    D3DDDIARG_CREATEPIXELSHADER fragment{UINT(sizeof(ps)), reinterpret_cast<HANDLE>(UINT_PTR(0x456))};
    const auto v = snapshot(vertex), p = snapshot(fragment);
    CHECK((pixel ? f->table.pfnCreatePixelShader(f->device, &fragment, ps.data())
                 : f->table.pfnCreateVertexShaderFunc(f->device, &vertex, vs.data())) == D3DERR_DEVICELOST);
    CHECK(snapshot(vertex) == v && snapshot(fragment) == p && f->shaderCreates == f->shaderCloses);
    const auto queries = f->queries;
    --f->generation;
    CHECK(f->table.pfnSetPixelShader(f->device, nullptr) == D3DERR_DEVICELOST && f->queries == queries);
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    D3DDDIARG_CREATEPIXELSHADER pixel{UINT(sizeof(ps)), nullptr};
    CHECK(f->table.pfnCreatePixelShader(f->device, &pixel, ps.data()) == S_OK);
    CHECK(f->table.pfnSetPixelShader(f->device, pixel.ShaderHandle) == S_OK);
    f->stateResult = D3DERR_DEVICELOST;
    CHECK(f->table.pfnDeletePixelShader(f->device, pixel.ShaderHandle) == S_OK);
    D3DDDIARG_SETPIXELSHADERCONST args{0, 1}; float values[4] = {};
    f->constantResult = D3DERR_DEVICELOST;
    CHECK(f->table.pfnSetPixelShaderConst(f->device, &args, values) == D3DERR_DEVICELOST);
    CHECK(f->table.pfnDeletePixelShader(f->device, pixel.ShaderHandle) == E_INVALIDARG);
    closeDevice(); closeAdapter();
  }
  {
    Fixture a; initialize(a); createDevice();
    D3DDDIARG_CREATEVERTEXSHADERFUNC vertex{UINT(sizeof(vs)), nullptr};
    CHECK(a.table.pfnCreateVertexShaderFunc(a.device, &vertex, vs.data()) == S_OK);
    Fixture b; initialize(b); createDevice();
    CHECK(b.table.pfnSetVertexShaderFunc(b.device, vertex.ShaderHandle) == E_INVALIDARG);
    CHECK(b.table.pfnDeleteVertexShaderFunc(b.device, vertex.ShaderHandle) == E_INVALIDARG);
    closeDevice(); closeAdapter();
    f = &a;
    CHECK(a.table.pfnSetVertexShaderFunc(a.device, vertex.ShaderHandle) == S_OK);
    closeDevice(); closeAdapter();
  }
}

static D3DDDIARG_CREATERESOURCE textureArgs(HANDLE cookie, D3DDDI_SURFACEINFO* levels,
    UINT count, bool system = false, bool target = false) {
  auto args = resourceArgs(cookie, levels, count, target);
  args.Flags.Texture = 1; args.MipLevels = count;
  args.Pool = system ? D3DDDIPOOL_SYSTEMMEM : D3DDDIPOOL_VIDEOMEMORY;
  return args;
}

static void textureContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnSetTexture && f->table.pfnSetTextureStageState && f->table.pfnTexBlt);
    char cookie;
    D3DDDI_SURFACEINFO levels[3] = {{5,3,UINT_MAX,nullptr,UINT_MAX,UINT_MAX},
      {2,1,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}, {1,1,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}};
    auto args = textureArgs(&cookie, levels, 3);
    auto prior = snapshot(args);
    for (const UINT count : {0u,2u,4u,33u,UINT_MAX}) {
      args.MipLevels = count; prior = snapshot(args);
      CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG && snapshot(args) == prior);
    }
    args.MipLevels = 3;
    for (const UINT flag : {0x10u,0x800u,0x20000u,0x40000u}) {
      args.Flags.Value = 0x10000 | flag; prior = snapshot(args);
      CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG && snapshot(args) == prior);
    }
    args.Flags.Value = 0x10000;
    for (unsigned i = 0; i < 3; ++i) {
      const UINT width = levels[i].Width;
      levels[i].Width = width + 1; prior = snapshot(args);
      CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG && snapshot(args) == prior);
      levels[i].Width = width;
    }
    prior = snapshot(args);
    f->failSurfaceAttempt = 2;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_OUTOFMEMORY && snapshot(args) == prior);
    CHECK(f->textureCreates == f->textureCloses && f->surfaceCreates == f->surfaceCloses);
    f->failSurfaceAttempt = 0;
    for (const HRESULT hr : {S_FALSE, E_OUTOFMEMORY}) {
      f->textureResult = hr;
      CHECK(f->table.pfnCreateResource(f->device, &args) == (hr == S_FALSE ? E_FAIL : hr));
      CHECK(snapshot(args) == prior && f->textureCreates == f->textureCloses && f->surfaceCreates == f->surfaceCloses);
    }
    f->textureResult = S_OK; f->nullTexture = true;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_FAIL && snapshot(args) == prior);
    f->nullTexture = false; f->nullSurface = true;
    CHECK(f->table.pfnCreateResource(f->device, &args) == E_FAIL && snapshot(args) == prior);
    f->nullSurface = false;
    f->descriptions.clear();
    f->queryHook = [&] {
      auto nested = textureArgs(reinterpret_cast<HANDLE>(UINT_PTR(2)), reinterpret_cast<D3DDDI_SURFACEINFO*>(UINT_PTR(1)), 3);
      CHECK(f->table.pfnCreateResource(f->device, &nested) == D3DERR_WASSTILLDRAWING);
      levels[0].Width = 999; levels[1].Height = 999;
      args.pSurfList = reinterpret_cast<D3DDDI_SURFACEINFO*>(UINT_PTR(1)); args.MipLevels = 1;
      CHECK(f->table.pfnSetTexture(f->device, 0, nullptr) == D3DERR_WASSTILLDRAWING);
    };
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE texture = args.hResource;
    CHECK(f->descriptions.size() == 3 && f->descriptions[0].width == 5
      && f->descriptions[1].width == 2 && f->descriptions[1].height == 1 && !f->descriptions[0].lockable);
    CHECK(texture != &cookie && texture != f->device);
    for (const UINT stage : {0u,15u,UINT(D3DVERTEXTEXTURESAMPLER0),UINT(D3DVERTEXTEXTURESAMPLER3)})
      CHECK(f->table.pfnSetTexture(f->device, stage, texture) == S_OK);
    for (const UINT stage : {16u,255u,256u,261u,UINT_MAX})
      CHECK(f->table.pfnSetTexture(f->device, stage, texture) == E_INVALIDARG);
    CHECK(f->table.pfnSetTexture(f->device, 0, &cookie) == E_INVALIDARG);
    CHECK(f->table.pfnSetTexture(f->device, 0, f->device) == E_INVALIDARG);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = texture;
    CHECK(f->table.pfnLock(f->device, &mapping) == E_INVALIDARG);
    const auto closes = f->textureCloses;
    f->stateResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDestroyResource(f->device, texture) == E_OUTOFMEMORY && f->textureCloses == closes);
    f->stateResult = S_OK; f->flushResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDestroyResource(f->device, texture) == E_OUTOFMEMORY && f->textureCloses == closes);
    CHECK(f->table.pfnSetTexture(f->device, 7, texture) == S_OK);
    f->flushResult = S_OK;
    CHECK(f->table.pfnDestroyResource(f->device, texture) == S_OK && f->textureCloses == closes + 1);
    CHECK(f->table.pfnSetTexture(f->device, 0, texture) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyResource(f->device, texture) == E_INVALIDARG);
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char srcCookie, dstCookie;
    std::array<std::vector<uint8_t>,3> bytes = {std::vector<uint8_t>(96,0xa1),std::vector<uint8_t>(16,0xb2),std::vector<uint8_t>(8,0xc3)};
    D3DDDI_SURFACEINFO srcLevels[3] = {{5,3,UINT_MAX,bytes[0].data(),32,UINT_MAX},
      {2,1,UINT_MAX,bytes[1].data(),16,UINT_MAX}, {1,1,UINT_MAX,bytes[2].data(),8,UINT_MAX}};
    D3DDDI_SURFACEINFO dstLevels[2] = {{2,1,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}, {1,1,UINT_MAX,nullptr,UINT_MAX,UINT_MAX}};
    auto source = textureArgs(&srcCookie, srcLevels, 3, true);
    auto destination = textureArgs(&dstCookie, dstLevels, 2);
    CHECK(f->table.pfnCreateResource(f->device, &source) == S_OK);
    CHECK(f->table.pfnCreateResource(f->device, &destination) == S_OK);
    CHECK(f->table.pfnSetTexture(f->device, 0, source.hResource) == E_INVALIDARG);
    D3DDDIARG_TEXBLT copy = {destination.hResource,source.hResource,UINT_MAX,{0,0},{0,0,5,3}};
    f->queryHook = [&] {
      for (auto& level : bytes) std::fill(level.begin(),level.end(),0xdd);
      copy.hSrcResource = nullptr; copy.SrcRect.right = 999; copy.DstPoint.x = 999;
      CHECK(f->table.pfnDestroyResource(f->device, destination.hResource) == D3DERR_WASSTILLDRAWING);
    };
    CHECK(f->table.pfnTexBlt(f->device, &copy) == S_OK);
    CHECK(f->copies == 2 && f->copyWidths == std::vector<UINT>({2,1}));
    CHECK(f->uploads == std::vector<std::vector<uint8_t>>({std::vector<uint8_t>(8,0xb2),std::vector<uint8_t>(4,0xc3)}));
    CHECK(f->copySources[0].right == 2 && f->copySources[1].right == 1);
    for (UINT i = 0; i < 2; ++i) {
      char readCookie; auto readback = resourceArgs(&readCookie, &dstLevels[i], 1);
      CHECK(f->table.pfnCreateResource(f->device, &readback) == S_OK);
      D3DDDIARG_BLT read = {}; read.hSrcResource = destination.hResource; read.SrcSubResourceIndex = i;
      read.hDstResource = readback.hResource; read.SrcRect = read.DstRect = {0,0,LONG(dstLevels[i].Width),1};
      CHECK(f->table.pfnBlt(f->device, &read) == S_OK);
      D3DDDIARG_LOCK mapping = {}; mapping.hResource = readback.hResource;
      CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
      for (UINT j = 0; j < dstLevels[i].Width * 4; ++j) CHECK(static_cast<uint8_t*>(mapping.pSurfData)[j] == (i ? 0xc3 : 0xb2));
      D3DDDIARG_UNLOCK unlock = {}; unlock.hResource = readback.hResource;
      CHECK(f->table.pfnUnlock(f->device, &unlock) == S_OK);
      CHECK(f->table.pfnDestroyResource(f->device, readback.hResource) == S_OK);
    }
    copy = {destination.hResource,source.hResource,0,{0,0},{0,0,5,3}};
    const auto count = f->copies;
    copy.DstPoint.x = 1;
    CHECK(f->table.pfnTexBlt(f->device, &copy) == E_INVALIDARG && f->copies == count);
    copy.DstPoint.x = 0; copy.SrcRect.left = -1;
    CHECK(f->table.pfnTexBlt(f->device, &copy) == E_INVALIDARG && f->copies == count);
    copy.SrcRect.left = 0;
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = source.hResource; mapping.SubResourceIndex = 1; mapping.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && mapping.Pitch == 16 && mapping.pSurfData == bytes[1].data());
    CHECK(f->table.pfnTexBlt(f->device, &copy) == E_INVALIDARG && f->copies == count);
    D3DDDIARG_UNLOCK unlock = {}; unlock.hResource = source.hResource; unlock.SubResourceIndex = 1; unlock.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnUnlock(f->device, &unlock) == S_OK);
    f->copyResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnTexBlt(f->device, &copy) == E_OUTOFMEMORY && f->copies == count + 1);
    f->copyResult = S_OK;
    CHECK(f->table.pfnTexBlt(f->device, &copy) == S_OK && f->copies == count + 3);
    CHECK(f->table.pfnSetTexture(f->device, 0, destination.hResource) == S_OK);
    // Device teardown unbinds textures before releasing any mip or parent.
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    const std::array<D3DDDITEXTURESTAGESTATETYPE,13> native = {D3DDDITSS_ADDRESSU,D3DDDITSS_ADDRESSV,D3DDDITSS_ADDRESSW,
      D3DDDITSS_BORDERCOLOR,D3DDDITSS_MAGFILTER,D3DDDITSS_MINFILTER,D3DDDITSS_MIPFILTER,D3DDDITSS_MIPMAPLODBIAS,
      D3DDDITSS_MAXMIPLEVEL,D3DDDITSS_MAXANISOTROPY,D3DDDITSS_SRGBTEXTURE,D3DDDITSS_ELEMENTINDEX,D3DDDITSS_DMAPOFFSET};
    const std::array<D3DSAMPLERSTATETYPE,13> publicStates = {D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_ADDRESSW,
      D3DSAMP_BORDERCOLOR,D3DSAMP_MAGFILTER,D3DSAMP_MINFILTER,D3DSAMP_MIPFILTER,D3DSAMP_MIPMAPLODBIAS,
      D3DSAMP_MAXMIPLEVEL,D3DSAMP_MAXANISOTROPY,D3DSAMP_SRGBTEXTURE,D3DSAMP_ELEMENTINDEX,D3DSAMP_DMAPOFFSET};
    for (UINT stage : {0u,15u,UINT(D3DVERTEXTEXTURESAMPLER0),UINT(D3DVERTEXTEXTURESAMPLER3)}) {
      for (size_t i = 0; i < native.size(); ++i) {
        D3DDDIARG_TEXTURESTAGESTATE args = {stage,native[i],0x80000123};
        CHECK(f->table.pfnSetTextureStageState(f->device, &args) == S_OK);
        CHECK(f->lastSampler && f->samplerState == publicStates[i] && f->textureStage == stage && f->textureValue == 0x80000123);
      }
    }
    D3DDDIARG_TEXTURESTAGESTATE args = {7,D3DDDITSS_TEXTURETRANSFORMFLAGS,0xdeadbeef};
    f->queryHook = [&] { args.Stage = 9; args.State = D3DDDITSS_MINFILTER; args.Value = 0; };
    CHECK(f->table.pfnSetTextureStageState(f->device, &args) == S_OK);
    CHECK(!f->lastSampler && f->textureState == D3DTSS_TEXTURETRANSFORMFLAGS && f->textureStage == 7 && f->textureValue == 0xdeadbeef);
    for (auto state : {D3DDDITSS_TEXTUREMAP,D3DDDITSS_DISABLETEXTURECOLORKEY,D3DDDITSS_TEXTURECOLORKEYVAL,D3DDDITSS_FORCE_DWORD}) {
      args = {0,state,1}; CHECK(f->table.pfnSetTextureStageState(f->device, &args) == E_INVALIDARG);
    }
    args = {8,D3DDDITSS_COLOROP,D3DTOP_MODULATE};
    CHECK(f->table.pfnSetTextureStageState(f->device, &args) == E_INVALIDARG);
    args = {D3DVERTEXTEXTURESAMPLER0,D3DDDITSS_COLOROP,D3DTOP_MODULATE};
    CHECK(f->table.pfnSetTextureStageState(f->device, &args) == E_INVALIDARG);
    args = {D3DDMAPSAMPLER,D3DDDITSS_MINFILTER,D3DTEXF_POINT};
    CHECK(f->table.pfnSetTextureStageState(f->device, &args) == E_INVALIDARG);
    f->stateResult = S_FALSE; args = {0,D3DDDITSS_MINFILTER,D3DTEXF_POINT};
    CHECK(f->table.pfnSetTextureStageState(f->device, &args) == E_FAIL);
    f->stateResult = S_OK; closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie; D3DDDI_SURFACEINFO level = {2,2,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = textureArgs(&cookie, &level, 1);
    const auto prior = snapshot(args);
    f->surfaceHook = [] { ++f->generation; };
    CHECK(f->table.pfnCreateResource(f->device, &args) == D3DERR_DEVICELOST && snapshot(args) == prior);
    CHECK(f->textureCreates == f->textureCloses && f->surfaceCreates == f->surfaceCloses);
    --f->generation;
    CHECK(f->table.pfnSetTexture(f->device, 0, nullptr) == D3DERR_DEVICELOST);
    closeDevice(); closeAdapter();
  }
  {
    Fixture a; initialize(a); createDevice();
    char cookie; D3DDDI_SURFACEINFO level = {2,2,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = textureArgs(&cookie, &level, 1, false, true);
    CHECK(a.table.pfnCreateResource(a.device, &args) == S_OK);
    D3DDDIARG_SETRENDERTARGET target = {0,args.hResource,0};
    CHECK(a.table.pfnSetRenderTarget(a.device, &target) == S_OK);
    CHECK(a.table.pfnSetTexture(a.device, D3DVERTEXTEXTURESAMPLER1, args.hResource) == S_OK);
    Fixture b; initialize(b); createDevice();
    CHECK(b.table.pfnSetTexture(b.device, 0, args.hResource) == E_INVALIDARG);
    CHECK(b.table.pfnDestroyResource(b.device, args.hResource) == E_INVALIDARG);
    closeDevice(); closeAdapter(); f = &a;
    f->stateResult = D3DERR_DEVICELOST;
    CHECK(a.table.pfnDestroyResource(a.device, args.hResource) == S_OK);
    closeDevice(); closeAdapter();
  }
}

static void dynamicTextureContracts() {
  for (const D3DFORMAT format : {D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8}) {
    for (const D3DDDI_POOL pool : {D3DDDIPOOL_VIDEOMEMORY, D3DDDIPOOL_LOCALVIDMEM,
        D3DDDIPOOL_NONLOCALVIDMEM, D3DDDIPOOL_SYSTEMMEM}) {
      Fixture fixture; initialize(fixture); createDevice();
      char cookie;
      D3DDDI_SURFACEINFO levels[3] = {{4,4,0,nullptr,0,0}, {2,2,0,nullptr,0,0}, {1,1,0,nullptr,0,0}};
      auto args = textureArgs(&cookie, levels, 3);
      args.Format = static_cast<D3DDDIFORMAT>(format); args.Pool = pool; args.Flags.Dynamic = 1;
      const auto valid = args;
      for (unsigned invalid = 0; invalid < 3; ++invalid) {
        args = valid;
        if (invalid == 0) args.Flags.Texture = 0;
        if (invalid == 1) args.Flags.RenderTarget = 1;
        if (invalid == 2) { args.Flags.Texture = 0; args.Flags.ZBuffer = 1; }
        const auto before = snapshot(args); const auto attempts = f->surfaceAttempts;
        CHECK(f->table.pfnCreateResource(f->device, &args) == E_INVALIDARG);
        CHECK(snapshot(args) == before && f->surfaceAttempts == attempts);
      }
      args = valid;
      // The private identity callback cannot rewrite dynamic usage or the
      // already captured mip dimensions before construction on the worker.
      f->queryHook = [&] { args.Flags.Dynamic = 0; args.MipLevels = UINT_MAX; levels[1].Width = 99; };
      CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
      const HANDLE texture = args.hResource;
      CHECK(f->descriptions.size() == 3);
      for (UINT level = 0; level < 3; ++level) {
        CHECK(f->descriptions[level].dynamic && f->descriptions[level].lockable);
        CHECK(f->descriptions[level].format == format && f->descriptions[level].width == (4u >> level));
      }
      D3DDDIARG_LOCK mapping = {}; mapping.hResource = texture;
      mapping.Flags.Discard = 1;
      const auto whole = mapping;
      auto rejected = [&](D3DDDIARG_LOCK bad) {
        bad.pSurfData = reinterpret_cast<void*>(UINT_PTR(1)); bad.Pitch = 73; bad.SlicePitch = 91;
        const auto before = snapshot(bad); const auto calls = f->surfaceLockFlags.size();
        CHECK(f->table.pfnLock(f->device, &bad) == E_INVALIDARG);
        CHECK(snapshot(bad) == before && f->surfaceLockFlags.size() == calls);
      };
      for (unsigned invalid = 0; invalid < 8; ++invalid) {
        auto bad = whole;
        if (invalid == 0) bad.Flags.ReadOnly = 1;
        if (invalid == 1) { bad.Flags.AreaValid = 1; bad.Area = {0,0,4,4}; }
        if (invalid == 2) bad.Flags.NoOverwrite = 1;
        if (invalid == 3) bad.Flags.RangeValid = 1;
        if (invalid == 4) bad.Flags.BoxValid = 1;
        if (invalid == 5) bad.Flags.NotifyOnly = 1;
        if (invalid == 6) bad.SubResourceIndex = 1;
        if (invalid == 7) bad.SubResourceIndex = 3;
        rejected(bad);
      }
      mapping = whole; mapping.Flags.DoNotWait = 1;
      CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
      CHECK(mapping.pSurfData && mapping.Pitch == 16 && !mapping.SlicePitch);
      CHECK(f->surfaceLockFlags.back() == (D3DLOCK_DISCARD | D3DLOCK_DONOTWAIT));
      std::array<uint8_t,64> expected;
      for (UINT i = 0; i < expected.size(); ++i)
        expected[i] = static_cast<uint8_t*>(mapping.pSurfData)[i] = uint8_t(i + 0x30);
      rejected(whole);
      CHECK(f->table.pfnDestroyResource(f->device, texture) == E_INVALIDARG);
      D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = texture;
      f->surfaceUnlockResult = E_OUTOFMEMORY;
      CHECK(f->table.pfnUnlock(f->device, &unmap) == E_OUTOFMEMORY);
      rejected(whole);
      f->surfaceUnlockResult = S_OK;
      CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
      mapping = {}; mapping.hResource = texture; mapping.Flags.ReadOnly = 1;
      CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
      CHECK(f->surfaceLockFlags.back() == D3DLOCK_READONLY);
      CHECK(!std::memcmp(mapping.pSurfData, expected.data(), expected.size()));
      CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
      // Discard of level zero cannot invalidate an outstanding lower map.
      mapping = {}; mapping.hResource = texture; mapping.SubResourceIndex = 1;
      CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && mapping.Pitch == 8);
      rejected(whole);
      unmap.SubResourceIndex = 1;
      CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
      for (const HRESULT failure : {S_FALSE, E_OUTOFMEMORY, DXGI_ERROR_WAS_STILL_DRAWING}) {
        f->surfaceLockResult = failure; mapping = whole;
        const auto before = snapshot(mapping);
        CHECK(f->table.pfnLock(f->device, &mapping) == (failure == S_FALSE ? E_FAIL
          : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure));
        CHECK(snapshot(mapping) == before && f->surfaceLockFlags.back() == D3DLOCK_DISCARD);
      }
      f->surfaceLockResult = S_OK; mapping = whole;
      CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
      unmap.SubResourceIndex = 0;
      CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
      const auto bindsBefore = f->textureSets;
      if (pool == D3DDDIPOOL_SYSTEMMEM) {
        // SYSTEMMEM textures remain transfer/lock resources even when
        // dynamic. Sampling them cannot reach the private renderer.
        CHECK(f->table.pfnSetTexture(f->device, 0, texture) == E_INVALIDARG);
        CHECK(f->textureSets == bindsBefore);
      } else {
        CHECK(f->table.pfnSetTexture(f->device, 0, texture) == S_OK);
        CHECK(f->textureSets == bindsBefore + 1);
      }
      CHECK(f->table.pfnDestroyResource(f->device, texture) == S_OK);
      CHECK(f->textureSets == bindsBefore + (pool == D3DDDIPOOL_SYSTEMMEM ? 0 : 2));
      CHECK(f->textureCreates == f->textureCloses && f->surfaceCreates == f->surfaceCloses);
      // Static SYSTEMMEM still rejects DISCARD without invoking the backend.
      levels[1].Width = 2; args = textureArgs(&cookie, levels, 3, true);
      CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
      auto bad = whole; bad.hResource = args.hResource; rejected(bad);
      mapping = {}; mapping.hResource = args.hResource; mapping.SubResourceIndex = 2;
      CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
      closeDevice(); CHECK(f->teardownDiscard); closeAdapter();
      CHECK(f->surfaceLocks == f->surfaceUnlocks && f->surfaceCreates == f->surfaceCloses);
    }
  }
}

static D3DDDIARG_CREATERESOURCE bufferArgs(HANDLE cookie, D3DDDI_SURFACEINFO* info,
    D3DFORMAT format = D3DFMT_VERTEXDATA) {
  auto args = resourceArgs(cookie, info, 1, true);
  args.Flags.Value = 0;
  args.Flags.VertexBuffer = format == D3DFMT_VERTEXDATA;
  args.Flags.IndexBuffer = format != D3DFMT_VERTEXDATA;
  args.Format = static_cast<D3DDDIFORMAT>(format);
  args.Fvf = format == D3DFMT_VERTEXDATA ? D3DFVF_XYZ : UINT_MAX;
  return args;
}

static void bufferTransferContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnBufBlt);
    char borrowedCookie, videoCookie, ownedCookie;
    std::array<uint8_t,64> external, expected;
    for (size_t i = 0; i < external.size(); ++i) expected[i] = external[i] = uint8_t(0xa0 + i);
    D3DDDI_SURFACEINFO info = {64,UINT_MAX,UINT_MAX,external.data(),UINT_MAX,UINT_MAX};
    auto borrowed = bufferArgs(&borrowedCookie, &info); borrowed.Pool = D3DDDIPOOL_SYSTEMMEM;
    auto invalid = borrowed;
    info.pSysMem = reinterpret_cast<void*>(UINTPTR_MAX);
    const auto prior = snapshot(invalid);
    CHECK(f->table.pfnCreateResource(f->device, &invalid) == E_INVALIDARG && snapshot(invalid) == prior);
    info.pSysMem = external.data();
    f->queryHook = [&] {
      auto nested = borrowed; nested.pSurfList = reinterpret_cast<D3DDDI_SURFACEINFO*>(UINT_PTR(1));
      CHECK(f->table.pfnCreateResource(f->device, &nested) == D3DERR_WASSTILLDRAWING);
      external.fill(0xdd); info.Width = 1; info.pSysMem = reinterpret_cast<void*>(UINT_PTR(1));
      borrowed.Pool = D3DDDIPOOL_VIDEOMEMORY;
    };
    CHECK(f->table.pfnCreateResource(f->device, &borrowed) == S_OK);
    CHECK(f->bufferDescriptions.back().systemMemory && f->bufferDescriptions.back().systemData == external.data()
      && f->bufferDescriptions.back().bytes == 64);
    auto read = [&](HANDLE resource, bool notify, const std::array<uint8_t,64>& bytes) {
      D3DDDIARG_LOCK mapping = {}; mapping.hResource = resource;
      mapping.Flags.ReadOnly = 1; mapping.Flags.NotifyOnly = notify;
      CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
      CHECK(mapping.Pitch == 0 && mapping.SlicePitch == 0);
      for (size_t i = 0; i < bytes.size(); ++i) CHECK(static_cast<uint8_t*>(mapping.pSurfData)[i] == bytes[i]);
      D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = resource; unmap.Flags.NotifyOnly = notify;
      CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    };
    read(borrowed.hResource,true,expected);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = borrowed.hResource;
    const auto unchanged = snapshot(mapping);
    CHECK(f->table.pfnLock(f->device,&mapping) == E_INVALIDARG && snapshot(mapping) == unchanged);
    mapping.Flags.NotifyOnly = mapping.Flags.RangeValid = 1; mapping.Range = {7,13};
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK && mapping.pSurfData == external.data() + 7);
    for (UINT i = 0; i < 13; ++i) expected[7+i] = external[7+i] = uint8_t(0x41+i);
    D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = borrowed.hResource;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == E_INVALIDARG);
    unmap.Flags.NotifyOnly = 1;
    f->queryHook = [&] {
      CHECK(f->table.pfnUnlock(f->device,&unmap) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyResource(f->device,borrowed.hResource) == D3DERR_WASSTILLDRAWING);
      external.fill(0xee); unmap.Flags.Value = 0; unmap.hResource = nullptr;
    };
    CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    read(borrowed.hResource,true,expected);
    mapping = {}; mapping.hResource = borrowed.hResource;
    mapping.Flags.NotifyOnly = mapping.Flags.RangeValid = 1; mapping.Range = {9,3};
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
    for (UINT i = 0; i < 3; ++i) expected[9+i] = external[9+i] = uint8_t(0x91+i);
    unmap = {}; unmap.hResource = borrowed.hResource; unmap.Flags.NotifyOnly = 1;
    f->bufferUnlockResult = S_FALSE;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == E_FAIL);
    CHECK(f->table.pfnDestroyResource(f->device,borrowed.hResource) == E_INVALIDARG);
    f->bufferUnlockResult = DXGI_ERROR_WAS_STILL_DRAWING;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == D3DERR_WASSTILLDRAWING);
    f->bufferUnlockResult = S_OK;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    read(borrowed.hResource,true,expected);

    info = {64,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto video = bufferArgs(&videoCookie,&info);
    video.Flags.NotLockable = video.Flags.WriteOnly = 1;
    CHECK(f->table.pfnCreateResource(f->device,&video) == S_OK);
    auto owned = bufferArgs(&ownedCookie,&info,D3DFMT_INDEX32); owned.Pool = D3DDDIPOOL_SYSTEMMEM;
    CHECK(f->table.pfnCreateResource(f->device,&owned) == S_OK && f->bufferDescriptions.back().systemMemory
      && !f->bufferDescriptions.back().systemData);
    D3DDDIARG_BUFFERBLT copy = {video.hResource,borrowed.hResource,11,{3,17}};
    const std::vector<uint8_t> copied(expected.begin()+3,expected.begin()+20);
    const auto count = f->bufferCopies;
    f->queryHook = [&] {
      CHECK(f->table.pfnBufBlt(f->device,&copy) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyResource(f->device,video.hResource) == D3DERR_WASSTILLDRAWING);
      external.fill(0xcc); copy = {nullptr,nullptr,UINT_MAX,{UINT_MAX,UINT_MAX}};
    };
    CHECK(f->table.pfnBufBlt(f->device,&copy) == S_OK);
    CHECK(f->bufferCopies == count+1 && f->bufferDestinationOffset == 11 && f->bufferSourceOffset == 3
      && f->bufferCopyBytes == 17 && f->bufferCopiedData == copied);
    std::array<uint8_t,64> destination; destination.fill(0x6d);
    for (UINT i = 0; i < 17; ++i) destination[11+i] = copied[i];
    // Raw buffer transfer includes VB-to-IB; public write-only/nonlockable
    // restrictions do not block an internal copy or its preserved guard bytes.
    copy = {owned.hResource,video.hResource,0,{0,64}};
    CHECK(f->table.pfnBufBlt(f->device,&copy) == S_OK);
    read(owned.hResource,false,destination);
    mapping = {}; mapping.hResource = video.hResource; mapping.Flags.ReadOnly = 1;
    CHECK(f->table.pfnLock(f->device,&mapping) == E_INVALIDARG);
    CHECK(f->table.pfnBufBlt(f->device,nullptr) == E_INVALIDARG);
    const auto valid = copy;
    for (UINT field = 0; field < 6; ++field) {
      copy = valid;
      if (field == 0) copy.hSrcResource = nullptr;
      if (field == 1) copy.hDstResource = nullptr;
      if (field == 2) copy.Offset = 1;
      if (field == 3) copy.SrcRange.Offset = 1;
      if (field == 4) copy.Offset = UINT_MAX;
      if (field == 5) copy.SrcRange = {UINT_MAX,2};
      const auto before = snapshot(copy); const auto calls = f->bufferCopies;
      CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG && snapshot(copy) == before
        && f->bufferCopies == calls);
    }
    copy = {owned.hResource,video.hResource,64,{64,0}};
    const auto calls = f->bufferCopies;
    CHECK(f->table.pfnBufBlt(f->device,&copy) == S_OK && f->bufferCopies == calls);
    copy.Offset = 65;
    CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG && f->bufferCopies == calls);
    copy = valid;
    mapping = {}; mapping.hResource = owned.hResource;
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
    CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG && f->bufferCopies == calls);
    unmap = {}; unmap.hResource = owned.hResource;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    copy = {video.hResource,borrowed.hResource,11,{3,17}};
    mapping = {}; mapping.hResource = borrowed.hResource; mapping.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
    CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG && f->bufferCopies == calls);
    unmap = {}; unmap.hResource = borrowed.hResource; unmap.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    for (const HRESULT failure : {S_FALSE,E_OUTOFMEMORY,DXGI_ERROR_WAS_STILL_DRAWING}) {
      f->bufferCopyResult = failure; copy = valid;
      CHECK(f->table.pfnBufBlt(f->device,&copy) == (failure == S_FALSE ? E_FAIL
        : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure));
      read(owned.hResource,false,destination);
    }
    f->bufferCopyResult = S_OK;
    // GPU-to-borrowed SYSTEMMEM must update only the requested byte range.
    external.fill(0xad); destination.fill(0xad);
    copy = {borrowed.hResource,video.hResource,2,{11,17}};
    CHECK(f->table.pfnBufBlt(f->device,&copy) == S_OK);
    for (UINT i = 0; i < 17; ++i) destination[2+i] = copied[i];
    CHECK(external == destination);
    char surfaceCookie; D3DDDI_SURFACEINFO surfaceInfo = {1,1,0,nullptr,0,0};
    auto surfaceArgs = resourceArgs(&surfaceCookie,&surfaceInfo,1,true);
    CHECK(f->table.pfnCreateResource(f->device,&surfaceArgs) == S_OK);
    copy = {owned.hResource,surfaceArgs.hResource,0,{0,1}};
    CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG);
    copy = {surfaceArgs.hResource,owned.hResource,0,{0,1}};
    CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG);
    Fixture other; initialize(other); createDevice();
    copy = {owned.hResource,video.hResource,0,{0,1}};
    CHECK(other.table.pfnBufBlt(other.device,&copy) == E_INVALIDARG);
    closeDevice(); closeAdapter(); f = &fixture;
    const HANDLE stale = video.hResource;
    CHECK(f->table.pfnDestroyResource(f->device,stale) == S_OK);
    copy = {owned.hResource,stale,0,{0,1}};
    CHECK(f->table.pfnBufBlt(f->device,&copy) == E_INVALIDARG);
    D3DDDIARG_SETSTREAMSOURCE stream = {0,borrowed.hResource,0,12};
    CHECK(f->table.pfnSetStreamSource(f->device,&stream) == S_OK);
    mapping = {}; mapping.hResource = borrowed.hResource; mapping.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
    f->teardownDiscard = false;
    closeDevice(); CHECK(f->teardownDiscard); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie; D3DDDI_SURFACEINFO info = {64,0,0,nullptr,0,0};
    auto buffer = bufferArgs(&cookie,&info); buffer.Pool = D3DDDIPOOL_SYSTEMMEM; buffer.Flags.Dynamic = 1;
    CHECK(f->table.pfnCreateResource(f->device,&buffer) == S_OK);
    std::array<uint8_t,64> expected;
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = buffer.hResource;
    CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
    for (UINT i = 0; i < 64; ++i) expected[i] = static_cast<uint8_t*>(mapping.pSurfData)[i] = uint8_t(i+0x10);
    D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = buffer.hResource;
    CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    for (const D3DDDIARG_BUFFERBLT args : {
        D3DDDIARG_BUFFERBLT{buffer.hResource,buffer.hResource,5,{0,17}},
        D3DDDIARG_BUFFERBLT{buffer.hResource,buffer.hResource,0,{7,19}},
        D3DDDIARG_BUFFERBLT{buffer.hResource,buffer.hResource,4,{4,31}}}) {
      const auto original = expected;
      for (UINT i = 0; i < args.SrcRange.Size; ++i) expected[args.Offset+i] = original[args.SrcRange.Offset+i];
      CHECK(f->table.pfnBufBlt(f->device,&args) == S_OK);
      mapping.Flags.ReadOnly = 1;
      CHECK(f->table.pfnLock(f->device,&mapping) == S_OK);
      for (UINT i = 0; i < 64; ++i) CHECK(static_cast<uint8_t*>(mapping.pSurfData)[i] == expected[i]);
      CHECK(f->table.pfnUnlock(f->device,&unmap) == S_OK);
    }
    ++f->generation;
    CHECK(f->table.pfnFlush(f->device) == D3DERR_DEVICELOST);
    D3DDDIARG_BUFFERBLT copy = {buffer.hResource,buffer.hResource,0,{0,1}};
    CHECK(f->table.pfnBufBlt(f->device,&copy) == D3DERR_DEVICELOST);
    auto create = buffer; create.pSurfList = reinterpret_cast<D3DDDI_SURFACEINFO*>(UINT_PTR(1));
    CHECK(f->table.pfnCreateResource(f->device,&create) == D3DERR_DEVICELOST);
    closeDevice(); closeAdapter();
  }
}

static void bufferContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnSetStreamSource && f->table.pfnSetIndices && f->table.pfnDrawIndexedPrimitive);
    char cookie, lockedCookie, staticCookie, indexCookie;
    D3DDDI_SURFACEINFO info = {64,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = bufferArgs(&cookie, &info);
    const auto original = args;
    auto reject = [&](HRESULT hr) {
      const auto prior = snapshot(args);
      CHECK(f->table.pfnCreateResource(f->device, &args) == hr && snapshot(args) == prior);
    };
    for (const UINT flags : {UINT(0x180000),UINT(0x80001),UINT(0x90000),UINT(0x80002),UINT(0x80010)}) {
      args.Flags.Value = flags; reject(E_INVALIDARG);
    }
    args = original; args.SurfCount = 2; reject(E_INVALIDARG);
    args = original; args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_INDEX16); reject(E_INVALIDARG);
    args = original;
    for (const UINT bytes : {UINT(0),UINT_MAX,UINT_MAX-254}) {
      info.Width = bytes; reject(E_INVALIDARG);
    }
    info.Width = 64; info.pSysMem = reinterpret_cast<void*>(UINT_PTR(1)); reject(E_INVALIDARG);
    info.pSysMem = nullptr;
    args = bufferArgs(&indexCookie, &info, D3DFMT_INDEX16);
    info.Width = 3; reject(E_INVALIDARG);
    args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_INDEX32); info.Width = 6; reject(E_INVALIDARG);
    args.Format = static_cast<D3DDDIFORMAT>(D3DFMT_A8R8G8B8); info.Width = 64; reject(E_INVALIDARG);
    args = original;
    for (const HRESULT hr : {S_FALSE,E_FAIL,E_OUTOFMEMORY,D3DERR_NOTAVAILABLE}) {
      f->bufferResult = hr; reject(hr == S_FALSE ? E_FAIL : hr);
    }
    f->bufferResult = S_OK; f->nullBuffer = true; reject(E_FAIL); f->nullBuffer = false;
    args.Flags.Dynamic = args.Flags.WriteOnly = 1;
    f->queryHook = [&] {
      auto nested = original; nested.pSurfList = reinterpret_cast<D3DDDI_SURFACEINFO*>(UINT_PTR(1));
      CHECK(f->table.pfnCreateResource(f->device, &nested) == D3DERR_WASSTILLDRAWING);
      info.Width = 8; args.Fvf = 0; args.Flags.Value = 0; args.hResource = nullptr;
    };
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE dynamic = args.hResource;
    CHECK(f->bufferDescriptions.size() == 1 && f->bufferDescriptions[0].bytes == 64
      && f->bufferDescriptions[0].fvf == D3DFVF_XYZ && f->bufferDescriptions[0].dynamic
      && f->bufferDescriptions[0].writeOnly && !f->bufferDescriptions[0].index);
    info.Width = 64; args = original; reject(E_INVALIDARG);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = dynamic;
    mapping.pSurfData = reinterpret_cast<void*>(UINT_PTR(0x1234));
    mapping.Pitch = mapping.SlicePitch = UINT_MAX;
    auto rejectLock = [&] {
      const auto prior = snapshot(mapping);
      CHECK(f->table.pfnLock(f->device, &mapping) == E_INVALIDARG && snapshot(mapping) == prior);
    };
    for (const UINT flags : {UINT(1),UINT(3),UINT(0xc),UINT(9),UINT(0x20),UINT(0x40),UINT(0x80),UINT(0x100),UINT(0x400)}) {
      mapping.Flags.Value = flags; rejectLock();
    }
    mapping.Flags.Value = 0x10;
    for (const D3DDDIRANGE range : {D3DDDIRANGE{0,0},D3DDDIRANGE{63,2},D3DDDIRANGE{UINT_MAX,2}}) {
      mapping.Range = range; rejectLock();
    }
    mapping.Range = {8,16}; mapping.SubResourceIndex = 1; rejectLock(); mapping.SubResourceIndex = 0;
    mapping.Flags.Value = 0x10 | 8 | 2 | 0x200;
    f->queryHook = [&] { mapping.Range = {0,1}; mapping.Flags.Value = 0; };
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && mapping.pSurfData
      && mapping.Pitch == 0 && mapping.SlicePitch == 0);
    CHECK(f->bufferOffset == 8 && f->bufferBytes == 16 && f->bufferFlags == (D3DLOCK_DISCARD | D3DLOCK_DONOTWAIT));
    std::memset(mapping.pSurfData,0x92,16);
    rejectLock();
    CHECK(f->table.pfnDestroyResource(f->device, dynamic) == E_INVALIDARG);
    D3DDDIARG_SETSTREAMSOURCE stream = {0,dynamic,0,12};
    CHECK(f->table.pfnSetStreamSource(f->device, &stream) == E_INVALIDARG);
    D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = dynamic;
    unmap.Flags.NotifyOnly = 1;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_INVALIDARG); unmap.Flags.Value = 0;
    unmap.SubResourceIndex = 1;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_INVALIDARG); unmap.SubResourceIndex = 0;
    f->bufferUnlockResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_OUTOFMEMORY);
    rejectLock(); f->bufferUnlockResult = S_OK;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    CHECK(f->table.pfnUnlock(f->device, &unmap) == E_INVALIDARG);
    mapping.Flags.Value = 0; mapping.Range = {UINT_MAX,UINT_MAX};
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && f->bufferOffset == 0 && f->bufferBytes == 64);
    const auto bytes = static_cast<const uint8_t*>(mapping.pSurfData);
    for (UINT i = 0; i < 64; ++i) CHECK(bytes[i] == (i >= 8 && i < 24 ? 0x92 : 0x6d));
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    mapping.Flags.Value = 4;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && f->bufferFlags == D3DLOCK_NOOVERWRITE);
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    mapping.Flags.Value = 0;
    const auto prior = snapshot(mapping);
    f->badBufferMapping = true;
    CHECK(f->table.pfnLock(f->device, &mapping) == E_FAIL && snapshot(mapping) == prior);
    f->bufferUnlockResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnLock(f->device, &mapping) == E_FAIL && snapshot(mapping) == prior);
    f->badBufferMapping = false; rejectLock(); f->bufferUnlockResult = S_OK;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);

    args = bufferArgs(&staticCookie, &info); args.Flags.HintStatic = 1;
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    const HANDLE staticBuffer = args.hResource;
    mapping = {}; mapping.hResource = staticBuffer;
    for (const UINT flags : {UINT(4),UINT(8)}) { mapping.Flags.Value = flags; rejectLock(); }
    mapping.Flags.Value = 1;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && f->bufferFlags == D3DLOCK_READONLY);
    unmap.hResource = staticBuffer;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    args = bufferArgs(&lockedCookie, &info); args.Flags.NotLockable = 1;
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    mapping.hResource = args.hResource; rejectLock();
    CHECK(f->table.pfnDestroyResource(f->device, args.hResource) == S_OK);
    args = bufferArgs(&indexCookie, &info, D3DFMT_INDEX32);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK && f->bufferDescriptions.back().fvf == 0);
    // Close drains remaining mapped and bound buffers on the worker.
    D3DDDIARG_SETINDICES indices = {args.hResource,4};
    CHECK(f->table.pfnSetIndices(f->device, &indices) == S_OK);
    stream = {15,dynamic,0,12}; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    mapping = {}; mapping.hResource = staticBuffer;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
    closeDevice();
    CHECK(f->bufferCreates == f->bufferCloses && f->bufferLocks == f->bufferUnlocks - 2);
    closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char positionCookie, colorCookie, index16Cookie, index32Cookie, targetCookie;
    D3DDDI_SURFACEINFO positionInfo = {96,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    D3DDDI_SURFACEINFO colorInfo = {32,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    D3DDDI_SURFACEINFO indexInfo = {10,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = bufferArgs(&positionCookie, &positionInfo);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK); const HANDLE position = args.hResource;
    args = bufferArgs(&colorCookie, &colorInfo);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK); const HANDLE color = args.hResource;
    args = bufferArgs(&index16Cookie, &indexInfo, D3DFMT_INDEX16);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK); const HANDLE index16 = args.hResource;
    indexInfo.Width = 20; args = bufferArgs(&index32Cookie, &indexInfo, D3DFMT_INDEX32);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK); const HANDLE index32 = args.hResource;
    D3DDDI_SURFACEINFO targetInfo = {8,8,1,nullptr,0,0}; args = resourceArgs(&targetCookie, &targetInfo, 1, true);
    CHECK(f->table.pfnCreateResource(f->device, &args) == S_OK);
    D3DDDIARG_SETRENDERTARGET target = {0,args.hResource,0};
    CHECK(f->table.pfnSetRenderTarget(f->device, &target) == S_OK);
    D3DDDIVERTEXELEMENT elements[] = {{3,4,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
      {7,0,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}, {15,4,D3DDECLTYPE_FLOAT1,0,D3DDECLUSAGE_TEXCOORD,0}};
    D3DDDIARG_CREATEVERTEXSHADERDECL declaration = {3,nullptr};
    CHECK(f->table.pfnCreateVertexShaderDecl(f->device, &declaration, elements) == S_OK);
    CHECK(f->table.pfnSetVertexShaderDecl(f->device, declaration.ShaderHandle) == S_OK);
    D3DDDIARG_SETSTREAMSOURCE stream = {3,position,8,24};
    CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    stream = {15,position,8,24}; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    D3DDDIARG_DRAWPRIMITIVE draw = {D3DPT_TRIANGLELIST,1,1};
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG);
    stream = {7,color,4,8}; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == S_OK);
    CHECK(f->drawType == D3DPT_TRIANGLELIST && f->drawStart == 1 && f->drawCount == 1 && f->draws == 1);
    // Last color element ends at byte32; unused final stride padding is absent.
    const auto originalStream = stream;
    for (const D3DDDIARG_SETSTREAMSOURCE invalid : {D3DDDIARG_SETSTREAMSOURCE{16,color,0,8},
        D3DDDIARG_SETSTREAMSOURCE{7,index16,0,8}, D3DDDIARG_SETSTREAMSOURCE{7,color,32,8},
        D3DDDIARG_SETSTREAMSOURCE{7,color,0,0}, D3DDDIARG_SETSTREAMSOURCE{7,&positionCookie,0,8}}) {
      CHECK(f->table.pfnSetStreamSource(f->device, &invalid) == E_INVALIDARG);
    }
    stream.Stride = 3; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG);
    stream = originalStream; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    f->stateResult = E_OUTOFMEMORY; stream.Offset = 8;
    CHECK(f->table.pfnSetStreamSource(f->device, &stream) == E_OUTOFMEMORY); f->stateResult = S_OK;
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == S_OK);
    f->queryHook = [&] { draw.VStart = UINT_MAX; draw.PrimitiveCount = UINT_MAX; };
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == S_OK && f->drawStart == 1 && f->drawCount == 1);
    draw = {D3DPT_TRIANGLELIST,1,2}; const auto draws = f->draws;
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG && f->draws == draws);
    draw = {D3DPT_TRIANGLELIST,0,UINT_MAX};
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG && f->draws == draws);
    draw = {D3DPT_POINTLIST,2,UINT_MAX};
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG && f->draws == draws);
    D3DDDIARG_SETINDICES indices = {index16,2};
    CHECK(f->table.pfnSetIndices(f->device, &indices) == S_OK);
    for (const D3DDDIARG_SETINDICES invalid : {D3DDDIARG_SETINDICES{position,2},
        D3DDDIARG_SETINDICES{index16,4},D3DDDIARG_SETINDICES{&index16Cookie,2}}) {
      CHECK(f->table.pfnSetIndices(f->device, &invalid) == E_INVALIDARG);
    }
    D3DDDIARG_DRAWINDEXEDPRIMITIVE indexed = {D3DPT_TRIANGLELIST,-3,4,3,2,1};
    f->queryHook = [&] { indexed = {D3DPT_POINTLIST,INT_MIN,0,UINT_MAX,UINT_MAX,UINT_MAX}; };
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == S_OK);
    CHECK(f->drawType == D3DPT_TRIANGLELIST && f->drawBase == -3 && f->drawMinimum == 4
      && f->drawVertexCount == 3 && f->drawStart == 2 && f->drawCount == 1);
    const D3DDDIARG_DRAWINDEXEDPRIMITIVE originalIndexed = {D3DPT_TRIANGLELIST,-3,4,3,2,1};
    for (const D3DDDIARG_DRAWINDEXEDPRIMITIVE invalid : {
        D3DDDIARG_DRAWINDEXEDPRIMITIVE{D3DPT_TRIANGLELIST,-5,4,3,2,1},
        D3DDDIARG_DRAWINDEXEDPRIMITIVE{D3DPT_TRIANGLELIST,-3,4,4,2,1},
        D3DDDIARG_DRAWINDEXEDPRIMITIVE{D3DPT_TRIANGLELIST,-3,4,3,3,1},
        D3DDDIARG_DRAWINDEXEDPRIMITIVE{D3DPT_TRIANGLELIST,-3,4,0,2,1},
        D3DDDIARG_DRAWINDEXEDPRIMITIVE{D3DPT_POINTLIST,INT_MAX,UINT_MAX,UINT_MAX,0,1},
        D3DDDIARG_DRAWINDEXEDPRIMITIVE{D3DPT_TRIANGLELIST,0,0,3,UINT_MAX,1}}) {
      const auto prior = f->draws;
      CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &invalid) == E_INVALIDARG && f->draws == prior);
    }
    f->stateResult = E_OUTOFMEMORY; indices = {index32,4};
    CHECK(f->table.pfnSetIndices(f->device, &indices) == E_OUTOFMEMORY); f->stateResult = S_OK;
    indexed = originalIndexed; CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == S_OK);
    CHECK(f->table.pfnSetIndices(f->device, &indices) == S_OK);
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == S_OK);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = index32;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
    CHECK(f->table.pfnSetIndices(f->device, &indices) == E_INVALIDARG);
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == E_INVALIDARG);
    D3DDDIARG_UNLOCK unmap = {}; unmap.hResource = index32;
    CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    mapping.hResource = position; CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
    draw = {D3DPT_TRIANGLELIST,1,1};
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == E_INVALIDARG);
    unmap.hResource = position; CHECK(f->table.pfnUnlock(f->device, &unmap) == S_OK);
    f->drawResult = S_FALSE;
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == E_FAIL); f->drawResult = S_OK;
    indexed.PrimitiveCount = indexed.NumVertices = 0; indexed.StartIndex = 5;
    const auto noOpDraws = f->draws;
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &indexed) == S_OK && f->draws == noOpDraws);
    indices = {nullptr,UINT_MAX}; CHECK(f->table.pfnSetIndices(f->device, &indices) == S_OK);
    CHECK(f->table.pfnDrawIndexedPrimitive(f->device, &originalIndexed) == E_INVALIDARG);
    f->stateResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDestroyResource(f->device, position) == E_OUTOFMEMORY); f->stateResult = S_OK;
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == S_OK);
    f->flushResult = E_OUTOFMEMORY;
    CHECK(f->table.pfnDestroyResource(f->device, position) == E_OUTOFMEMORY); f->flushResult = S_OK;
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG);
    stream = {3,position,8,24}; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    stream.Stream = 15; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    // Replacing stream0 with caller storage retires its backend binding.
    stream = {0,position,0,12}; CHECK(f->table.pfnSetStreamSource(f->device, &stream) == S_OK);
    std::array<float,12> user = {};
    D3DDDIARG_SETSTREAMSOURCEUM userStream = {0,12};
    CHECK(f->table.pfnSetStreamSourceUm(f->device, &userStream, user.data()) == S_OK);
    CHECK(f->table.pfnDrawPrimitive(f->device, &draw, nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnSetStreamSourceUm(f->device, &userStream, nullptr) == S_OK);
    CHECK(f->table.pfnDestroyResource(f->device, position) == S_OK);
    CHECK(f->table.pfnSetStreamSource(f->device, &stream) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyResource(f->device, position) == E_INVALIDARG);
    indices = {index32,4}; CHECK(f->table.pfnSetIndices(f->device, &indices) == S_OK);
    CHECK(f->table.pfnDestroyResource(f->device, index32) == S_OK);
    CHECK(f->table.pfnSetIndices(f->device, &indices) == E_INVALIDARG);
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie; D3DDDI_SURFACEINFO info = {32,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = bufferArgs(&cookie, &info); const auto prior = snapshot(args);
    f->bufferHook = [] { ++f->generation; };
    CHECK(f->table.pfnCreateResource(f->device, &args) == D3DERR_DEVICELOST && snapshot(args) == prior);
    CHECK(f->bufferCreates == f->bufferCloses);
    --f->generation;
    CHECK(f->table.pfnCreateResource(f->device, &args) == D3DERR_DEVICELOST);
    closeDevice(); closeAdapter();
  }
  {
    Fixture a; initialize(a); createDevice();
    char cookie; D3DDDI_SURFACEINFO info = {32,UINT_MAX,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
    auto args = bufferArgs(&cookie, &info);
    CHECK(a.table.pfnCreateResource(a.device, &args) == S_OK);
    D3DDDIARG_SETSTREAMSOURCE stream = {0,args.hResource,0,12};
    CHECK(a.table.pfnSetStreamSource(a.device, &stream) == S_OK);
    Fixture b; initialize(b); createDevice();
    CHECK(b.table.pfnSetStreamSource(b.device, &stream) == E_INVALIDARG);
    D3DDDIARG_SETINDICES indices = {args.hResource,2};
    CHECK(b.table.pfnSetIndices(b.device, &indices) == E_INVALIDARG);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = args.hResource;
    const auto prior = snapshot(mapping);
    CHECK(b.table.pfnLock(b.device, &mapping) == E_INVALIDARG && snapshot(mapping) == prior);
    CHECK(b.table.pfnDestroyResource(b.device, args.hResource) == E_INVALIDARG);
    closeDevice(); closeAdapter(); f = &a;
    closeDevice(); closeAdapter();
  }
}

static void fixedFunctionContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnSetTransform && f->table.pfnMultiplyTransform && f->table.pfnSetMaterial
      && f->table.pfnCreateLight && f->table.pfnSetLight && f->table.pfnDestroyLight);
    CHECK(f->table.pfnSetTransform(f->device,nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnMultiplyTransform(f->device,nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnSetMaterial(f->device,nullptr) == E_INVALIDARG);
    D3DDDIARG_SETTRANSFORM transform = {};
    for (unsigned row = 0; row < 4; ++row)
      for (unsigned column = 0; column < 4; ++column)
        transform.Matrix.m[row][column] = float(row * 4 + column) * 0.25f - 2.0f;
    const auto matrix = transform.Matrix;
    for (const auto type : {D3DTS_VIEW,D3DTS_PROJECTION,D3DTS_TEXTURE0,D3DTS_TEXTURE7,
        D3DTS_WORLD,D3DTS_WORLDMATRIX(255)}) {
      transform.TransformType = type; transform.Matrix = matrix;
      f->queryHook = [&] { transform.TransformType = D3DTS_WORLD; transform.Matrix = {}; };
      CHECK(f->table.pfnSetTransform(f->device,&transform) == S_OK);
      CHECK(f->transformState == type && snapshot(f->transform) == snapshot(matrix) && !f->multiplyTransform);
      D3DDDIARG_MULTIPLYTRANSFORM multiply = {type,matrix};
      f->queryHook = [&] { multiply.TransformType = D3DTS_VIEW; multiply.Matrix = {}; };
      CHECK(f->table.pfnMultiplyTransform(f->device,&multiply) == S_OK);
      CHECK(f->transformState == type && snapshot(f->transform) == snapshot(matrix) && f->multiplyTransform);
    }
    const auto sets = f->transformSets;
    for (const UINT value : {0u,1u,4u,15u,24u,255u,512u,UINT_MAX}) {
      transform.TransformType = static_cast<D3DTRANSFORMSTATETYPE>(value);
      CHECK(f->table.pfnSetTransform(f->device,&transform) == E_INVALIDARG);
      D3DDDIARG_MULTIPLYTRANSFORM multiply = {transform.TransformType,matrix};
      CHECK(f->table.pfnMultiplyTransform(f->device,&multiply) == E_INVALIDARG);
    }
    CHECK(f->transformSets == sets);
    D3DDDIARG_SETMATERIAL material = {{.1f,.2f,.3f,.4f},{.5f,.6f,.7f,.8f},
      {.9f,1.0f,1.1f,1.2f},{1.3f,1.4f,1.5f,1.6f},17.5f};
    const auto expected = material;
    f->queryHook = [&] { material = {}; };
    CHECK(f->table.pfnSetMaterial(f->device,&material) == S_OK);
    CHECK(snapshot(f->material) == snapshot(expected));
    for (const HRESULT failure : {S_FALSE,E_FAIL,DXGI_ERROR_WAS_STILL_DRAWING}) {
      const HRESULT result = failure == S_FALSE ? E_FAIL
        : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure;
      f->fixedResult = failure;
      transform = {D3DTS_WORLD,matrix};
      CHECK(f->table.pfnSetTransform(f->device,&transform) == result);
      CHECK(f->table.pfnSetMaterial(f->device,&material) == result);
      CHECK(snapshot(f->material) == snapshot(expected));
    }
    f->fixedResult = S_OK;
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnCreateLight(f->device,nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnSetLight(f->device,nullptr,nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyLight(f->device,nullptr) == E_INVALIDARG);
    const auto ignored = reinterpret_cast<const D3DDDI_LIGHT*>(UINT_PTR(1));
    UINT slot = 0;
    for (const UINT index : {0u,23u,0x80000000u,UINT_MAX}) {
      D3DDDIARG_CREATELIGHT create = {index};
      CHECK(f->table.pfnCreateLight(f->device,&create) == S_OK && f->lightSlot == slot);
      CHECK(f->lights[slot].Type == D3DLIGHT_DIRECTIONAL && !f->lightsEnabled[slot]
        && f->lights[slot].Diffuse.r == 1 && f->lights[slot].Diffuse.g == 1
        && f->lights[slot].Diffuse.b == 1 && f->lights[slot].Diffuse.a == 0 && f->lights[slot].Direction.z == 1);
      CHECK(f->table.pfnCreateLight(f->device,&create) == E_INVALIDARG);
      for (const auto type : {D3DLIGHT_POINT,D3DLIGHT_SPOT,D3DLIGHT_DIRECTIONAL}) {
        D3DDDI_LIGHT properties = {type,{.1f,.2f,.3f,.4f},{.5f,.6f,.7f,.8f},
          {.9f,1.0f,1.1f,1.2f},{2,3,4},{5,6,7},8,9,10,11,12,13,14};
        const auto expected = properties;
        D3DDDIARG_SETLIGHT args = {index,D3DDDI_SETLIGHT_DATA};
        f->queryHook = [&] {
          // Serialization must reject reentry before inspecting a pointed light.
          CHECK(f->table.pfnSetLight(f->device,&args,ignored) == D3DERR_WASSTILLDRAWING);
          CHECK(f->table.pfnCreateLight(f->device,&create) == D3DERR_WASSTILLDRAWING);
          D3DDDIARG_DESTROYLIGHT destroy = {index};
          CHECK(f->table.pfnDestroyLight(f->device,&destroy) == D3DERR_WASSTILLDRAWING);
          properties = {}; args = {12345,D3DDDI_SETLIGHT_DISABLE};
        };
        CHECK(f->table.pfnSetLight(f->device,&args,&properties) == S_OK);
        CHECK(f->lightSlot == slot && snapshot(f->lights[slot]) == snapshot(expected)
          && !f->lightsEnabled[slot]);
      }
      D3DDDIARG_SETLIGHT args = {index,D3DDDI_SETLIGHT_ENABLE};
      CHECK(f->table.pfnSetLight(f->device,&args,ignored) == S_OK && f->lightsEnabled[slot]);
      args.DataType = D3DDDI_SETLIGHT_DATA;
      D3DDDI_LIGHT properties = {}; properties.Type = D3DLIGHT_DIRECTIONAL;
      CHECK(f->table.pfnSetLight(f->device,&args,&properties) == S_OK && f->lightsEnabled[slot]);
      const auto last = snapshot(f->lights[slot]);
      CHECK(f->table.pfnSetLight(f->device,&args,nullptr) == E_INVALIDARG);
      CHECK(f->table.pfnSetLight(f->device,&args,
        reinterpret_cast<const D3DDDI_LIGHT*>(UINTPTR_MAX)) == E_INVALIDARG);
      for (const UINT type : {0u,4u,UINT_MAX}) {
        properties.Type = static_cast<D3DLIGHTTYPE>(type);
        CHECK(f->table.pfnSetLight(f->device,&args,&properties) == E_INVALIDARG);
      }
      for (const UINT type : {3u,4u,UINT_MAX}) {
        args.DataType = static_cast<D3DDDI_SETLIGHT_TYPE>(type);
        CHECK(f->table.pfnSetLight(f->device,&args,ignored) == E_INVALIDARG);
      }
      CHECK(snapshot(f->lights[slot]) == last && f->lightsEnabled[slot]);
      args = {index,D3DDDI_SETLIGHT_DISABLE};
      f->lightEnableResult = S_FALSE;
      CHECK(f->table.pfnSetLight(f->device,&args,ignored) == E_FAIL && f->lightsEnabled[slot]);
      f->lightEnableResult = S_OK;
      CHECK(f->table.pfnSetLight(f->device,&args,ignored) == S_OK && !f->lightsEnabled[slot]);
      ++slot;
    }
    D3DDDIARG_SETLIGHT absent = {12345,D3DDDI_SETLIGHT_DATA};
    CHECK(f->table.pfnSetLight(f->device,&absent,ignored) == E_INVALIDARG);
    D3DDDIARG_DESTROYLIGHT destroy = {12345};
    CHECK(f->table.pfnDestroyLight(f->device,&destroy) == E_INVALIDARG);
    destroy.Index = 23;
    D3DDDIARG_SETLIGHT enable = {23,D3DDDI_SETLIGHT_ENABLE};
    CHECK(f->table.pfnSetLight(f->device,&enable,nullptr) == S_OK);
    f->lightEnableResult = DXGI_ERROR_WAS_STILL_DRAWING;
    CHECK(f->table.pfnDestroyLight(f->device,&destroy) == D3DERR_WASSTILLDRAWING && f->lightsEnabled[1]);
    f->lightEnableResult = S_OK;
    CHECK(f->table.pfnDestroyLight(f->device,&destroy) == S_OK && !f->lightsEnabled[1]);
    CHECK(f->table.pfnSetLight(f->device,&enable,ignored) == E_INVALIDARG);
    CHECK(f->table.pfnDestroyLight(f->device,&destroy) == E_INVALIDARG);
    D3DDDIARG_CREATELIGHT recreate = {23};
    CHECK(f->table.pfnCreateLight(f->device,&recreate) == S_OK && f->lightSlot == 1
      && f->lights[1].Diffuse.r == 1 && !f->lightsEnabled[1]);
    CHECK(f->table.pfnSetLight(f->device,&enable,nullptr) == S_OK);
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    D3DDDIARG_CREATELIGHT create = {UINT_MAX};
    for (const HRESULT failure : {S_FALSE,E_FAIL,DXGI_ERROR_WAS_STILL_DRAWING}) {
      f->lightResult = failure;
      const HRESULT expected = failure == S_FALSE ? E_FAIL
        : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure;
      CHECK(f->table.pfnCreateLight(f->device,&create) == expected);
      D3DDDIARG_SETLIGHT bind = {UINT_MAX,D3DDDI_SETLIGHT_ENABLE};
      CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == E_INVALIDARG);
    }
    f->lightResult = S_OK; f->throwLight = true;
    CHECK(f->table.pfnCreateLight(f->device,&create) == E_OUTOFMEMORY);
    f->throwLight = false;
    CHECK(f->table.pfnCreateLight(f->device,&create) == S_OK && f->lightSlot == 0);
    for (UINT index = 1; index <= 8; ++index) {
      create.Index = index * 101;
      CHECK(f->table.pfnCreateLight(f->device,&create) == S_OK && f->lightSlot == index);
    }
    D3DDDIARG_SETLIGHT bind = {UINT_MAX,D3DDDI_SETLIGHT_ENABLE};
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == S_OK);
    for (UINT index = 1; index <= 7; ++index) {
      bind.Index = index * 101;
      CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == S_OK);
      CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == S_OK);
    }
    bind.Index = 808;
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == D3DERR_INVALIDCALL && !f->lightsEnabled[8]);
    bind = {UINT_MAX,D3DDDI_SETLIGHT_DISABLE};
    f->lightEnableResult = S_FALSE;
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == E_FAIL);
    bind = {808,D3DDDI_SETLIGHT_ENABLE};
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == D3DERR_INVALIDCALL);
    f->lightEnableResult = S_OK;
    bind = {UINT_MAX,D3DDDI_SETLIGHT_DISABLE};
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == S_OK);
    bind = {808,D3DDDI_SETLIGHT_ENABLE};
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == S_OK && f->lightsEnabled[8]);
    D3DDDIARG_DESTROYLIGHT destroy = {101};
    CHECK(f->table.pfnDestroyLight(f->device,&destroy) == S_OK && !f->lightsEnabled[1]);
    create.Index = 909;
    CHECK(f->table.pfnCreateLight(f->device,&create) == S_OK && f->lightSlot == 1);
    bind = {909,D3DDDI_SETLIGHT_ENABLE};
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == S_OK);
    // Losing the owner epoch must reject before pointed light preparation.
    ++f->generation;
    CHECK(f->table.pfnSetLight(f->device,&bind,nullptr) == D3DERR_DEVICELOST);
    bind.DataType = D3DDDI_SETLIGHT_DATA;
    CHECK(f->table.pfnSetLight(f->device,&bind,reinterpret_cast<const D3DDDI_LIGHT*>(UINT_PTR(1)))
      == D3DERR_DEVICELOST);
    closeDevice(); closeAdapter();
  }
}

static void clipPlaneContracts() {
  Fixture fixture; initialize(fixture); createDevice();
  CHECK(f->table.pfnSetClipPlane);
  D3DDDIARG_SETCLIPPLANE args = {0,{1.0f,-2.0f,3.0f,-4.0f}};
  const unsigned initialQueries = f->queries;
  const auto initialPlanes = snapshot(f->clipPlanes);
  CHECK(f->table.pfnSetClipPlane(f->device,nullptr) == E_INVALIDARG);
  for (const UINT index : {6u,7u,UINT_MAX}) {
    args.Index = index;
    const auto before = snapshot(args);
    CHECK(f->table.pfnSetClipPlane(f->device,&args) == E_INVALIDARG && snapshot(args) == before);
  }
  args.Index = 0;
  CHECK(f->table.pfnSetClipPlane(nullptr,&args) == E_INVALIDARG);
  CHECK(f->table.pfnSetClipPlane(reinterpret_cast<HANDLE>(UINT_PTR(0xfeed)),&args) == E_INVALIDARG);
  CHECK(!f->clipPlaneSets && f->queries == initialQueries && snapshot(f->clipPlanes) == initialPlanes);
  for (UINT index = 0; index < 6; ++index) {
    args = {index,{float(index)+0.25f,-float(index)-0.5f,0.75f,-1.5f}};
    const auto expected = args;
    auto expectedPlanes = f->clipPlanes;
    std::memcpy(expectedPlanes[index].data(),expected.Plane,sizeof(expected.Plane));
    f->queryHook = [&] {
      CHECK(f->table.pfnSetClipPlane(f->device,&args) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
      args = {(index+1)%6,{91.0f,92.0f,93.0f,94.0f}};
    };
    CHECK(f->table.pfnSetClipPlane(f->device,&args) == S_OK);
    CHECK(f->clipPlaneIndex == index && snapshot(f->clipPlanes) == snapshot(expectedPlanes));
  }
  const auto beforeFailure = snapshot(f->clipPlanes);
  args = {5,{-7.0f,8.0f,-9.0f,10.0f}};
  for (const HRESULT failure : {S_FALSE,E_FAIL,E_OUTOFMEMORY,D3DERR_NOTAVAILABLE,DXGI_ERROR_WAS_STILL_DRAWING}) {
    f->fixedResult = failure;
    const HRESULT expected = failure == S_FALSE ? E_FAIL
      : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure;
    CHECK(f->table.pfnSetClipPlane(f->device,&args) == expected);
    CHECK(snapshot(f->clipPlanes) == beforeFailure);
  }
  f->fixedResult = S_OK;
  CHECK(f->table.pfnSetClipPlane(f->device,&args) == S_OK);
  CHECK(!std::memcmp(f->clipPlanes[5].data(),args.Plane,sizeof(args.Plane)));
  const unsigned beforeLost = f->clipPlaneSets;
  ++f->generation;
  CHECK(f->table.pfnSetClipPlane(f->device,&args) == D3DERR_DEVICELOST);
  CHECK(f->table.pfnSetClipPlane(f->device,&args) == D3DERR_DEVICELOST && f->clipPlaneSets == beforeLost);
  const HANDLE stale = f->device;
  closeDevice();
  CHECK(f->table.pfnSetClipPlane(stale,&args) == E_INVALIDARG && f->clipPlaneSets == beforeLost);
  closeAdapter();
}

static void queryContracts() {
  {
    Fixture fixture; initialize(fixture); createDevice();
    CHECK(f->table.pfnCreateQuery && f->table.pfnIssueQuery && f->table.pfnGetQueryData && f->table.pfnDestroyQuery);
    char cookie;
    D3DDDIARG_CREATEQUERY args = {D3DDDIQUERYTYPE_EVENT, &cookie};
    const auto original = snapshot(args);
    CHECK(f->table.pfnCreateQuery(f->device, nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnCreateQuery(nullptr, &args) == E_INVALIDARG && snapshot(args) == original);
    for (const auto type : {D3DDDIQUERYTYPE_RESOURCEMANAGER, D3DDDIQUERYTYPE_VERTEXSTATS,
        D3DDDIQUERYTYPE_PIPELINETIMINGS, static_cast<D3DDDIQUERYTYPE>(UINT_MAX)}) {
      args.QueryType = type; const auto before = snapshot(args);
      CHECK(f->table.pfnCreateQuery(f->device, &args) == D3DERR_NOTAVAILABLE && snapshot(args) == before);
    }
    args.QueryType = D3DDDIQUERYTYPE_EVENT;
    for (const HRESULT failure : {S_FALSE, E_FAIL, E_OUTOFMEMORY, D3DERR_NOTAVAILABLE,
        DXGI_ERROR_WAS_STILL_DRAWING}) {
      f->queryCreateResult = failure;
      const HRESULT expected = failure == S_FALSE ? E_FAIL
        : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure;
      CHECK(f->table.pfnCreateQuery(f->device, &args) == expected && snapshot(args) == original);
    }
    f->queryCreateResult = S_OK; f->nullQuery = true;
    CHECK(f->table.pfnCreateQuery(f->device, &args) == E_FAIL && snapshot(args) == original);
    f->nullQuery = false; f->queryThrowAllocation = true;
    CHECK(f->table.pfnCreateQuery(f->device, &args) == E_OUTOFMEMORY && snapshot(args) == original);
    f->queryThrowAllocation = false; f->queryThrowOther = true;
    CHECK(f->table.pfnCreateQuery(f->device, &args) == E_FAIL && snapshot(args) == original);
    f->queryThrowOther = false;
    CHECK(f->queryCreates == 0);

    const D3DDDIQUERYTYPE types[] = {D3DDDIQUERYTYPE_VCACHE, D3DDDIQUERYTYPE_EVENT,
      D3DDDIQUERYTYPE_OCCLUSION, D3DDDIQUERYTYPE_TIMESTAMP,
      D3DDDIQUERYTYPE_TIMESTAMPDISJOINT, D3DDDIQUERYTYPE_TIMESTAMPFREQ};
    const D3DQUERYTYPE coreTypes[] = {D3DQUERYTYPE_VCACHE, D3DQUERYTYPE_EVENT,
      D3DQUERYTYPE_OCCLUSION, D3DQUERYTYPE_TIMESTAMP,
      D3DQUERYTYPE_TIMESTAMPDISJOINT, D3DQUERYTYPE_TIMESTAMPFREQ};
    const UINT sizes[] = {16, 4, 4, 8, 4, 8};
    for (unsigned index = 0; index < 6; index++) {
      args = {types[index], &cookie};
      f->queryHook = [&] {
        auto nested = args;
        CHECK(f->table.pfnCreateQuery(f->device, &nested) == D3DERR_WASSTILLDRAWING);
        CHECK(nested.hQuery == &cookie);
        CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
        args.QueryType = static_cast<D3DDDIQUERYTYPE>(UINT_MAX);
      };
      CHECK(f->table.pfnCreateQuery(f->device, &args) == S_OK);
      const HANDLE token = args.hQuery;
      CHECK(token && token != &cookie && f->queryType == coreTypes[index]);
      std::array<uint8_t, 32> output, alternate; output.fill(0xa5); alternate.fill(0x93);
      const auto untouched = output, untouchedAlternate = alternate;
      D3DDDIARG_GETQUERYDATA get = {token, output.data() + 5};
      const auto reads = f->queryReads;
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_FALSE && output == untouched && f->queryReads == reads);
      CHECK(f->table.pfnGetQueryData(f->device, nullptr) == E_INVALIDARG);
      D3DDDIARG_ISSUEQUERY issue = {}; issue.hQuery = token;
      CHECK(f->table.pfnIssueQuery(f->device, nullptr) == E_INVALIDARG);
      for (const UINT flags : {0u, 3u, 4u, UINT_MAX}) {
        issue.Flags.Value = flags;
        CHECK(f->table.pfnIssueQuery(f->device, &issue) == E_INVALIDARG);
      }
      issue.Flags.Value = 1;
      const bool begin = index == 2 || index == 4;
      CHECK(f->table.pfnIssueQuery(f->device, &issue) == (begin ? S_OK : E_INVALIDARG));
      if (begin) CHECK(f->queryIssueFlags == D3DISSUE_BEGIN);
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_FALSE && output == untouched && f->queryReads == reads);
      issue.Flags.Value = 2;
      f->queryHook = [&] {
        CHECK(f->table.pfnGetQueryData(f->device, &get) == D3DERR_WASSTILLDRAWING);
        CHECK(f->table.pfnDestroyQuery(f->device, token) == D3DERR_WASSTILLDRAWING);
        std::thread concurrent([&] {
          CHECK(f->table.pfnGetQueryData(f->device, &get) == D3DERR_WASSTILLDRAWING);
        }); concurrent.join();
        issue.hQuery = nullptr; issue.Flags.Value = 1;
      };
      CHECK(f->table.pfnIssueQuery(f->device, &issue) == S_OK && f->queryIssueFlags == D3DISSUE_END);
      issue.hQuery = token; issue.Flags.Value = 2;
      for (unsigned i = 0; i < f->queryBytes.size(); i++) f->queryBytes[i] = uint8_t(17 + i + index);
      f->querySingleByteEvent = index == 1;
      for (const HRESULT failure : {S_FALSE, HRESULT(2), E_FAIL, E_OUTOFMEMORY,
          D3DERR_NOTAVAILABLE, DXGI_ERROR_WAS_STILL_DRAWING}) {
        f->queryDataResult = failure;
        const HRESULT expected = failure == HRESULT(2) ? E_FAIL
          : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure;
        CHECK(f->table.pfnGetQueryData(f->device, &get) == expected && output == untouched);
        CHECK(f->queryDataBytes == sizes[index]);
      }
      f->queryDataResult = S_OK;
      f->queryHook = [&] {
        CHECK(f->table.pfnGetQueryData(f->device, &get) == D3DERR_WASSTILLDRAWING);
        get.hQuery = nullptr; get.pData = alternate.data() + 5;
      };
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_OK);
      auto expected = untouched;
      if (index == 1) { const BOOL value = TRUE; std::memcpy(expected.data() + 5, &value, 4); }
      else std::memcpy(expected.data() + 5, f->queryBytes.data(), sizes[index]);
      CHECK(output == expected && alternate == untouchedAlternate);
      // Cached EVENT completion still writes the full native BOOL.
      output = untouched; get = {token, output.data() + 5};
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_OK && output == expected);
      get = {token, nullptr};
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_OK && f->queryDataBytes == 0);
      get.pData = reinterpret_cast<void*>(UINTPTR_MAX - 1);
      const auto beforeInvalid = f->queryReads;
      CHECK(f->table.pfnGetQueryData(f->device, &get) == E_INVALIDARG && f->queryReads == beforeInvalid);
      get = {token, output.data() + 5};
      for (const HRESULT failure : {S_FALSE, E_OUTOFMEMORY, DXGI_ERROR_WAS_STILL_DRAWING}) {
        f->queryIssueResult = failure;
        const HRESULT expectedIssue = failure == S_FALSE ? E_FAIL
          : failure == DXGI_ERROR_WAS_STILL_DRAWING ? D3DERR_WASSTILLDRAWING : failure;
        CHECK(f->table.pfnIssueQuery(f->device, &issue) == expectedIssue);
        CHECK(f->table.pfnGetQueryData(f->device, &get) == S_OK);
      }
      f->queryIssueResult = S_OK;
      CHECK(f->table.pfnIssueQuery(f->device, &issue) == S_OK);
      f->queryDataResult = S_FALSE; output = untouched;
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_FALSE && output == untouched);
      f->queryDataResult = S_OK;
      f->flushResult = S_FALSE;
      CHECK(f->table.pfnDestroyQuery(f->device, token) == E_FAIL);
      CHECK(f->table.pfnGetQueryData(f->device, &get) == S_OK);
      f->flushResult = S_OK;
      CHECK(f->table.pfnDestroyQuery(f->device, token) == S_OK);
      CHECK(f->table.pfnDestroyQuery(f->device, token) == E_INVALIDARG);
      CHECK(f->table.pfnGetQueryData(f->device, &get) == E_INVALIDARG);
      CHECK(f->queryCreates == f->queryCloses);
    }
    closeDevice(); closeAdapter();
  }
  // Backend completion followed by a reset must not publish a token or data.
  for (unsigned phase = 0; phase < 4; phase++) {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie; D3DDDIARG_CREATEQUERY args = {D3DDDIQUERYTYPE_EVENT, &cookie};
    if (!phase) f->queryCreateHook = [] { f->queryHook = [] { ++f->generation; }; };
    const HRESULT created = f->table.pfnCreateQuery(f->device, &args);
    if (!phase) CHECK(created == D3DERR_DEVICELOST && args.hQuery == &cookie && f->queryCreates == f->queryCloses);
    else {
      CHECK(created == S_OK);
      D3DDDIARG_ISSUEQUERY issue = {}; issue.hQuery = args.hQuery; issue.Flags.End = 1;
      if (phase == 1) f->queryIssueHook = [] { f->queryHook = [] { ++f->generation; }; };
      const HRESULT issued = f->table.pfnIssueQuery(f->device, &issue);
      if (phase == 1) CHECK(issued == D3DERR_DEVICELOST);
      else {
        CHECK(issued == S_OK);
        std::array<uint8_t, 12> output; output.fill(0xa5); const auto before = output;
        D3DDDIARG_GETQUERYDATA get = {args.hQuery, output.data() + 3};
        f->queryDataResult = phase == 2 ? S_OK : S_FALSE;
        f->queryDataHook = [] { f->queryHook = [] { ++f->generation; }; };
        CHECK(f->table.pfnGetQueryData(f->device, &get) == D3DERR_DEVICELOST && output == before);
        const auto reads = f->queryReads;
        CHECK(f->table.pfnGetQueryData(f->device, &get) == D3DERR_DEVICELOST && f->queryReads == reads);
      }
      CHECK(f->table.pfnDestroyQuery(f->device, args.hQuery) == D3DERR_DEVICELOST);
      CHECK(f->queryCreates == f->queryCloses);
    }
    const HANDLE stale = f->device;
    closeDevice();
    CHECK(f->table.pfnCreateQuery(stale, &args) == E_INVALIDARG);
    closeAdapter();
  }
  {
    Fixture a; initialize(a); createDevice();
    D3DDDIARG_CREATEQUERY args = {D3DDDIQUERYTYPE_OCCLUSION, nullptr};
    CHECK(a.table.pfnCreateQuery(a.device, &args) == S_OK);
    const HANDLE foreign = args.hQuery;
    Fixture b; initialize(b); createDevice();
    D3DDDIARG_GETQUERYDATA get = {foreign, nullptr};
    D3DDDIARG_ISSUEQUERY issue = {}; issue.hQuery = foreign; issue.Flags.End = 1;
    CHECK(b.table.pfnGetQueryData(b.device, &get) == E_INVALIDARG);
    CHECK(b.table.pfnIssueQuery(b.device, &issue) == E_INVALIDARG);
    CHECK(b.table.pfnDestroyQuery(b.device, foreign) == E_INVALIDARG);
    closeDevice(); closeAdapter();
    f = &a; issue.Flags.Value = 1;
    CHECK(a.table.pfnIssueQuery(a.device, &issue) == S_OK);
    // Closing a device also retires a still-begun query on its worker.
    closeDevice(); CHECK(a.queryCreates == a.queryCloses); closeAdapter();
  }
}

static D3DDDIARG_PRESENT presentArgs(HANDLE resource) {
  D3DDDIARG_PRESENT args = {};
  args.hSrcResource = resource; args.Flags.Blt = 1;
  args.DstSubResourceIndex = UINT_MAX;
  args.FlipInterval = static_cast<D3DDDI_FLIPINTERVAL_TYPE>(UINT_MAX);
  return args;
}

static HANDLE presentTarget(HANDLE cookie, D3DFORMAT format = D3DFMT_A8R8G8B8) {
  D3DDDI_SURFACEINFO info = {3,2,UINT_MAX,nullptr,UINT_MAX,UINT_MAX};
  auto resource = resourceArgs(cookie, &info, 1, true);
  resource.Format = static_cast<D3DDDIFORMAT>(format);
  CHECK(f->table.pfnCreateResource(f->device, &resource) == S_OK);
  D3DDDIARG_LOCK mapping = {}; mapping.hResource = resource.hResource;
  CHECK(f->table.pfnLock(f->device, &mapping) == S_OK && mapping.pSurfData && mapping.Pitch >= 12);
  const std::array<DWORD,6> colors{0xff123456,0xffa53179,0xff05d8e2,0xfff38216,0xff3142e7,0xff5b1c93};
  f->expectedPresentPixels.resize(sizeof(colors));
  std::memcpy(f->expectedPresentPixels.data(), colors.data(), sizeof(colors));
  for (unsigned row = 0; row < 2; ++row)
    std::memcpy(static_cast<uint8_t*>(mapping.pSurfData) + size_t(row) * mapping.Pitch,
      colors.data() + row * 3, 12);
  D3DDDIARG_UNLOCK unlock = {}; unlock.hResource = resource.hResource;
  CHECK(f->table.pfnUnlock(f->device, &unlock) == S_OK);
  return resource.hResource;
}

static void closePresentDevice(HRESULT expected = S_OK) {
  CHECK(f->table.pfnDestroyDevice(f->device) == expected);
  CHECK(!f->allocated && !f->mapped && f->contexts == f->contextCloses);
  CHECK(f->allocations == f->deallocations && f->locks == f->unlocks);
  CHECK(f->presentAllocations.empty() && f->presentAllocationCreates == f->presentReleases);
  CHECK(f->presentLocks == f->presentUnlocks && !f->presentContextLive);
  CHECK(f->presentContexts == f->presentContextCloses && f->surfaceCreates == f->surfaceCloses);
  CHECK(!f->cleanup.empty() && f->cleanup.back() == 'C');
  f->callbacksValid = false;
  CHECK(f->table.pfnDestroyDevice(f->device) == E_INVALIDARG);
  CHECK(f->table.pfnPresent(f->device, nullptr) == E_INVALIDARG);
  f->callbacksValid = true;
}

static void presentationContracts() {
  for (const auto format : {D3DFMT_A8R8G8B8,D3DFMT_X8R8G8B8}) {
    Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback;
    // Typed callbacks are copied before any callback can mutate the input table.
    f->queryHook = [] { f->input.pfnPresentCb = nullptr; };
    createDevice(); CHECK(f->table.pfnPresent);
    char cookie; const HANDLE resource = presentTarget(&cookie, format);
    auto args = presentArgs(resource); const auto before = snapshot(args);
    f->queryHook = [&] { args.hSrcResource = nullptr; args.Flags.Value = 0; };
    // Kernel callbacks can overwrite the renderer's private storage only after
    // readback. The published frame must still use the DDI-owned original copy.
    f->presentAllocateHook = [] {
      CHECK(f->rendererPixels && f->surfaceReads == 1);
      f->rendererPixels->assign(f->rendererPixels->size(), 0x1d);
    };
    CHECK(f->table.pfnPresent(f->device, &args) == S_OK);
    CHECK(snapshot(args) != before && f->presents == 1 && f->surfaceReads == 1);
    CHECK(f->presentAllocations.size() == 1 && f->presentAllocations.count(&cookie));
    CHECK(f->presentAllocations.begin()->second.info.format == (format == D3DFMT_X8R8G8B8 ? 2u : 1u));
    CHECK(f->lastPresent.hSrcAllocation == 71 && uintptr_t(resource) != 71);
    *f->rendererPixels = f->expectedPresentPixels;
    args = presentArgs(resource);
    f->presentLockHook = [] { f->rendererPixels->assign(f->rendererPixels->size(), 0x72); };
    CHECK(f->table.pfnPresent(f->device, &args) == S_OK);
    CHECK(snapshot(args) == before && f->presents == 2 && f->surfaceReads == 2);
    CHECK(f->presentAllocationCreates == 1 && f->presentContexts == 1);
    CHECK(f->presentLocks == 2 && f->presentUnlocks == 2);
    CHECK(f->table.pfnDestroyResource(f->device, resource) == S_OK);
    CHECK(f->presentReleases == 1 && f->presentAllocations.empty());
    CHECK(f->table.pfnPresent(f->device, &args) == E_INVALIDARG);
    closePresentDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); createDevice();
    char cookie; const HANDLE resource = presentTarget(&cookie);
    auto args = presentArgs(resource);
    CHECK(f->table.pfnPresent(f->device, &args) == D3DERR_NOTAVAILABLE);
    CHECK(!f->surfaceReads && !f->presentAllocationAttempts && !f->presents);
    closeDevice(); closeAdapter();
  }
  {
    Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback; createDevice();
    char cookie, otherCookie, multiCookie, bufferCookie;
    const HANDLE resource = presentTarget(&cookie);
    auto valid = presentArgs(resource);
    CHECK(f->table.pfnPresent(f->device, nullptr) == E_INVALIDARG);
    CHECK(f->table.pfnPresent(nullptr, &valid) == E_INVALIDARG);
    for (unsigned field = 0; field < 5; ++field) {
      auto args = valid;
      if (field == 0) args.hSrcResource = nullptr;
      if (field == 1) args.hSrcResource = reinterpret_cast<HANDLE>(UINT_PTR(0xcafef00d));
      if (field == 2) args.hDstResource = resource;
      if (field == 3) args.SrcSubResourceIndex = 1;
      if (field == 4) args.Flags.Value = 4;
      const auto before = snapshot(args);
      CHECK(f->table.pfnPresent(f->device, &args) == E_INVALIDARG && snapshot(args) == before);
    }
    for (const UINT flags : {0u,2u,3u,5u,8u,0x80000001u}) {
      auto args = valid; args.Flags.Value = flags;
      CHECK(f->table.pfnPresent(f->device, &args) == E_INVALIDARG);
    }
    D3DDDI_SURFACEINFO infos[2] = {{3,2,0,nullptr,0,0},{3,2,0,nullptr,0,0}};
    auto other = resourceArgs(&otherCookie, infos, 1);
    CHECK(f->table.pfnCreateResource(f->device, &other) == S_OK);
    auto args = presentArgs(other.hResource);
    CHECK(f->table.pfnPresent(f->device, &args) == E_INVALIDARG);
    auto multi = resourceArgs(&multiCookie, infos, 2, true);
    CHECK(f->table.pfnCreateResource(f->device, &multi) == S_OK);
    args = presentArgs(multi.hResource);
    CHECK(f->table.pfnPresent(f->device, &args) == D3DERR_NOTAVAILABLE);
    infos[0].Width = 24;
    auto buffer = bufferArgs(&bufferCookie, infos);
    CHECK(f->table.pfnCreateResource(f->device, &buffer) == S_OK);
    args = presentArgs(buffer.hResource);
    CHECK(f->table.pfnPresent(f->device, &args) == D3DERR_NOTAVAILABLE);
    D3DDDIARG_LOCK mapping = {}; mapping.hResource = resource;
    CHECK(f->table.pfnLock(f->device, &mapping) == S_OK);
    CHECK(f->table.pfnPresent(f->device, &valid) == E_INVALIDARG);
    D3DDDIARG_UNLOCK unlock = {}; unlock.hResource = resource;
    CHECK(f->table.pfnUnlock(f->device, &unlock) == S_OK);
    CHECK(!f->surfaceReads && !f->presentAllocationCreates && !f->presents);
    closePresentDevice(); closeAdapter();
  }
  // Each failed stage must stop before submission; successful acquisition must
  // still be balanced, including unexpected positive statuses and null outputs.
  for (unsigned failure = 0; failure < 16; ++failure) {
    Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback; createDevice();
    char cookie; const HANDLE resource = presentTarget(&cookie);
    auto args = presentArgs(resource); HRESULT expected = E_FAIL;
    if (failure == 0) f->flushResult = E_FAIL;
    if (failure == 1) f->flushResult = S_FALSE;
    if (failure == 2) f->readbackResult = E_FAIL;
    if (failure == 3) f->readbackResult = S_FALSE;
    if (failure == 4) f->shortReadback = true;
    if (failure == 5) { f->presentAllocateResult = E_OUTOFMEMORY; expected = E_OUTOFMEMORY; }
    if (failure == 6) f->presentAllocateResult = S_FALSE;
    if (failure == 7) f->nullPresentAllocation = true;
    if (failure == 8) f->nullPresentResource = true;
    if (failure == 9) f->presentLockResult = E_FAIL;
    if (failure == 10) f->presentLockResult = S_FALSE;
    if (failure == 11) f->badPresentMapping = true;
    if (failure == 12) f->presentUnlockResult = E_FAIL;
    if (failure == 13) f->presentUnlockResult = S_FALSE;
    if (failure == 14) f->presentContextResult = S_FALSE;
    if (failure == 15) f->nullPresentContext = true;
    const auto before = snapshot(args);
    CHECK(f->table.pfnPresent(f->device, &args) == expected && snapshot(args) == before);
    CHECK(!f->presents && f->presentLocks == f->presentUnlocks);
    if (failure < 5) CHECK(!f->presentAllocationAttempts);
    if (failure >= 6 && failure <= 8) CHECK(f->presentAllocationCreates == f->presentReleases);
    if (failure == 14) CHECK(f->presentContexts == 1 && f->presentContextCloses == 1);
    f->flushResult = f->readbackResult = f->presentAllocateResult = S_OK;
    f->presentLockResult = f->presentUnlockResult = f->presentContextResult = S_OK;
    f->shortReadback = f->nullPresentAllocation = f->nullPresentResource = false;
    f->badPresentMapping = f->nullPresentContext = false;
    CHECK(f->table.pfnPresent(f->device, &args) == S_OK && f->presents == 1);
    closePresentDevice(); closeAdapter();
  }
  for (const HRESULT callbackResult : {E_FAIL,S_FALSE}) {
    Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback; createDevice();
    char cookie; const HANDLE resource = presentTarget(&cookie);
    auto args = presentArgs(resource); f->presentResult = callbackResult;
    CHECK(f->table.pfnPresent(f->device, &args) == E_FAIL && f->presents == 1);
    f->presentResult = S_OK;
    CHECK(f->table.pfnPresent(f->device, &args) == S_OK && f->presents == 2);
    CHECK(f->presentAllocationCreates == 1 && f->presentContexts == 1);
    closePresentDevice(); closeAdapter();
  }
  for (unsigned stage = 0; stage < 6; ++stage) {
    Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback; createDevice();
    char cookie; const HANDLE resource = presentTarget(&cookie);
    auto args = presentArgs(resource);
    auto reentry = [&] {
      CHECK(f->table.pfnPresent(f->device, &args) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyResource(f->device, resource) == D3DERR_WASSTILLDRAWING);
      CHECK(f->table.pfnDestroyDevice(f->device) == D3DERR_WASSTILLDRAWING);
    };
    if (stage == 0) f->queryHook = reentry;
    if (stage == 1) f->presentAllocateHook = reentry;
    if (stage == 2) f->presentLockHook = reentry;
    if (stage == 3) f->presentUnlockHook = reentry;
    if (stage == 4) f->presentContextHook = reentry;
    if (stage == 5) f->presentHook = reentry;
    CHECK(f->table.pfnPresent(f->device, &args) == S_OK && f->presents == 1);
    f->presentReleaseHook = reentry;
    CHECK(f->table.pfnDestroyResource(f->device, resource) == S_OK && f->presentReleases == 1);
    closePresentDevice(); closeAdapter();
  }
  for (const HRESULT released : {E_FAIL,S_FALSE}) {
    Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback; createDevice();
    char cookie; const HANDLE resource = presentTarget(&cookie);
    auto args = presentArgs(resource);
    CHECK(f->table.pfnPresent(f->device, &args) == S_OK);
    f->presentReleaseResult = released;
    CHECK(f->table.pfnDestroyResource(f->device, resource) == E_FAIL);
    CHECK(f->presentReleaseAttempts == 1 && f->presentReleases == (released == S_FALSE ? 1u : 0u));
    f->presentReleaseResult = S_OK;
    CHECK(f->table.pfnDestroyResource(f->device, resource) == S_OK);
    CHECK(f->presentReleases == 1 && f->presentReleaseAttempts == (released == S_FALSE ? 1u : 2u));
    CHECK(f->table.pfnDestroyResource(f->device, resource) == E_INVALIDARG);
    closePresentDevice(); closeAdapter();
  }
  for (unsigned phase = 0; phase < 6; ++phase) {
    for (const bool retireAdapter : {false,true}) {
      if (phase == 0 && retireAdapter) continue; // Readback runs on the worker.
      Fixture fixture; initialize(fixture); f->input.pfnPresentCb = presentCallback; createDevice();
      char cookie; const HANDLE resource = presentTarget(&cookie);
      auto args = presentArgs(resource);
      auto retire = [&] { if (retireAdapter) closeAdapter(); else ++f->generation; };
      if (phase == 0) f->readbackHook = retire;
      if (phase == 1) f->presentAllocateHook = retire;
      if (phase == 2) f->presentLockHook = retire;
      if (phase == 3) f->presentUnlockHook = retire;
      if (phase == 4) f->presentContextHook = retire;
      if (phase == 5) f->presentHook = retire;
      CHECK(f->table.pfnPresent(f->device, &args) == D3DERR_DEVICELOST);
      CHECK(f->presents == (phase == 5 ? 1u : 0u));
      if (!retireAdapter) --f->generation;
      const auto queries = f->queries, reads = f->surfaceReads, submits = f->presents;
      CHECK(f->table.pfnPresent(f->device, &args) == D3DERR_DEVICELOST);
      CHECK(f->queries == queries && f->surfaceReads == reads && f->presents == submits);
      closePresentDevice();
      if (!retireAdapter) closeAdapter();
    }
  }
  {
    Fixture a; initialize(a); a.input.pfnPresentCb = presentCallback; createDevice();
    char cookie; auto args = presentArgs(presentTarget(&cookie));
    Fixture b; initialize(b); b.input.pfnPresentCb = presentCallback; createDevice();
    CHECK(b.table.pfnPresent(b.device, &args) == E_INVALIDARG);
    CHECK(!b.surfaceReads && !b.presentAllocationAttempts && !b.presents);
    closePresentDevice(); closeAdapter();
    f = &a;
    CHECK(a.table.pfnPresent(a.device, &args) == S_OK);
    closePresentDevice(); closeAdapter();
  }
}

int main() {
  creationFlagContracts();
  presentationContracts();
  queryContracts();
  bufferTransferContracts();
  fixedFunctionContracts();
  depthContracts();
  bufferContracts();
  textureContracts();
  dynamicTextureContracts();
  shaderContracts();
  ownedServiceStartup();
  resourceContracts();
  drawContracts();
  clipPlaneContracts();
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
