#include "umd_d3d9_adapter.h"
#include "umd_d3d9_backend.h"
#include "umd_runtime_gpu.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

namespace {
HRESULT result(HRESULT hr) {
  switch (hr) {
    case S_OK: return S_OK;
    case DXGI_ERROR_UNSUPPORTED:
    case DXGI_ERROR_NOT_FOUND: return D3DERR_NOTAVAILABLE;
    case DXGI_ERROR_WAS_STILL_DRAWING: return D3DERR_WASSTILLDRAWING;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET:
    case D3DERR_DEVICEREMOVED: return D3DERR_DEVICELOST;
    default: return FAILED(hr) ? hr : E_FAIL;
  }
}

struct Device {
  std::shared_ptr<dxvk::umd::RuntimeService> service = std::make_shared<dxvk::umd::RuntimeService>(true);
  std::shared_ptr<dxvk::umd::RuntimeGpu> gpu;
  std::unique_ptr<dxvk::umd::D3D9Backend> backend;
  HANDLE runtime = nullptr;
  bool busy = false, closing = false;
  std::atomic<bool> removed{false};

  HRESULT close() noexcept {
    if (closing) return S_OK;
    closing = true;
    dxvk::umd::RuntimeService::Scope scope(service.get());
    HRESULT hr = S_OK;
    try { service->drain([&] { backend.reset(); }); }
    catch (...) { hr = E_FAIL; }
    try {
      const HRESULT cleanup = gpu ? result(gpu->close()) : S_OK;
      if (FAILED(cleanup)) hr = cleanup;
    } catch (...) { hr = E_FAIL; }
    service->close();
    return hr;
  }
  ~Device() { close(); }
};

std::mutex devicesMutex;
std::unordered_map<HANDLE, std::shared_ptr<Device>> devices;
std::unordered_map<HANDLE, std::shared_ptr<Device>> runtimeDevices;
uintptr_t nextHandle = 1;

void releaseRuntime(const std::shared_ptr<Device>& owner) {
  std::lock_guard<std::mutex> lock(devicesMutex);
  const auto entry = runtimeDevices.find(owner->runtime);
  if (entry != runtimeDevices.end() && entry->second == owner) runtimeDevices.erase(entry);
}

HRESULT APIENTRY flush(HANDLE handle) {
  std::shared_ptr<Device> owner;
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    const auto entry = devices.find(handle);
    if (entry == devices.end()) return E_INVALIDARG;
    owner = entry->second;
    if (owner->removed) return D3DERR_DEVICELOST;
    if (owner->busy) return D3DERR_WASSTILLDRAWING;
    owner->busy = true;
  }
  struct Guard {
    Device& device;
    ~Guard() { std::lock_guard<std::mutex> lock(devicesMutex); device.busy = false; }
  } guard{*owner};
  HRESULT hr;
  try { hr = result(owner->service->run([&] { return owner->backend->flush(); })); }
  catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
  catch (...) { hr = E_FAIL; }
  if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET) owner->removed = true;
  return hr;
}

HRESULT APIENTRY destroyDevice(HANDLE handle) {
  std::shared_ptr<Device> owner;
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    const auto entry = devices.find(handle);
    if (entry == devices.end()) return E_INVALIDARG;
    owner = entry->second;
    // A callback may reenter while its backend worker is suspended. Teardown
    // must not join that worker or let the runtime expire its handles yet.
    if (owner->busy) return D3DERR_WASSTILLDRAWING;
    owner->busy = true;
    devices.erase(entry);
  }
  const HRESULT hr = owner->close();
  releaseRuntime(owner);
  return hr;
}
}

HRESULT dxvk::umd::createAdapterDevice9(const std::shared_ptr<const AdapterIdentity>& identity,
                                      D3DDDIARG_CREATEDEVICE* args) {
  if (!identity || !args || !args->hDevice || !args->pCallbacks || !args->pDeviceFuncs)
    return E_INVALIDARG;
  if (args->Interface != 9 || args->Flags.Value) return D3DERR_NOTAVAILABLE;
  const auto& cb = *args->pCallbacks;
  if (!cb.pfnAllocateCb || !cb.pfnDeallocateCb || !cb.pfnLockCb || !cb.pfnUnlockCb
      || !cb.pfnCreateContextCb || !cb.pfnDestroyContextCb || !cb.pfnEscapeCb || !cb.pfnRenderCb)
    return E_INVALIDARG;
  auto owner = std::make_shared<Device>();
  owner->runtime = args->hDevice;
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    if (!runtimeDevices.emplace(owner->runtime, owner).second) return E_INVALIDARG;
  }
  struct Guard {
    std::shared_ptr<Device> owner;
    bool published = false;
    ~Guard() { if (!published) { owner->close(); releaseRuntime(owner); } }
  } guard{owner};
  owner->gpu = RuntimeGpu::create(owner->runtime, cb, identity, owner->service);
  const RuntimeBackend runtime = owner->gpu->backend();
  AdapterLuid luid;
  std::memcpy(luid.data(), &identity->luid, luid.size());
  const HRESULT hr = result(owner->service->run([&] {
    return D3D9Backend::create(luid, &runtime, owner->backend);
  }));
  if (FAILED(hr)) return hr;
  if (!owner->backend) return E_FAIL;
  D3DDDI_DEVICEFUNCS table = {};
  table.pfnFlush = flush;
  table.pfnDestroyDevice = destroyDevice;
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    if (!nextHandle) return E_OUTOFMEMORY;
    const HANDLE handle = reinterpret_cast<HANDLE>(nextHandle++);
    devices.emplace(handle, owner);
    *args->pDeviceFuncs = table;
    args->hDevice = handle;
  }
  guard.published = true;
  return S_OK;
}
