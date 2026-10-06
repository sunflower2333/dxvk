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
  HANDLE runtime = nullptr;
  PFND3DDDI_QUERYADAPTERINFOCB query = nullptr;
  dxvk::umd::RuntimeIdentity identity;
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
    const HRESULT hr = queryError(dxvk::umd::queryRuntimeIdentity(
      adapter->runtime, adapter->query, observed));
    if (adapter->closed || adapter->removed) return D3DERR_DEVICELOST;
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET) {
      adapter->removed = true;
      return D3DERR_DEVICELOST;
    }
    if (FAILED(hr)) return hr;
    if (observed.luid != adapter->identity.luid
        || observed.generation != adapter->identity.generation
        || observed.capabilities != adapter->identity.capabilities) {
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
  hr = current(adapter);
  if (FAILED(hr)) return hr;
  // No embedded D3D9 device yet. Preserve all in/out handles, command buffers,
  // allocation/patch lists and function tables on this admission failure.
  return D3DERR_NOTAVAILABLE;
}

HRESULT APIENTRY closeAdapter(HANDLE handle) {
  std::lock_guard<std::mutex> lock(adaptersMutex);
  const auto entry = adapters.find(handle);
  if (entry == adapters.end()) return E_INVALIDARG;
  entry->second->closed = true;
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
    adapter->runtime = args->hAdapter;
    adapter->query = args->pAdapterCallbacks->pfnQueryAdapterInfoCb;
    const HRESULT hr = queryError(dxvk::umd::queryRuntimeIdentity(
      adapter->runtime, adapter->query, adapter->identity));
    if (FAILED(hr)) return hr;
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
