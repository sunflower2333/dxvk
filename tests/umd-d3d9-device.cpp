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
  unsigned shaderCreates = 0, shaderCloses = 0, shaderSets = 0, constantSets = 0;
  HRESULT shaderResult = S_OK, constantResult = S_OK;
  bool nullShader = false;
  std::function<void()> shaderHook;
  dxvk::umd::D3D9ShaderStage shaderStage = dxvk::umd::D3D9ShaderStage::Vertex;
  std::vector<DWORD> shaderCode;
  std::vector<uint8_t> constantBytes;
  UINT constantFirst = 0, constantCount = 0;
  char constantType = 0;
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
  D3D9VertexDeclaration* declaration = nullptr;
  std::array<D3D9Shader*, 2> shaders = {};
  std::array<D3D9TextureResource*, 20> textures = {};
};

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
  unsigned* textureLevels = nullptr;
};
dxvk::umd::D3D9SurfaceResource::D3D9SurfaceResource() : m_state(std::make_unique<State>()) { }
dxvk::umd::D3D9SurfaceResource::~D3D9SurfaceResource() {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(!m_state->locked);
  if (m_state->textureLevels) --*m_state->textureLevels;
  ++f->surfaceCloses;
}
dxvk::umd::D3D9Backend::D3D9Backend() : m_state(std::make_unique<State>()) { }
dxvk::umd::D3D9Backend::~D3D9Backend() {
  CHECK(GetCurrentThreadId() != f->caller);
  CHECK(f->surfaceCreates == f->surfaceCloses && !m_state->target);
  CHECK(f->declarationCreates == f->declarationCloses && !m_state->declaration);
  CHECK(f->shaderCreates == f->shaderCloses && !m_state->shaders[0] && !m_state->shaders[1]);
  CHECK(f->textureCreates == f->textureCloses);
  for (const auto texture : m_state->textures) CHECK(!texture);
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
  expectedTable.pfnSetViewport = f->table.pfnSetViewport;
  expectedTable.pfnSetZRange = f->table.pfnSetZRange;
  expectedTable.pfnSetScissorRect = f->table.pfnSetScissorRect;
  expectedTable.pfnSetStreamSourceUm = f->table.pfnSetStreamSourceUm;
  expectedTable.pfnDrawPrimitive = f->table.pfnDrawPrimitive;
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
    for (const UINT flag : {4u,0x10u,0x800u,0x20000u,0x40000u}) {
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

int main() {
  textureContracts();
  shaderContracts();
  ownedServiceStartup();
  resourceContracts();
  drawContracts();
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
