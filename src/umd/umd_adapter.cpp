#include "umd_adapter.h"
#include "umd_contract.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

namespace {
struct Adapter {
  std::shared_ptr<const dxvk::umd::AdapterIdentity> identity;
  std::atomic<bool> closed{false}, removed{false};
  std::atomic_flag querying = ATOMIC_FLAG_INIT;
  bool development = false;
  bool validation11Fl10_0 = false;
  UINT legacyInterface = 0;
};

std::mutex adaptersMutex;
std::unordered_map<void*, std::shared_ptr<Adapter>> adapters;
std::uintptr_t nextAdapterToken = 1;

std::shared_ptr<Adapter> retain(D3D10DDI_HADAPTER handle) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle.pDrvPrivate);
  return entry == adapters.end() ? nullptr : entry->second;
}

HRESULT state(const std::shared_ptr<Adapter>& adapter) noexcept {
  if (!adapter) return E_INVALIDARG;
  return adapter->closed || adapter->removed ? DXGI_ERROR_DEVICE_REMOVED : S_OK;
}

bool admitted(const Adapter& adapter, UINT interfaceVersion, UINT flags = 0) {
  // Only the dedicated SYSTEM-validation entry may break the ordinary-proof
  // dependency cycle. Its tag remains owned by this adapter until CloseAdapter.
  if (adapter.validation11Fl10_0)
    return interfaceVersion == D3D11_0_DDI_INTERFACE_VERSION
      && !(flags & ~D3D11DDI_CREATEDEVICE_FLAG_SINGLETHREADED);
  return (!adapter.legacyInterface || adapter.legacyInterface == interfaceVersion)
    && (adapter.development || dxvk::umd::runtimeSupportsNativeInterface(interfaceVersion, flags));
}

HRESULT current(const std::shared_ptr<Adapter>& adapter) noexcept {
  HRESULT hr = state(adapter);
  if (FAILED(hr)) return hr;
  // Runtime callbacks can synchronously reenter the UMD. Never hold the
  // registry mutex across them, and never recurse or wait on our own query.
  if (adapter->querying.test_and_set()) return DXGI_ERROR_WAS_STILL_DRAWING;
  struct QueryGuard { Adapter& adapter; ~QueryGuard() { adapter.querying.clear(); } } guard{*adapter};
  try {
    const auto& expected = *adapter->identity;
    dxvk::umd::RuntimeIdentity observed;
    hr = dxvk::umd::queryRuntimeIdentity(expected.runtime, expected.query, observed);
    if (FAILED(state(adapter))) return DXGI_ERROR_DEVICE_REMOVED;
    if (FAILED(hr)) return hr;
    if (std::memcmp(observed.luid.data(), &expected.luid, sizeof(LUID))
        || observed.generation != expected.generation
        || observed.capabilities != expected.capabilities) {
      adapter->removed = true;
      return DXGI_ERROR_DEVICE_REMOVED;
    }
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

SIZE_T APIENTRY privateDeviceSize(D3D10DDI_HADAPTER handle,
    const D3D10DDIARG_CALCPRIVATEDEVICESIZE* args) {
  auto adapter = retain(handle);
  if (FAILED(state(adapter)) || !args) return 0;
  const auto input = *args;
  if (!dxvk::umd::supportedNativeInterface(input.Interface, input.Version, input.Flags)
      || !admitted(*adapter, input.Interface, input.Flags) || FAILED(current(adapter))) return 0;
  const SIZE_T size = VioGpuDxvkPrivateDeviceSize();
  std::lock_guard<std::mutex> lock(adaptersMutex);
  return SUCCEEDED(state(adapter)) ? size : 0;
}

template<typename Functions, typename DxgiFunctions>
HRESULT createAndPublish(const std::shared_ptr<Adapter>& adapter,
    D3D10DDIARG_CREATEDEVICE& local, Functions& table, Functions* output,
    DxgiFunctions& dxgi, DxgiFunctions* dxgiOutput) {
  HRESULT hr = current(adapter);
  if (FAILED(hr)) return hr;
  hr = dxvk::umd::createAdapterDevice(adapter->identity, &local);
  if (FAILED(hr)) return hr;
  if (!table.pfnDestroyDevice) return E_FAIL;
  struct DeviceGuard {
    D3D10DDI_HDEVICE handle;
    PFND3D10DDI_DESTROYDEVICE destroy;
    ~DeviceGuard() { if (handle.pDrvPrivate) destroy(handle); }
  } guard{local.hDrvDevice, table.pfnDestroyDevice};
  if (hr != S_OK) return E_FAIL;
  hr = current(adapter);
  {
    // Serialize the final state check and both output writes with CloseAdapter.
    // Backend destruction runs after unlocking because it may call the runtime.
    std::lock_guard<std::mutex> lock(adaptersMutex);
    if (SUCCEEDED(hr)) hr = state(adapter);
    if (SUCCEEDED(hr)) {
      *output = table;
      if (dxgiOutput) *dxgiOutput = dxgi;
      guard.handle = {};
      return S_OK;
    }
  }
  return hr;
}

template<typename Functions>
HRESULT createWithDxgi(const std::shared_ptr<Adapter>& adapter,
    D3D10DDIARG_CREATEDEVICE& local, Functions& table, Functions* output,
    const DXGI_DDI_BASE_ARGS& input) {
  if (dxvk::umd::nativeDxgiUses1_1(local.Interface, local.Version)) {
    auto dxgiOutput = input.pDXGIDDIBaseFunctions2;
    if (!adapter->development && !dxgiOutput) return E_INVALIDARG;
    DXGI1_1_DDI_BASE_FUNCTIONS dxgi = {};
    if (dxgiOutput) local.DXGIBaseDDI.pDXGIDDIBaseFunctions2 = &dxgi;
    return createAndPublish(adapter, local, table, output, dxgi, dxgiOutput);
  }
  auto dxgiOutput = input.pDXGIDDIBaseFunctions;
  if (!adapter->development && !dxgiOutput) return E_INVALIDARG;
  DXGI_DDI_BASE_FUNCTIONS dxgi = {};
  if (dxgiOutput) local.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgi;
  return createAndPublish(adapter, local, table, output, dxgi, dxgiOutput);
}

HRESULT APIENTRY createDevice(D3D10DDI_HADAPTER handle, D3D10DDIARG_CREATEDEVICE* args) {
  auto adapter = retain(handle);
  HRESULT hr = state(adapter);
  if (FAILED(hr)) return hr;
  if (!args) return E_INVALIDARG;
  try {
    // Copy only members in the selected historical creation ABI. In particular,
    // ppfnRetrieveSubObject and current-SDK callback tails are never read.
    D3D10DDIARG_CREATEDEVICE local = {};
    local.Interface = args->Interface; local.Version = args->Version; local.Flags = args->Flags;
    if (!dxvk::umd::supportedNativeInterface(local.Interface, local.Version, local.Flags)
        || !admitted(*adapter, local.Interface, local.Flags)) return DXGI_ERROR_UNSUPPORTED;
    local.hDrvDevice = args->hDrvDevice; local.hRTDevice = args->hRTDevice;
    local.hRTCoreLayer = args->hRTCoreLayer;
    if (!local.hDrvDevice.pDrvPrivate || !local.hRTDevice.handle
        || !local.hRTCoreLayer.handle || !args->pKTCallbacks) return E_INVALIDARG;
    D3DDDI_DEVICECALLBACKS kernel = {};
    const auto& source = *args->pKTCallbacks;
    kernel.pfnAllocateCb = source.pfnAllocateCb; kernel.pfnDeallocateCb = source.pfnDeallocateCb;
    kernel.pfnSetPriorityCb = source.pfnSetPriorityCb;
    kernel.pfnQueryResidencyCb = source.pfnQueryResidencyCb;
    kernel.pfnLockCb = source.pfnLockCb; kernel.pfnUnlockCb = source.pfnUnlockCb;
    kernel.pfnCreateContextCb = source.pfnCreateContextCb;
    kernel.pfnDestroyContextCb = source.pfnDestroyContextCb;
    kernel.pfnEscapeCb = source.pfnEscapeCb; kernel.pfnRenderCb = source.pfnRenderCb;
    local.pKTCallbacks = &kernel;
    // DXGI callbacks and D3D core callbacks are runtime-owned live tables.
    // Their original pointer is retained; function addresses must not be cached.
    DXGI_DDI_BASE_ARGS dxgiInput = {};
    dxgiInput.pDXGIBaseCallbacks = args->DXGIBaseDDI.pDXGIBaseCallbacks;
    if (dxvk::umd::nativeDxgiUses1_1(local.Interface, local.Version))
      dxgiInput.pDXGIDDIBaseFunctions2 = args->DXGIBaseDDI.pDXGIDDIBaseFunctions2;
    else dxgiInput.pDXGIDDIBaseFunctions = args->DXGIBaseDDI.pDXGIDDIBaseFunctions;
    local.DXGIBaseDDI.pDXGIBaseCallbacks = dxgiInput.pDXGIBaseCallbacks;
    if (!adapter->development && (!kernel.pfnAllocateCb || !kernel.pfnDeallocateCb
        || !kernel.pfnLockCb || !kernel.pfnUnlockCb || !kernel.pfnCreateContextCb
        || !kernel.pfnDestroyContextCb || !local.DXGIBaseDDI.pDXGIBaseCallbacks
        || !local.DXGIBaseDDI.pDXGIBaseCallbacks->pfnPresentCb)) return E_INVALIDARG;

    switch (dxvk::umd::nativeInterface(local.Interface)) {
      case dxvk::umd::NativeInterface::D3D10:
      case dxvk::umd::NativeInterface::D3D10_1: {
        if (!args->pUMCallbacks || !args->pUMCallbacks->pfnSetErrorCb) return E_INVALIDARG;
        local.pUMCallbacks = args->pUMCallbacks;
        if (dxvk::umd::nativeInterface(local.Interface) == dxvk::umd::NativeInterface::D3D10) {
          auto output = args->pDeviceFuncs;
          if (!output) return E_INVALIDARG;
          D3D10DDI_DEVICEFUNCS table = {};
          local.pDeviceFuncs = &table;
          return createWithDxgi(adapter, local, table, output, dxgiInput);
        }
        auto output = args->p10_1DeviceFuncs;
        if (!output) return E_INVALIDARG;
        D3D10_1DDI_DEVICEFUNCS table = {};
        local.p10_1DeviceFuncs = &table;
        return createWithDxgi(adapter, local, table, output, dxgiInput);
      }
      case dxvk::umd::NativeInterface::D3D11: {
        if (!args->p11UMCallbacks || !args->p11UMCallbacks->pfnSetErrorCb) return E_INVALIDARG;
        local.p11UMCallbacks = args->p11UMCallbacks;
        auto output = args->p11DeviceFuncs;
        if (!output) return E_INVALIDARG;
        D3D11DDI_DEVICEFUNCS table = {};
        local.p11DeviceFuncs = &table;
        return createWithDxgi(adapter, local, table, output, dxgiInput);
      }
      default: return DXGI_ERROR_UNSUPPORTED;
    }
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY closeAdapter(D3D10DDI_HADAPTER handle) {
  std::shared_ptr<Adapter> adapter;
  {
    std::lock_guard<std::mutex> lock(adaptersMutex);
    const auto entry = adapters.find(handle.pDrvPrivate);
    if (entry == adapters.end()) return E_INVALIDARG;
    adapter = entry->second;
    adapter->closed = true;
    adapters.erase(entry);
  }
  // In-flight calls retain Adapter; devices retain immutable identity. No
  // later device destruction dereferences the closed runtime adapter handle.
  return S_OK;
}

HRESULT APIENTRY supportedVersions(D3D10DDI_HADAPTER handle, UINT32* entries, UINT64* versions) {
  if (!entries) return E_INVALIDARG;
  auto adapter = retain(handle);
  HRESULT hr = state(adapter);
  if (FAILED(hr)) return hr;
  const UINT32 capacity = versions ? *entries : 0;
  UINT64 supported[3] = {};
  UINT32 required = 0;
  if (admitted(*adapter, D3D10_0_DDI_INTERFACE_VERSION)) supported[required++] = D3D10_0_DDI_SUPPORTED;
  if (admitted(*adapter, D3D10_1_DDI_INTERFACE_VERSION)) supported[required++] = D3D10_1_DDI_SUPPORTED;
  if (admitted(*adapter, D3D11_0_DDI_INTERFACE_VERSION)) supported[required++] = D3D11_0_DDI_SUPPORTED;
  hr = current(adapter);
  if (FAILED(hr)) return hr;
  std::lock_guard<std::mutex> lock(adaptersMutex);
  hr = state(adapter);
  if (FAILED(hr)) return hr;
  *entries = required;
  if (versions && capacity < required) return E_OUTOFMEMORY;
  if (versions) std::memcpy(versions, supported, required * sizeof(UINT64));
  return S_OK;
}

HRESULT APIENTRY getCaps(D3D10DDI_HADAPTER handle, const D3D10_2DDIARG_GETCAPS* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (!input.pData || input.pInfo) return E_INVALIDARG;
  UINT expected = 0;
  switch (input.Type) {
    case D3D11DDICAPS_THREADING: expected = sizeof(D3D11DDI_THREADING_CAPS); break;
    case D3D11DDICAPS_SHADER: expected = sizeof(D3D11DDI_SHADER_CAPS); break;
    case D3D11DDICAPS_3DPIPELINESUPPORT: expected = sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS); break;
    default: return DXGI_ERROR_UNSUPPORTED;
  }
  if (input.DataSize != expected) return E_INVALIDARG;
  auto adapter = retain(handle);
  HRESULT hr = current(adapter);
  if (FAILED(hr)) return hr;
  D3D11DDI_3DPIPELINESUPPORT_CAPS pipelines = {};
  if (adapter->validation11Fl10_0) {
    pipelines.Caps = D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_0);
  } else {
    if (dxvk::umd::runtimeSupportsD3D10())
      pipelines.Caps |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_0);
    if (dxvk::umd::runtimeMissingD3D10_1Requirements() == 0)
      pipelines.Caps |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_1);
    if (dxvk::umd::runtimeMissingD3D11Requirements(D3D_FEATURE_LEVEL_11_0) == 0)
      pipelines.Caps |= D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_11_0);
  }
  std::lock_guard<std::mutex> lock(adaptersMutex);
  hr = state(adapter);
  if (FAILED(hr)) return hr;
  // Generic development tables retain production pipeline policy. Dedicated
  // validation advertises only FL10.0, with no optional threading/shader bits.
  if (input.Type == D3D11DDICAPS_3DPIPELINESUPPORT)
    std::memcpy(input.pData, &pipelines, sizeof(pipelines));
  else std::memset(input.pData, 0, expected);
  return S_OK;
}

HRESULT open(D3D10DDIARG_OPENADAPTER* args, bool modern, bool development,
    bool validation11Fl10_0 = false) {
  if (!args) return E_INVALIDARG;
  args->hAdapter = {};
  auto legacyOutput = modern ? nullptr : args->pAdapterFuncs;
  auto modernOutput = modern ? args->pAdapterFuncs_2 : nullptr;
  if (modern ? !modernOutput : !legacyOutput) return E_INVALIDARG;
  if (modern) *modernOutput = {};
  else *legacyOutput = {};
  const UINT interfaceVersion = modern ? 0 : args->Interface;
  const UINT version = modern ? 0 : args->Version;
  // OpenAdapter10_2 ignores initial Interface/Version. Legacy adapter creation
  // binds only the requested 10.0 or 10.1 contract; it never selects a 11 table.
  if (!modern && (dxvk::umd::nativeInterface(interfaceVersion) == dxvk::umd::NativeInterface::D3D11
      || !dxvk::umd::supportedNativeInterface(interfaceVersion, version)
      || (!development && !dxvk::umd::runtimeSupportsNativeInterface(interfaceVersion))))
    return DXGI_ERROR_UNSUPPORTED;
  if (!args->pAdapterCallbacks) return E_INVALIDARG;
  const auto runtime = args->hRTAdapter;
  const auto query = args->pAdapterCallbacks->pfnQueryAdapterInfoCb;
  try {
    dxvk::umd::RuntimeIdentity reply;
    HRESULT hr = dxvk::umd::queryRuntimeIdentity(runtime, query, reply);
    if (FAILED(hr)) return hr;
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    std::memcpy(&identity->luid, reply.luid.data(), sizeof(LUID));
    identity->runtime = runtime.handle;
    identity->query = query;
    identity->generation = reply.generation;
    identity->capabilities = reply.capabilities;
    auto adapter = std::make_shared<Adapter>();
    adapter->identity = std::move(identity);
    adapter->development = development;
    adapter->validation11Fl10_0 = validation11Fl10_0;
    adapter->legacyInterface = interfaceVersion;
    {
      std::lock_guard<std::mutex> lock(adaptersMutex);
      // Driver handles are opaque, monotonically issued tokens. Closing and
      // reopening an adapter cannot make a stale handle address a new owner.
      if (!nextAdapterToken) return E_OUTOFMEMORY;
      auto token = reinterpret_cast<void*>(nextAdapterToken++);
      adapters.emplace(token, adapter);
      if (modern) {
        modernOutput->pfnCalcPrivateDeviceSize = privateDeviceSize;
        modernOutput->pfnCreateDevice = createDevice;
        modernOutput->pfnCloseAdapter = closeAdapter;
        modernOutput->pfnGetSupportedVersions = supportedVersions;
        modernOutput->pfnGetCaps = getCaps;
      } else {
        legacyOutput->pfnCalcPrivateDeviceSize = privateDeviceSize;
        legacyOutput->pfnCreateDevice = createDevice;
        legacyOutput->pfnCloseAdapter = closeAdapter;
      }
      args->hAdapter.pDrvPrivate = token;
    }
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
}

extern "C" HRESULT APIENTRY OpenAdapter10(D3D10DDIARG_OPENADAPTER* args) {
  return open(args, false, false);
}
extern "C" HRESULT APIENTRY OpenAdapter10_2(D3D10DDIARG_OPENADAPTER* args) {
  return open(args, true, false);
}
HRESULT dxvk::umd::openAdapterForTest(D3D10DDIARG_OPENADAPTER* args, bool modern) {
  return open(args, modern, true);
}
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapterForTest(D3D10DDIARG_OPENADAPTER* args) {
  return dxvk::umd::openAdapterForTest(args, false);
}
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter10_2ForTest(D3D10DDIARG_OPENADAPTER* args) {
  return dxvk::umd::openAdapterForTest(args, true);
}
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter11Fl10_0ForValidation(D3D10DDIARG_OPENADAPTER* args) {
  // This remains production-strength callback validation and real device
  // creation. The separate validation frontend owns any temporary binding.
  return open(args, true, false, true);
}
