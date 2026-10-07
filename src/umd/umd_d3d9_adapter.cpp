#include "umd_d3d9_adapter.h"
#include "umd_runtime_query.h"
#include <dxgi.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>

namespace {
struct Adapter {
  std::shared_ptr<const dxvk::umd::AdapterIdentity> identity;
  std::shared_ptr<std::atomic<bool>> live = std::make_shared<std::atomic<bool>>(true);
  std::atomic<bool> closed{false}, removed{false};
  std::atomic_flag querying = ATOMIC_FLAG_INIT;
};

std::mutex adaptersMutex;
std::unordered_map<HANDLE, std::shared_ptr<Adapter>> adapters;
uintptr_t nextHandle = 1;
thread_local bool opening = false;

std::shared_ptr<Adapter> retain(HANDLE handle) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle);
  return entry == adapters.end() ? nullptr : entry->second;
}

HRESULT queryError(HRESULT hr) {
  switch (hr) {
    case DXGI_ERROR_UNSUPPORTED: return D3DERR_NOTAVAILABLE;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET:
    case D3DERR_DEVICEREMOVED: return D3DERR_DEVICELOST;
    default: return hr;
  }
}

HRESULT state(const std::shared_ptr<Adapter>& adapter) {
  if (!adapter) return E_INVALIDARG;
  return adapter->closed || adapter->removed ? D3DERR_DEVICELOST : S_OK;
}

HRESULT current(const std::shared_ptr<Adapter>& adapter) noexcept {
  const HRESULT status = state(adapter);
  if (FAILED(status)) return status;
  // No registry lock survives a runtime callback, which may close the adapter
  // or reenter this table. In-flight calls keep the original owner alive.
  if (adapter->querying.test_and_set()) return D3DERR_WASSTILLDRAWING;
  struct Guard { Adapter& adapter; ~Guard() { adapter.querying.clear(); } } guard{*adapter};
  try {
    dxvk::umd::RuntimeIdentity observed;
    const auto& expected = *adapter->identity;
    const HRESULT hr = queryError(dxvk::umd::queryRuntimeIdentity(
      expected.runtime, expected.query, observed));
    if (adapter->closed || adapter->removed) return D3DERR_DEVICELOST;
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET) {
      adapter->removed = true;
      return D3DERR_DEVICELOST;
    }
    if (FAILED(hr)) return hr;
    if (std::memcmp(observed.luid.data(), &expected.luid, sizeof(LUID))
        || observed.generation != expected.generation
        || observed.capabilities != expected.capabilities) {
      adapter->removed = true;
      return D3DERR_DEVICELOST;
    }
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY getCaps(HANDLE handle, const D3DDDIARG_GETCAPS* args) {
  auto adapter = retain(handle);
  HRESULT hr = state(adapter);
  if (FAILED(hr)) return hr;
  if (!args || args->pInfo) return E_INVALIDARG;
  UINT size = 0;
  switch (args->Type) {
    case D3DDDICAPS_GETFORMATCOUNT:
    case D3DDDICAPS_GETD3DQUERYCOUNT: size = sizeof(UINT); break;
    case D3DDDICAPS_GETD3D9CAPS: size = sizeof(D3DCAPS9); break;
    case D3DDDICAPS_GETFORMATDATA:
    case D3DDDICAPS_GETD3DQUERYDATA: break;
    default: return D3DERR_NOTAVAILABLE;
  }
  // GetCaps takes a const argument in the WDK ABI. Do not rewrite DataSize
  // or accept a partial caps buffer. Empty lists have no output bytes.
  if (args->DataSize != size || (size && !args->pData)) return E_INVALIDARG;
  hr = current(adapter);
  if (FAILED(hr)) return hr;
  std::lock_guard<std::mutex> lock(adaptersMutex);
  hr = state(adapter);
  if (FAILED(hr)) return hr;
  if (size) std::memset(args->pData, 0, size);
  return S_OK;
}

HRESULT APIENTRY createDevice(HANDLE handle, D3DDDIARG_CREATEDEVICE* args) {
  auto adapter = retain(handle);
  HRESULT hr = state(adapter);
  if (FAILED(hr)) return hr;
  if (!args) return E_INVALIDARG;
  // Interface is the literal API version; Version is an opaque runtime build
  // identifier, with no D3D10-style packed build requirement.
  if (args->Interface != 9 || args->Flags.Value) return D3DERR_NOTAVAILABLE;
  if (!args->hDevice || !args->pCallbacks || !args->pDeviceFuncs) return E_INVALIDARG;
  try {
    // Snapshot inputs before the first callback can reenter or replace them.
    // Legacy command/allocation/patch buffers are obsolete and never read.
    D3DDDI_DEVICECALLBACKS callbacks = {};
    const auto& source = *args->pCallbacks;
    callbacks.pfnAllocateCb = source.pfnAllocateCb;
    callbacks.pfnDeallocateCb = source.pfnDeallocateCb;
    callbacks.pfnLockCb = source.pfnLockCb; callbacks.pfnUnlockCb = source.pfnUnlockCb;
    callbacks.pfnCreateContextCb = source.pfnCreateContextCb;
    callbacks.pfnDestroyContextCb = source.pfnDestroyContextCb;
    callbacks.pfnEscapeCb = source.pfnEscapeCb; callbacks.pfnRenderCb = source.pfnRenderCb;
    callbacks.pfnPresentCb = source.pfnPresentCb;
    D3DDDI_DEVICEFUNCS table = {};
    auto output = args->pDeviceFuncs;
    D3DDDIARG_CREATEDEVICE local = {};
    local.hDevice = args->hDevice; local.Interface = args->Interface;
    local.Version = args->Version; local.Flags = args->Flags;
    local.pCallbacks = &callbacks; local.pDeviceFuncs = &table;
    hr = current(adapter);
    if (FAILED(hr)) return hr;
    hr = dxvk::umd::createAdapterDevice9(adapter->identity, &local);
    if (hr != S_OK) return FAILED(hr) ? hr : E_FAIL;
    struct DeviceGuard {
      HANDLE handle;
      PFND3DDDI_DESTROYDEVICE destroy;
      ~DeviceGuard() { if (handle) destroy(handle); }
    } guard{local.hDevice, table.pfnDestroyDevice};
    hr = current(adapter);
    {
      std::lock_guard<std::mutex> lock(adaptersMutex);
      if (SUCCEEDED(hr)) hr = state(adapter);
      if (SUCCEEDED(hr)) {
        *output = table;
        args->hDevice = local.hDevice;
        guard.handle = nullptr;
        return S_OK;
      }
    }
    return hr;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY closeAdapter(HANDLE handle) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle);
  if (entry == adapters.end()) return E_INVALIDARG;
  entry->second->closed = true;
  *entry->second->live = false;
  adapters.erase(entry);
  return S_OK;
}
}

extern "C" HRESULT APIENTRY VioGpuDxvkOpenAdapter9ForTest(D3DDDIARG_OPENADAPTER* args) {
  if (!args || !args->hAdapter || !args->pAdapterFuncs || !args->pAdapterCallbacks
      || !args->pAdapterCallbacks->pfnQueryAdapterInfoCb) return E_INVALIDARG;
  if (args->Interface != 9) return D3DERR_NOTAVAILABLE;
  if (opening) return D3DERR_WASSTILLDRAWING;
  opening = true;
  struct Guard { ~Guard() { opening = false; } } guard;
  try {
    auto adapter = std::make_shared<Adapter>();
    auto identity = std::make_shared<dxvk::umd::AdapterIdentity>();
    identity->runtime = args->hAdapter;
    identity->query = args->pAdapterCallbacks->pfnQueryAdapterInfoCb;
    dxvk::umd::RuntimeIdentity observed;
    const HRESULT hr = queryError(dxvk::umd::queryRuntimeIdentity(
      identity->runtime, identity->query, observed));
    if (FAILED(hr)) return hr;
    std::memcpy(&identity->luid, observed.luid.data(), sizeof(LUID));
    identity->generation = observed.generation;
    identity->capabilities = observed.capabilities;
    identity->live = adapter->live;
    adapter->identity = std::move(identity);
    const D3DDDI_ADAPTERFUNCS functions = {getCaps, createDevice, closeAdapter};
    std::lock_guard<std::mutex> lock(adaptersMutex);
    // Opaque tokens are never reused, including after shared owners expire.
    // Wraparound fails closed rather than making a stale handle valid again.
    if (!nextHandle) return E_OUTOFMEMORY;
    const HANDLE handle = reinterpret_cast<HANDLE>(nextHandle++);
    adapters.emplace(handle, std::move(adapter));
    *args->pAdapterFuncs = functions;
    args->DriverVersion = D3D_UMD_INTERFACE_VERSION;
    args->hAdapter = handle;
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
