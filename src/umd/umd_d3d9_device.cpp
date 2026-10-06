#include "umd_d3d9_adapter.h"
#include "umd_d3d9_backend.h"
#include "umd_runtime_gpu.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <limits>
#include <climits>

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

struct Surface {
  dxvk::umd::D3D9SurfaceDesc desc;
  std::unique_ptr<dxvk::umd::D3D9SurfaceResource> backend;
  bool locked = false, notifyOnly = false;
};
struct Resource {
  HANDLE runtime = nullptr;
  std::vector<Surface> surfaces;
};
struct Device {
  std::shared_ptr<dxvk::umd::RuntimeService> service = std::make_shared<dxvk::umd::RuntimeService>(true);
  std::shared_ptr<dxvk::umd::RuntimeGpu> gpu;
  std::unique_ptr<dxvk::umd::D3D9Backend> backend;
  HANDLE runtime = nullptr;
  bool busy = false, closing = false;
  std::atomic<bool> removed{false};
  std::unordered_map<HANDLE, std::unique_ptr<Resource>> resources;
  std::unordered_set<HANDLE> runtimeResources;
  HANDLE target = nullptr;
  UINT targetIndex = 0;

  HRESULT close() noexcept {
    if (closing) return S_OK;
    closing = true;
    dxvk::umd::RuntimeService::Scope scope(service.get());
    HRESULT hr = S_OK;
    try { service->drain([&] {
      try { if (backend) {
        for (auto& entry : resources)
          for (auto& surface : entry.second->surfaces)
            if (surface.locked) backend->unlockSurface(*surface.backend, false);
        if (target) backend->setRenderTarget(nullptr);
        if (!resources.empty()) backend->flush();
      } } catch (...) { hr = E_FAIL; }
      resources.clear();
      runtimeResources.clear();
      backend.reset();
    }); }
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

// Serialize the device before inspecting its private handles. A runtime
// callback can reenter any DDI while its worker is suspended; it must receive
// a retry result without touching in-progress renderer or resource storage.
template<typename Function>
HRESULT operation(HANDLE handle, Function&& function, bool cleanup = false) noexcept {
  std::shared_ptr<Device> owner;
  {
    std::lock_guard<std::mutex> lock(devicesMutex);
    const auto entry = devices.find(handle);
    if (entry == devices.end()) return E_INVALIDARG;
    owner = entry->second;
    if (owner->removed && !cleanup) return D3DERR_DEVICELOST;
    if (owner->busy) return D3DERR_WASSTILLDRAWING;
    owner->busy = true;
  }
  struct Guard {
    Device& device;
    ~Guard() { std::lock_guard<std::mutex> lock(devicesMutex); device.busy = false; }
  } guard{*owner};
  HRESULT hr;
  try {
    hr = result(owner->service->run([&] {
      if (!cleanup) {
        const auto runtime = owner->gpu->backend();
        const HRESULT status = runtime.create.callbacks->status(runtime.create.owner);
        if (FAILED(status)) return status;
      }
      return function(*owner);
    }));
  } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
    catch (...) { hr = E_FAIL; }
  if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET) owner->removed = true;
  return hr;
}

Surface* surface(Device& device, HANDLE resource, UINT index) {
  const auto entry = device.resources.find(resource);
  if (entry == device.resources.end() || index >= entry->second->surfaces.size()) return nullptr;
  return &entry->second->surfaces[index];
}
bool validArea(const RECT& area, const dxvk::umd::D3D9SurfaceDesc& desc) {
  return area.left >= 0 && area.top >= 0 && area.right > area.left && area.bottom > area.top
    && UINT(area.right) <= desc.width && UINT(area.bottom) <= desc.height;
}

HRESULT APIENTRY createResource(HANDLE handle, D3DDDIARG_CREATERESOURCE* args) {
  if (!args || !args->hResource || !args->pSurfList || !args->SurfCount) return E_INVALIDARG;
  // Snapshot the complete group before the first runtime callback. Fields
  // reserved for absent usage flags intentionally do not affect admission.
  const auto input = *args;
  if (input.Flags.Value & ~(UINT(1) | UINT(0x80))) return E_INVALIDARG;
  const bool target = input.Flags.RenderTarget != 0;
  if (input.Flags.NotLockable && !target) return E_INVALIDARG;
  if (target && (input.MultisampleType != D3DDDIMULTISAMPLE_NONE || input.MultisampleQuality))
    return E_INVALIDARG;
  if (input.Pool != D3DDDIPOOL_SYSTEMMEM && input.Pool != D3DDDIPOOL_VIDEOMEMORY
      && input.Pool != D3DDDIPOOL_LOCALVIDMEM && input.Pool != D3DDDIPOOL_NONLOCALVIDMEM)
    return E_INVALIDARG;
  if (target && input.Pool == D3DDDIPOOL_SYSTEMMEM) return E_INVALIDARG;
  const auto format = static_cast<D3DFORMAT>(input.Format);
  if (format != D3DFMT_A8R8G8B8 && format != D3DFMT_X8R8G8B8) return E_INVALIDARG;
  try {
    std::vector<dxvk::umd::D3D9SurfaceDesc> descriptions;
    descriptions.reserve(input.SurfCount);
    for (UINT i = 0; i < input.SurfCount; ++i) {
      const auto info = input.pSurfList[i];
      dxvk::umd::D3D9SurfaceDesc desc;
      desc.width = info.Width; desc.height = info.Height; desc.format = format;
      desc.renderTarget = target; desc.systemMemory = input.Pool == D3DDDIPOOL_SYSTEMMEM;
      desc.lockable = !input.Flags.NotLockable;
      if (!desc.width || !desc.height || desc.width > UINT(INT_MAX / 4) || desc.height > UINT(INT_MAX))
        return E_INVALIDARG;
      if (info.pSysMem) {
        if (!desc.systemMemory || info.SysMemPitch < desc.width * 4 || info.SysMemPitch > UINT(INT_MAX))
          return E_INVALIDARG;
        const uint64_t size = uint64_t(info.SysMemPitch) * (desc.height - 1) + uint64_t(desc.width) * 4;
        if (size > UINTPTR_MAX - reinterpret_cast<uintptr_t>(info.pSysMem)) return E_INVALIDARG;
        desc.systemData = const_cast<void*>(info.pSysMem); desc.systemPitch = info.SysMemPitch;
      }
      descriptions.push_back(desc);
    }
    return operation(handle, [&](Device& device) {
      if (!device.runtimeResources.emplace(input.hResource).second) return E_INVALIDARG;
      struct Reservation {
        Device& device; HANDLE runtime; bool published = false;
        ~Reservation() { if (!published) device.runtimeResources.erase(runtime); }
      } reservation{device, input.hResource};
      auto resource = std::make_unique<Resource>();
      resource->runtime = input.hResource;
      resource->surfaces.reserve(descriptions.size());
      for (const auto& desc : descriptions) {
        Surface entry; entry.desc = desc;
        const HRESULT hr = result(device.backend->createSurface(desc, entry.backend));
        if (FAILED(hr)) return hr;
        if (!entry.backend) return E_FAIL;
        resource->surfaces.push_back(std::move(entry));
      }
      const auto runtime = device.gpu->backend();
      const HRESULT status = runtime.create.callbacks->status(runtime.create.owner);
      if (FAILED(status)) return status;
      std::lock_guard<std::mutex> lock(devicesMutex);
      if (!nextHandle) return E_OUTOFMEMORY;
      const HANDLE token = reinterpret_cast<HANDLE>(nextHandle++);
      device.resources.emplace(token, std::move(resource));
      args->hResource = token;
      reservation.published = true;
      return S_OK;
    });
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY destroyResource(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    const auto entry = device.resources.find(token);
    if (entry == device.resources.end()) return E_INVALIDARG;
    for (const auto& item : entry->second->surfaces)
      if (item.locked) return E_INVALIDARG;
    if (device.target == token) {
      const HRESULT hr = result(device.backend->setRenderTarget(nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.target = nullptr;
    }
    // The backend joins recording/submission before runtime backing expires.
    const HRESULT hr = result(device.backend->flush());
    if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
    device.runtimeResources.erase(entry->second->runtime);
    device.resources.erase(entry);
    return hr;
  }, true);
}

HRESULT APIENTRY setRenderTarget(HANDLE handle, const D3DDDIARG_SETRENDERTARGET* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.RenderTargetIndex) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    auto target = input.hRenderTarget ? surface(device, input.hRenderTarget, input.SubResourceIndex) : nullptr;
    if (input.hRenderTarget && (!target || !target->desc.renderTarget || target->locked)) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setRenderTarget(target ? target->backend.get() : nullptr));
    if (SUCCEEDED(hr)) { device.target = input.hRenderTarget; device.targetIndex = input.SubResourceIndex; }
    return hr;
  });
}

HRESULT APIENTRY clear(HANDLE handle, const D3DDDIARG_CLEAR* args, UINT count, const RECT* rects) {
  if (!args || (count && !rects)) return E_INVALIDARG;
  const auto input = *args;
  constexpr UINT computeRects = 8; // D3DCLEAR_COMPUTERECTS, native DDI only.
  if (input.Flags != D3DCLEAR_TARGET && input.Flags != (D3DCLEAR_TARGET | computeRects)) return E_INVALIDARG;
  try {
    std::vector<RECT> areas;
    if (count) areas.assign(rects, rects + count);
    return operation(handle, [&](Device& device) {
      auto target = surface(device, device.target, device.targetIndex);
      if (!target || target->locked) return E_INVALIDARG;
      for (const auto& area : areas) {
        if (area.right < area.left || area.bottom < area.top) return E_INVALIDARG;
        if (!(input.Flags & computeRects) && (area.left < 0 || area.top < 0
            || UINT(area.right) > target->desc.width || UINT(area.bottom) > target->desc.height))
          return E_INVALIDARG;
      }
      return device.backend->clear(input.FillColor, count, count ? areas.data() : nullptr,
                                    (input.Flags & computeRects) != 0);
    });
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY blt(HANDLE handle, const D3DDDIARG_BLT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Flags.Value & ~UINT(1)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    auto src = surface(device, input.hSrcResource, input.SrcSubResourceIndex);
    auto dst = surface(device, input.hDstResource, input.DstSubResourceIndex);
    if (!src || !dst || src->locked || dst->locked || src->desc.format != dst->desc.format
        || !validArea(input.SrcRect, src->desc) || !validArea(input.DstRect, dst->desc)
        || input.SrcRect.right - input.SrcRect.left != input.DstRect.right - input.DstRect.left
        || input.SrcRect.bottom - input.SrcRect.top != input.DstRect.bottom - input.DstRect.top)
      return E_INVALIDARG;
    return device.backend->copySurface(*dst->backend, input.DstRect, *src->backend, input.SrcRect);
  });
}

HRESULT APIENTRY lockResource(HANDLE handle, D3DDDIARG_LOCK* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Flags.Value & ~UINT(0x2a3) || (input.Flags.ReadOnly && input.Flags.WriteOnly)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    auto item = surface(device, input.hResource, input.SubResourceIndex);
    if (!item || !item->desc.lockable || item->locked
        || bool(input.Flags.NotifyOnly) != bool(item->desc.systemData)) return E_INVALIDARG;
    if (input.Flags.AreaValid && !validArea(input.Area, item->desc)) return E_INVALIDARG;
    const DWORD flags = (input.Flags.ReadOnly ? D3DLOCK_READONLY : 0)
      | (input.Flags.DoNotWait ? D3DLOCK_DONOTWAIT : 0);
    D3DLOCKED_RECT mapping = {};
    const HRESULT hr = result(device.backend->lockSurface(*item->backend,
      input.Flags.AreaValid ? &input.Area : nullptr, flags, mapping));
    if (FAILED(hr)) return hr;
    if (!mapping.pBits || mapping.Pitch <= 0) {
      device.backend->unlockSurface(*item->backend, false);
      return E_FAIL;
    }
    item->locked = true; item->notifyOnly = input.Flags.NotifyOnly != 0;
    args->pSurfData = mapping.pBits; args->Pitch = UINT(mapping.Pitch); args->SlicePitch = 0;
    return S_OK;
  });
}

HRESULT APIENTRY unlockResource(HANDLE handle, const D3DDDIARG_UNLOCK* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Flags.Value & ~UINT(1)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    auto item = surface(device, input.hResource, input.SubResourceIndex);
    if (!item || !item->locked || bool(input.Flags.NotifyOnly) != item->notifyOnly) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->unlockSurface(*item->backend));
    if (SUCCEEDED(hr)) item->locked = false;
    return hr;
  });
}

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
  table.pfnCreateResource = createResource;
  table.pfnDestroyResource = destroyResource;
  table.pfnSetRenderTarget = setRenderTarget;
  table.pfnClear = clear;
  table.pfnBlt = blt;
  table.pfnLock = lockResource;
  table.pfnUnlock = unlockResource;
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
