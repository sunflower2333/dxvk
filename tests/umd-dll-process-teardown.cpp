// Exact production close/destructor, ordered registry declarations, shutdown
// guard and DllMain are extracted by the adjacent Python runner. RuntimeService,
// ProcessStorage and PrivateChildren are production headers. GPU/COM peers are
// controlled; this DLL never loads a renderer or calls a graphics driver.
#include "umd_runtime_service.h"
#include "umd_process_storage.h"
#include "umd_private_children.h"
#include <d3d9.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fixture {
static void log(const char* text) {
  DWORD written;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, DWORD(std::strlen(text)), &written, nullptr);
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &written, nullptr);
}
[[noreturn]] static void fail(const char* text) {
  log(text); TerminateProcess(GetCurrentProcess(), 90); __builtin_unreachable();
}
#define CHECK(x) do { if (!(x)) fixture::fail("ORACLE_FAIL " #x); } while (0)
static bool setupComplete, terminatingObserved;
static DWORD callbackThread;
static LONG made, released, callbacks, gpuClosed, memoryClosed, adapterClosed;
static LONG detachReleased, detachCallbacks, detachGpu, detachMemory, detachAdapter;
static void touch(const char* name, LONG& count) {
  if (terminatingObserved) fail("ORACLE_FAIL cleanup after terminating DllMain");
  InterlockedIncrement(&count); log(name);
}
// Declared before every production registry so its destructor observes the
// actual completed destruction order. This observer is not part of production.
struct Oracle {
  std::weak_ptr<void> device, service;
  ~Oracle() {
    if (!setupComplete) return;
    if (terminatingObserved) {
      CHECK(released == detachReleased && callbacks == detachCallbacks);
      CHECK(gpuClosed == detachGpu && memoryClosed == detachMemory);
      CHECK(adapterClosed == detachAdapter);
      log("ORACLE_EXIT_NO_CLEANUP_PASS");
    } else {
      CHECK(released == made && made > 0 && callbacks == made);
      CHECK(gpuClosed == 1 && memoryClosed == 1);
      CHECK(adapterClosed == 1);
      CHECK(device.expired() && service.expired());
      log("ORACLE_UNLOAD_CLEAN_PASS");
    }
  }
} oracle;
// Only the identity destructor is controlled; actual adapter registries and
// both CloseAdapter DDIs below remain exact production source.
struct Adapter {
  std::atomic<bool> closed{false};
  std::shared_ptr<bool> live = std::make_shared<bool>(true);
  ~Adapter() { touch("ADAPTER_IDENTITY_RELEASE", adapterClosed); }
};
struct D3D10DDI_HDEVICE { void* pDrvPrivate; };
struct D3D10DDI_HADAPTER { void* pDrvPrivate; };
#include "adapter_globals.inc"
#include "adapter_close.inc"
struct IUnknown {
  dxvk::umd::RuntimeService* service;
  explicit IUnknown(dxvk::umd::RuntimeService* value) : service(value) { ++made; }
  virtual ~IUnknown() {
    touch("COM_RELEASE", released);
    CHECK(service->invoke([] {
      CHECK(GetCurrentThreadId() == callbackThread);
      touch("RUNTIME_CALLBACK", callbacks); return S_OK;
    }) == S_OK);
  }
};
template<class T> struct ComPtr {
  T* p = nullptr;
  ComPtr() = default; ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  ComPtr(ComPtr&& other) noexcept : p(std::exchange(other.p, nullptr)) { }
  ~ComPtr() { Reset(); }
  void Attach(T* value) { CHECK(!p); p = value; }
  T* Detach() { return std::exchange(p, nullptr); }
  void Reset() { delete std::exchange(p, nullptr); }
};
struct Gpu { HRESULT close() { touch("GPU_CLOSE", gpuClosed); return S_OK; } };
struct Memory {
  HRESULT close() { touch("MEMORY_CLOSE", memoryClosed); return S_OK; }
  HRESULT closeDeviceAllocations() { return close(); }
};

#ifdef FIXTURE_LEGACY
enum class ShaderStage { Vertex, Pixel };
struct Backend {
  template<class... T> void unlockBuffer(T&&...) { }
  template<class... T> void unlockSurface(T&&...) { }
  template<class... T> void setRenderTarget(T&&...) { }
  template<class... T> void setDepthStencil(T&&...) { }
  template<class... T> void setVertexDeclaration(T&&...) { }
  template<class... T> void setShader(T&&...) { }
  template<class... T> void setTexture(T&&...) { }
  template<class... T> void setStreamSource(T&&...) { }
  template<class... T> void setIndices(T&&...) { }
  template<class... T> void setLightEnabled(T&&...) { }
  void flush() { }
};
struct Present { HRESULT release() { return S_OK; } };
struct Surface { bool locked = false; std::unique_ptr<int> backend; std::unique_ptr<Present> present; };
struct Resource { bool bufferLocked = false; std::unique_ptr<int> buffer; std::vector<Surface> surfaces; };
#include "result_mapping.inc"
struct Device {
  // Production legacy policy: deferred callbacks, worker mode.
  std::shared_ptr<dxvk::umd::RuntimeService> service = std::make_shared<dxvk::umd::RuntimeService>(true);
  std::shared_ptr<Gpu> gpu = std::make_shared<Gpu>();
  Memory memory;
  std::unique_ptr<Backend> backend = std::make_unique<Backend>();
  bool closing = false, busy = false;
  HANDLE runtime = HANDLE(2);
  std::unordered_map<HANDLE, std::unique_ptr<Resource>> resources;
  std::unordered_set<HANDLE> runtimeResources;
  std::unordered_map<HANDLE, std::unique_ptr<IUnknown>> declarations, shaders, queries;
  std::array<HANDLE, 2> boundShaders = {};
  std::array<HANDLE, 20> boundTextures = {};
  HANDLE declaration = nullptr;
  const void* userVertices = nullptr;
  UINT userStride = 0;
  struct Stream { HANDLE buffer = nullptr; UINT offset = 0, stride = 0; };
  std::array<Stream, 16> streams = {};
  HANDLE indices = nullptr, target = nullptr, depthStencil = nullptr;
  struct Light { UINT slot = 0; bool enabled = false; };
  std::unordered_map<UINT, Light> lights;
#include "device_close.inc"
};
#include "registry_globals.inc"
#include "device_destroy.inc"
static std::mutex& registryMutex() { return devicesMutex; }
static std::shared_ptr<Device> make(bool inlineWork) {
  CHECK(!inlineWork);
  auto owner = std::make_shared<Device>();
  owner->queries.emplace(HANDLE(16), std::make_unique<IUnknown>(owner->service.get()));
  devices.emplace(HANDLE(1), owner); runtimeDevices.emplace(HANDLE(2), owner);
  return owner;
}
static void explicitClose() {
  {
    auto owner = devices.at(HANDLE(1));
    owner->service->drain([&] { owner->queries.clear(); });
  }
  CHECK(destroyDevice(HANDLE(1)) == S_OK);
  CHECK(devices.empty() && runtimeDevices.empty());
  CHECK(destroyDevice(HANDLE(1)) == E_INVALIDARG);
  CHECK(closeAdapter(HANDLE(3)) == S_OK && adapters.empty());
}
#else
struct Device; struct InputLayout;
void clearRegisteredChildren(Device*) noexcept;
struct ID3D11Query : IUnknown { using IUnknown::IUnknown; };
struct ID3D11InputLayout : IUnknown { using IUnknown::IUnknown; };
struct Device {
  explicit Device(bool inlineWork) : service(std::make_shared<dxvk::umd::RuntimeService>(false, inlineWork)) { }
  std::shared_ptr<dxvk::umd::RuntimeService> service;
  std::shared_ptr<Gpu> gpu = std::make_shared<Gpu>();
  Memory memory;
  struct Bindings { std::vector<void*> pointers; std::vector<std::unique_ptr<int>> owners; };
  std::array<Bindings, 6> classBindings;
  std::shared_ptr<InputLayout> inputLayout;
  ComPtr<IUnknown> rotationScratch, predicate, context, backend;
  bool closing = false;
  std::atomic<bool> retired{false};
  void error(HRESULT) { fail("ORACLE_FAIL unexpected destroy error callback"); }
#include "device_close.inc"
};
#include "child_types.inc"
struct Query : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11Query> backend;
  std::array<ComPtr<ID3D11Query>, 3> additionalStreams;
  bool retired = false;
};
struct InputLayout : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11InputLayout> backend;
  bool retired = false;
};
#include "device_record.inc"
#include "registry_globals.inc"
#include "child_retirement.inc"
#include "device_release.inc"
#include "device_destroy.inc"
static std::mutex& registryMutex() { return deviceStorageMutex; }
static std::shared_ptr<Device> make(bool inlineWork) {
  auto owner = std::make_shared<Device>(inlineWork);
  deviceStorage.emplace(reinterpret_cast<void*>(4096), DeviceRecord{DevicePhase::Live, owner});
  auto query = std::make_shared<Query>(); query->owner = owner.get();
  query->retirement = std::make_unique<ComRetirement>();
  query->backend.Attach(new ID3D11Query(owner->service.get()));
  for (auto& stream : query->additionalStreams) stream.Attach(new ID3D11Query(owner->service.get()));
  auto q = queryStorage.begin(reinterpret_cast<void*>(4112), owner->service); CHECK(q && q.publish(query));
  auto layout = std::make_shared<InputLayout>(); layout->owner = owner.get();
  layout->retirement = std::make_unique<ComRetirement>();
  layout->backend.Attach(new ID3D11InputLayout(owner->service.get()));
  auto l = layoutStorage.begin(reinterpret_cast<void*>(4128), owner->service); CHECK(l && l.publish(layout));
  owner->inputLayout = layout;
  return owner;
}
static void explicitClose() {
  {
    auto owner = deviceStorage.at(reinterpret_cast<void*>(4096)).owner;
    clearRegisteredChildren(owner.get());
    owner->inputLayout.reset();
  }
  destroyDevice({reinterpret_cast<void*>(4096)});
  CHECK(deviceStorage.empty());
  destroyDevice({reinterpret_cast<void*>(4096)});
  CHECK(closeAdapter({reinterpret_cast<void*>(3)}) == S_OK && adapters.empty());
}
#endif
static HANDLE lockReady;
static DWORD WINAPI holdRegistry(void*) {
  registryMutex().lock(); log("REGISTRY_LOCK_HELD"); SetEvent(lockReady);
  Sleep(INFINITE); return 99;
}
} // namespace fixture

extern "C" __declspec(dllexport) BOOL ProbeSetup(BOOL inlineWork, BOOL warm, BOOL locked) {
  using namespace fixture;
  CHECK(!setupComplete); callbackThread = GetCurrentThreadId();
  adapters.emplace(reinterpret_cast<void*>(3), std::make_shared<Adapter>());
  auto owner = make(inlineWork != FALSE); oracle.device = owner; oracle.service = owner->service;
  if (warm) owner->service->drain([&] {
    log("WARM_JOB"); CHECK(owner->service->invoke([] {
      CHECK(GetCurrentThreadId() == callbackThread); log("WARM_CALLBACK"); return S_OK;
    }) == S_OK);
  });
  owner.reset(); setupComplete = true;
  if (locked) {
    lockReady = CreateEventW(nullptr, TRUE, FALSE, nullptr); CHECK(lockReady);
    auto thread = CreateThread(nullptr, 0, holdRegistry, nullptr, 0, nullptr); CHECK(thread);
    CHECK(WaitForSingleObject(lockReady, 5000) == WAIT_OBJECT_0); CloseHandle(thread);
  }
  log("PROBE_SETUP_DONE"); return TRUE;
}
extern "C" __declspec(dllexport) void ProbeClose() {
  fixture::explicitClose(); fixture::log("EXPLICIT_CLOSE_DONE");
}
// Only the function's symbol is renamed; its body and calling convention are
// extracted verbatim. The independent observer cannot hide a missing flag.
#include "production_dllmain.inc"
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_DETACH) {
    using namespace fixture;
    terminatingObserved = reserved != nullptr;
    detachReleased = released; detachCallbacks = callbacks;
    detachGpu = gpuClosed; detachMemory = memoryClosed;
    detachAdapter = adapterClosed;
    log(reserved ? "DLL_TERMINATING" : "DLL_UNLOADING");
  }
  return ProductionDllMain(instance, reason, reserved);
}
