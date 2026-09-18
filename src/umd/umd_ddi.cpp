#include "umd_ddi.h"
#include "umd_api.h"
#include "umd_adapter.h"
#include "umd_shader.h"
#include "umd_query.h"
#include "umd_allocation.h"
#include "umd_runtime_gpu.h"
#include "umd_map.h"
#include "umd_view.h"
#include "umd_texture1d.h"
#include "umd_transfer_policy.h"
#include "umd_transfer_format.h"
#include "umd_generate_mips.h"
#include "umd_state.h"
#include "umd_stream_output.h"
#include "umd_output_merger.h"
#include "umd_shared_surface.h"

#include <wrl/client.h>
#include <cstring>
#include <memory>
#include <new>
#include <vector>
#include <array>
#include <mutex>
#include <unordered_map>
#include <type_traits>
#include <atomic>
#include <optional>

namespace {
using Microsoft::WRL::ComPtr;
struct Shader;
struct InputLayout;
struct Device {
  std::shared_ptr<dxvk::umd::RuntimeService> service = std::make_shared<dxvk::umd::RuntimeService>();
  std::shared_ptr<const dxvk::umd::AdapterIdentity> adapter;
  std::shared_ptr<dxvk::umd::RuntimeGpu> gpu;
  ComPtr<ID3D11Device> backend;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Query> predicate;
  BOOL predicateValue = FALSE;
  bool suppressCommands = false;
  D3D10DDI_HRTCORELAYER runtime;
  D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks;
  dxvk::umd::RuntimeMemory memory;
  bool vertexBound = false;
  bool pixelBound = false;
  bool targetBound = false;
  bool viewportBound = false;
  bool topologyBound = false;
  bool indexBound = false;
  Shader* vertexShader = nullptr;
  Shader* geometryShader = nullptr;
  Shader* pixelShader = nullptr;
  InputLayout* inputLayout = nullptr;
  // Shared surfaces bound to the pipeline right now, by stage and slot. A draw
  // reads only these, so it refreshes only what it samples and dirties only
  // what it renders to -- never a sweep of every shared surface the device
  // owns. Held by shared_ptr because a view outlives its resource DDI.
  std::array<std::array<std::shared_ptr<dxvk::umd::SharedSurface>,
    D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>, 3> boundShared;
  std::array<UINT, 3> boundSharedHigh{};
  std::array<std::shared_ptr<dxvk::umd::SharedSurface>,
    D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> targetShared;
  // Every shared surface this device created or opened, for the publish sweep
  // that Flush and Present owe the other process. Weak, so a destroyed
  // resource leaves nothing behind to publish into a freed allocation.
  std::vector<std::weak_ptr<dxvk::umd::SharedSurface>> sharedSurfaces;
  // Ownership epoch. Bumped wherever this device's work becomes visible to
  // another process and vice versa, which is the boundary a refresh is
  // memoized against. Never zero: SharedSurface::refreshed starts at zero to
  // mean "never read".
  uint64_t sharedEpoch = 1;
  bool anySharedSurface = false;
  bool closing = false;
  std::atomic<bool> retired{false};
  HRESULT close() noexcept {
    if (closing) return S_OK;
    closing = true;
    dxvk::umd::RuntimeService::Scope scope(service.get());
    HRESULT result = S_OK;
    try {
      // Backend release can join workers that need runtime callbacks. Pump
      // those requests on this DDI caller while release runs separately.
      service->drain([&] { predicate.Reset(); context.Reset(); backend.Reset(); });
    } catch (...) { result = E_FAIL; }
    try {
      const HRESULT gpuResult = gpu ? gpu->close() : S_OK;
      if (FAILED(gpuResult)) result = gpuResult;
      const HRESULT presentResult = memory.close();
      if (FAILED(presentResult)) result = presentResult;
    } catch (...) { result = E_FAIL; }
    service->close();
    return result;
  }
  ~Device() { close(); }
  void error(HRESULT hr) {
    if (!FAILED(hr)) return;
    auto report = [&] {
      callbacks.pfnSetErrorCb(runtime, dxvk::umd::ddiResult(hr));
      return S_OK;
    };
    // DestroyDevice may report removal after GPU service closes, while still
    // inside its original runtime DDI. No destructor reports late errors.
    if (service->isCaller()) report();
    else service->invoke(report);
  }
};
// Runtime private bytes are only a registration key. Callable state is
// independently owned; DestroyDevice finishes runtime cleanup before return
// and no later registry lookup dereferences the retired private bytes.
struct DevicePrivate { uintptr_t reserved; };
enum class DevicePhase { Creating, Live };
struct DeviceRecord {
  DevicePhase phase;
  std::shared_ptr<Device> owner;
};
std::mutex deviceStorageMutex;
std::unordered_map<void*, DeviceRecord> deviceStorage;

void releaseDeviceStorage(void* storage, const std::shared_ptr<Device>& owner) {
  std::lock_guard<std::mutex> lock(deviceStorageMutex);
  const auto entry = deviceStorage.find(storage);
  if (entry != deviceStorage.end() && entry->second.owner == owner)
    deviceStorage.erase(entry);
}

struct DeviceOperation {
  void* storage;
  std::shared_ptr<Device> owner;
  DeviceOperation* previous = nullptr;
  static thread_local DeviceOperation* current;
  explicit DeviceOperation(void* value, std::shared_ptr<Device> pinned = {})
  : storage(value), owner(std::move(pinned)) {
    if (!owner) {
      std::lock_guard<std::mutex> lock(deviceStorageMutex);
      const auto entry = deviceStorage.find(storage);
      if (entry != deviceStorage.end() && entry->second.phase == DevicePhase::Live)
        owner = entry->second.owner;
    }
    previous = current;
    current = this;
  }
  ~DeviceOperation() {
    current = previous;
    // Drop the final callable owner only after every local DDI object/guard
    // and callback lock was destroyed. No runtime private pointer is read.
    owner.reset();
  }
};
thread_local DeviceOperation* DeviceOperation::current = nullptr;
void APIENTRY flush(D3D10DDI_HDEVICE h);
template<auto function> constexpr bool isFlushEntry = false;
template<> constexpr bool isFlushEntry<&flush> = true;

// All published D3D device entries pass through this typed WDK-ABI wrapper.
// Thread-local operation lookup preserves the old owner when a callback
// reuses the same runtime storage for a new device before the outer DDI ends.
template<typename Function, Function function, bool predicated> struct DeviceEntry;
template<typename Result, typename... Args,
    Result (APIENTRY *function)(D3D10DDI_HDEVICE, Args...), bool predicated>
struct DeviceEntry<Result (APIENTRY *)(D3D10DDI_HDEVICE, Args...), function, predicated> {
  static Result APIENTRY call(D3D10DDI_HDEVICE h, Args... args) {
    DeviceOperation operation(h.pDrvPrivate);
    if (!operation.owner) {
      if constexpr (std::is_void_v<Result>) return;
      // The sole BOOL entry is IsStagingBusy; a retired owner is not ready.
      else if constexpr (std::is_same_v<Result, BOOL>) return TRUE;
      else return Result{};
    }
    // CalcPrivate functions remain genuinely concurrent, as required even
    // for D3D10 drivers without D3D11 FREETHREADED admission. They only return
    // object sizes and never run backend work or runtime callbacks.
    if constexpr (std::is_same_v<Result, SIZE_T>) return function(h, args...);
    else {
      dxvk::umd::RuntimeService::Scope scope(operation.owner->service.get());
      try {
        auto backend = [&]() -> Result {
          DeviceOperation worker(h.pDrvPrivate, operation.owner);
          if constexpr (predicated) {
            if (operation.owner->suppressCommands) return;
          }
          return function(h, args...);
        };
        if constexpr (isFlushEntry<function>) {
          // Runtime RenderCb can retire children during this Flush. The
          // caller pump releases their COM owners only after that callback
          // returns; release can itself defer backend cleanup. Flush again
          // after those releases so no follow-up application DDI is required.
          auto service = operation.owner->service;
          uint64_t epoch;
          do {
            epoch = service->retirementEpoch();
            service->run(backend);
          } while (!operation.owner->retired && epoch != service->retirementEpoch());
          return;
        } else return operation.owner->service->run(backend);
      } catch (...) {
        operation.owner->error(DXGI_ERROR_DEVICE_REMOVED);
        if constexpr (std::is_void_v<Result>) return;
        else if constexpr (std::is_same_v<Result, BOOL>) return TRUE;
        else return Result{};
      }
    }
  }
};
template<auto function, bool predicated = false>
constexpr auto deviceEntry = &DeviceEntry<decltype(function), function, predicated>::call;
struct ResourceRetirement;
struct Resource {
  Device* owner = nullptr;
  ComPtr<ID3D11Resource> backend;
  ComPtr<ID3D11Texture2D> presentReadback;
  dxvk::umd::RuntimeAllocation allocation;
  // Set only for a resource created MISC_SHARED or opened from another
  // process. Its allocation lives on the surface, not in the member above, so
  // that a view holding the last reference keeps the backing alive.
  std::shared_ptr<dxvk::umd::SharedSurface> shared;
  std::unique_ptr<ResourceRetirement> retirement;
};
struct ResourceRetirement final : dxvk::umd::RuntimeService::Retirement {
  std::optional<Resource> resource;
  void release() noexcept override {
    auto device = resource->owner;
    const HRESULT hr = resource->allocation.release();
    resource->presentReadback.Reset();
    // A view may still hold the surface; its allocation retires with the last
    // reference, not with this resource. An opened allocation is never
    // deallocated here in any case -- it belongs to the process that made it.
    resource->shared.reset();
    resource->backend.Reset();
    if (FAILED(hr)) device->error(hr);
  }
};
enum class ResourcePhase { Creating, Live };
struct ResourceRecord {
  Device* owner;
  ResourcePhase phase;
  std::shared_ptr<const char> reservation;
};
std::mutex resourceStorageMutex;
std::unordered_map<void*, ResourceRecord> resourceStorage;
struct ComRetirement final : dxvk::umd::RuntimeService::Retirement {
  std::array<ComPtr<IUnknown>, 3> references;
  void release() noexcept override {
    for (auto& reference : references) reference.Reset();
  }
};
struct Child {
  std::unique_ptr<ComRetirement> retirement;
};
// A view can hold the final reference to a resource even after its resource
// DDI was destroyed. Apply the same no-final-release rule to every COM child.
template<typename Object, typename Function>
void createChildBackend(Device* device, Object* object, Function&& function) {
  try {
    auto retirement = std::make_unique<ComRetirement>();
    const HRESULT hr = function();
    if (hr == S_OK) object->retirement = std::move(retirement);
    device->error(hr);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
template<typename Object, typename... Interfaces>
void retireChild(Device* device, Object* object, ComPtr<Interfaces>&... references) {
  auto retired = std::move(object->retirement);
  if (retired) {
    unsigned index = 0;
    (retired->references[index++].Attach(references.Detach()), ...);
  }
  object->~Object();
  if (retired) device->service->retire(retired.release());
}
struct RenderTarget : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11RenderTargetView> backend;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  std::shared_ptr<dxvk::umd::SharedSurface> shared;
};
struct ShaderView : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11ShaderResourceView> backend;
  std::shared_ptr<dxvk::umd::SharedSurface> shared;
};
struct Sampler : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11SamplerState> backend;
};
struct Shader : Child {
  Device* owner = nullptr;
  dxvk::umd::ShaderStage stage = dxvk::umd::ShaderStage::Vertex;
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11GeometryShader> geometry;
  ComPtr<ID3D11PixelShader> pixel;
  std::vector<UINT> code;
  std::vector<dxvk::umd::ShaderSignatureEntry> inputs;
  std::vector<dxvk::umd::ShaderSignatureEntry> outputs;
  std::array<dxvk::umd::ShaderScalar,32> compiledInputTypes = {};
  std::array<dxvk::umd::ShaderScalar,32> compiledOutputTypes = {};
  bool needsLayout = false;
  bool needsLinkage = false;
  bool withStreamOutput = false;
  dxvk::umd::StreamOutput streamOutput;
};
struct InputLayout : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11InputLayout> backend;
  std::array<dxvk::umd::ShaderScalar,32> inputTypes = {};
};
struct Rasterizer : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11RasterizerState> backend;
};
struct BlendState : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11BlendState> backend;
};
struct DepthView : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11DepthStencilView> backend;
};
struct DepthState : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11DepthStencilState> backend;
};
// Build resource views locally; a failed CreateView receives no DestroyView.
// All staged backend references are released before the error callback can reenter.
template<typename Object, typename Create>
HRESULT createViewStorage(Device* device, void* storage, Create&& create) {
  if (!storage || uintptr_t(storage) % alignof(Object)) return E_INVALIDARG;
  HRESULT hr = E_FAIL;
  {
    Object staged;
    staged.owner = device;
    try {
      staged.retirement = std::make_unique<ComRetirement>();
      hr = create(staged);
      if (hr == S_OK && !staged.backend) hr = E_FAIL;
      if (hr == S_OK && device->retired) hr = DXGI_ERROR_DEVICE_REMOVED;
      if (hr == S_OK) new (storage) Object(std::move(staged));
      else if (!FAILED(hr)) hr = E_FAIL;
    } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
      catch (...) { hr = E_FAIL; }
  }
  return hr;
}
struct Query : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11Query> backend;
  dxvk::umd::QueryInfo info;
  bool begun = false;
  bool issued = false;
};
Device* get(D3D10DDI_HDEVICE h) {
  for (auto operation = DeviceOperation::current; operation; operation = operation->previous)
    if (operation->storage == h.pDrvPrivate) return operation->owner.get();
  return nullptr;
}
Resource* get(D3D10DDI_HRESOURCE h) { return static_cast<Resource*>(h.pDrvPrivate); }
RenderTarget* get(D3D10DDI_HRENDERTARGETVIEW h) { return static_cast<RenderTarget*>(h.pDrvPrivate); }
ShaderView* get(D3D10DDI_HSHADERRESOURCEVIEW h) { return static_cast<ShaderView*>(h.pDrvPrivate); }
Sampler* get(D3D10DDI_HSAMPLER h) { return static_cast<Sampler*>(h.pDrvPrivate); }
Shader* get(D3D10DDI_HSHADER h) { return static_cast<Shader*>(h.pDrvPrivate); }
InputLayout* get(D3D10DDI_HELEMENTLAYOUT h) { return static_cast<InputLayout*>(h.pDrvPrivate); }
Rasterizer* get(D3D10DDI_HRASTERIZERSTATE h) { return static_cast<Rasterizer*>(h.pDrvPrivate); }
BlendState* get(D3D10DDI_HBLENDSTATE h) { return static_cast<BlendState*>(h.pDrvPrivate); }
DepthView* get(D3D10DDI_HDEPTHSTENCILVIEW h) { return static_cast<DepthView*>(h.pDrvPrivate); }
DepthState* get(D3D10DDI_HDEPTHSTENCILSTATE h) { return static_cast<DepthState*>(h.pDrvPrivate); }
Query* get(D3D10DDI_HQUERY h) { return static_cast<Query*>(h.pDrvPrivate); }

bool owned(Device* device, Query* query) {
  if (!query || query->owner != device || !query->backend) {
    device->error(E_INVALIDARG);
    return false;
  }
  return true;
}

SIZE_T APIENTRY querySize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATEQUERY*) { return sizeof(Query); }
void APIENTRY createQuery(D3D10DDI_HDEVICE h, const D3D10DDIARG_CREATEQUERY* args,
    D3D10DDI_HQUERY out, D3D10DDI_HRTQUERY) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto query = new (out.pDrvPrivate) Query();
  query->owner = device;
  if (!args || !dxvk::umd::queryInfo(args->Query, args->MiscFlags, query->info)) {
    query->~Query(); device->error(E_INVALIDARG); return;
  }
  // Failed CreateQuery handles receive no DestroyQuery from the runtime.
  // Unwind all private state before reporting an error (which may reenter).
  HRESULT result = S_OK;
  try {
    query->retirement = std::make_unique<ComRetirement>();
    D3D11_QUERY_DESC desc = {query->info.type,
      query->info.hint ? D3D11_QUERY_MISC_PREDICATEHINT : 0u};
    if (query->info.predicate) {
      ComPtr<ID3D11Predicate> predicate;
      result = device->backend->CreatePredicate(&desc, &predicate);
      if (result == S_OK) query->backend = predicate;
    } else result = device->backend->CreateQuery(&desc, &query->backend);
  } catch (const std::bad_alloc&) { result = E_OUTOFMEMORY; }
    catch (...) { result = E_FAIL; }
  if (result != S_OK) {
    query->~Query();
    device->error(FAILED(result) ? result : E_FAIL);
  }
}
void APIENTRY destroyQuery(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object) {
  auto query = get(object);
  if (!query || query->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  if (get(h)->predicate.Get() == query->backend.Get()) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), query, query->backend);
}
void APIENTRY beginQuery(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object) {
  auto device = get(h); auto query = get(object);
  if (!owned(device, query)) return;
  if (!query->info.beginRequired || query->begun || device->predicate.Get() == query->backend.Get()) {
    device->error(E_INVALIDARG); return;
  }
  try {
    device->context->Begin(query->backend.Get());
    query->begun = true; query->issued = false;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY endQuery(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object) {
  auto device = get(h); auto query = get(object);
  if (!owned(device, query)) return;
  if (device->predicate.Get() == query->backend.Get()) { device->error(E_INVALIDARG); return; }
  try {
    // D3D10 QueryEnd without Begin is an empty query interval, including
    // reuse of an already issued query. This is explicitly legal in the DDI.
    if (query->info.beginRequired && !query->begun)
      device->context->Begin(query->backend.Get());
    device->context->End(query->backend.Get());
    query->begun = false; query->issued = true;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

void APIENTRY setPredication(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object, BOOL value) {
  auto device = get(h);
  if (!object.pDrvPrivate) {
    device->predicate.Reset();
    device->predicateValue = value;
    device->suppressCommands = false;
    return;
  }
  auto query = get(object);
  if (!owned(device, query)) return;
  if (!query->info.predicate || !query->issued) { device->error(E_INVALIDARG); return; }
  // DXVK's public SetPredication is still a stub. Until GPU conditional
  // rendering is implemented, resolve a guaranteed predicate once here and
  // suppress the native operations explicitly. This correctness fallback
  // synchronizes CPU/GPU; it is not the efficient final implementation.
  // Hold independent owners across Flush/runtime callbacks: private query
  // bytes may be reclaimed from a nested runtime callback.
  auto backend = query->backend;
  const auto info = query->info;
  BOOL result = FALSE;
  if (!info.hint) {
    const HRESULT submitted = dxvk::umd::flushRuntimeSubmission(device->context.Get());
    if (FAILED(submitted)) { device->error(submitted); return; }
    const ULONGLONG deadline = GetTickCount64() + 2000;
    for (;;) {
      const HRESULT hr = device->context->GetData(backend.Get(), &result, sizeof(result),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
      if (hr == S_OK) break;
      if (hr != S_FALSE) { device->error(FAILED(hr) ? hr : E_FAIL); return; }
      if (device->retired || GetTickCount64() >= deadline) {
        device->error(DXGI_ERROR_DEVICE_REMOVED); return;
      }
      Sleep(1);
    }
  }
  if (device->retired) return;
  device->predicate = std::move(backend);
  device->predicateValue = value;
  device->suppressCommands = !info.hint && ((result != FALSE) == (value != FALSE));
}
void APIENTRY getQueryData(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object, void* data, UINT size, UINT flags) {
  auto device = get(h); auto query = get(object);
  if (!owned(device, query)) return;
  if (!query->issued) { device->error(E_INVALIDARG); return; }
  try {
    device->error(dxvk::umd::readQueryData(query->info, data, size, flags,
      [&](void* output, UINT outputSize, UINT apiFlags) {
        return device->context->GetData(query->backend.Get(), output, outputSize, apiFlags);
      }));
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

bool owned(Device* device, Resource* resource) {
  if (!resource || resource->owner != device || !resource->backend) {
    device->error(E_INVALIDARG);
    return false;
  }
  return true;
}
bool owned(Device* device, RenderTarget* target) {
  if (!target || target->owner != device || !target->backend) {
    device->error(E_INVALIDARG);
    return false;
  }
  return true;
}

// Record a surface for the publish sweep Flush and Present owe the other
// process, dropping entries whose resource and views are both gone.
void trackSharedSurface(Device* device,
    const std::shared_ptr<dxvk::umd::SharedSurface>& surface) {
  auto& list = device->sharedSurfaces;
  size_t kept = 0;
  for (size_t i = 0; i < list.size(); i++)
    if (!list[i].expired()) list[kept++] = std::move(list[i]);
  list.resize(kept);
  list.push_back(surface);
  device->anySharedSurface = true;
}

// Hand this device's writes to whoever else can see them. Every dirty surface
// gets its attempt and the first failure is what is reported: stopping at that
// failure would leave every surface after it stale as well, which is strictly
// more corruption than the one that already went wrong.
HRESULT publishSharedSurfaces(Device* device) {
  if (!device->anySharedSurface) return S_OK;
  HRESULT result = S_OK;
  for (auto& entry : device->sharedSurfaces) {
    auto surface = entry.lock();
    if (!surface) continue;
    const HRESULT hr = dxvk::umd::publishSharedSurface(device->backend.Get(),
      device->context.Get(), device->memory, *surface, device->sharedEpoch);
    if (FAILED(hr) && SUCCEEDED(result)) result = hr;
  }
  return result;
}

// Open the next ownership epoch, retiring every memoized refresh. Anything
// another process wrote before this point must be read again before it is
// sampled; see umd_shared_surface.h for why that boundary is the right one.
void openSharedEpoch(Device* device) {
  if (device->anySharedSurface)
    device->sharedEpoch = dxvk::umd::nextSharedEpoch(device->sharedEpoch);
}

// Make a bound surface's cache current for this epoch before the pipeline
// reads it. Reports through SetError and answers whether the draw may run.
bool readSharedSurface(Device* device, const std::shared_ptr<dxvk::umd::SharedSurface>& surface) {
  if (!surface) return true;
  const HRESULT hr = dxvk::umd::refreshSharedSurface(device->backend.Get(),
    device->context.Get(), device->memory, *surface, device->sharedEpoch);
  if (FAILED(hr)) { device->error(hr); return false; }
  return true;
}

// A write that defines every pixel of the surface. The cache becomes
// authoritative with no read at all, which is the one case where this bridge
// beats a refresh-before-every-use policy outright instead of deferring it.
void wroteSharedSurface(Device* device, const std::shared_ptr<dxvk::umd::SharedSurface>& surface) {
  if (!surface) return;
  dxvk::umd::sharedWroteWhole(surface->state, device->sharedEpoch);
}

// A write that leaves some pixels as the owning process last left them, so the
// cache has to hold those before it can be published back.
bool wroteSharedRegion(Device* device, const std::shared_ptr<dxvk::umd::SharedSurface>& surface) {
  if (!surface) return true;
  if (!readSharedSurface(device, surface)) return false;
  dxvk::umd::sharedWroteRegion(surface->state);
  return true;
}

// The pixel layout a shared surface must have for AllocationInfo to describe
// it: one linear image, one sample, one of the two formats the wire protocol
// names, and no CPU mapping of the cache behind the publisher's back.
bool sharedSurfaceShape(const D3D10DDIARG_CREATERESOURCE& args) {
  return args.ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D
      && args.MipLevels == 1 && args.ArraySize == 1
      && args.SampleDesc.Count == 1 && !args.SampleDesc.Quality
      && args.Usage == D3D10_DDI_USAGE_DEFAULT && !args.MapFlags
      && (args.Format == DXGI_FORMAT_R8G8B8A8_UNORM
          || args.Format == DXGI_FORMAT_B8G8R8A8_UNORM);
}

// Give a freshly created resource its kernel allocation and shared surface.
// Runs after the cache exists, so a failure here leaves the caller to destroy
// exactly what it staged.
HRESULT createSharedResource(Device* device, Resource* resource,
    const D3D10DDIARG_CREATERESOURCE& args, HANDLE runtime) {
  if (!device->memory.available() || !runtime) return DXGI_ERROR_UNSUPPORTED;
  auto surface = std::make_shared<dxvk::umd::SharedSurface>();
  HRESULT hr = resource->backend.As(&surface->cache);
  if (FAILED(hr)) return hr;
  hr = device->memory.allocate(surface->allocation, runtime,
    args.pMipInfoList[0].TexelWidth, args.pMipInfoList[0].TexelHeight, args.Format);
  if (FAILED(hr)) return hr;
  if (args.pInitialDataUP) {
    // The cache already holds the caller's pixels while the allocation is
    // still the kernel's zeros. The cache is authoritative and owes a publish;
    // treating it as clean here would show another process an empty surface.
    dxvk::umd::sharedWroteWhole(surface->state, device->sharedEpoch);
  } else {
    // Both sides are zero-initialised, so they already agree and the first
    // read would copy nothing.
    dxvk::umd::sharedRefreshed(surface->state, device->sharedEpoch);
  }
  trackSharedSurface(device, surface);
  resource->shared = std::move(surface);
  return S_OK;
}

SIZE_T APIENTRY resourceSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATERESOURCE*) {
  return sizeof(Resource);
}
SIZE_T APIENTRY openedResourceSize(D3D10DDI_HDEVICE, const D3D10DDIARG_OPENRESOURCE*) {
  return sizeof(Resource);
}
// Open a surface another process created: build a cache the pipeline can draw
// with, and take a view of the kernel allocation that carries its pixels.
// Nothing is copied here. The first use refreshes, because the owner's writes
// are what the allocation holds and this cache has never read it.
HRESULT openResourceData(Device* device, const D3D10DDIARG_OPENRESOURCE* args,
    Resource* resource) {
  resource->owner = device;
  if (!args || !args->pOpenAllocationInfo) return E_INVALIDARG;
  // One allocation only -- and that restriction is also what makes the WDK's
  // pOpenAllocationInfo / pOpenAllocationInfo2 union safe to read. The two
  // structures begin with the same hAllocation, pPrivateDriverData and
  // PrivateDriverDataSize and differ only in what follows, so element zero
  // decodes identically whichever one the runtime filled. Any higher count
  // would need the element stride, and the stride is precisely what the union
  // hides from a driver that negotiated neither shape explicitly.
  if (args->NumAllocations != 1 || !device->memory.available())
    return DXGI_ERROR_UNSUPPORTED;
  const auto& opened = args->pOpenAllocationInfo[0];
  if (!opened.hAllocation || !opened.pPrivateDriverData
      || opened.PrivateDriverDataSize != sizeof(dxvk::umd::AllocationInfo))
    return E_INVALIDARG;
  dxvk::umd::AllocationInfo info;
  std::memcpy(&info, opened.pPrivateDriverData, sizeof info);
  const DXGI_FORMAT format = dxvk::umd::allocationFormat(info.format);
  if (format == DXGI_FORMAT_UNKNOWN) return DXGI_ERROR_UNSUPPORTED;
  try {
    // Reserve the retirement node before any owner exists, for the same reason
    // creation does: destruction must not have to allocate.
    resource->retirement = std::make_unique<ResourceRetirement>();
    auto surface = std::make_shared<dxvk::umd::SharedSurface>();
    // adopt validates the rest of the wire metadata. Do that before anything
    // sizes a copy from it: a private-data blob from another process is the
    // only thing between this path and an out-of-bounds transfer.
    HRESULT hr = device->memory.adopt(surface->allocation, opened.hAllocation,
      args->hKMResource.handle, info);
    if (FAILED(hr)) return hr;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = info.width;
    desc.Height = info.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    hr = device->backend->CreateTexture2D(&desc, nullptr, &surface->cache);
    if (FAILED(hr)) return hr;
    resource->backend = surface->cache;
    trackSharedSurface(device, surface);
    resource->shared = std::move(surface);
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
HRESULT createResourceData(Device* device,
    const D3D10DDIARG_CREATERESOURCE* args, Resource* resource,
    D3D10DDI_HRTRESOURCE runtime) {
  resource->owner = device;
  UINT miscFlags = 0;
  bool shared = false;
  if (!args || !args->pMipInfoList || !args->MipLevels || !args->ArraySize ||
      args->MipLevels > D3D11_REQ_MIP_LEVELS || args->ArraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION ||
      args->pPrimaryDesc ||
      !dxvk::umd::textureMiscFlags(*args, miscFlags, &shared) || (args->MapFlags & ~D3D10_DDI_CPU_ACCESS_MASK) ||
      (args->BindFlags & ~(D3D10_DDI_BIND_PIPELINE_MASK | D3D10_DDI_BIND_PRESENT))) {
    return E_INVALIDARG;
  }
  D3D11_TEXTURE1D_DESC oneDimensional = {};
  if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D &&
      !dxvk::umd::texture1DDesc(*args, miscFlags, oneDimensional)) return E_INVALIDARG;
  const bool presentable = (args->BindFlags & D3D10_DDI_BIND_PRESENT) != 0;
  if (presentable && (!device->memory.available() || !runtime.handle
      || args->ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D
      || args->MipLevels != 1 || args->ArraySize != 1
      || args->SampleDesc.Count != 1 || args->SampleDesc.Quality
      || args->Usage != D3D10_DDI_USAGE_DEFAULT || args->MapFlags
      || !(args->BindFlags & D3D10_DDI_BIND_RENDER_TARGET)
      || (args->Format != DXGI_FORMAT_R8G8B8A8_UNORM && args->Format != DXGI_FORMAT_B8G8R8A8_UNORM))) {
    return DXGI_ERROR_UNSUPPORTED;
  }
  // A presentable shared surface would need one allocation to serve both the
  // present path's ownership rules and the shared cache's, which is the
  // primary/DXGI contract this bridge still reports as missing. Refuse the
  // combination rather than half-implement it.
  if (shared && (presentable || !sharedSurfaceShape(*args) || !runtime.handle
      || !device->memory.available())) {
    return DXGI_ERROR_UNSUPPORTED;
  }
  try {
    // Reserve the intrusive retirement node before entering any backend or
    // runtime operation. DestroyResource cannot allocate under memory pressure.
    resource->retirement = std::make_unique<ResourceRetirement>();
    std::vector<D3D11_SUBRESOURCE_DATA> initial;
    if (args->pInitialDataUP) {
      initial.resize(size_t(args->MipLevels) * args->ArraySize);
      for (size_t i = 0; i < initial.size(); i++) {
        if (!args->pInitialDataUP[i].pSysMem) return E_INVALIDARG;
        initial[i].pSysMem = args->pInitialDataUP[i].pSysMem;
        initial[i].SysMemPitch = args->pInitialDataUP[i].SysMemPitch;
        initial[i].SysMemSlicePitch = args->pInitialDataUP[i].SysMemSlicePitch;
      }
    }
    auto data = initial.empty() ? nullptr : initial.data();
    HRESULT hr = E_INVALIDARG;
    if (args->ResourceDimension == D3D10DDIRESOURCE_BUFFER && args->MipLevels == 1 && args->ArraySize == 1) {
      D3D11_BUFFER_DESC desc = {};
      desc.ByteWidth = args->pMipInfoList[0].TexelWidth;
      desc.Usage = static_cast<D3D11_USAGE>(args->Usage);
      desc.BindFlags = args->BindFlags & D3D10_DDI_BIND_PIPELINE_MASK;
      desc.CPUAccessFlags = ((args->MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
                         | ((args->MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
      ComPtr<ID3D11Buffer> buffer;
      hr = device->backend->CreateBuffer(&desc, data, &buffer);
      resource->backend = buffer;
    } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
      ComPtr<ID3D11Texture1D> texture;
      hr = device->backend->CreateTexture1D(&oneDimensional, data, &texture);
      resource->backend = texture;
    } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
      D3D11_TEXTURE2D_DESC desc = {};
      desc.Width = args->pMipInfoList[0].TexelWidth;
      desc.Height = args->pMipInfoList[0].TexelHeight;
      desc.MipLevels = args->MipLevels;
      desc.ArraySize = args->ArraySize;
      desc.Format = args->Format;
      desc.SampleDesc = args->SampleDesc;
      desc.Usage = static_cast<D3D11_USAGE>(args->Usage);
      desc.MiscFlags = miscFlags;
      desc.BindFlags = args->BindFlags & D3D10_DDI_BIND_PIPELINE_MASK;
      desc.CPUAccessFlags = ((args->MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
                         | ((args->MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
      ComPtr<ID3D11Texture2D> texture;
      hr = device->backend->CreateTexture2D(&desc, data, &texture);
      resource->backend = texture;
    }
    if (hr == S_OK && !resource->backend) hr = E_FAIL;
    if (hr == S_OK && presentable) {
      hr = device->memory.allocate(resource->allocation, runtime.handle,
        args->pMipInfoList[0].TexelWidth, args->pMipInfoList[0].TexelHeight, args->Format);
      if (FAILED(hr)) resource->backend.Reset();
    }
    if (hr == S_OK && shared) {
      hr = createSharedResource(device, resource, *args, runtime.handle);
      if (FAILED(hr)) { resource->shared.reset(); resource->backend.Reset(); }
    }
    return hr == S_OK || FAILED(hr) ? hr : E_FAIL;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
// Creating and opening share one lifetime contract, so they share one body.
// Both receive no DestroyResource when they fail, both publish into runtime
// private storage that a nested callback may reclaim, and both must leave no
// half-built owner behind. Keeping the two paths on the same code is what
// stops them drifting apart on exactly those rules.
template<typename Build>
void publishNewResource(Device* device, D3D10DDI_HRESOURCE out, Build&& build) {
  HRESULT hr = S_OK;
  std::shared_ptr<const char> reservation;
  if (!out.pDrvPrivate || uintptr_t(out.pDrvPrivate) % alignof(Resource)) {
    device->error(E_INVALIDARG); return;
  }
  try {
    reservation = std::make_shared<const char>(0);
    std::lock_guard<std::mutex> lock(resourceStorageMutex);
    if (!resourceStorage.emplace(out.pDrvPrivate,
        ResourceRecord{device, ResourcePhase::Creating, reservation}).second) hr = E_INVALIDARG;
  } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
    catch (...) { hr = E_FAIL; }
  if (FAILED(hr)) { device->error(hr); return; }
  {
    // Stage every backend/allocation owner locally and publish private storage
    // only when all steps succeeded. No runtime callback runs under the lock.
    Resource staged;
    hr = build(&staged);
    if (hr == S_OK) {
      std::lock_guard<std::mutex> lock(resourceStorageMutex);
      const auto entry = resourceStorage.find(out.pDrvPrivate);
      if (device->retired || entry == resourceStorage.end() || entry->second.reservation != reservation)
        hr = DXGI_ERROR_DEVICE_REMOVED;
      else {
        new (out.pDrvPrivate) Resource(std::move(staged));
        entry->second.phase = ResourcePhase::Live;
      }
    }
  } // Failed staged owners are fully destroyed before SetError can reenter.
  if (FAILED(hr)) {
    {
      std::lock_guard<std::mutex> lock(resourceStorageMutex);
      const auto entry = resourceStorage.find(out.pDrvPrivate);
      if (entry != resourceStorage.end() && entry->second.reservation == reservation)
        resourceStorage.erase(entry);
    }
    device->error(hr);
  }
}
void APIENTRY createResource(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATERESOURCE* args, D3D10DDI_HRESOURCE out,
    D3D10DDI_HRTRESOURCE runtime) {
  auto device = get(h);
  publishNewResource(device, out, [&](Resource* staged) {
    return createResourceData(device, args, staged, runtime);
  });
}
void APIENTRY openResource(D3D10DDI_HDEVICE h, const D3D10DDIARG_OPENRESOURCE* args,
    D3D10DDI_HRESOURCE out, D3D10DDI_HRTRESOURCE) {
  auto device = get(h);
  publishNewResource(device, out, [&](Resource* staged) {
    return openResourceData(device, args, staged);
  });
}
void APIENTRY destroyResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  {
    std::lock_guard<std::mutex> lock(resourceStorageMutex);
    const auto entry = resourceStorage.find(resource.pDrvPrivate);
    if (entry == resourceStorage.end() || entry->second.owner != get(h)) return;
    const bool live = entry->second.phase == ResourcePhase::Live;
    resourceStorage.erase(entry);
    // Cancel unpublished creation without accessing runtime private storage.
    if (!live) return;
  }
  auto device = get(h);
  auto object = get(resource);
  auto retired = std::move(object->retirement);
  retired->resource.emplace(std::move(*object));
  object->~Resource();
  // D3D10 RenderCb may reenter here while its submit worker owns the Mesa
  // WDDM lock. Retire private bytes now; final COM/HRESOURCE release runs only
  // after the outer callback has returned, before its outer DDI returns.
  device->service->retire(retired.release());
}

SIZE_T APIENTRY shaderViewSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATESHADERRESOURCEVIEW*) {
  return sizeof(ShaderView);
}
// Translate an SRV for the actual 1D/2D resource, never reinterpret a DDI union.
void APIENTRY createShaderView(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATESHADERRESOURCEVIEW* args,
    D3D10DDI_HSHADERRESOURCEVIEW out, D3D10DDI_HRTSHADERRESOURCEVIEW) {
  auto device = get(h);
  if (!args) { device->error(E_INVALIDARG); return; }
  if (!owned(device, get(args->hDrvResource))) return;
  HRESULT hr;
  {
    auto resource = get(args->hDrvResource)->backend;
    // The view carries the surface, not a pointer back to the resource: a view
    // can outlive its resource DDI, and binding needs the surface after that.
    auto source = get(args->hDrvResource)->shared;
    hr = createViewStorage<ShaderView>(device, out.pDrvPrivate, [&](ShaderView& view) {
      view.shared = source;
      D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
      if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
        ComPtr<ID3D11Texture1D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE1D_DESC info = {}; texture->GetDesc(&info);
        if (!dxvk::umd::textureShaderView(*args, info, desc)) return E_INVALIDARG;
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC info = {}; texture->GetDesc(&info);
        if (!dxvk::umd::textureShaderView(*args, info, desc)) return E_INVALIDARG;
      } else return E_INVALIDARG;
      return device->backend->CreateShaderResourceView(resource.Get(), &desc, &view.backend);
    });
  }
  device->error(hr);
}
void APIENTRY destroyShaderView(D3D10DDI_HDEVICE h, D3D10DDI_HSHADERRESOURCEVIEW object) {
  auto view = get(object);
  if (!view || view->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), view, view->backend);
}
void APIENTRY generateMips(D3D10DDI_HDEVICE h, D3D10DDI_HSHADERRESOURCEVIEW object) {
  auto device = get(h); auto view = get(object);
  if (!view || view->owner != device || !view->backend) {
    device->error(E_INVALIDARG); return;
  }
  ComPtr<ID3D11Resource> resource;
  view->backend->GetResource(&resource);
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  if (resource) resource->GetType(&dimension);
  D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {}; view->backend->GetDesc(&viewDesc);
  HRESULT hr = E_INVALIDARG;
  if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
    ComPtr<ID3D11Texture1D> texture;
    if (FAILED(resource.As(&texture))) { device->error(E_INVALIDARG); return; }
    D3D11_TEXTURE1D_DESC desc = {}; texture->GetDesc(&desc);
    hr = dxvk::umd::mipGenerationStatus(desc, viewDesc);
  } else if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(resource.As(&texture))) { device->error(E_INVALIDARG); return; }
    D3D11_TEXTURE2D_DESC desc = {}; texture->GetDesc(&desc);
    hr = dxvk::umd::mipGenerationStatus(desc, viewDesc);
  }
  if (FAILED(hr)) { device->error(hr); return; }
  try {
    UINT support = 0;
    hr = device->backend->CheckFormatSupport(viewDesc.Format, &support);
    if (FAILED(hr)) { device->error(hr); return; }
    if (!(support & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN)) {
      device->error(E_INVALIDARG); return;
    }
    // Keep view-local mip semantics even on backends which mishandle 1D
    // array offsets. The helper uses ordered GPU copies, never CPU texels.
    device->error(dxvk::umd::generateViewMips(
      device->backend.Get(), device->context.Get(), view->backend.Get()));
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
template<dxvk::umd::ShaderStage Stage>
void APIENTRY setShaderResources(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D10DDI_HSHADERRESOURCEVIEW* objects) {
  auto device = get(h);
  constexpr UINT slots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && !objects)) { device->error(E_INVALIDARG); return; }
  ID3D11ShaderResourceView* views[slots] = {};
  for (UINT i = 0; i < count; i++) {
    auto view = get(objects[i]);
    if (view && (view->owner != device || !view->backend)) { device->error(E_INVALIDARG); return; }
    views[i] = view ? view->backend.Get() : nullptr;
  }
  // Record which slots a shared surface now occupies, so a draw refreshes what
  // it samples instead of sweeping every surface the device holds. The high
  // watermark keeps that per-draw scan proportional to what was ever bound.
  auto& bound = device->boundShared[unsigned(Stage)];
  auto& high = device->boundSharedHigh[unsigned(Stage)];
  for (UINT i = 0; i < count; i++) {
    auto view = get(objects[i]);
    bound[start + i] = view ? view->shared : nullptr;
    if (bound[start + i] && start + i >= high) high = start + i + 1;
  }
  try {
    if (Stage == dxvk::umd::ShaderStage::Vertex) device->context->VSSetShaderResources(start, count, views);
    else if (Stage == dxvk::umd::ShaderStage::Geometry) device->context->GSSetShaderResources(start, count, views);
    else device->context->PSSetShaderResources(start, count, views);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY samplerSize(D3D10DDI_HDEVICE, const D3D10_DDI_SAMPLER_DESC*) { return sizeof(Sampler); }
void APIENTRY createSampler(D3D10DDI_HDEVICE h, const D3D10_DDI_SAMPLER_DESC* args,
    D3D10DDI_HSAMPLER out, D3D10DDI_HRTSAMPLER) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto sampler = new (out.pDrvPrivate) Sampler(); sampler->owner = device;
  if (!args) { device->error(E_INVALIDARG); return; }
  D3D11_SAMPLER_DESC desc = {};
  desc.Filter = static_cast<D3D11_FILTER>(args->Filter);
  desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(args->AddressU);
  desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(args->AddressV);
  desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(args->AddressW);
  desc.MipLODBias = args->MipLODBias; desc.MaxAnisotropy = args->MaxAnisotropy;
  desc.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(args->ComparisonFunc);
  for (unsigned i = 0; i < 4; i++) desc.BorderColor[i] = args->BorderColor[i];
  desc.MinLOD = args->MinLOD; desc.MaxLOD = args->MaxLOD;
  createChildBackend(device, sampler, [&] { return device->backend->CreateSamplerState(&desc, &sampler->backend); });
}
void APIENTRY destroySampler(D3D10DDI_HDEVICE h, D3D10DDI_HSAMPLER object) {
  auto sampler = get(object);
  if (!sampler || sampler->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), sampler, sampler->backend);
}
template<dxvk::umd::ShaderStage Stage>
void APIENTRY setSamplers(D3D10DDI_HDEVICE h, UINT start, UINT count, const D3D10DDI_HSAMPLER* objects) {
  auto device = get(h);
  constexpr UINT slots = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && !objects)) { device->error(E_INVALIDARG); return; }
  ID3D11SamplerState* samplers[slots] = {};
  for (UINT i = 0; i < count; i++) {
    auto sampler = get(objects[i]);
    if (sampler && (sampler->owner != device || !sampler->backend)) { device->error(E_INVALIDARG); return; }
    samplers[i] = sampler ? sampler->backend.Get() : nullptr;
  }
  try {
    if (Stage == dxvk::umd::ShaderStage::Vertex) device->context->VSSetSamplers(start, count, samplers);
    else if (Stage == dxvk::umd::ShaderStage::Geometry) device->context->GSSetSamplers(start, count, samplers);
    else device->context->PSSetSamplers(start, count, samplers);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY targetSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATERENDERTARGETVIEW*) {
  return sizeof(RenderTarget);
}
// Stage the actual Texture1D/Texture2D target and its retirement owner together.
void APIENTRY createTarget(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATERENDERTARGETVIEW* args,
    D3D10DDI_HRENDERTARGETVIEW out, D3D10DDI_HRTRENDERTARGETVIEW) {
  auto device = get(h);
  if (!args) { device->error(E_INVALIDARG); return; }
  if (!owned(device, get(args->hDrvResource))) return;
  HRESULT hr;
  {
    auto resource = get(args->hDrvResource)->backend;
    auto source = get(args->hDrvResource)->shared;
    hr = createViewStorage<RenderTarget>(device, out.pDrvPrivate, [&](RenderTarget& target) {
      target.shared = source;
      D3D11_RENDER_TARGET_VIEW_DESC desc = {};
      if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
        ComPtr<ID3D11Texture1D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE1D_DESC info = {}; texture->GetDesc(&info);
        if (!dxvk::umd::textureTargetView(*args, info, desc)) return E_INVALIDARG;
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC info = {}; texture->GetDesc(&info);
        if (!dxvk::umd::textureTargetView(*args, info, desc)) return E_INVALIDARG;
      } else return E_INVALIDARG;
      target.format = desc.Format;
      return device->backend->CreateRenderTargetView(resource.Get(), &desc, &target.backend);
    });
  }
  device->error(hr);
}
void APIENTRY destroyTarget(D3D10DDI_HDEVICE h, D3D10DDI_HRENDERTARGETVIEW target) {
  auto object = get(target);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), object, object->backend);
}
void APIENTRY clearTarget(D3D10DDI_HDEVICE h, D3D10DDI_HRENDERTARGETVIEW target, FLOAT color[4]) {
  auto device = get(h);
  if (!owned(device, get(target))) return;
  device->context->ClearRenderTargetView(get(target)->backend.Get(), color);
  // A shared surface is one mip of one slice, so its view is the whole image
  // and this clear defines all of it. No refresh is owed.
  wroteSharedSurface(device, get(target)->shared);
}
SIZE_T APIENTRY depthViewSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATEDEPTHSTENCILVIEW*) {
  return sizeof(DepthView);
}
// Keep depth view creation atomic for both supported texture dimensions.
void APIENTRY createDepthView(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATEDEPTHSTENCILVIEW* args,
    D3D10DDI_HDEPTHSTENCILVIEW out, D3D10DDI_HRTDEPTHSTENCILVIEW) {
  auto device = get(h);
  if (!args) { device->error(E_INVALIDARG); return; }
  if (!owned(device, get(args->hDrvResource))) return;
  HRESULT hr;
  {
    auto resource = get(args->hDrvResource)->backend;
    hr = createViewStorage<DepthView>(device, out.pDrvPrivate, [&](DepthView& view) {
      D3D11_DEPTH_STENCIL_VIEW_DESC desc = {}; desc.Format = args->Format;
      if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
        ComPtr<ID3D11Texture1D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE1D_DESC info = {}; texture->GetDesc(&info);
        if (!dxvk::umd::textureDepthView(*args, info, desc)) return E_INVALIDARG;
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_DEPTH_STENCIL) ||
            !info.SampleDesc.Count || args->Tex2D.MipSlice >= info.MipLevels ||
            !dxvk::umd::viewRange(args->Tex2D.FirstArraySlice, args->Tex2D.ArraySize, info.ArraySize))
          return E_INVALIDARG;
        if (info.SampleDesc.Count > 1) {
          if (args->Tex2D.MipSlice) return E_INVALIDARG;
          desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
          desc.Texture2DMSArray.FirstArraySlice = args->Tex2D.FirstArraySlice;
          desc.Texture2DMSArray.ArraySize = args->Tex2D.ArraySize;
        } else {
          desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
          desc.Texture2DArray.MipSlice = args->Tex2D.MipSlice;
          desc.Texture2DArray.FirstArraySlice = args->Tex2D.FirstArraySlice;
          desc.Texture2DArray.ArraySize = args->Tex2D.ArraySize;
        }
      } else return E_INVALIDARG;
      return device->backend->CreateDepthStencilView(resource.Get(), &desc, &view.backend);
    });
  }
  device->error(hr);
}
void APIENTRY destroyDepthView(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILVIEW object) {
  auto view = get(object);
  if (!view || view->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), view, view->backend);
}
void APIENTRY clearDepthView(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILVIEW object,
    UINT flags, FLOAT depth, UINT8 stencil) {
  auto device = get(h); auto view = get(object);
  if (!view || view->owner != device || !view->backend
      || (flags & ~(D3D10_DDI_CLEAR_DEPTH | D3D10_DDI_CLEAR_STENCIL))) {
    device->error(E_INVALIDARG); return;
  }
  const UINT apiFlags = ((flags & D3D10_DDI_CLEAR_DEPTH) ? D3D11_CLEAR_DEPTH : 0)
                      | ((flags & D3D10_DDI_CLEAR_STENCIL) ? D3D11_CLEAR_STENCIL : 0);
  try { if (apiFlags) device->context->ClearDepthStencilView(view->backend.Get(), apiFlags, depth, stencil); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY copyResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, D3D10DDI_HRESOURCE src) {
  auto device = get(h);
  if (!owned(device, get(dst)) || !owned(device, get(src))) return;
  if (!readSharedSurface(device, get(src)->shared)) return;
  device->context->CopyResource(get(dst)->backend.Get(), get(src)->backend.Get());
  // CopyResource replaces the destination entirely.
  wroteSharedSurface(device, get(dst)->shared);
}

void APIENTRY resolveResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT dstIndex,
    D3D10DDI_HRESOURCE src, UINT srcIndex, DXGI_FORMAT format) {
  auto device = get(h);
  if (!owned(device, get(dst)) || !owned(device, get(src))) return;
  ComPtr<ID3D11Texture2D> destination, source;
  if (FAILED(get(dst)->backend.As(&destination)) || FAILED(get(src)->backend.As(&source))) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_TEXTURE2D_DESC dstDesc = {}, srcDesc = {};
  destination->GetDesc(&dstDesc); source->GetDesc(&srcDesc);
  if (!dxvk::umd::resolveSubresources(dstDesc, dstIndex, srcDesc, srcIndex, format)) {
    device->error(E_INVALIDARG); return;
  }
  try {
    UINT support = 0;
    const HRESULT hr = device->backend->CheckFormatSupport(format, &support);
    if (FAILED(hr)) { device->error(hr); return; }
    if (!(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
      device->error(DXGI_ERROR_UNSUPPORTED); return;
    }
    device->context->ResolveSubresource(destination.Get(), dstIndex, source.Get(), srcIndex, format);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

void APIENTRY checkFormat(D3D10DDI_HDEVICE h, DXGI_FORMAT format, UINT* output) {
  auto device = get(h);
  if (!output) { device->error(E_INVALIDARG); return; }
  *output = 0;
  try {
    UINT support = 0;
    const HRESULT hr = device->backend->CheckFormatSupport(format, &support);
    if (FAILED(hr)) { device->error(hr == E_INVALIDARG ? E_FAIL : hr); return; }
    *output = dxvk::umd::nativeFormatCaps(support);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

void APIENTRY checkMultisample(D3D10DDI_HDEVICE h, DXGI_FORMAT format, UINT count, UINT* output) {
  auto device = get(h);
  if (!output) { device->error(E_INVALIDARG); return; }
  *output = 0;
  if (!count || count > D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT) return;
  try { device->error(device->backend->CheckMultisampleQualityLevels(format, count, output)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}

struct SubresourceInfo {
  UINT width = 0, height = 1, texelBytes = 1;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
  UINT bindings = 0;
};
bool subresourceInfo(Resource* resource, UINT index, SubresourceInfo& info) {
  resource->backend->GetType(&info.dimension);
  if (info.dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
    if (index) return false;
    ComPtr<ID3D11Buffer> buffer;
    if (FAILED(resource->backend.As(&buffer))) return false;
    D3D11_BUFFER_DESC desc = {}; buffer->GetDesc(&desc);
    info.width = desc.ByteWidth; info.usage = desc.Usage; info.bindings = desc.BindFlags;
    return true;
  }
  if (info.dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
    ComPtr<ID3D11Texture1D> texture;
    if (FAILED(resource->backend.As(&texture))) return false;
    D3D11_TEXTURE1D_DESC desc = {}; texture->GetDesc(&desc);
    if (!desc.MipLevels || index / desc.MipLevels >= desc.ArraySize ||
        index % desc.MipLevels >= D3D11_REQ_MIP_LEVELS) return false;
    info.width = std::max(1u, desc.Width >> (index % desc.MipLevels));
    info.height = 1;
    info.format = desc.Format; info.usage = desc.Usage; info.bindings = desc.BindFlags;
    info.texelBytes = dxvk::umd::transferTexelBytes(info.format);
    return info.texelBytes != 0;
  }
  if (info.dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(resource->backend.As(&texture))) return false;
    D3D11_TEXTURE2D_DESC desc = {}; texture->GetDesc(&desc);
    if (!desc.MipLevels || index / desc.MipLevels >= desc.ArraySize || desc.SampleDesc.Count != 1)
      return false;
    const UINT mip = index % desc.MipLevels;
    if (mip >= D3D11_REQ_MIP_LEVELS) return false;
    info.width = desc.Width >> mip; if (!info.width) info.width = 1;
    info.height = desc.Height >> mip; if (!info.height) info.height = 1;
    info.format = desc.Format; info.usage = desc.Usage; info.bindings = desc.BindFlags;
    info.texelBytes = dxvk::umd::transferTexelBytes(info.format);
    return info.texelBytes != 0;
  }
  return false;
}
bool subresourceBox(const SubresourceInfo& info, const D3D10_DDI_BOX* input, D3D11_BOX& box) {
  if (input && (input->left < 0 || input->top < 0 || input->front < 0 ||
      input->right < 0 || input->bottom < 0 || input->back < 0)) return false;
  box = input ? D3D11_BOX{UINT(input->left), UINT(input->top), UINT(input->front),
                         UINT(input->right), UINT(input->bottom), UINT(input->back)}
              : D3D11_BOX{0, 0, 0, info.width, info.height, 1};
  return box.left <= box.right && box.top <= box.bottom && box.front <= box.back
      && box.right <= info.width && box.bottom <= info.height && box.back <= 1;
}
void APIENTRY copyRegion(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT dstIndex,
    UINT x, UINT y, UINT z, D3D10DDI_HRESOURCE src, UINT srcIndex, const D3D10_DDI_BOX* input) {
  auto device = get(h);
  if (!owned(device, get(dst)) || !owned(device, get(src))) return;
  SubresourceInfo source, destination; D3D11_BOX box;
  if (!subresourceInfo(get(src), srcIndex, source) || !subresourceInfo(get(dst), dstIndex, destination)
      || source.dimension != destination.dimension || source.format != destination.format
      || ((source.bindings | destination.bindings) & D3D11_BIND_DEPTH_STENCIL)
      || destination.usage == D3D11_USAGE_IMMUTABLE || !subresourceBox(source, input, box)) {
    device->error(E_INVALIDARG); return;
  }
  if (box.left == box.right || box.top == box.bottom || box.front == box.back) return;
  if ((get(src)->backend.Get() == get(dst)->backend.Get() && srcIndex == dstIndex)
      || x > destination.width || box.right - box.left > destination.width - x
      || y > destination.height || box.bottom - box.top > destination.height - y || z) {
    device->error(E_INVALIDARG); return;
  }
  if (!readSharedSurface(device, get(src)->shared)) return;
  if (!wroteSharedRegion(device, get(dst)->shared)) return;
  try {
    device->context->CopySubresourceRegion(get(dst)->backend.Get(), dstIndex, x, y, z,
      get(src)->backend.Get(), srcIndex, input ? &box : nullptr);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY updateResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT index,
    const D3D10_DDI_BOX* input, const void* source, UINT rowPitch, UINT depthPitch) {
  auto device = get(h);
  if (!owned(device, get(dst))) return;
  SubresourceInfo destination; D3D11_BOX box;
  if (!subresourceInfo(get(dst), index, destination) || destination.usage != D3D11_USAGE_DEFAULT
      || (destination.bindings & D3D11_BIND_DEPTH_STENCIL)
      || !subresourceBox(destination, input, box)
      || (input && (destination.bindings & D3D11_BIND_CONSTANT_BUFFER))) {
    device->error(E_INVALIDARG); return;
  }
  if (box.left == box.right || box.top == box.bottom || box.front == box.back) return;
  uint64_t requiredBytes = 0;
  // Do not assume RGBA8. Validate the last byte on both 32-bit and 64-bit
  // callers before the backend can read rows from runtime-owned source data.
  if (!source || !dxvk::umd::uploadSpan(box.right - box.left, box.bottom - box.top,
      destination.texelBytes, rowPitch, destination.dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D,
      uint64_t(UINTPTR_MAX) - reinterpret_cast<uintptr_t>(source) + 1, requiredBytes)) {
    device->error(E_INVALIDARG); return;
  }
  // Without a box the update replaces the whole subresource; with one it does
  // not, and the untouched pixels are still the other process's.
  if (input) {
    if (!wroteSharedRegion(device, get(dst)->shared)) return;
  } else {
    wroteSharedSurface(device, get(dst)->shared);
  }
  try {
    device->context->UpdateSubresource(get(dst)->backend.Get(), index, input ? &box : nullptr,
      source, rowPitch, depthPitch);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY mapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource,
    UINT subresource, D3D10_DDI_MAP type, UINT flags, D3D10DDI_MAPPED_SUBRESOURCE* out) {
  auto device = get(h);
  if (!out) { device->error(E_INVALIDARG); return; }
  *out = {};
  if (!owned(device, get(resource))) return;
  try {
    device->error(dxvk::umd::mapSubresource(type, flags, out,
      [&](D3D11_MAP apiType, UINT apiFlags, D3D11_MAPPED_SUBRESOURCE* mapped) {
        return device->context->Map(get(resource)->backend.Get(), subresource, apiType, apiFlags, mapped);
      }));
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
BOOL APIENTRY isStagingBusy(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return TRUE;
  BOOL busy = TRUE;
  const HRESULT removed = device->backend->GetDeviceRemovedReason();
  if (FAILED(removed)) { device->error(removed); return TRUE; }
  device->error(dxvk::umd::isStagingResourceBusy(device->context.Get(),
    get(resource)->backend.Get(), &busy));
  return busy;
}
void APIENTRY resourceHazard(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  get(resource)->backend->GetType(&dimension);
  if (dimension != D3D11_RESOURCE_DIMENSION_BUFFER) {
    device->error(E_INVALIDARG); return;
  }
  // DXVK tracks Vulkan access transitions at the ensuing buffer bind/use.
  // This notification does not require a CPU/GPU synchronization operation.
}
void APIENTRY shaderViewHazard(D3D10DDI_HDEVICE h, D3D10DDI_HSHADERRESOURCEVIEW object,
    D3D10DDI_HRESOURCE resource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return;
  auto view = get(object);
  if (!view || view->owner != device || !view->backend) {
    device->error(E_INVALIDARG); return;
  }
  ComPtr<ID3D11Resource> viewed;
  view->backend->GetResource(&viewed);
  if (viewed.Get() != get(resource)->backend.Get()) {
    device->error(E_INVALIDARG); return;
  }
  // The embedded view retains its exact subresource range. DXVK's resource
  // tracking inserts the necessary Vulkan barriers when that view is used.
}
void APIENTRY unmapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT subresource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return;
  try { device->context->Unmap(get(resource)->backend.Get(), subresource); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY shaderSize(D3D10DDI_HDEVICE, const UINT*, const D3D10DDIARG_STAGE_IO_SIGNATURES*) {
  return sizeof(Shader);
}
void createShader(D3D10DDI_HDEVICE h, const UINT* code, D3D10DDI_HSHADER out,
    const D3D10DDIARG_STAGE_IO_SIGNATURES* signature, dxvk::umd::ShaderStage stage,
    const D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* streamOutput = nullptr) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto shader = new (out.pDrvPrivate) Shader();
  shader->owner = device;
  shader->stage = stage;
  if (!code || !signature || signature->NumInputSignatureEntries > 32 ||
      !signature->NumOutputSignatureEntries || signature->NumOutputSignatureEntries > 32 || !signature->pOutputSignature ||
      (signature->NumInputSignatureEntries && !signature->pInputSignature)) {
    device->error(E_INVALIDARG); return;
  }
  static_assert(D3D10_SB_NAME_POSITION == 1 && D3D10_SB_NAME_VERTEX_ID == 6);
  try {
    Shader candidate; candidate.owner = device; candidate.stage = stage;
    candidate.retirement = std::make_unique<ComRetirement>();
    if (streamOutput) {
      if (!dxvk::umd::streamOutputDeclaration(*streamOutput, *signature, candidate.streamOutput)) {
        device->error(E_INVALIDARG); return;
      }
      candidate.withStreamOutput = true;
    }
    for (UINT i = 0; i < signature->NumInputSignatureEntries; i++) {
      const auto& entry = signature->pInputSignature[i];
      candidate.inputs.push_back({uint32_t(entry.SystemValue), entry.Register, entry.Mask});
      candidate.needsLayout |= stage == dxvk::umd::ShaderStage::Vertex && entry.SystemValue == D3D10_SB_NAME_UNDEFINED;
    }
    for (UINT i = 0; i < signature->NumOutputSignatureEntries; i++) {
      const auto& entry = signature->pOutputSignature[i];
      candidate.outputs.push_back({uint32_t(entry.SystemValue), entry.Register, entry.Mask});
      candidate.needsLinkage |= stage != dxvk::umd::ShaderStage::Pixel && entry.SystemValue == D3D10_SB_NAME_UNDEFINED;
    }
    if (stage == dxvk::umd::ShaderStage::Pixel) {
      std::vector<dxvk::umd::ShaderSignatureEntry> resolved;
      if (!dxvk::umd::resolvePixelInputs(code, code[1], candidate.inputs.data(), candidate.inputs.size(), resolved)) {
        device->error(E_INVALIDARG); return;
      }
      candidate.inputs = std::move(resolved);
    } else if (stage == dxvk::umd::ShaderStage::Geometry) {
      std::vector<dxvk::umd::ShaderSignatureEntry> resolved;
      if (!dxvk::umd::resolveGeometryInputs(code, code[1], candidate.inputs.data(), candidate.inputs.size(), resolved)) {
        device->error(E_INVALIDARG); return;
      }
      candidate.inputs = std::move(resolved);
      candidate.needsLinkage = true;
    }
    auto validationInputs = candidate.inputs;
    auto validationOutputs = candidate.outputs;
    // Validate raw tokens and register structure now. These provisional
    // signature types are discarded and never enter the DXVK compiler.
    // The bound layout supplies actual types when the shader is first drawn.
    if (stage != dxvk::umd::ShaderStage::Pixel) {
      if (stage == dxvk::umd::ShaderStage::Vertex)
        for (auto& input : validationInputs)
          if (!input.systemValue) input.scalar = dxvk::umd::ShaderScalar::Float32;
      for (auto& output : validationOutputs)
        if (!output.systemValue) output.scalar = dxvk::umd::ShaderScalar::Uint32;
    }
    std::vector<unsigned char> bytecode;
    if (!dxvk::umd::buildShaderContainer(stage, code, code[1], validationInputs.data(),
        validationInputs.size(), validationOutputs.data(), validationOutputs.size(), bytecode)) {
      device->error(E_INVALIDARG); return;
    }
    HRESULT hr = S_OK;
    if (candidate.needsLayout || candidate.needsLinkage) candidate.code.assign(code, code + code[1]);
    else if (stage == dxvk::umd::ShaderStage::Vertex)
      hr = device->backend->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &candidate.vertex);
    else hr = device->backend->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &candidate.pixel);
    if (FAILED(hr)) { device->error(hr); return; }
    *shader = std::move(candidate);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY createVertexShader(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER out, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* sig) {
  createShader(h, code, out, sig, dxvk::umd::ShaderStage::Vertex);
}
void APIENTRY createPixelShader(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER out, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* sig) {
  createShader(h, code, out, sig, dxvk::umd::ShaderStage::Pixel);
}
void APIENTRY createGeometryShader(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER out, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* sig) {
  createShader(h, code, out, sig, dxvk::umd::ShaderStage::Geometry);
}
SIZE_T APIENTRY geometryStreamSize(D3D10DDI_HDEVICE,
    const D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT*, const D3D10DDIARG_STAGE_IO_SIGNATURES*) {
  return sizeof(Shader);
}
void APIENTRY createGeometryStream(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* args,
    D3D10DDI_HSHADER out, D3D10DDI_HRTSHADER,
    const D3D10DDIARG_STAGE_IO_SIGNATURES* signature) {
  if (!args) { get(h)->error(E_INVALIDARG); return; }
  // This slice accepts actual GS bytecode. Null-GS signature-only passthrough
  // remains an explicit admission gap rather than an invented shader.
  createShader(h, args->pShaderCode, out, signature, dxvk::umd::ShaderStage::Geometry, args);
}
void APIENTRY destroyShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto object = get(shader);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  if (get(h)->vertexShader == object) {
    get(h)->context->VSSetShader(nullptr, nullptr, 0);
    get(h)->vertexShader = nullptr; get(h)->vertexBound = false;
  }
  if (get(h)->pixelShader == object) {
    get(h)->context->PSSetShader(nullptr, nullptr, 0);
    get(h)->pixelShader = nullptr; get(h)->pixelBound = false;
  }
  if (get(h)->geometryShader == object) {
    get(h)->context->GSSetShader(nullptr, nullptr, 0);
    get(h)->geometryShader = nullptr;
  }
  retireChild(get(h), object, object->vertex, object->geometry, object->pixel);
}
void APIENTRY setVertexShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h);
  auto object = get(shader);
  if (object && (object->owner != device || object->stage != dxvk::umd::ShaderStage::Vertex
      || (!object->vertex && !object->needsLayout && !object->needsLinkage))) { device->error(E_INVALIDARG); return; }
  device->context->VSSetShader(object ? object->vertex.Get() : nullptr, nullptr, 0);
  device->vertexShader = object;
  device->vertexBound = object != nullptr;
}
void APIENTRY setPixelShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h);
  auto object = get(shader);
  if (object && (object->owner != device || !object->pixel)) { device->error(E_INVALIDARG); return; }
  device->context->PSSetShader(object ? object->pixel.Get() : nullptr, nullptr, 0);
  device->pixelShader = object;
  device->pixelBound = object != nullptr;
}
void APIENTRY setGeometryShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h); auto object = get(shader);
  if (object && (object->owner != device || object->stage != dxvk::umd::ShaderStage::Geometry
      || object->code.empty())) { device->error(E_INVALIDARG); return; }
  try {
    device->context->GSSetShader(object ? object->geometry.Get() : nullptr, nullptr, 0);
    device->geometryShader = object;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
template<dxvk::umd::ShaderStage Stage>
void APIENTRY setConstantBuffers(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D10DDI_HRESOURCE* resources) {
  auto device = get(h);
  constexpr UINT slots = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && !resources)) {
    device->error(E_INVALIDARG); return;
  }
  ComPtr<ID3D11Buffer> ownedBuffers[slots];
  ID3D11Buffer* buffers[slots] = {};
  for (UINT i = 0; i < count; i++) {
    if (!resources[i].pDrvPrivate) continue;
    auto resource = get(resources[i]);
    if (!owned(device, resource)) return;
    if (FAILED(resource->backend.As(&ownedBuffers[i]))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc = {}; ownedBuffers[i]->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)) { device->error(E_INVALIDARG); return; }
    buffers[i] = ownedBuffers[i].Get();
  }
  // Validate every resource before changing state, so an invalid tail cannot
  // leave a partially updated binding range. Null entries explicitly unbind.
  try {
    if (Stage == dxvk::umd::ShaderStage::Vertex) device->context->VSSetConstantBuffers(start, count, buffers);
    else if (Stage == dxvk::umd::ShaderStage::Geometry) device->context->GSSetConstantBuffers(start, count, buffers);
    else device->context->PSSetConstantBuffers(start, count, buffers);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
// Validate the whole output-merger transaction before changing any binding.
void APIENTRY setRenderTargets(
    D3D10DDI_HDEVICE h,
    const D3D10DDI_HRENDERTARGETVIEW* targets,
    UINT count,
    UINT clear,
    D3D10DDI_HDEPTHSTENCILVIEW depth) {
  auto device = get(h);
  constexpr UINT slots = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
  if (!dxvk::umd::validRenderTargetRange(count, clear) || (count && !targets)) {
    device->error(E_INVALIDARG); return;
  }
  auto depthView = get(depth);
  if (depthView && (depthView->owner != device || !depthView->backend)) {
    device->error(E_INVALIDARG); return;
  }
  std::array<ID3D11RenderTargetView*, slots> translated = {};
  std::array<dxvk::umd::OutputView, slots> views;
  // Staged beside the translated bindings and committed with them: an early
  // return here leaves the pipeline binding unchanged, so the record of which
  // shared surfaces are bound must not change either.
  std::array<std::shared_ptr<dxvk::umd::SharedSurface>, slots> staged;
  dxvk::umd::OutputShape shape = {};
  bool anyColor = false;
  for (UINT i = 0; i < count; i++) {
    if (!targets[i].pDrvPrivate) continue;
    auto object = get(targets[i]);
    if (!owned(device, object)) return;
    // The shader bridge currently reconstructs float outputs only.
    if (object->format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        object->format != DXGI_FORMAT_B8G8R8A8_UNORM) {
      device->error(E_INVALIDARG); return;
    }
    if (!dxvk::umd::outputView(object->backend.Get(), views[i]) ||
        !dxvk::umd::mergeOutputShape(shape, views[i].shape)) {
      device->error(E_INVALIDARG); return;
    }
    for (UINT j = 0; j < i; j++) {
      if (translated[j] && dxvk::umd::overlappingOutputs(views[j], views[i])) {
        device->error(E_INVALIDARG); return;
      }
    }
    translated[i] = object->backend.Get();
    staged[i] = object->shared;
    anyColor = true;
  }
  if (depthView) {
    dxvk::umd::OutputShape depthShape;
    if (!dxvk::umd::depthOutputShape(depthView->backend.Get(), depthShape) ||
        !dxvk::umd::mergeOutputShape(shape, depthShape)) {
      device->error(E_INVALIDARG); return;
    }
  }
  // ClearSlots is a hint, not a partial update. Preserve null slots and clear
  // the complete omitted tail, even with clear=0 and count=0.
  try {
    device->context->OMSetRenderTargets(count, count ? translated.data() : nullptr,
      depthView ? depthView->backend.Get() : nullptr);
    device->targetShared = std::move(staged);
    device->targetBound = anyColor;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY setViewports(D3D10DDI_HDEVICE h, UINT count, UINT clear, const D3D10_DDI_VIEWPORT* views) {
  auto device = get(h);
  try {
    if (!dxvk::umd::replaceViewports(count, clear, views,
        [&](UINT size, const D3D11_VIEWPORT* translated) {
          device->context->RSSetViewports(size, translated);
        })) { device->error(E_INVALIDARG); return; }
    device->viewportBound = count != 0;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY setScissors(D3D10DDI_HDEVICE h, UINT count, UINT clear, const D3D10_DDI_RECT* input) {
  auto device = get(h);
  constexpr UINT slots = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  if (count > slots || clear > slots - count || (count && !input)) { device->error(E_INVALIDARG); return; }
  D3D11_RECT rects[slots] = {};
  for (UINT i = 0; i < count; i++)
    rects[i] = {input[i].left, input[i].top, input[i].right, input[i].bottom};
  try { device->context->RSSetScissorRects(count, count ? rects : nullptr); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY rasterizerSize(D3D10DDI_HDEVICE, const D3D10_DDI_RASTERIZER_DESC*) { return sizeof(Rasterizer); }
void APIENTRY createRasterizer(D3D10DDI_HDEVICE h, const D3D10_DDI_RASTERIZER_DESC* args,
    D3D10DDI_HRASTERIZERSTATE out, D3D10DDI_HRTRASTERIZERSTATE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto object = new (out.pDrvPrivate) Rasterizer();
  object->owner = device;
  if (!args) { device->error(E_INVALIDARG); return; }
  D3D11_RASTERIZER_DESC desc = {};
  desc.FillMode = static_cast<D3D11_FILL_MODE>(args->FillMode);
  desc.CullMode = static_cast<D3D11_CULL_MODE>(args->CullMode);
  desc.FrontCounterClockwise = args->FrontCounterClockwise;
  desc.DepthBias = args->DepthBias; desc.DepthBiasClamp = args->DepthBiasClamp;
  desc.SlopeScaledDepthBias = args->SlopeScaledDepthBias;
  desc.DepthClipEnable = args->DepthClipEnable; desc.ScissorEnable = args->ScissorEnable;
  desc.MultisampleEnable = args->MultisampleEnable; desc.AntialiasedLineEnable = args->AntialiasedLineEnable;
  createChildBackend(device, object, [&] { return device->backend->CreateRasterizerState(&desc, &object->backend); });
}
void APIENTRY destroyRasterizer(D3D10DDI_HDEVICE h, D3D10DDI_HRASTERIZERSTATE state) {
  auto object = get(state);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), object, object->backend);
}
void APIENTRY setRasterizer(D3D10DDI_HDEVICE h, D3D10DDI_HRASTERIZERSTATE state) {
  auto device = get(h);
  auto object = get(state);
  if (object && (object->owner != device || !object->backend)) { device->error(E_INVALIDARG); return; }
  device->context->RSSetState(object ? object->backend.Get() : nullptr);
}
void APIENTRY setTopology(D3D10DDI_HDEVICE h, D3D10_DDI_PRIMITIVE_TOPOLOGY topology) {
  auto device = get(h);
  D3D11_PRIMITIVE_TOPOLOGY api;
  if (!dxvk::umd::primitiveTopology(topology, api)) { device->error(E_INVALIDARG); return; }
  try {
    device->context->IASetPrimitiveTopology(api);
    device->topologyBound = api != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY blendSize(D3D10DDI_HDEVICE, const D3D10_DDI_BLEND_DESC*) { return sizeof(BlendState); }
void APIENTRY createBlend(D3D10DDI_HDEVICE h, const D3D10_DDI_BLEND_DESC* args,
    D3D10DDI_HBLENDSTATE out, D3D10DDI_HRTBLENDSTATE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto state = new (out.pDrvPrivate) BlendState(); state->owner = device;
  if (!args) { device->error(E_INVALIDARG); return; }
  D3D11_BLEND_DESC desc = {};
  desc.AlphaToCoverageEnable = args->AlphaToCoverageEnable;
  // Match DXVK's D3D10Device translation: enables and masks are per target,
  // while factors/equations are shared by the D3D10.0 descriptor.
  desc.IndependentBlendEnable = TRUE;
  for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
    auto& target = desc.RenderTarget[i];
    target.BlendEnable = args->BlendEnable[i];
    target.RenderTargetWriteMask = args->RenderTargetWriteMask[i];
    target.SrcBlend = static_cast<D3D11_BLEND>(args->SrcBlend);
    target.DestBlend = static_cast<D3D11_BLEND>(args->DestBlend);
    target.BlendOp = static_cast<D3D11_BLEND_OP>(args->BlendOp);
    target.SrcBlendAlpha = static_cast<D3D11_BLEND>(args->SrcBlendAlpha);
    target.DestBlendAlpha = static_cast<D3D11_BLEND>(args->DestBlendAlpha);
    target.BlendOpAlpha = static_cast<D3D11_BLEND_OP>(args->BlendOpAlpha);
  }
  createChildBackend(device, state, [&] { return device->backend->CreateBlendState(&desc, &state->backend); });
}
void APIENTRY destroyBlend(D3D10DDI_HDEVICE h, D3D10DDI_HBLENDSTATE object) {
  auto state = get(object);
  if (!state || state->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), state, state->backend);
}
void APIENTRY setBlend(D3D10DDI_HDEVICE h, D3D10DDI_HBLENDSTATE object, const FLOAT factor[4], UINT sampleMask) {
  auto device = get(h); auto state = get(object);
  if (state && (state->owner != device || !state->backend)) { device->error(E_INVALIDARG); return; }
  try { device->context->OMSetBlendState(state ? state->backend.Get() : nullptr, factor, sampleMask); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY depthStateSize(D3D10DDI_HDEVICE, const D3D10_DDI_DEPTH_STENCIL_DESC*) { return sizeof(DepthState); }
void APIENTRY createDepthState(D3D10DDI_HDEVICE h, const D3D10_DDI_DEPTH_STENCIL_DESC* args,
    D3D10DDI_HDEPTHSTENCILSTATE out, D3D10DDI_HRTDEPTHSTENCILSTATE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto state = new (out.pDrvPrivate) DepthState(); state->owner = device;
  if (!args || (args->StencilEnable && !args->FrontEnable && !args->BackEnable)) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_DEPTH_STENCIL_DESC desc = {};
  desc.DepthEnable = args->DepthEnable;
  desc.DepthWriteMask = static_cast<D3D11_DEPTH_WRITE_MASK>(args->DepthWriteMask);
  desc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(args->DepthFunc);
  desc.StencilEnable = args->StencilEnable;
  desc.StencilReadMask = args->StencilReadMask; desc.StencilWriteMask = args->StencilWriteMask;
  auto face = [](const D3D10_DDI_DEPTH_STENCILOP_DESC& input, bool enabled) {
    // The native DDI has per-face enables, unlike the public D3D11 API.
    // A disabled face must not compare or modify stencil contents.
    if (!enabled) return D3D11_DEPTH_STENCILOP_DESC {
      D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};
    return D3D11_DEPTH_STENCILOP_DESC {
      static_cast<D3D11_STENCIL_OP>(input.StencilFailOp),
      static_cast<D3D11_STENCIL_OP>(input.StencilDepthFailOp),
      static_cast<D3D11_STENCIL_OP>(input.StencilPassOp),
      static_cast<D3D11_COMPARISON_FUNC>(input.StencilFunc)};
  };
  desc.FrontFace = face(args->FrontFace, args->StencilEnable && args->FrontEnable);
  desc.BackFace = face(args->BackFace, args->StencilEnable && args->BackEnable);
  createChildBackend(device, state, [&] { return device->backend->CreateDepthStencilState(&desc, &state->backend); });
}
void APIENTRY destroyDepthState(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILSTATE object) {
  auto state = get(object);
  if (!state || state->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), state, state->backend);
}
void APIENTRY setDepthState(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILSTATE object, UINT stencil) {
  auto device = get(h); auto state = get(object);
  if (state && (state->owner != device || !state->backend)) { device->error(E_INVALIDARG); return; }
  try { device->context->OMSetDepthStencilState(state ? state->backend.Get() : nullptr, stencil); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
bool inputFormat(DXGI_FORMAT format, dxvk::umd::ShaderScalar& scalar, uint8_t& mask) {
  using Scalar = dxvk::umd::ShaderScalar;
  switch (format) {
    case DXGI_FORMAT_R32_FLOAT: scalar = Scalar::Float32; mask = 1; return true;
    case DXGI_FORMAT_R32G32_FLOAT: scalar = Scalar::Float32; mask = 3; return true;
    case DXGI_FORMAT_R32G32B32_FLOAT: scalar = Scalar::Float32; mask = 7; return true;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: scalar = Scalar::Float32; mask = 15; return true;
    case DXGI_FORMAT_R32_UINT: scalar = Scalar::Uint32; mask = 1; return true;
    case DXGI_FORMAT_R32G32_UINT: scalar = Scalar::Uint32; mask = 3; return true;
    case DXGI_FORMAT_R32G32B32_UINT: scalar = Scalar::Uint32; mask = 7; return true;
    case DXGI_FORMAT_R32G32B32A32_UINT: scalar = Scalar::Uint32; mask = 15; return true;
    case DXGI_FORMAT_R32_SINT: scalar = Scalar::Sint32; mask = 1; return true;
    case DXGI_FORMAT_R32G32_SINT: scalar = Scalar::Sint32; mask = 3; return true;
    case DXGI_FORMAT_R32G32B32_SINT: scalar = Scalar::Sint32; mask = 7; return true;
    case DXGI_FORMAT_R32G32B32A32_SINT: scalar = Scalar::Sint32; mask = 15; return true;
    default: return false;
  }
}
SIZE_T APIENTRY layoutSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATEELEMENTLAYOUT*) { return sizeof(InputLayout); }
void APIENTRY createLayout(D3D10DDI_HDEVICE h, const D3D10DDIARG_CREATEELEMENTLAYOUT* args,
    D3D10DDI_HELEMENTLAYOUT out, D3D10DDI_HRTELEMENTLAYOUT) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto layout = new (out.pDrvPrivate) InputLayout(); layout->owner = device;
  if (!args || args->NumElements > 32 || (args->NumElements && !args->pVertexElements)) {
    device->error(E_INVALIDARG); return;
  }
  try {
    InputLayout candidate; candidate.owner = device;
    candidate.retirement = std::make_unique<ComRetirement>();
    D3D11_INPUT_ELEMENT_DESC elements[32] = {};
    dxvk::umd::ShaderSignatureEntry inputs[32] = {};
    for (UINT i = 0; i < args->NumElements; i++) {
      const auto& input = args->pVertexElements[i];
      if (input.InputRegister >= 32 || input.InputSlot >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT
          || input.AlignedByteOffset > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES
          || input.AlignedByteOffset % 4
          || (input.InputSlotClass != D3D10_DDI_INPUT_PER_VERTEX_DATA && input.InputSlotClass != D3D10_DDI_INPUT_PER_INSTANCE_DATA)
          || (input.InputSlotClass == D3D10_DDI_INPUT_PER_VERTEX_DATA && input.InstanceDataStepRate)
          || candidate.inputTypes[input.InputRegister] != dxvk::umd::ShaderScalar::Unknown
          || !inputFormat(input.Format, inputs[i].scalar, inputs[i].mask)) {
        device->error(E_INVALIDARG); return;
      }
      for (UINT j = 0; j < i; j++)
        if (elements[j].InputSlot == input.InputSlot
            && (elements[j].InputSlotClass != static_cast<D3D11_INPUT_CLASSIFICATION>(input.InputSlotClass)
                || elements[j].InstanceDataStepRate != input.InstanceDataStepRate)) {
          device->error(E_INVALIDARG); return;
        }
      inputs[i].registerIndex = input.InputRegister;
      candidate.inputTypes[input.InputRegister] = inputs[i].scalar;
      elements[i] = {dxvk::umd::inputRegisterSemantic, input.InputRegister, input.Format,
        input.InputSlot, input.AlignedByteOffset, static_cast<D3D11_INPUT_CLASSIFICATION>(input.InputSlotClass),
        input.InstanceDataStepRate};
    }
    // CreateInputLayout only consumes the signature. The minimal code chunk
    // is never executed; no synthetic shader is substituted for the app VS.
    const uint32_t code[] = {0x10040,3,0x0100003e};
    dxvk::umd::ShaderSignatureEntry output = {1,0,15};
    std::vector<unsigned char> binary;
    if (!dxvk::umd::buildShaderContainer(dxvk::umd::ShaderStage::Vertex, code, 3,
        inputs, args->NumElements, &output, 1, binary)) { device->error(E_INVALIDARG); return; }
    const HRESULT hr = device->backend->CreateInputLayout(elements, args->NumElements,
      binary.data(), binary.size(), &candidate.backend);
    if (FAILED(hr)) { device->error(hr); return; }
    *layout = std::move(candidate);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyLayout(D3D10DDI_HDEVICE h, D3D10DDI_HELEMENTLAYOUT object) {
  auto device = get(h); auto layout = get(object);
  if (!layout || layout->owner != device) { device->error(E_INVALIDARG); return; }
  if (device->inputLayout == layout) {
    device->context->IASetInputLayout(nullptr); device->inputLayout = nullptr;
  }
  retireChild(device, layout, layout->backend);
}
void APIENTRY setLayout(D3D10DDI_HDEVICE h, D3D10DDI_HELEMENTLAYOUT object) {
  auto device = get(h); auto layout = get(object);
  if (layout && (layout->owner != device || !layout->backend)) { device->error(E_INVALIDARG); return; }
  try {
    device->context->IASetInputLayout(layout ? layout->backend.Get() : nullptr);
    device->inputLayout = layout;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY setVertexBuffers(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D10DDI_HRESOURCE* objects, const UINT* strides, const UINT* offsets) {
  auto device = get(h);
  constexpr UINT slots = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && (!objects || !strides || !offsets))) {
    device->error(E_INVALIDARG); return;
  }
  ID3D11Buffer* buffers[slots] = {};
  ComPtr<ID3D11Buffer> references[slots];
  for (UINT i = 0; i < count; i++) {
    if (!objects[i].pDrvPrivate) continue;
    auto resource = get(objects[i]);
    if (!owned(device, resource)) return;
    if (FAILED(resource->backend.As(&references[i]))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc = {}; references[i]->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_VERTEX_BUFFER) || offsets[i] > desc.ByteWidth
        || strides[i] > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES) { device->error(E_INVALIDARG); return; }
    buffers[i] = references[i].Get();
  }
  try { if (count) device->context->IASetVertexBuffers(start, count, buffers, strides, offsets); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY setStreamTargets(D3D10DDI_HDEVICE h, UINT count, UINT clear,
    const D3D10DDI_HRESOURCE* resources, const UINT* offsets) {
  auto device = get(h);
  constexpr UINT slots = D3D11_SO_BUFFER_SLOT_COUNT;
  if (count > slots || clear > slots-count || (count && (!resources || !offsets))) {
    device->error(E_INVALIDARG); return;
  }
  std::array<ComPtr<ID3D11Buffer>,slots> owners;
  std::array<ID3D11Buffer*,slots> buffers{};
  std::array<UINT,slots> positions{};
  for (UINT i = 0; i < count; ++i) {
    if (!resources[i].pDrvPrivate) continue;
    auto resource = get(resources[i]);
    if (!owned(device, resource)) return;
    if (FAILED(resource->backend.As(&owners[i]))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc{}; owners[i]->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_STREAM_OUTPUT)
        || (offsets[i] != UINT(-1) && (offsets[i] > desc.ByteWidth || offsets[i]%4))) {
      device->error(E_INVALIDARG); return;
    }
    buffers[i] = owners[i].Get(); positions[i] = offsets[i];
    for (UINT j = 0; j < i; ++j) if (buffers[j] == buffers[i]) { device->error(E_INVALIDARG); return; }
  }
  // D3D11 clears every higher slot; ClearTargets is only an optimization hint.
  device->context->SOSetTargets(count, buffers.data(), positions.data());
}
bool prepareGeometryShader(Device* device) {
  auto shader = device->geometryShader;
  if (!shader) return true;
  std::vector<dxvk::umd::ShaderSignatureEntry> outputs;
  const auto pixel = shader->withStreamOutput ? nullptr : device->pixelShader;
  if (!dxvk::umd::linkVertexOutputs(shader->outputs.data(), shader->outputs.size(),
      pixel ? pixel->inputs.data() : nullptr, pixel ? pixel->inputs.size() : 0, outputs)) {
    device->error(E_INVALIDARG); return false;
  }
  std::array<dxvk::umd::ShaderScalar,32> outputTypes = {};
  for (const auto& output : outputs) outputTypes[output.registerIndex] = output.scalar;
  if (!shader->geometry || shader->compiledOutputTypes != outputTypes) {
    std::vector<unsigned char> bytecode;
    if (!dxvk::umd::buildShaderContainer(shader->stage, shader->code.data(), shader->code.size(),
        shader->inputs.data(), shader->inputs.size(), outputs.data(), outputs.size(), bytecode)) {
      device->error(E_INVALIDARG); return false;
    }
    ComPtr<ID3D11GeometryShader> compiled;
    const auto& stream = shader->streamOutput;
    const HRESULT hr = shader->withStreamOutput
      ? device->backend->CreateGeometryShaderWithStreamOutput(bytecode.data(), bytecode.size(),
          stream.entries.data(), UINT(stream.entries.size()), stream.strides.data(), stream.strideCount,
          D3D11_SO_NO_RASTERIZED_STREAM, nullptr, &compiled)
      : device->backend->CreateGeometryShader(bytecode.data(), bytecode.size(), nullptr, &compiled);
    if (FAILED(hr)) { device->error(hr); return false; }
    shader->geometry = std::move(compiled); shader->compiledOutputTypes = outputTypes;
  }
  device->context->GSSetShader(shader->geometry.Get(), nullptr, 0);
  return true;
}
bool prepareVertexShader(Device* device) {
  auto shader = device->vertexShader;
  if (!shader || (!device->pixelShader && !device->geometryShader)) return false;
  auto layout = device->inputLayout;
  if (shader->needsLayout && (!layout || !layout->backend)) { device->error(E_INVALIDARG); return false; }
  try {
    if (!prepareGeometryShader(device)) return false;
    const auto next = device->geometryShader ? device->geometryShader : device->pixelShader;
    std::vector<dxvk::umd::ShaderSignatureEntry> outputs;
    if (!dxvk::umd::linkVertexOutputs(shader->outputs.data(), shader->outputs.size(),
        next->inputs.data(), next->inputs.size(), outputs)) {
      device->error(E_INVALIDARG); return false;
    }
    std::array<dxvk::umd::ShaderScalar,32> outputTypes = {}, inputTypes = {};
    for (const auto& output : outputs) outputTypes[output.registerIndex] = output.scalar;
    if (shader->needsLayout) inputTypes = layout->inputTypes;
    if (!shader->vertex || shader->compiledInputTypes != inputTypes ||
        (shader->needsLinkage && shader->compiledOutputTypes != outputTypes)) {
      auto inputs = shader->inputs;
      for (auto& input : inputs) if (!input.systemValue) {
        input.scalar = inputTypes[input.registerIndex];
        if (input.scalar == dxvk::umd::ShaderScalar::Unknown) { device->error(E_INVALIDARG); return false; }
      }
      std::vector<unsigned char> bytecode;
      if (!dxvk::umd::buildShaderContainer(shader->stage, shader->code.data(), shader->code.size(),
          inputs.data(), inputs.size(), outputs.data(), outputs.size(), bytecode)) { device->error(E_INVALIDARG); return false; }
      ComPtr<ID3D11VertexShader> compiled;
      const HRESULT hr = device->backend->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &compiled);
      if (FAILED(hr)) { device->error(hr); return false; }
      shader->vertex = std::move(compiled); shader->compiledInputTypes = inputTypes;
      shader->compiledOutputTypes = outputTypes;
    }
    device->context->VSSetShader(shader->vertex.Get(), nullptr, 0);
    return true;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
  return false;
}
void APIENTRY setIndexBuffer(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE object, DXGI_FORMAT format, UINT offset) {
  auto device = get(h);
  ComPtr<ID3D11Buffer> buffer;
  if (object.pDrvPrivate) {
    auto resource = get(object);
    if (!owned(device, resource)) return;
    if ((format != DXGI_FORMAT_R16_UINT && format != DXGI_FORMAT_R32_UINT)
        || FAILED(resource->backend.As(&buffer))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc = {}; buffer->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_INDEX_BUFFER) || offset > desc.ByteWidth
        || offset % (format == DXGI_FORMAT_R16_UINT ? 2 : 4)) { device->error(E_INVALIDARG); return; }
  }
  try {
    device->context->IASetIndexBuffer(buffer.Get(), format, offset);
    device->indexBound = buffer.Get() != nullptr;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
// Make every shared surface this draw touches current, and note the ones it
// will write. Only bound slots are considered: a device may hold many shared
// surfaces while a given draw reads one of them.
bool prepareSharedDraw(Device* device) {
  if (!device->anySharedSurface) return true;
  for (size_t stage = 0; stage < device->boundShared.size(); stage++) {
    auto& bound = device->boundShared[stage];
    for (UINT slot = 0; slot < device->boundSharedHigh[stage]; slot++)
      if (!readSharedSurface(device, bound[slot])) return false;
  }
  for (auto& surface : device->targetShared) {
    if (!surface) continue;
    // A render target is read-modify-write as far as this bridge can tell:
    // blending, a partial viewport and a scissor all leave pixels the owning
    // process wrote, so the cache has to start from the allocation. A full
    // clear is the one case that does not, and clearTarget says so directly.
    if (!readSharedSurface(device, surface)) return false;
    dxvk::umd::sharedWroteRegion(surface->state);
  }
  return true;
}

bool drawReady(Device* device, bool indexed = false) {
  const bool streamOnly = device->geometryShader && device->geometryShader->withStreamOutput;
  if (!device->vertexBound || (!streamOnly && (!device->pixelBound || !device->targetBound || !device->viewportBound))
      || !device->topologyBound || (indexed && !device->indexBound)) {
    device->error(E_INVALIDARG); return false;
  }
  return prepareVertexShader(device) && prepareSharedDraw(device);
}
void APIENTRY draw(D3D10DDI_HDEVICE h, UINT count, UINT start) {
  auto device = get(h);
  if (!drawReady(device)) return;
  try { device->context->Draw(count, start); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY drawAuto(D3D10DDI_HDEVICE h) {
  auto device = get(h);
  if (drawReady(device)) device->context->DrawAuto();
}
void APIENTRY drawIndexed(D3D10DDI_HDEVICE h, UINT count, UINT start, INT base) {
  auto device = get(h);
  if (!drawReady(device, true)) return;
  try { device->context->DrawIndexed(count, start, base); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY drawInstanced(D3D10DDI_HDEVICE h, UINT count, UINT instances, UINT start, UINT firstInstance) {
  auto device = get(h);
  if (!drawReady(device)) return;
  try { device->context->DrawInstanced(count, instances, start, firstInstance); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY drawIndexedInstanced(D3D10DDI_HDEVICE h, UINT count, UINT instances,
    UINT start, INT base, UINT firstInstance) {
  auto device = get(h);
  if (!drawReady(device, true)) return;
  try { device->context->DrawIndexedInstanced(count, instances, start, base, firstInstance); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY flush(D3D10DDI_HDEVICE h) {
  auto device = get(h);
  // Hand over this device's shared writes before the submission barrier. A
  // publish is itself a synchronized readback, so it must happen while the
  // caller can still service runtime callbacks, and the allocation has to
  // carry them before anyone else is told the work is done.
  const HRESULT published = publishSharedSurfaces(device);
  // D3D10 Flush permits only device-removed reporting. Backend command
  // recording AND Vulkan queue submission must finish while its caller can
  // service runtime callbacks; GPU completion is deliberately asynchronous.
  if (FAILED(published) || FAILED(dxvk::umd::flushRuntimeSubmission(device->context.Get())))
    device->error(DXGI_ERROR_DEVICE_REMOVED);
  // Anything another process wrote before this point must be read again.
  openSharedEpoch(device);
}
void APIENTRY relocateDeviceFunctions(D3D10DDI_HDEVICE h, D3D10DDI_DEVICEFUNCS* functions) {
  if (!functions) { get(h)->error(E_INVALIDARG); return; }
  // The runtime has already copied its table. No driver object caches a
  // pointer to that table, and none of our entrypoints require rebasing.
}
void APIENTRY counterInfo(D3D10DDI_HDEVICE h, D3D10DDI_COUNTER_INFO* info) {
  if (!info) { get(h)->error(E_INVALIDARG); return; }
  // The embedded DXVK device exposes no performance counters. This is the
  // cached creation-time capability, independent of later device removal.
  *info = {};
}
void APIENTRY checkCounter(D3D10DDI_HDEVICE h, D3D10DDI_QUERY query,
    D3D10DDI_COUNTER_TYPE*, UINT*, LPSTR, UINT*, LPSTR, UINT*, LPSTR, UINT*) {
  // Known optional counters are unsupported; no device-dependent range exists.
  get(h)->error(query >= D3D10DDI_COUNTER_GPU_IDLE && query <= D3D10DDI_COUNTER_TEXTURE_CACHE_HIT_RATE
    ? DXGI_DDI_ERR_UNSUPPORTED : E_INVALIDARG);
}
// D3D10 removed the D3D9 widening text filter, so the runtime only ever
// requests the identity size here. Accepting exactly that and rejecting any
// other request is the entire contract, not a placeholder.
void APIENTRY setTextFilterSize(D3D10DDI_HDEVICE h, UINT width, UINT height) {
  if (width != 1 || height != 1) get(h)->error(DXGI_DDI_ERR_UNSUPPORTED);
}
void APIENTRY destroyDevice(D3D10DDI_HDEVICE h) {
  std::shared_ptr<Device> owner;
  {
    std::lock_guard<std::mutex> lock(deviceStorageMutex);
    const auto entry = deviceStorage.find(h.pDrvPrivate);
    if (entry == deviceStorage.end() || entry->second.phase != DevicePhase::Live) return;
    owner = std::move(entry->second.owner);
    owner->retired = true;
    deviceStorage.erase(entry);
  }
  // The runtime destroys all children first. Deferred backend objects and
  // runtime allocations must be gone before this DDI returns. Its caller
  // services worker callbacks during the synchronous backend drain.
  dxvk::umd::RuntimeService::Scope scope(owner->service.get());
  const HRESULT hr = owner->close();
  // DestroyDevice permits only device-removed error reporting. A failed
  // terminal cleanup makes this device unusable; never report transient busy.
  if (FAILED(hr)) owner->error(DXGI_ERROR_DEVICE_REMOVED);
  owner.reset();
}

HRESULT presentData(Device* device, DXGI_DDI_ARG_PRESENT* args) {
  auto resource = reinterpret_cast<Resource*>(args->hSurfaceToPresent);
  if (resource->owner != device || !resource->backend || !resource->allocation.handle()
      || args->SrcSubResourceIndex || args->DstSubResourceIndex
      || args->hDstResource || !args->pDXGIContext || args->Flags.Value != 1)
    return E_INVALIDARG;
  try {
    ComPtr<ID3D11Texture2D> source;
    HRESULT hr = resource->backend.As(&source);
    if (FAILED(hr)) return hr;
    if (!resource->presentReadback) {
      D3D11_TEXTURE2D_DESC desc = {}; source->GetDesc(&desc);
      desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
      hr = device->backend->CreateTexture2D(&desc, nullptr, &resource->presentReadback);
      if (FAILED(hr)) return hr;
    }
    device->context->CopyResource(resource->presentReadback.Get(), source.Get());
    D3D11_MAPPED_SUBRESOURCE map = {};
    // Synchronous Map is the GPU completion barrier before any guest CPU
    // publication. Correctness checkpoint; this is not a zero-copy path.
    hr = device->context->Map(resource->presentReadback.Get(), 0, D3D11_MAP_READ, 0, &map);
    if (FAILED(hr)) return hr;
    struct Unmap {
      ID3D11DeviceContext* context;
      ID3D11Resource* resource;
      ~Unmap() { context->Unmap(resource, 0); }
    } unmap = {device->context.Get(), resource->presentReadback.Get()};
    hr = device->memory.upload(resource->allocation, map.pData, map.RowPitch);
    if (FAILED(hr)) return hr;
    if (device->retired) return DXGI_ERROR_DEVICE_REMOVED;
    return device->memory.present(resource->allocation, *args);
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY present(DXGI_DDI_ARG_PRESENT* args) {
  if (!args || !args->hDevice || !args->hSurfaceToPresent) return E_INVALIDARG;
  DeviceOperation operation(reinterpret_cast<void*>(args->hDevice));
  if (!operation.owner) return DXGI_ERROR_DEVICE_REMOVED;
  try {
    return operation.owner->service->run([&] {
      DeviceOperation worker(operation.storage, operation.owner);
      auto device = operation.owner.get();
      // Present is an ownership boundary like Flush: whatever this device drew
      // into a shared surface has to be out in the allocation before the frame
      // is handed over, and whatever anyone else drew must be read again after.
      const HRESULT published = publishSharedSurfaces(device);
      if (FAILED(published)) return published;
      const HRESULT hr = presentData(device, args);
      openSharedEpoch(device);
      return hr;
    });
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
}

extern "C" SIZE_T APIENTRY VioGpuDxvkPrivateDeviceSize() { return sizeof(DevicePrivate); }

namespace {
HRESULT createDdiDevice(
    const LUID* luid, D3D10DDI_HDEVICE h, D3D10DDI_HRTCORELAYER runtime,
    const D3D10DDI_CORELAYER_DEVICECALLBACKS* callbacks, D3D10DDI_DEVICEFUNCS* table,
    std::shared_ptr<const dxvk::umd::AdapterIdentity> identity = {},
    const D3D10DDIARG_CREATEDEVICE* native = nullptr) {
  if (!luid || !h.pDrvPrivate || uintptr_t(h.pDrvPrivate) % alignof(DevicePrivate)
      || !callbacks || !callbacks->pfnSetErrorCb || !table)
    return E_INVALIDARG;
  auto owner = std::make_shared<Device>();
  dxvk::umd::RuntimeService::Scope scope(owner->service.get());
  try {
    std::lock_guard<std::mutex> lock(deviceStorageMutex);
    if (!deviceStorage.emplace(h.pDrvPrivate, DeviceRecord{DevicePhase::Creating, owner}).second)
      return E_INVALIDARG;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
  struct Creation {
    void* storage;
    const std::shared_ptr<Device>& owner;
    bool published = false;
    ~Creation() { if (!published) releaseDeviceStorage(storage, owner); }
  } guard{h.pDrvPrivate, owner};
  auto device = owner.get();
  device->runtime = runtime;
  // Only the callback used by this exact interface is read. The runtime owns
  // its original table and may have supplied an older WDK structure size.
  device->callbacks.pfnSetErrorCb = callbacks->pfnSetErrorCb;
  DXGI_DDI_BASE_FUNCTIONS* dxgiTable = nullptr;
  if (native) {
    // These callbacks are available during the first Vulkan allocation, before
    // CreateDevice returns. Do not read caller tables again after backend entry.
    device->adapter = identity;
    device->memory.initialize(native->hRTDevice.handle, *native->pKTCallbacks,
      native->DXGIBaseDDI.pDXGIBaseCallbacks, identity, device->service);
    device->gpu = dxvk::umd::RuntimeGpu::create(native->hRTDevice.handle,
      *native->pKTCallbacks, identity, device->service);
    dxgiTable = native->DXGIBaseDDI.pDXGIDDIBaseFunctions;
    device->service->allowDeferredCalls();
  }
  auto backendRuntime = device->gpu ? device->gpu->backend() : dxvk::umd::RuntimeBackend{};
  const HRESULT hr = device->service->run([&] {
    return dxvk::umd::createDevice(*luid, D3D_FEATURE_LEVEL_10_0,
      &device->backend, &device->context, device->gpu ? &backendRuntime : nullptr);
  });
  if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
  if (!device->backend || !device->context) return E_FAIL;
  *table = {};
  table->pfnCalcPrivateResourceSize = deviceEntry<resourceSize>;
  table->pfnCreateResource = deviceEntry<createResource>;
  table->pfnDestroyResource = deviceEntry<destroyResource>;
  table->pfnCalcPrivateOpenedResourceSize = deviceEntry<openedResourceSize>;
  table->pfnOpenResource = deviceEntry<openResource>;
  table->pfnCalcPrivateShaderResourceViewSize = deviceEntry<shaderViewSize>;
  table->pfnCreateShaderResourceView = deviceEntry<createShaderView>;
  table->pfnDestroyShaderResourceView = deviceEntry<destroyShaderView>;
  // Explicitly tag every predicated DDI at registration. State changes,
  // queries, Map/Unmap and Flush retain the ordinary non-predicated wrapper.
  table->pfnGenMips = deviceEntry<generateMips, true>;
  table->pfnVsSetShaderResources = deviceEntry<setShaderResources<dxvk::umd::ShaderStage::Vertex>>;
  table->pfnGsSetShaderResources = deviceEntry<setShaderResources<dxvk::umd::ShaderStage::Geometry>>;
  table->pfnPsSetShaderResources = deviceEntry<setShaderResources<dxvk::umd::ShaderStage::Pixel>>;
  table->pfnCalcPrivateSamplerSize = deviceEntry<samplerSize>;
  table->pfnCreateSampler = deviceEntry<createSampler>;
  table->pfnDestroySampler = deviceEntry<destroySampler>;
  table->pfnVsSetSamplers = deviceEntry<setSamplers<dxvk::umd::ShaderStage::Vertex>>;
  table->pfnGsSetSamplers = deviceEntry<setSamplers<dxvk::umd::ShaderStage::Geometry>>;
  table->pfnPsSetSamplers = deviceEntry<setSamplers<dxvk::umd::ShaderStage::Pixel>>;
  table->pfnCalcPrivateRenderTargetViewSize = deviceEntry<targetSize>;
  table->pfnCreateRenderTargetView = deviceEntry<createTarget>;
  table->pfnDestroyRenderTargetView = deviceEntry<destroyTarget>;
  table->pfnClearRenderTargetView = deviceEntry<clearTarget, true>;
  table->pfnCalcPrivateDepthStencilViewSize = deviceEntry<depthViewSize>;
  table->pfnCreateDepthStencilView = deviceEntry<createDepthView>;
  table->pfnDestroyDepthStencilView = deviceEntry<destroyDepthView>;
  table->pfnClearDepthStencilView = deviceEntry<clearDepthView, true>;
  table->pfnCalcPrivateDepthStencilStateSize = deviceEntry<depthStateSize>;
  table->pfnCreateDepthStencilState = deviceEntry<createDepthState>;
  table->pfnDestroyDepthStencilState = deviceEntry<destroyDepthState>;
  table->pfnSetDepthStencilState = deviceEntry<setDepthState>;
  table->pfnResourceCopy = deviceEntry<copyResource, true>;
  table->pfnResourceResolveSubresource = deviceEntry<resolveResource, true>;
  table->pfnCheckFormatSupport = deviceEntry<checkFormat>;
  table->pfnCheckMultisampleQualityLevels = deviceEntry<checkMultisample>;
  table->pfnResourceCopyRegion = deviceEntry<copyRegion, true>;
  table->pfnResourceUpdateSubresourceUP = deviceEntry<updateResource, true>;
  table->pfnDefaultConstantBufferUpdateSubresourceUP = deviceEntry<updateResource, true>;
  table->pfnCalcPrivateQuerySize = deviceEntry<querySize>;
  table->pfnCreateQuery = deviceEntry<createQuery>;
  table->pfnDestroyQuery = deviceEntry<destroyQuery>;
  table->pfnQueryBegin = deviceEntry<beginQuery>;
  table->pfnQueryEnd = deviceEntry<endQuery>;
  table->pfnQueryGetData = deviceEntry<getQueryData>;
  table->pfnSetPredication = deviceEntry<setPredication>;
  table->pfnResourceMap = deviceEntry<mapResource>;
  table->pfnResourceUnmap = deviceEntry<unmapResource>;
  table->pfnStagingResourceMap = deviceEntry<mapResource>;
  table->pfnStagingResourceUnmap = deviceEntry<unmapResource>;
  table->pfnResourceIsStagingBusy = deviceEntry<isStagingBusy>;
  table->pfnResourceReadAfterWriteHazard = deviceEntry<resourceHazard>;
  table->pfnShaderResourceViewReadAfterWriteHazard = deviceEntry<shaderViewHazard>;
  table->pfnDynamicIABufferMapDiscard = deviceEntry<mapResource>;
  table->pfnDynamicIABufferMapNoOverwrite = deviceEntry<mapResource>;
  table->pfnDynamicIABufferUnmap = deviceEntry<unmapResource>;
  table->pfnDynamicConstantBufferMapDiscard = deviceEntry<mapResource>;
  table->pfnDynamicConstantBufferUnmap = deviceEntry<unmapResource>;
  table->pfnDynamicResourceMapDiscard = deviceEntry<mapResource>;
  table->pfnDynamicResourceUnmap = deviceEntry<unmapResource>;
  table->pfnCalcPrivateShaderSize = deviceEntry<shaderSize>;
  table->pfnCreateVertexShader = deviceEntry<createVertexShader>;
  table->pfnCreatePixelShader = deviceEntry<createPixelShader>;
  table->pfnCreateGeometryShader = deviceEntry<createGeometryShader>;
  table->pfnCalcPrivateGeometryShaderWithStreamOutput = deviceEntry<geometryStreamSize>;
  table->pfnCreateGeometryShaderWithStreamOutput = deviceEntry<createGeometryStream>;
  table->pfnSoSetTargets = deviceEntry<setStreamTargets>;
  table->pfnDestroyShader = deviceEntry<destroyShader>;
  table->pfnVsSetShader = deviceEntry<setVertexShader>;
  table->pfnPsSetShader = deviceEntry<setPixelShader>;
  table->pfnGsSetShader = deviceEntry<setGeometryShader>;
  table->pfnVsSetConstantBuffers = deviceEntry<setConstantBuffers<dxvk::umd::ShaderStage::Vertex>>;
  table->pfnGsSetConstantBuffers = deviceEntry<setConstantBuffers<dxvk::umd::ShaderStage::Geometry>>;
  table->pfnPsSetConstantBuffers = deviceEntry<setConstantBuffers<dxvk::umd::ShaderStage::Pixel>>;
  table->pfnSetRenderTargets = deviceEntry<setRenderTargets>;
  table->pfnSetViewports = deviceEntry<setViewports>;
  table->pfnSetScissorRects = deviceEntry<setScissors>;
  table->pfnCalcPrivateRasterizerStateSize = deviceEntry<rasterizerSize>;
  table->pfnCreateRasterizerState = deviceEntry<createRasterizer>;
  table->pfnDestroyRasterizerState = deviceEntry<destroyRasterizer>;
  table->pfnSetRasterizerState = deviceEntry<setRasterizer>;
  table->pfnIaSetTopology = deviceEntry<setTopology>;
  table->pfnIaSetIndexBuffer = deviceEntry<setIndexBuffer>;
  table->pfnIaSetVertexBuffers = deviceEntry<setVertexBuffers>;
  table->pfnCalcPrivateElementLayoutSize = deviceEntry<layoutSize>;
  table->pfnCreateElementLayout = deviceEntry<createLayout>;
  table->pfnDestroyElementLayout = deviceEntry<destroyLayout>;
  table->pfnIaSetInputLayout = deviceEntry<setLayout>;
  table->pfnCalcPrivateBlendStateSize = deviceEntry<blendSize>;
  table->pfnCreateBlendState = deviceEntry<createBlend>;
  table->pfnDestroyBlendState = deviceEntry<destroyBlend>;
  table->pfnSetBlendState = deviceEntry<setBlend>;
  table->pfnDraw = deviceEntry<draw, true>;
  table->pfnDrawAuto = deviceEntry<drawAuto, true>;
  table->pfnDrawIndexed = deviceEntry<drawIndexed, true>;
  table->pfnDrawInstanced = deviceEntry<drawInstanced, true>;
  table->pfnDrawIndexedInstanced = deviceEntry<drawIndexedInstanced, true>;
  table->pfnFlush = deviceEntry<flush>;
  table->pfnRelocateDeviceFuncs = deviceEntry<relocateDeviceFunctions>;
  table->pfnCheckCounterInfo = deviceEntry<counterInfo>;
  table->pfnCheckCounter = deviceEntry<checkCounter>;
  table->pfnSetTextFilterSize = deviceEntry<setTextFilterSize>;
  table->pfnDestroyDevice = destroyDevice;
  if (dxgiTable) {
    *dxgiTable = {};
    if (device->memory.available()) dxgiTable->pfnPresent = present;
  }
  {
    std::lock_guard<std::mutex> lock(deviceStorageMutex);
    deviceStorage.at(h.pDrvPrivate).phase = DevicePhase::Live;
  }
  guard.published = true;
  return S_OK;
}
}

extern "C" HRESULT APIENTRY VioGpuDxvkCreateDdiTestDevice(
    const LUID* luid, D3D10DDI_HDEVICE h, D3D10DDI_HRTCORELAYER runtime,
    const D3D10DDI_CORELAYER_DEVICECALLBACKS* callbacks, D3D10DDI_DEVICEFUNCS* table) {
  try { return createDdiDevice(luid, h, runtime, callbacks, table); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  catch (...) { return E_FAIL; }
}

HRESULT dxvk::umd::createAdapterDevice(
    const std::shared_ptr<const AdapterIdentity>& identity, D3D10DDIARG_CREATEDEVICE* args) {
  return createDdiDevice(&identity->luid, args->hDrvDevice,
    args->hRTCoreLayer, args->pUMCallbacks, args->pDeviceFuncs, identity, args);
}
