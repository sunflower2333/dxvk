#include "umd_adapter.h"
#include "umd_contract.h"
#include <atomic>
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
};

std::mutex adaptersMutex;
std::unordered_map<void*, std::shared_ptr<Adapter>> adapters;

std::shared_ptr<Adapter> retain(D3D10DDI_HADAPTER handle) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle.pDrvPrivate);
  return entry == adapters.end() ? nullptr : entry->second;
}

bool supported(UINT interfaceVersion, UINT version, UINT flags = 0) {
  // No casts between D3D10 and D3D11 table layouts or ignored threading flags.
  return interfaceVersion == D3D10_0_DDI_INTERFACE_VERSION
      && (version >> 16) >= D3D10_0_DDI_BUILD_VERSION && !flags;
}

bool admitted(const Adapter& adapter) {
  return adapter.development || dxvk::umd::runtimeSupportsD3D10();
}

HRESULT current(const std::shared_ptr<Adapter>& adapter) noexcept {
  if (!adapter) return E_INVALIDARG;
  if (adapter->closed || adapter->removed) return DXGI_ERROR_DEVICE_REMOVED;
  // Runtime callbacks can synchronously reenter the UMD. Never hold the
  // registry mutex across them, and never recurse or wait on our own query.
  if (adapter->querying.test_and_set()) return DXGI_ERROR_WAS_STILL_DRAWING;
  struct QueryGuard { Adapter& adapter; ~QueryGuard() { adapter.querying.clear(); } } guard{*adapter};
  try {
    const auto& expected = *adapter->identity;
    dxvk::umd::RuntimeIdentity observed;
    HRESULT hr = dxvk::umd::queryRuntimeIdentity(expected.runtime, expected.query, observed);
    if (adapter->closed || adapter->removed) return DXGI_ERROR_DEVICE_REMOVED;
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
  if (!adapter || !args || !supported(args->Interface, args->Version, args->Flags)
      || !admitted(*adapter) || FAILED(current(adapter))) return 0;
  return VioGpuDxvkPrivateDeviceSize();
}

HRESULT APIENTRY createDevice(D3D10DDI_HADAPTER handle, D3D10DDIARG_CREATEDEVICE* args) {
  auto adapter = retain(handle);
  if (!adapter || !args) return E_INVALIDARG;
  if (!supported(args->Interface, args->Version, args->Flags) || !admitted(*adapter))
    return DXGI_ERROR_UNSUPPORTED;
  if (!args->pDeviceFuncs || !args->hDrvDevice.pDrvPrivate || !args->hRTDevice.handle
      || !args->pKTCallbacks || !args->hRTCoreLayer.handle
      || !args->pUMCallbacks || !args->pUMCallbacks->pfnSetErrorCb) return E_INVALIDARG;
  try {
    HRESULT hr = current(adapter);
    if (FAILED(hr)) return hr;
    // Publish only after backend creation and a second identity query. Failure
    // never leaves a half-created table or writes into an incompatible union.
    D3D10DDI_DEVICEFUNCS table = {};
    DXGI_DDI_BASE_FUNCTIONS dxgi = {};
    auto local = *args;
    local.pDeviceFuncs = &table;
    if (args->DXGIBaseDDI.pDXGIDDIBaseFunctions)
      local.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgi;
    hr = dxvk::umd::createAdapterDevice(adapter->identity, &local);
    if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
    hr = current(adapter);
    if (FAILED(hr)) {
      // Every successful production creation guarantees this slot, including
      // a reset or close while the backend was opening.
      table.pfnDestroyDevice(local.hDrvDevice);
      return hr;
    }
    *args->pDeviceFuncs = table;
    if (args->DXGIBaseDDI.pDXGIDDIBaseFunctions)
      *args->DXGIBaseDDI.pDXGIDDIBaseFunctions = dxgi;
    return S_OK;
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
  HRESULT hr = current(adapter);
  if (FAILED(hr)) return hr;
  const UINT32 required = dxvk::umd::runtimeSupportsD3D10() ? 1 : 0;
  const UINT32 capacity = *entries;
  *entries = required;
  if (versions && capacity < required) return E_OUTOFMEMORY;
  if (versions && required) versions[0] = D3D10_0_DDI_SUPPORTED;
  return S_OK;
}

HRESULT APIENTRY getCaps(D3D10DDI_HADAPTER handle, const D3D10_2DDIARG_GETCAPS* args) {
  if (!args || !args->pData || args->pInfo) return E_INVALIDARG;
  UINT expected = 0;
  switch (args->Type) {
    case D3D11DDICAPS_THREADING: expected = sizeof(D3D11DDI_THREADING_CAPS); break;
    case D3D11DDICAPS_SHADER: expected = sizeof(D3D11DDI_SHADER_CAPS); break;
    case D3D11DDICAPS_3DPIPELINESUPPORT: expected = sizeof(D3D11DDI_3DPIPELINESUPPORT_CAPS); break;
    default: return DXGI_ERROR_UNSUPPORTED;
  }
  if (args->DataSize != expected) return E_INVALIDARG;
  HRESULT hr = current(retain(handle));
  if (FAILED(hr)) return hr;
  // No free-threaded device, command lists, doubles or incomplete pipeline.
  std::memset(args->pData, 0, expected);
  if (args->Type == D3D11DDICAPS_3DPIPELINESUPPORT && dxvk::umd::runtimeSupportsD3D10())
    static_cast<D3D11DDI_3DPIPELINESUPPORT_CAPS*>(args->pData)->Caps =
      D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(D3D11DDI_3DPIPELINELEVEL_10_0);
  return S_OK;
}

HRESULT open(D3D10DDIARG_OPENADAPTER* args, bool modern, bool development) {
  if (!args) return E_INVALIDARG;
  args->hAdapter = {};
  if (!args->pAdapterFuncs) return E_INVALIDARG;
  if (modern) *args->pAdapterFuncs_2 = {};
  else *args->pAdapterFuncs = {};
  // OpenAdapter10_2 ignores initial Interface/Version. GetSupportedVersions
  // selects the device ABI after successful adapter open.
  if (!modern && (!supported(args->Interface, args->Version)
      || (!development && !dxvk::umd::runtimeSupportsD3D10()))) return DXGI_ERROR_UNSUPPORTED;
  if (!args->pAdapterCallbacks) return E_INVALIDARG;
  try {
    const auto query = args->pAdapterCallbacks->pfnQueryAdapterInfoCb;
    dxvk::umd::RuntimeIdentity reply;
    HRESULT hr = dxvk::umd::queryRuntimeIdentity(args->hRTAdapter, query, reply);
    if (FAILED(hr)) return hr;
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    std::memcpy(&identity->luid, reply.luid.data(), sizeof(LUID));
    identity->runtime = args->hRTAdapter;
    identity->query = query;
    identity->generation = reply.generation;
    identity->capabilities = reply.capabilities;
    auto adapter = std::make_shared<Adapter>();
    adapter->identity = std::move(identity);
    adapter->development = development;
    {
      std::lock_guard<std::mutex> lock(adaptersMutex);
      adapters.emplace(adapter.get(), adapter);
    }
    if (modern) {
      args->pAdapterFuncs_2->pfnCalcPrivateDeviceSize = privateDeviceSize;
      args->pAdapterFuncs_2->pfnCreateDevice = createDevice;
      args->pAdapterFuncs_2->pfnCloseAdapter = closeAdapter;
      args->pAdapterFuncs_2->pfnGetSupportedVersions = supportedVersions;
      args->pAdapterFuncs_2->pfnGetCaps = getCaps;
    } else {
      args->pAdapterFuncs->pfnCalcPrivateDeviceSize = privateDeviceSize;
      args->pAdapterFuncs->pfnCreateDevice = createDevice;
      args->pAdapterFuncs->pfnCloseAdapter = closeAdapter;
    }
    args->hAdapter.pDrvPrivate = adapter.get();
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
extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapterForTest(D3D10DDIARG_OPENADAPTER* args) {
  return open(args, false, true);
}
