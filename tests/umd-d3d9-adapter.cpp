#include "../src/umd/umd_d3d9_adapter.h"
#include "../src/umd/umd_d3d8_compat.h"
#include <dxgi.h>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>

// The adapter fixture isolates the handshake from backend construction.
static std::atomic<unsigned> backendCreateCalls{0};
static std::atomic<UINT> backendCreateFlags{UINT_MAX};
static std::atomic<UINT> backendLegacyApi{UINT_MAX}, backendInterface{UINT_MAX};
#ifdef VIOGPU_DXVK_PUBLIC_LEGACY_ENTRY
static bool publishDevice;
static void (*afterDeviceCreation)() = nullptr;
static unsigned destroyedDevices;
static UINT deviceCookie;
static HRESULT APIENTRY destroyPublishedDevice(HANDLE handle) {
  if (handle != &deviceCookie) return E_INVALIDARG;
  ++destroyedDevices;
  return S_OK;
}
#endif
HRESULT dxvk::umd::createAdapterDevice9(const std::shared_ptr<const AdapterIdentity>& identity,
                                      D3DDDIARG_CREATEDEVICE* args) {
  ++backendCreateCalls;
  backendCreateFlags = args->Flags.Value;
  backendLegacyApi = UINT(identity->legacyApi); backendInterface = args->Interface;
#ifdef VIOGPU_DXVK_PUBLIC_LEGACY_ENTRY
  if (publishDevice) {
    *args->pDeviceFuncs = {};
    args->pDeviceFuncs->pfnDestroyDevice = destroyPublishedDevice;
    args->hDevice = &deviceCookie;
    if (afterDeviceCreation) afterDeviceCreation();
    return S_OK;
  }
#endif
  return D3DERR_NOTAVAILABLE;
}

#ifdef VIOGPU_DXVK_PUBLIC_LEGACY_ENTRY
static constexpr auto adapterEntry = OpenAdapter;
#else
static constexpr auto adapterEntry = VioGpuDxvkOpenAdapter9ForTest;
#endif

static std::atomic<unsigned> checks{0};
#define CHECK(condition) do { const auto n = ++checks; if (!(condition)) { \
  std::fprintf(stderr, "D3D9 adapter check %u failed at line %d: %s\n", n, __LINE__, #condition); \
  std::exit(1); } } while (0)

template<typename T> struct Guarded {
  std::array<uint8_t, 16> before;
  T value;
  std::array<uint8_t, 16> after;
  Guarded() { std::memset(this, 0xa5, sizeof(*this)); }
  void intact() const {
    for (auto byte : before) CHECK(byte == 0xa5);
    for (auto byte : after) CHECK(byte == 0xa5);
  }
};

template<typename T> std::array<uint8_t, sizeof(T)> snapshot(const T& value) {
  std::array<uint8_t, sizeof(T)> bytes;
  std::memcpy(bytes.data(), &value, bytes.size());
  return bytes;
}

enum class Action { None, NestedCaps, NestedCreate, NestedOpen, Close, CloseReopen,
  Block, ReplaceCapsOutput, ReplaceOpen, ReplaceCreate, ThrowAllocation, ThrowOther };
struct Runtime {
  UINT api = 9;
  std::array<uint8_t, 160> reply;
  HRESULT result = S_OK;
  unsigned calls = 0;
  HANDLE driver = nullptr;
  D3DDDI_ADAPTERFUNCS functions = {};
  Action action = Action::None;
  void set(size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) reply[offset+i] = uint8_t(value >> (8*i));
  }
  void valid(uint32_t luid) {
    reply = {};
    set(0, 0x504d5644); set(8, 128); set(16, 0x41); set(24, 7);
    set(128, 0x44494c56); set(132, 1); set(136, 32); set(140, 1);
    set(144, luid); set(148, 0xffffffb3); set(152, 1);
    result = S_OK; action = Action::None;
  }
};
static Runtime first, second;
static unsigned wrongCalls;
static D3DDDIARG_GETCAPS* changingCaps = nullptr;
static D3DDDIARG_OPENADAPTER* changingOpen = nullptr;
static D3DDDIARG_CREATEDEVICE* changingCreate = nullptr;
static D3DDDI_ADAPTERFUNCS alternateAdapterTable = {};
static D3DDDI_DEVICEFUNCS alternateDeviceTable = {};
static UINT replacedOutput = 0x11223344;
static std::mutex callbackMutex;
static std::condition_variable callbackChanged;
static bool callbackEntered, callbackReleased;

static HRESULT APIENTRY query(HANDLE, const D3DDDICB_QUERYADAPTERINFO*);
static HRESULT APIENTRY wrongQuery(HANDLE, const D3DDDICB_QUERYADAPTERINFO*) {
  wrongCalls++;
  return E_FAIL;
}
static D3DDDIARG_OPENADAPTER request(Runtime& owner, D3DDDI_ADAPTERFUNCS& functions,
    const D3DDDI_ADAPTERCALLBACKS& callbacks, UINT version = 0) {
  D3DDDIARG_OPENADAPTER args = {};
  args.hAdapter = &owner; args.Interface = owner.api; args.Version = version;
  args.pAdapterCallbacks = &callbacks; args.pAdapterFuncs = &functions;
  args.DriverVersion = 0xbadc0ffe;
  return args;
}
static void open(Runtime& owner, UINT version = 0) {
  D3DDDI_ADAPTERCALLBACKS callbacks = {};
  callbacks.pfnQueryAdapterInfoCb = query;
  Guarded<D3DDDI_ADAPTERFUNCS> table;
  auto args = request(owner, table.value, callbacks, version);
  CHECK(adapterEntry(&args) == S_OK);
  CHECK(args.hAdapter && args.hAdapter != &owner);
  CHECK(args.Interface == owner.api && args.Version == version);
  CHECK(args.DriverVersion == D3D_UMD_INTERFACE_VERSION_VISTA);
  CHECK(table.value.pfnGetCaps && table.value.pfnCreateDevice && table.value.pfnCloseAdapter);
  table.intact();
  owner.driver = args.hAdapter; owner.functions = table.value;
}
static void unchangedOpen(D3DDDIARG_OPENADAPTER args, HRESULT expected) {
  const auto original = snapshot(args);
  const auto table = args.pAdapterFuncs ? snapshot(*args.pAdapterFuncs)
    : std::array<uint8_t, sizeof(D3DDDI_ADAPTERFUNCS)>{};
  CHECK(adapterEntry(&args) == expected);
  CHECK(snapshot(args) == original);
  if (args.pAdapterFuncs) CHECK(snapshot(*args.pAdapterFuncs) == table);
}
static void unchangedCaps(Runtime& owner, HRESULT expected, HANDLE handle = nullptr) {
  Guarded<UINT> count;
  const auto before = snapshot(count);
  const D3DDDIARG_GETCAPS args = {D3DDDICAPS_GETFORMATCOUNT, nullptr, &count.value, sizeof(UINT)};
  const auto input = snapshot(args);
  CHECK(owner.functions.pfnGetCaps(handle ? handle : owner.driver, &args) == expected);
  CHECK(snapshot(count) == before && snapshot(args) == input);
}
static void countCaps(Runtime& owner) {
  for (const auto type : {D3DDDICAPS_GETFORMATCOUNT, D3DDDICAPS_GETD3DQUERYCOUNT}) {
    Guarded<UINT> count;
    const D3DDDIARG_GETCAPS args = {type, nullptr, &count.value, sizeof(UINT)};
    const auto input = snapshot(args);
    CHECK(owner.functions.pfnGetCaps(owner.driver, &args) == S_OK);
    CHECK(count.value == (type == D3DDDICAPS_GETFORMATCOUNT ? 4u : 6u) && snapshot(args) == input);
    count.intact();
  }
}
static HRESULT APIENTRY query(HANDLE runtime, const D3DDDICB_QUERYADAPTERINFO* args) {
  CHECK(runtime == &first || runtime == &second);
  auto& owner = *static_cast<Runtime*>(runtime);
  owner.calls++;
  CHECK(args && args->pPrivateDriverData && args->PrivateDriverDataSize == owner.reply.size());
  auto bytes = static_cast<uint8_t*>(args->pPrivateDriverData);
  for (size_t i = 0; i < owner.reply.size(); i++) CHECK(bytes[i] == 0);
  const auto action = owner.action;
  owner.action = Action::None;
  switch (action) {
    case Action::NestedCaps: {
      const auto calls = owner.calls;
      unchangedCaps(owner, D3DERR_WASSTILLDRAWING);
      CHECK(owner.calls == calls);
    } break;
    case Action::NestedCreate: {
      D3DDDI_DEVICECALLBACKS callbacks = {};
      Guarded<D3DDDI_DEVICEFUNCS> table;
      D3DDDIARG_CREATEDEVICE create = {};
      create.hDevice = &owner; create.Interface = owner.api;
      create.pCallbacks = &callbacks; create.pDeviceFuncs = &table.value;
      const auto input = snapshot(create); const auto output = snapshot(table);
      const auto calls = owner.calls;
      CHECK(owner.functions.pfnCreateDevice(owner.driver, &create) == D3DERR_WASSTILLDRAWING);
      CHECK(snapshot(create) == input && snapshot(table) == output && owner.calls == calls);
    } break;
    case Action::NestedOpen: {
      Guarded<D3DDDI_ADAPTERFUNCS> table;
      D3DDDI_ADAPTERCALLBACKS callbacks = {};
      callbacks.pfnQueryAdapterInfoCb = query;
      const auto calls = owner.calls;
      unchangedOpen(request(owner, table.value, callbacks), D3DERR_WASSTILLDRAWING);
      CHECK(owner.calls == calls); table.intact();
    } break;
    case Action::Close:
      CHECK(owner.functions.pfnCloseAdapter(owner.driver) == S_OK);
      break;
    case Action::CloseReopen: {
      const HANDLE old = owner.driver;
      CHECK(owner.functions.pfnCloseAdapter(old) == S_OK);
      open(owner);
      CHECK(owner.driver != old);
    } break;
    case Action::Block: {
      std::unique_lock<std::mutex> lock(callbackMutex);
      callbackEntered = true; callbackChanged.notify_one();
      CHECK(callbackChanged.wait_for(lock, std::chrono::seconds(10), [] { return callbackReleased; }));
    } break;
    case Action::ReplaceCapsOutput:
      CHECK(changingCaps);
      changingCaps->Type = D3DDDICAPS_DDRAW;
      changingCaps->pData = &replacedOutput;
      changingCaps->DataSize = UINT_MAX;
      changingCaps->pInfo = &owner;
      break;
    case Action::ReplaceOpen:
      CHECK(changingOpen);
      changingOpen->Interface = 9; changingOpen->Version = UINT_MAX;
      changingOpen->hAdapter = &second;
      changingOpen->pAdapterFuncs = &alternateAdapterTable;
      changingOpen->pAdapterCallbacks = reinterpret_cast<D3DDDI_ADAPTERCALLBACKS*>(UINT_PTR(1));
      break;
    case Action::ReplaceCreate:
      CHECK(changingCreate);
      changingCreate->Interface = 9; changingCreate->Version = UINT_MAX;
      changingCreate->Flags.Value = UINT_MAX;
      changingCreate->pCallbacks = reinterpret_cast<D3DDDI_DEVICECALLBACKS*>(UINT_PTR(1));
      changingCreate->pDeviceFuncs = &alternateDeviceTable;
      break;
    case Action::ThrowAllocation: throw std::bad_alloc();
    case Action::ThrowOther: throw 1;
    case Action::None: break;
  }
  std::memcpy(bytes, owner.reply.data(), owner.reply.size());
  return owner.result;
}

static void legacyAdapterContracts() {
  static_assert(std::is_const_v<decltype(dxvk::umd::AdapterIdentity::legacyApi)>);
  first.valid(0x12345678); first.api = 8;
  D3DDDI_ADAPTERCALLBACKS callbacks = {}; callbacks.pfnQueryAdapterInfoCb = query;
  Guarded<D3DDDI_ADAPTERFUNCS> table;
  for (const HRESULT failure : {S_FALSE,E_FAIL,DXGI_ERROR_DEVICE_REMOVED}) {
    first.result = failure;
    unchangedOpen(request(first,table.value,callbacks),
      failure == S_FALSE ? E_FAIL : failure == DXGI_ERROR_DEVICE_REMOVED ? D3DERR_DEVICELOST : failure);
  }
  first.valid(0x12345678); first.reply[0] ^= 1;
  unchangedOpen(request(first,table.value,callbacks),D3DERR_NOTAVAILABLE);
  first.valid(0x12345678);
  auto args = request(first, table.value, callbacks, 0x11000);
  std::memset(&alternateAdapterTable, 0xa5, sizeof(alternateAdapterTable));
  const auto alternateBefore = snapshot(alternateAdapterTable);
  changingOpen = &args; first.action = Action::ReplaceOpen;
  CHECK(adapterEntry(&args) == S_OK); changingOpen = nullptr;
  CHECK(args.Interface == 9 && args.Version == UINT_MAX && args.DriverVersion == D3D_UMD_INTERFACE_VERSION_VISTA);
  CHECK(snapshot(alternateAdapterTable) == alternateBefore);
  first.driver = args.hAdapter; first.functions = table.value; table.intact();
  // Caller metadata now says9 and the callback storage can expire. Identity
  // remains8 with the original callback/runtime handle and output destination.
  callbacks.pfnQueryAdapterInfoCb = wrongQuery;
  Guarded<dxvk::umd::D3D8CapsPrefix> caps;
  D3DDDIARG_GETCAPS get = {D3DDDICAPS_GETD3D8CAPS, nullptr, caps.value.data(), sizeof(caps.value)};
  const auto capsRequest = snapshot(get);
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == S_OK && snapshot(get) == capsRequest);
  CHECK(caps.value[0] == D3DDEVTYPE_HAL && caps.value[49] == D3DVS_VERSION(1,1));
  CHECK(caps.value[50] == 96 && caps.value[51] == D3DPS_VERSION(1,4) && !wrongCalls);
  CHECK(!caps.value[17] && !caps.value[18] && !caps.value[24]); caps.intact();
  get.pData = nullptr; const auto nullCalls = first.calls;
  CHECK(first.functions.pfnGetCaps(first.driver,&get) == E_INVALIDARG && first.calls == nullCalls);
  get.pData = caps.value.data();
  for (UINT bytes = 0; bytes <= sizeof(caps.value) + 1; ++bytes) {
    if (bytes == sizeof(caps.value)) continue;
    get.DataSize = bytes; const auto output = snapshot(caps); const auto request = snapshot(get); const auto calls = first.calls;
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG);
    CHECK(snapshot(caps) == output && snapshot(get) == request && first.calls == calls);
  }
  for (const auto type : {D3DDDICAPS_GETD3D9CAPS, D3DDDICAPS_GETD3D7CAPS, D3DDDICAPS_TYPE(999)}) {
    get = {type,nullptr,caps.value.data(),sizeof(caps.value)}; const auto output = snapshot(caps); const auto calls = first.calls;
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == D3DERR_NOTAVAILABLE);
    CHECK(snapshot(caps) == output && first.calls == calls);
  }
  get = {D3DDDICAPS_GETD3D8CAPS,nullptr,caps.value.data(),sizeof(caps.value)};
  changingCaps = &get; first.action = Action::ReplaceCapsOutput;
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == S_OK && replacedOutput == 0x11223344);
  changingCaps = nullptr; caps.intact();
  D3DDDI_DEVICECALLBACKS cb = {}; Guarded<D3DDDI_DEVICEFUNCS> deviceTable;
  D3DDDIARG_CREATEDEVICE create = {}; create.hDevice = &second; create.Interface = 9;
  create.pCallbacks = &cb; create.pDeviceFuncs = &deviceTable.value;
  const auto calls = first.calls; const auto backend = backendCreateCalls.load(); const auto output = snapshot(deviceTable);
  CHECK(first.functions.pfnCreateDevice(first.driver, &create) == D3DERR_NOTAVAILABLE);
  CHECK(first.calls == calls && backendCreateCalls == backend && snapshot(deviceTable) == output);
  create.Interface = 8;
  for (unsigned bit = 2; bit < 32; ++bit) {
    create.Flags.Value = UINT(1) << bit; const auto input = snapshot(create);
    CHECK(first.functions.pfnCreateDevice(first.driver,&create) == D3DERR_NOTAVAILABLE);
    CHECK(snapshot(create) == input && first.calls == calls && backendCreateCalls == backend && snapshot(deviceTable) == output);
  }
  create.Interface = 8; create.Flags.Value = 3;
  std::memset(&alternateDeviceTable,0xa5,sizeof(alternateDeviceTable)); const auto alternative = snapshot(alternateDeviceTable);
  changingCreate = &create; first.action = Action::ReplaceCreate;
  CHECK(first.functions.pfnCreateDevice(first.driver, &create) == D3DERR_NOTAVAILABLE); changingCreate = nullptr;
  CHECK(backendLegacyApi == 8 && backendInterface == 8 && backendCreateFlags == 3);
  CHECK(snapshot(deviceTable) == output && snapshot(alternateDeviceTable) == alternative);
  get = {D3DDDICAPS_GETD3D8CAPS,nullptr,caps.value.data(),sizeof(caps.value)};
  for (const auto action : {Action::NestedCaps,Action::NestedCreate,Action::ThrowAllocation,Action::ThrowOther,Action::Close}) {
    first.action = action; const auto prior = snapshot(caps);
    const HRESULT expected = action == Action::Close ? D3DERR_DEVICELOST : action == Action::ThrowAllocation ? E_OUTOFMEMORY
      : action == Action::ThrowOther ? E_FAIL : S_OK;
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == expected);
    if (expected != S_OK) CHECK(snapshot(caps) == prior);
  }
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG);
  first.api = 9; first.valid(0x12345678); first.calls = 0;
}

#ifdef VIOGPU_DXVK_PUBLIC_LEGACY_ENTRY
static void publicDeviceContracts() {
  publishDevice = true;
  for (const UINT api : {8u, 9u}) {
    first.api = api; first.valid(0x12345678);
    open(first);
    Guarded<D3DDDI_DEVICEFUNCS> table;
    D3DDDI_DEVICECALLBACKS callbacks = {};
    D3DDDIARG_CREATEDEVICE args = {};
    args.hDevice = &first; args.Interface = api; args.Version = UINT_MAX;
    args.pCallbacks = &callbacks; args.pDeviceFuncs = &table.value;
    const auto before = snapshot(table);
    const auto backendBefore = backendCreateCalls.load();
    const auto destroyBefore = destroyedDevices;
    CHECK(first.functions.pfnCreateDevice(first.driver, &args) == S_OK);
    CHECK(args.hDevice == &deviceCookie && args.Interface == api && args.Version == UINT_MAX);
    CHECK(backendCreateCalls == backendBefore + 1 && destroyedDevices == destroyBefore);
    CHECK(backendLegacyApi == api && backendInterface == api);
    CHECK(table.value.pfnDestroyDevice == destroyPublishedDevice);
    const auto after = snapshot(table);
    const size_t tail = offsetof(Guarded<D3DDDI_DEVICEFUNCS>, value)
      + dxvk::umd::d3d9DeviceFunctionBytes;
    for (size_t i = tail; i < after.size(); ++i) CHECK(after[i] == before[i]);
    table.intact();
    CHECK(table.value.pfnDestroyDevice(args.hDevice) == S_OK);
    CHECK(destroyedDevices == destroyBefore + 1);
    CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);
  }
  first.api = 9;
  // These failures occur only after the backend returned a live device.
  // Destroy it and leave both runtime output owners intact.
  for (const auto action : {Action::Close, Action::ThrowAllocation, Action::ThrowOther}) {
    first.valid(0x12345678); open(first);
    Guarded<D3DDDI_DEVICEFUNCS> table;
    D3DDDI_DEVICECALLBACKS callbacks = {};
    D3DDDIARG_CREATEDEVICE args = {};
    args.hDevice = &first; args.Interface = 9;
    args.pCallbacks = &callbacks; args.pDeviceFuncs = &table.value;
    const auto before = snapshot(args); const auto tableBefore = snapshot(table);
    const auto destroyBefore = destroyedDevices;
    second.action = action;
    afterDeviceCreation = [] { first.action = second.action; };
    const HRESULT expected = action == Action::Close ? D3DERR_DEVICELOST
      : action == Action::ThrowAllocation ? E_OUTOFMEMORY : E_FAIL;
    CHECK(first.functions.pfnCreateDevice(first.driver, &args) == expected);
    CHECK(snapshot(args) == before && snapshot(table) == tableBefore);
    CHECK(destroyedDevices == destroyBefore + 1);
    CHECK(first.functions.pfnCloseAdapter(first.driver)
      == (action == Action::Close ? E_INVALIDARG : S_OK));
  }
  afterDeviceCreation = nullptr; publishDevice = false;
  first.api = 9; first.valid(0x12345678); first.calls = 0;
}
#endif

int main() {
  legacyAdapterContracts();
  first.valid(0x12345678); second.valid(0x99887766);
  CHECK(adapterEntry(nullptr) == E_INVALIDARG);
  Guarded<D3DDDI_ADAPTERFUNCS> table;
  D3DDDI_ADAPTERCALLBACKS callbacks = {};
  callbacks.pfnQueryAdapterInfoCb = query;
  const auto valid = request(first, table.value, callbacks);
  for (const UINT interfaceVersion : {0u, 7u, 10u, 11u, 12u, 0x000a0000u, 0xffffffffu}) {
    auto args = valid; args.Interface = interfaceVersion;
    unchangedOpen(args, D3DERR_NOTAVAILABLE);
  }
  auto bad = valid; bad.hAdapter = nullptr; unchangedOpen(bad, E_INVALIDARG);
  bad = valid; bad.pAdapterFuncs = nullptr; unchangedOpen(bad, E_INVALIDARG);
  bad = valid; bad.pAdapterCallbacks = nullptr; unchangedOpen(bad, E_INVALIDARG);
  callbacks.pfnQueryAdapterInfoCb = nullptr; unchangedOpen(valid, E_INVALIDARG);
  callbacks.pfnQueryAdapterInfoCb = query;
  CHECK(first.calls == 0);

  const auto reply = first.reply;
  for (const size_t offset : {size_t(0), size_t(4), size_t(8), size_t(12),
      size_t(112), size_t(120), size_t(128), size_t(132), size_t(136), size_t(140), size_t(152), size_t(156)}) {
    first.reply[offset] ^= 1;
    unchangedOpen(valid, D3DERR_NOTAVAILABLE);
    first.reply = reply;
  }
  std::memset(first.reply.data()+144, 0, 8);
  unchangedOpen(valid, D3DERR_NOTAVAILABLE); first.reply = reply;
  std::memset(first.reply.data()+24, 0, 8);
  unchangedOpen(valid, D3DERR_NOTAVAILABLE); first.reply = reply;
  std::memset(first.reply.data()+128, 0, 32);
  unchangedOpen(valid, D3DERR_NOTAVAILABLE); first.reply = reply;
  for (const HRESULT result : {S_FALSE, E_FAIL, E_OUTOFMEMORY, DXGI_ERROR_DEVICE_REMOVED}) {
    first.result = result;
    unchangedOpen(valid, result == S_FALSE ? E_FAIL
      : result == DXGI_ERROR_DEVICE_REMOVED ? D3DERR_DEVICELOST : result);
  }
  first.result = S_OK;
  first.action = Action::ThrowAllocation; unchangedOpen(valid, E_OUTOFMEMORY);
  first.action = Action::ThrowOther; unchangedOpen(valid, E_FAIL);
  first.action = Action::NestedOpen;
  open(first); countCaps(first);
  CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);

  // Runtime build numbers do not follow the D3D10 packed-build policy.
  for (const UINT version : {0u, 1u, 9u, 0x00010000u, 0xffffffffu}) {
    open(first, version); countCaps(first);
    CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);
  }
  auto args = valid;
  CHECK(adapterEntry(&args) == S_OK);
  first.driver = args.hAdapter; first.functions = table.value;
  // Query ownership is captured before the runtime callback table changes.
  callbacks.pfnQueryAdapterInfoCb = wrongQuery;
  countCaps(first); CHECK(wrongCalls == 0);
  callbacks.pfnQueryAdapterInfoCb = query;

  Guarded<D3DCAPS9> caps;
  D3DDDIARG_GETCAPS get = {D3DDDICAPS_GETD3D9CAPS, nullptr, &caps.value, sizeof(D3DCAPS9)};
  const auto input = snapshot(get);
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == S_OK && snapshot(get) == input);
  CHECK(caps.value.DeviceType == D3DDEVTYPE_HAL);
  CHECK(caps.value.VertexShaderVersion == D3DVS_VERSION(2, 0));
  CHECK(caps.value.PixelShaderVersion == D3DPS_VERSION(2, 0));
  CHECK(caps.value.NumSimultaneousRTs == 1 && caps.value.MaxTextureWidth >= 2048);
  CHECK(!(caps.value.TextureCaps & (D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_VOLUMEMAP)));
  CHECK(caps.value.Caps2 & D3DCAPS2_DYNAMICTEXTURES);
  CHECK(caps.value.PrimitiveMiscCaps & dxvk::umd::d3d9DdiFogInFvf);
  CHECK(!(caps.value.Caps2 & (D3DCAPS2_CANAUTOGENMIPMAP | D3DCAPS2_CANSHARERESOURCE)));
  CHECK(!caps.value.CubeTextureFilterCaps && !caps.value.VolumeTextureFilterCaps
    && !caps.value.VertexTextureFilterCaps && !caps.value.StretchRectFilterCaps);
  caps.intact();
  for (const UINT size : {0u, UINT(sizeof(D3DCAPS9)-1), UINT(sizeof(D3DCAPS9)+1), 0xffffffffu}) {
    Guarded<D3DCAPS9> output; get.pData = &output.value; get.DataSize = size;
    const auto original = snapshot(output); const auto requestBytes = snapshot(get);
    const auto calls = first.calls;
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG);
    CHECK(snapshot(output) == original && snapshot(get) == requestBytes && first.calls == calls);
  }
  Guarded<UINT> count;
  get = {D3DDDICAPS_GETFORMATCOUNT, nullptr, &count.value, sizeof(UINT)};
  const auto countBefore = snapshot(count); const auto callsBefore = first.calls;
  get.pInfo = &count;
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG);
  get.pInfo = nullptr; get.pData = nullptr;
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG);
  CHECK(first.functions.pfnGetCaps(first.driver, nullptr) == E_INVALIDARG);
  CHECK(snapshot(count) == countBefore && first.calls == callsBefore);
  for (const auto type : {D3DDDICAPS_GETD3D8CAPS, D3DDDICAPS_GETD3D7CAPS,
      D3DDDICAPS_DDRAW, D3DDDICAPS_GETMULTISAMPLEQUALITYLEVELS, D3DDDICAPS_TYPE(999)}) {
    get = {type, nullptr, &count.value, sizeof(UINT)};
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == D3DERR_NOTAVAILABLE);
    CHECK(snapshot(count) == countBefore && first.calls == callsBefore);
  }
  Guarded<std::array<FORMATOP, 4>> formatData;
  Guarded<std::array<D3DDDIQUERYTYPE, 6>> queryData;
  for (const auto type : {D3DDDICAPS_GETFORMATDATA, D3DDDICAPS_GETD3DQUERYDATA}) {
    void* data = type == D3DDDICAPS_GETFORMATDATA ? static_cast<void*>(&formatData.value)
      : static_cast<void*>(&queryData.value);
    const UINT bytes = type == D3DDDICAPS_GETFORMATDATA ? sizeof(formatData.value) : sizeof(queryData.value);
    get = {type, nullptr, data, bytes};
    const auto requestBytes = snapshot(get);
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == S_OK && snapshot(get) == requestBytes);
    for (const UINT size : {0u, bytes-1, bytes+1, UINT_MAX}) {
      get.DataSize = size;
      const auto beforeFormats = snapshot(formatData); const auto beforeQueries = snapshot(queryData);
      const auto inputBytes = snapshot(get); const auto calls = first.calls;
      CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG);
      CHECK(snapshot(formatData) == beforeFormats && snapshot(queryData) == beforeQueries
        && snapshot(get) == inputBytes && first.calls == calls);
    }
    get = {type, nullptr, nullptr, bytes};
    const auto calls = first.calls;
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG && first.calls == calls);
  }
  formatData.intact(); queryData.intact();
  const std::array<D3DDDIFORMAT, 4> supportedFormats = {
    D3DDDIFMT_X8R8G8B8, D3DDDIFMT_A8R8G8B8, D3DDDIFMT_D16, D3DDDIFMT_D24S8};
  for (size_t i = 0; i < supportedFormats.size(); ++i) {
    const auto& format = formatData.value[i];
    CHECK(format.Format == supportedFormats[i]);
    CHECK(!format.FlipMsTypes && !format.BltMsTypes && !format.PrivateFormatBitCount);
    CHECK(!(format.Operations & (FORMATOP_CUBETEXTURE | FORMATOP_VOLUMETEXTURE | FORMATOP_OFFSCREENPLAIN
      | FORMATOP_AUTOGENMIPMAP | FORMATOP_VERTEXTEXTURE | FORMATOP_OVERLAY | FORMATOP_CONVERT_TO_ARGB)));
    if (i < 2) CHECK(format.Operations & FORMATOP_TEXTURE);
    else CHECK(format.Operations & FORMATOP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH);
    CHECK(bool(format.Operations & FORMATOP_DISPLAYMODE) == (i == 0));
    CHECK(bool(format.Operations & FORMATOP_3DACCELERATION) == (i == 0));
  }
  const std::array<D3DDDIQUERYTYPE, 6> supportedQueries = {D3DDDIQUERYTYPE_VCACHE, D3DDDIQUERYTYPE_EVENT,
    D3DDDIQUERYTYPE_OCCLUSION, D3DDDIQUERYTYPE_TIMESTAMP, D3DDDIQUERYTYPE_TIMESTAMPDISJOINT, D3DDDIQUERYTYPE_TIMESTAMPFREQ};
  CHECK(queryData.value == supportedQueries);
  Guarded<DDIGAMMACAPS> gamma;
  get = {D3DDDICAPS_GETGAMMARAMPCAPS, nullptr, &gamma.value, sizeof(gamma.value)};
  const auto gammaRequest = snapshot(get);
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == S_OK && !gamma.value.GammaCaps
    && snapshot(get) == gammaRequest);
  gamma.intact();
  for (const UINT size : {0u, UINT(sizeof(gamma.value)-1), UINT(sizeof(gamma.value)+1), UINT_MAX}) {
    get.DataSize = size;
    const auto original = snapshot(gamma); const auto calls = first.calls;
    CHECK(first.functions.pfnGetCaps(first.driver, &get) == E_INVALIDARG
      && snapshot(gamma) == original && first.calls == calls);
  }
  Guarded<UINT> originalOutput;
  get = {D3DDDICAPS_GETFORMATCOUNT, nullptr, &originalOutput.value, sizeof(UINT)};
  changingCaps = &get; first.action = Action::ReplaceCapsOutput;
  CHECK(first.functions.pfnGetCaps(first.driver, &get) == S_OK);
  changingCaps = nullptr;
  CHECK(originalOutput.value == 4 && replacedOutput == 0x11223344);
  CHECK(get.Type == D3DDDICAPS_DDRAW && get.DataSize == UINT_MAX
    && get.pData == &replacedOutput && get.pInfo == &first);
  originalOutput.intact();

  D3DDDI_DEVICECALLBACKS deviceCallbacks = {};
  Guarded<D3DDDI_DEVICEFUNCS> deviceTable;
  Guarded<D3DDDI_ALLOCATIONLIST> allocations;
  Guarded<D3DDDI_PATCHLOCATIONLIST> patches;
  Guarded<std::array<uint8_t, 32>> command;
  D3DDDIARG_CREATEDEVICE create = {};
  create.hDevice = &second; create.Interface = 9; create.Version = 1;
  create.pCallbacks = &deviceCallbacks; create.pDeviceFuncs = &deviceTable.value;
  create.pCommandBuffer = command.value.data(); create.CommandBufferSize = 32;
  create.pAllocationList = &allocations.value; create.AllocationListSize = 1;
  create.pPatchLocationList = &patches.value; create.PatchLocationListSize = 1;
  create.CommandBuffer = 0xcafef00d;
  const auto createBefore = snapshot(create); const auto deviceBefore = snapshot(deviceTable);
  const auto allocationsBefore = snapshot(allocations); const auto patchesBefore = snapshot(patches);
  const auto commandBefore = snapshot(command);
  CHECK(first.functions.pfnCreateDevice(first.driver, &create) == D3DERR_NOTAVAILABLE);
  CHECK(snapshot(create) == createBefore && snapshot(deviceTable) == deviceBefore);
  CHECK(snapshot(allocations) == allocationsBefore && snapshot(patches) == patchesBefore);
  CHECK(snapshot(command) == commandBefore);
  CHECK(first.functions.pfnCreateDevice(first.driver, nullptr) == E_INVALIDARG);
  for (unsigned field = 0; field < 5; field++) {
    auto invalid = create;
    if (field == 0) invalid.hDevice = nullptr;
    if (field == 1) invalid.pCallbacks = nullptr;
    if (field == 2) invalid.pDeviceFuncs = nullptr;
    if (field == 3) invalid.Interface = 10;
    if (field == 4) invalid.Flags.Value = 4;
    const auto original = snapshot(invalid); const auto calls = first.calls;
    CHECK(first.functions.pfnCreateDevice(first.driver, &invalid)
      == (field < 3 ? E_INVALIDARG : D3DERR_NOTAVAILABLE));
    CHECK(snapshot(invalid) == original && snapshot(deviceTable) == deviceBefore && first.calls == calls);
  }
  for (const UINT flags : {UINT(0), UINT(1), UINT(2), UINT(3)}) {
    auto permitted = create; permitted.Flags.Value = flags;
    const auto original = snapshot(permitted);
    const auto backendBefore = backendCreateCalls.load(); const auto queriesBefore = first.calls;
    CHECK(first.functions.pfnCreateDevice(first.driver, &permitted) == D3DERR_NOTAVAILABLE);
    CHECK(backendCreateCalls == backendBefore + 1 && backendCreateFlags == flags);
    CHECK(first.calls == queriesBefore + 1 && snapshot(permitted) == original);
    CHECK(snapshot(deviceTable) == deviceBefore && snapshot(allocations) == allocationsBefore);
    CHECK(snapshot(patches) == patchesBefore && snapshot(command) == commandBefore);
  }
  for (UINT bit = 2; bit < 32; ++bit) {
    auto reserved = create; reserved.Flags.Value = (UINT(1) << bit) | 3u;
    const auto original = snapshot(reserved);
    const auto backendBefore = backendCreateCalls.load(); const auto queriesBefore = first.calls;
    CHECK(first.functions.pfnCreateDevice(first.driver, &reserved) == D3DERR_NOTAVAILABLE);
    CHECK(backendCreateCalls == backendBefore && first.calls == queriesBefore);
    CHECK(snapshot(reserved) == original && snapshot(deviceTable) == deviceBefore);
  }
  first.action = Action::NestedCaps; countCaps(first);
  first.action = Action::NestedCreate; countCaps(first);
  first.action = Action::ThrowAllocation; unchangedCaps(first, E_OUTOFMEMORY);
  first.action = Action::ThrowOther; unchangedCaps(first, E_FAIL);
  first.result = S_FALSE; unchangedCaps(first, E_FAIL);
  first.result = E_FAIL; unchangedCaps(first, E_FAIL);
  first.result = S_OK;
  first.reply[128] ^= 1; unchangedCaps(first, D3DERR_NOTAVAILABLE);
  first.reply = reply; countCaps(first);
  const HANDLE stale = first.driver;
  first.action = Action::CloseReopen;
  unchangedCaps(first, D3DERR_DEVICELOST, stale);
  unchangedCaps(first, E_INVALIDARG, stale); countCaps(first);
  CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);

  for (const size_t offset : {size_t(144), size_t(24), size_t(16)}) {
    first.valid(0x12345678); open(first);
    first.reply[offset] ^= 1; unchangedCaps(first, D3DERR_DEVICELOST);
    const auto calls = first.calls;
    first.reply = reply; unchangedCaps(first, D3DERR_DEVICELOST);
    CHECK(first.calls == calls);
    CHECK(first.functions.pfnCreateDevice(first.driver, &create) == D3DERR_DEVICELOST);
    CHECK(snapshot(create) == createBefore && snapshot(deviceTable) == deviceBefore);
    CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);
  }
  for (const HRESULT result : {D3DERR_DEVICELOST, D3DERR_DEVICENOTRESET,
      D3DERR_DEVICEREMOVED, DXGI_ERROR_DEVICE_REMOVED, DXGI_ERROR_DEVICE_RESET}) {
    first.valid(0x12345678); open(first);
    first.result = result; unchangedCaps(first, D3DERR_DEVICELOST);
    const auto calls = first.calls; first.result = S_OK;
    unchangedCaps(first, D3DERR_DEVICELOST); CHECK(first.calls == calls);
    CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);
  }
  open(first); first.action = Action::Close;
  unchangedCaps(first, D3DERR_DEVICELOST);
  unchangedCaps(first, E_INVALIDARG);
  CHECK(first.functions.pfnCloseAdapter(first.driver) == E_INVALIDARG);

  // Real overlapping calls: close cannot free the blocked query's owner, and
  // an unrelated adapter keeps its own original callback handle and identity.
  open(first); const HANDLE blocked = first.driver;
  first.action = Action::Block;
  std::thread worker([&] { unchangedCaps(first, D3DERR_DEVICELOST, blocked); });
  {
    std::unique_lock<std::mutex> lock(callbackMutex);
    CHECK(callbackChanged.wait_for(lock, std::chrono::seconds(10), [] { return callbackEntered; }));
  }
  unchangedCaps(first, D3DERR_WASSTILLDRAWING);
  CHECK(first.functions.pfnCloseAdapter(blocked) == S_OK);
  open(second); countCaps(second);
  {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbackReleased = true; callbackChanged.notify_one();
  }
  worker.join();
  CHECK(second.functions.pfnCloseAdapter(second.driver) == S_OK);

  for (unsigned i = 0; i < 128; i++) {
    open(first);
    CHECK(first.driver != stale && first.driver != blocked);
    const auto calls = first.calls;
    unchangedCaps(first, E_INVALIDARG, stale);
    unchangedCaps(first, E_INVALIDARG, blocked);
    CHECK(first.calls == calls);
    CHECK(first.functions.pfnCloseAdapter(first.driver) == S_OK);
  }
  CHECK(first.functions.pfnCloseAdapter(nullptr) == E_INVALIDARG && wrongCalls == 0);
  table.intact();
#ifdef VIOGPU_DXVK_PUBLIC_LEGACY_ENTRY
  publicDeviceContracts();
  std::printf("native legacy OpenAdapter PASS checks=%u; Interface8/9 mock runtime, no rendering\n", checks.load());
#else
  std::printf("native D3D9 adapter PASS checks=%u; mock runtime, no rendering or admission\n", checks.load());
#endif
}
