// Synthetic CPU control for the actual probe frontend and its typed wrappers.
// No Microsoft CreateDevice, candidate DLL, registry or GPU call is made.
#include "umd-d3d9-runtime-front.cpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, calls = 0;
DWORD expectedThread = 0;
HANDLE expectedHandle = nullptr;
const void* expectedRequest = nullptr;
bool unreadableRequest = false;
std::vector<std::string> records;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
  std::fprintf(stderr, "callback control failure line=%u: %s\n", unsigned(__LINE__), #expression); \
  std::abort(); } } while (0)

void capture(const char* format, ...) {
  char line[2048]; va_list args; va_start(args, format);
  const int bytes = std::vsnprintf(line, sizeof(line), format, args); va_end(args);
  CHECK(bytes > 0 && size_t(bytes) < sizeof(line));
  records.emplace_back(line, size_t(bytes));
}
void called(HANDLE handle, const void* args) {
  ++calls; CHECK(handle == expectedHandle); CHECK(args == expectedRequest);
  CHECK(GetCurrentThreadId() == expectedThread);
}
HRESULT APIENTRY mockAllocate(HANDLE handle, D3DDDICB_ALLOCATE* args) {
  called(handle, args);
  if (!args || unreadableRequest) return E_POINTER;
  args->hKMResource = 43; args->pAllocationInfo[0].hAllocation = 44;
  return S_FALSE;
}
HRESULT APIENTRY mockCreate(HANDLE handle, D3DDDICB_CREATECONTEXT* args) {
  called(handle, args);
  if (!args || unreadableRequest) return E_POINTER;
  args->hContext = reinterpret_cast<HANDLE>(uintptr_t(0x200));
  args->pCommandBuffer = reinterpret_cast<void*>(uintptr_t(0x300));
  args->CommandBufferSize = 16384;
  args->pAllocationList = reinterpret_cast<D3DDDI_ALLOCATIONLIST*>(uintptr_t(0x400));
  args->AllocationListSize = 17;
  args->pPatchLocationList = reinterpret_cast<D3DDDI_PATCHLOCATIONLIST*>(uintptr_t(0x500));
  args->PatchLocationListSize = 19;
  return E_OUTOFMEMORY;
}
HRESULT APIENTRY mockRender(HANDLE handle, D3DDDICB_RENDER* args) {
  called(handle, args);
  if (!args || unreadableRequest) return E_POINTER;
  args->pNewCommandBuffer = reinterpret_cast<void*>(uintptr_t(0x600));
  args->NewCommandBufferSize = 32768;
  args->pNewAllocationList = reinterpret_cast<D3DDDI_ALLOCATIONLIST*>(uintptr_t(0x700));
  args->NewAllocationListSize = 23;
  args->pNewPatchLocationList = reinterpret_cast<D3DDDI_PATCHLOCATIONLIST*>(uintptr_t(0x800));
  args->NewPatchLocationListSize = 29; args->QueuedBufferCount = 7;
  return E_FAIL;
}
HRESULT APIENTRY replacementRender(HANDLE handle, D3DDDICB_RENDER* args) {
  called(handle, args); return S_FALSE;
}
HRESULT APIENTRY mockEscape(HANDLE adapter, const D3DDDICB_ESCAPE* args) {
  called(adapter, args);
  if (!args || unreadableRequest) return E_POINTER;
  return E_INVALIDARG;
}
HRESULT APIENTRY replacementEscape(HANDLE adapter, const D3DDDICB_ESCAPE* args) {
  called(adapter, args); return S_OK;
}

struct Guarded {
  void* memory = nullptr; BYTE* boundary = nullptr; DWORD pageSize = 0;
  Guarded() {
    SYSTEM_INFO info; GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    memory = VirtualAlloc(nullptr, size_t(info.dwPageSize) * 2,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); CHECK(memory);
    boundary = static_cast<BYTE*>(memory) + info.dwPageSize;
    DWORD prior = 0; CHECK(VirtualProtect(boundary, info.dwPageSize, PAGE_NOACCESS, &prior));
  }
  ~Guarded() { if (memory) VirtualFree(memory, 0, MEM_RELEASE); }
  template<typename Type> Type* put(const Type& value, size_t bytes) {
    CHECK(bytes <= pageSize && bytes % alignof(Type) == 0);
    void* address = boundary - bytes; std::memcpy(address, &value, bytes);
    return static_cast<Type*>(address);
  }
};

using Callbacks = dxvk::test::RuntimeCallbacks9;
Callbacks::Pin retirementOwner;
std::weak_ptr<Callbacks::Owner> retirementWeak;
HRESULT APIENTRY retireInCallback(HANDLE handle, D3DDDICB_RENDER* args) {
  called(handle, args); Callbacks::remove(retirementOwner); retirementOwner.reset();
  CHECK(!retirementWeak.expired()); return D3DERR_DEVICELOST;
}

void callbackControls() {
  const HANDLE runtime = reinterpret_cast<HANDLE>(uintptr_t(0x100));
  const HANDLE adapter = reinterpret_cast<HANDLE>(uintptr_t(0x110));
  D3DDDI_DEVICECALLBACKS original = {};
  original.pfnAllocateCb = mockAllocate; original.pfnCreateContextCb = mockCreate;
  original.pfnRenderCb = mockRender; original.pfnEscapeCb = mockEscape;
  Guarded callbackPage;
  auto* live = callbackPage.put(original, Callbacks::callbackBytes);
  auto owner = Callbacks::install(runtime, adapter, live, capture); CHECK(owner);
  CHECK(!Callbacks::install(runtime, adapter, live, capture));
  D3DDDI_DEVICECALLBACKS expected = original;
  expected.pfnAllocateCb = owner->wrapped.pfnAllocateCb;
  expected.pfnCreateContextCb = owner->wrapped.pfnCreateContextCb;
  expected.pfnRenderCb = owner->wrapped.pfnRenderCb;
  expected.pfnEscapeCb = owner->wrapped.pfnEscapeCb;
  CHECK(!std::memcmp(&expected, &owner->wrapped, Callbacks::callbackBytes));
  CHECK(!std::memcmp(live, &original, Callbacks::callbackBytes));

  expectedHandle = runtime;
  D3DDDI_ALLOCATIONINFO allocation = {};
  D3DDDICB_ALLOCATE allocateArgs = {}; allocateArgs.NumAllocations = 1;
  allocateArgs.pAllocationInfo = &allocation; expectedRequest = &allocateArgs;
  CHECK(owner->wrapped.pfnAllocateCb(runtime, &allocateArgs) == S_FALSE);
  CHECK(allocateArgs.hKMResource == 43 && allocation.hAllocation == 44);
  D3DDDICB_CREATECONTEXT createArgs = {}; createArgs.NodeOrdinal = 2; createArgs.EngineAffinity = 4;
  expectedRequest = &createArgs;
  CHECK(owner->wrapped.pfnCreateContextCb(runtime, &createArgs) == E_OUTOFMEMORY);
  CHECK(createArgs.hContext == reinterpret_cast<HANDLE>(uintptr_t(0x200)));
  CHECK(createArgs.CommandBufferSize == 16384 && createArgs.AllocationListSize == 17
    && createArgs.PatchLocationListSize == 19);

  D3DDDICB_RENDER renderArgs = {}; renderArgs.hContext = createArgs.hContext;
  renderArgs.CommandOffset = 16; renderArgs.CommandLength = 1024;
  renderArgs.NumAllocations = 5; renderArgs.NumPatchLocations = 5;
  constexpr size_t renderBytes = offsetof(D3DDDICB_RENDER, QueuedBufferCount) + sizeof(ULONG);
  constexpr size_t renderStorage = (renderBytes + alignof(D3DDDICB_RENDER) - 1)
    / alignof(D3DDDICB_RENDER) * alignof(D3DDDICB_RENDER);
  Guarded renderPage; auto* vistaRender = renderPage.put(renderArgs, renderStorage);
  expectedRequest = vistaRender;
  CHECK(owner->wrapped.pfnRenderCb(runtime, vistaRender) == E_FAIL);
  CHECK(vistaRender->CommandOffset == 16 && vistaRender->CommandLength == 1024
    && vistaRender->NumAllocations == 5 && vistaRender->NumPatchLocations == 5);
  CHECK(vistaRender->pNewCommandBuffer == reinterpret_cast<void*>(uintptr_t(0x600))
    && vistaRender->NewCommandBufferSize == 32768 && vistaRender->NewAllocationListSize == 23
    && vistaRender->NewPatchLocationListSize == 29 && vistaRender->QueuedBufferCount == 7);
  live->pfnRenderCb = replacementRender;
  CHECK(owner->wrapped.pfnRenderCb(runtime, vistaRender) == S_FALSE);

  uint32_t privateData[8] = {0x504d5644, 0, 32, 0, 1, 0, 3, 0};
  D3DDDICB_ESCAPE escapeArgs = {}; escapeArgs.hDevice = runtime;
  escapeArgs.pPrivateDriverData = privateData; escapeArgs.PrivateDriverDataSize = sizeof(privateData);
  expectedHandle = adapter; expectedRequest = &escapeArgs;
  CHECK(owner->wrapped.pfnEscapeCb(adapter, &escapeArgs) == E_INVALIDARG);
  live->pfnEscapeCb = replacementEscape;
  CHECK(owner->wrapped.pfnEscapeCb(adapter, &escapeArgs) == S_OK);
  Guarded payloadPage; escapeArgs.pPrivateDriverData = payloadPage.boundary;
  CHECK(owner->wrapped.pfnEscapeCb(adapter, &escapeArgs) == S_OK);
  CHECK(records.back().find("private_readable=0") != std::string::npos);

  expectedHandle = runtime; expectedRequest = nullptr;
  CHECK(owner->wrapped.pfnAllocateCb(runtime, nullptr) == E_POINTER);
  CHECK(owner->wrapped.pfnCreateContextCb(runtime, nullptr) == E_POINTER);
  live->pfnRenderCb = mockRender;
  CHECK(owner->wrapped.pfnRenderCb(runtime, nullptr) == E_POINTER);
  live->pfnEscapeCb = mockEscape; expectedHandle = adapter;
  CHECK(owner->wrapped.pfnEscapeCb(adapter, nullptr) == E_POINTER);
  unreadableRequest = true; expectedHandle = runtime; expectedRequest = renderPage.boundary;
  CHECK(owner->wrapped.pfnRenderCb(runtime,
    reinterpret_cast<D3DDDICB_RENDER*>(renderPage.boundary)) == E_POINTER);
  unreadableRequest = false;
  CHECK(calls == 12 && records.size() == 24);
  for (size_t i = 0; i < records.size(); i += 2) {
    CHECK(records[i].find("CALLBACK_BEGIN") != std::string::npos);
    CHECK(records[i+1].find("CALLBACK_END") != std::string::npos);
    const std::string id = "call=" + std::to_string(i/2 + 1) + " ";
    CHECK(records[i].find(id) != std::string::npos && records[i+1].find(id) != std::string::npos);
    CHECK(records[i].find("forwarded=1") != std::string::npos
      && records[i+1].find("forwarded=1") != std::string::npos);
  }
  DWORD protection = 0;
  CHECK(VirtualProtect(callbackPage.memory, callbackPage.pageSize, PAGE_NOACCESS, &protection));
  const auto priorCalls = calls;
  expectedHandle = runtime; expectedRequest = &renderArgs;
  CHECK(owner->wrapped.pfnRenderCb(runtime, &renderArgs) == E_FAIL && calls == priorCalls);
  CHECK(records.back().find("forwarded=0") != std::string::npos);
  CHECK(VirtualProtect(callbackPage.memory, callbackPage.pageSize, protection, &protection));
  expectedHandle = runtime; expectedRequest = &renderArgs;
  live->pfnRenderCb = retireInCallback; retirementOwner = owner; retirementWeak = owner;
  const auto wrappedRender = owner->wrapped.pfnRenderCb; owner.reset();
  CHECK(wrappedRender(runtime, &renderArgs) == D3DERR_DEVICELOST);
  CHECK(retirementWeak.expired());
  const auto count = calls; CHECK(wrappedRender(runtime, &renderArgs) == E_INVALIDARG);
  CHECK(calls == count);
}

HANDLE frontRuntime = reinterpret_cast<HANDLE>(uintptr_t(0x900));
HANDLE frontDriver = reinterpret_cast<HANDLE>(uintptr_t(0x901));
const D3DDDI_DEVICECALLBACKS* frontOriginal = nullptr;
const D3DDDI_DEVICECALLBACKS* frontWrapped = nullptr;
bool failCreation = false;
HRESULT APIENTRY resourceStub(HANDLE, D3DDDIARG_CREATERESOURCE*) { return E_NOTIMPL; }
HRESULT APIENTRY destroyResourceStub(HANDLE, HANDLE) { return E_NOTIMPL; }
HRESULT APIENTRY stateStub(HANDLE, const D3DDDIARG_RENDERSTATE*) { return E_NOTIMPL; }
HRESULT APIENTRY destroyStub(HANDLE handle) {
  CHECK(handle == frontDriver); CHECK(frontWrapped);
  D3DDDICB_RENDER args = {}; expectedHandle = frontRuntime; expectedRequest = &args;
  CHECK(frontWrapped->pfnRenderCb(frontRuntime, &args) == S_FALSE);
  return S_OK;
}
HRESULT APIENTRY frontCreate(HANDLE, D3DDDIARG_CREATEDEVICE* args) {
  CHECK(args->hDevice == frontRuntime && args->pCallbacks != frontOriginal);
  frontWrapped = args->pCallbacks;
  D3DDDICB_RENDER render = {}; expectedHandle = frontRuntime; expectedRequest = &render;
  CHECK(frontWrapped->pfnRenderCb(frontRuntime, &render) == S_FALSE);
  if (failCreation) return E_INVALIDARG;
  D3DDDI_DEVICEFUNCS output = {};
  output.pfnCreateResource = resourceStub; output.pfnDestroyResource = destroyResourceStub;
  output.pfnSetRenderState = stateStub; output.pfnDestroyDevice = destroyStub;
  std::memcpy(args->pDeviceFuncs, &output, lifecycleFunctionBytes);
  args->hDevice = frontDriver; return S_OK;
}

void frontendControls() {
  const HANDLE adapter = reinterpret_cast<HANDLE>(uintptr_t(0xa00));
  const HANDLE runtimeAdapter = reinterpret_cast<HANDLE>(uintptr_t(0xa01));
  D3DDDI_ADAPTERFUNCS functions = {}; functions.pfnCreateDevice = frontCreate;
  adapters.emplace(adapter, Adapter{functions, true, runtimeAdapter});
  D3DDDI_DEVICECALLBACKS callbacks = {}; callbacks.pfnRenderCb = replacementRender;
  frontOriginal = &callbacks;
  Guarded page; D3DDDI_DEVICEFUNCS empty = {};
  auto* table = page.put(empty, lifecycleFunctionBytes);
  D3DDDIARG_CREATEDEVICE args = {}; args.hDevice = frontRuntime;
  args.pCallbacks = &callbacks; args.pDeviceFuncs = table;
  failCreation = true; CHECK(createDevice(adapter, &args) == E_INVALIDARG);
  CHECK(args.pCallbacks == &callbacks && args.hDevice == frontRuntime && devices.empty());
  failCreation = false; CHECK(createDevice(adapter, &args) == S_OK);
  CHECK(args.pCallbacks == &callbacks && args.pDeviceFuncs == table && args.hDevice == frontDriver);
  CHECK(devices.size() == 1 && table->pfnDestroyDevice == lifecycleDestroyDevice);
  CHECK(table->pfnCreateResource == lifecycleCreateResource
    && table->pfnDestroyResource == lifecycleDestroyResource && table->pfnSetRenderState == lifecycleRenderState);
  CHECK(table->pfnDestroyDevice(args.hDevice) == S_OK && devices.empty());
  adapters.erase(adapter);
}
}

int main() {
  expectedThread = GetCurrentThreadId(); callbackControls(); frontendControls();
  std::printf("probe D3D9 typed runtime callbacks verified checks=%u calls=%u vista_callbacks=22 vista_functions=99 hardware_admission=0\n", checks, calls);
}
