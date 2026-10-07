#include "umd_d3d9_adapter.h"
#include "umd_d3d9_backend.h"
#include "umd_allocation.h"
#include "umd_runtime_gpu.h"
#include "../d3d9/d3d9_shader_code.h"
#include "../d3d9/d3d9_caps.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <limits>
#include <climits>
#include <cmath>
#include <utility>

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
  std::unique_ptr<dxvk::umd::RuntimeAllocation> present;
  std::unique_ptr<dxvk::umd::D3D9SurfaceResource> backend;
  bool locked = false, notifyOnly = false;
};
struct Resource {
  HANDLE runtime = nullptr;
  dxvk::umd::D3D9BufferDesc bufferDesc;
  std::unique_ptr<dxvk::umd::D3D9BufferResource> buffer;
  bool bufferLocked = false;
  bool bufferReadOnly = false;
  UINT bufferLockOffset = 0, bufferLockBytes = 0;
  // Member order releases mip surfaces before their owning texture.
  std::unique_ptr<dxvk::umd::D3D9TextureResource> texture;
  std::vector<Surface> surfaces;
};
struct Declaration {
  std::unique_ptr<dxvk::umd::D3D9VertexDeclaration> backend;
  UINT streams = 0, streamZeroSize = 0;
  std::array<UINT, 16> streamSizes = {};
};
using ShaderStage = dxvk::umd::D3D9ShaderStage;
struct Shader {
  ShaderStage stage;
  std::unique_ptr<dxvk::umd::D3D9Shader> backend;
};
struct Query {
  D3DQUERYTYPE type;
  UINT bytes = 0;
  bool ended = false;
  std::unique_ptr<dxvk::umd::D3D9QueryResource> backend;
};
struct Device {
  std::shared_ptr<dxvk::umd::RuntimeService> service = std::make_shared<dxvk::umd::RuntimeService>(true);
  std::shared_ptr<dxvk::umd::RuntimeGpu> gpu;
  dxvk::umd::RuntimeMemory memory;
  std::unique_ptr<dxvk::umd::D3D9Backend> backend;
  HANDLE runtime = nullptr;
  bool busy = false, closing = false;
  std::atomic<bool> removed{false};
  std::unordered_map<HANDLE, std::unique_ptr<Resource>> resources;
  std::unordered_set<HANDLE> runtimeResources;
  std::unordered_map<HANDLE, std::unique_ptr<Declaration>> declarations;
  std::unordered_map<HANDLE, std::unique_ptr<Shader>> shaders;
  std::unordered_map<HANDLE, std::unique_ptr<Query>> queries;
  std::array<HANDLE, 2> boundShaders = {};
  std::array<HANDLE, 20> boundTextures = {};
  HANDLE declaration = nullptr;
  const void* userVertices = nullptr;
  UINT userStride = 0;
  struct Stream { HANDLE buffer = nullptr; UINT offset = 0, stride = 0; };
  std::array<Stream, 16> streams = {};
  HANDLE indices = nullptr;
  HANDLE target = nullptr;
  UINT targetIndex = 0;
  HANDLE depthStencil = nullptr;
  // Runtime light indices can be sparse. Map them to reusable compact renderer
  // slots instead of allowing an arbitrary index to resize its private vector.
  struct Light { UINT slot = 0; bool enabled = false; };
  std::unordered_map<UINT, Light> lights;

  HRESULT close() noexcept {
    if (closing) return S_OK;
    closing = true;
    dxvk::umd::RuntimeService::Scope scope(service.get());
    HRESULT hr = S_OK;
    try { service->drain([&] {
      try { if (backend) {
        for (auto& entry : resources) {
          if (entry.second->bufferLocked) backend->unlockBuffer(*entry.second->buffer);
          for (auto& surface : entry.second->surfaces)
            if (surface.locked) backend->unlockSurface(*surface.backend, false);
        }
        if (target) backend->setRenderTarget(nullptr);
        if (depthStencil) backend->setDepthStencil(nullptr);
        if (declaration) backend->setVertexDeclaration(nullptr);
        if (boundShaders[0]) backend->setShader(ShaderStage::Vertex, nullptr);
        if (boundShaders[1]) backend->setShader(ShaderStage::Pixel, nullptr);
        for (UINT i = 0; i < boundTextures.size(); ++i)
          if (boundTextures[i]) backend->setTexture(i < 16 ? i : D3DVERTEXTEXTURESAMPLER0 + i - 16, nullptr);
        for (UINT i = 0; i < streams.size(); ++i)
          if (streams[i].buffer) backend->setStreamSource(i, nullptr, 0, 0);
        if (indices) backend->setIndices(nullptr);
        for (const auto& light : lights)
          if (light.second.enabled) backend->setLightEnabled(light.second.slot, false);
        if (!resources.empty() || !declarations.empty() || !shaders.empty() || !queries.empty()) backend->flush();
      } } catch (...) { hr = E_FAIL; }
      for (auto& resource : resources) {
        for (auto& surface : resource.second->surfaces) {
          if (!surface.present) continue;
          const HRESULT cleanup = result(surface.present->release());
          if (FAILED(cleanup)) hr = cleanup;
        }
      }
      resources.clear();
      runtimeResources.clear();
      declarations.clear();
      shaders.clear();
      queries.clear();
      boundShaders = {};
      boundTextures = {};
      declaration = nullptr;
      userVertices = nullptr;
      userStride = 0;
      streams = {};
      indices = nullptr;
      target = nullptr;
      depthStencil = nullptr;
      lights.clear();
      backend.reset();
    }); }
    catch (...) { hr = E_FAIL; }
    try {
      const HRESULT cleanup = result(memory.close());
      if (FAILED(cleanup)) hr = cleanup;
    } catch (...) { hr = E_FAIL; }
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
template<typename Prepare, typename Function>
HRESULT preparedOperation(HANDLE handle, Prepare&& prepare, Function&& function, bool cleanup = false) noexcept {
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
    // Snapshot user data on the DDI caller before the callback pump starts.
    // Device serialization protects private bindings during this preparation.
    hr = result(prepare(*owner));
    if (SUCCEEDED(hr)) {
      hr = result(owner->service->run([&] {
        if (!cleanup) {
          const auto runtime = owner->gpu->backend();
          const HRESULT status = runtime.create.callbacks->status(runtime.create.owner);
          if (FAILED(status)) return status;
        }
        return function(*owner);
      }));
    }
  } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
    catch (...) { hr = E_FAIL; }
  if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET) owner->removed = true;
  return hr;
}

template<typename Function>
HRESULT operation(HANDLE handle, Function&& function, bool cleanup = false) noexcept {
  return preparedOperation(handle, [](Device&) { return S_OK; },
                           std::forward<Function>(function), cleanup);
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
  const auto input = *args;
  const bool buffer = input.Flags.VertexBuffer || input.Flags.IndexBuffer;
  if (input.Flags.Value & ~(buffer ? UINT(0x1800cc) : UINT(0x10087))) return E_INVALIDARG;
  if (buffer && (bool(input.Flags.VertexBuffer) == bool(input.Flags.IndexBuffer) || input.SurfCount != 1))
    return E_INVALIDARG;
  const bool target = input.Flags.RenderTarget != 0;
  const bool depth = input.Flags.ZBuffer != 0;
  const bool texture = input.Flags.Texture != 0;
  const bool dynamic = input.Flags.Dynamic != 0;
  if (!buffer && dynamic && (!texture || target || depth)) return E_INVALIDARG;
  if (depth && (target || texture || input.SurfCount != 1)) return E_INVALIDARG;
  if (input.Flags.NotLockable && !target && !texture && !buffer && !depth) return E_INVALIDARG;
  if ((target || depth) && (input.MultisampleType != D3DDDIMULTISAMPLE_NONE || input.MultisampleQuality))
    return E_INVALIDARG;
  if (input.Pool != D3DDDIPOOL_SYSTEMMEM && input.Pool != D3DDDIPOOL_VIDEOMEMORY
      && input.Pool != D3DDDIPOOL_LOCALVIDMEM && input.Pool != D3DDDIPOOL_NONLOCALVIDMEM)
    return E_INVALIDARG;
  if ((target || depth) && input.Pool == D3DDDIPOOL_SYSTEMMEM) return E_INVALIDARG;
  if (texture && (!input.MipLevels || input.MipLevels != input.SurfCount || input.MipLevels > 32
      || (input.Flags.NotLockable && input.Pool == D3DDDIPOOL_SYSTEMMEM))) return E_INVALIDARG;
  const auto format = static_cast<D3DFORMAT>(input.Format);
  if (buffer) {
    if (input.Flags.IndexBuffer ? (format != D3DFMT_INDEX16 && format != D3DFMT_INDEX32)
                                : format != D3DFMT_VERTEXDATA) return E_INVALIDARG;
  } else if (depth) {
    if (format != D3DFMT_D16 && format != D3DFMT_D24S8) return E_INVALIDARG;
  } else if (format != D3DFMT_A8R8G8B8 && format != D3DFMT_X8R8G8B8) return E_INVALIDARG;
  dxvk::umd::D3D9BufferDesc bufferDesc;
  std::vector<uint8_t> bufferInitialData;
  std::vector<dxvk::umd::D3D9SurfaceDesc> descriptions;
  return preparedOperation(handle, [&](Device&) {
    // Serialize before reading pointed metadata: nested callbacks must not
    // inspect an in-progress caller's surface list. Reserved fields for
    // absent usage flags retain their native meaning.
    const uint64_t listBytes = uint64_t(input.SurfCount) * sizeof(D3DDDI_SURFACEINFO);
    if (listBytes > UINTPTR_MAX - reinterpret_cast<uintptr_t>(input.pSurfList)) return E_INVALIDARG;
    if (buffer) {
      const auto info = input.pSurfList[0];
      if (!info.Width || info.Width > UINT_MAX - 255
          || (info.pSysMem && input.Pool != D3DDDIPOOL_SYSTEMMEM)) return E_INVALIDARG;
      if (input.Flags.IndexBuffer && info.Width % (format == D3DFMT_INDEX16 ? 2 : 4)) return E_INVALIDARG;
      bufferDesc.bytes = info.Width; bufferDesc.format = format;
      bufferDesc.fvf = input.Flags.VertexBuffer ? input.Fvf : 0;
      bufferDesc.index = input.Flags.IndexBuffer != 0; bufferDesc.dynamic = input.Flags.Dynamic != 0;
      bufferDesc.writeOnly = input.Flags.WriteOnly != 0; bufferDesc.lockable = !input.Flags.NotLockable;
      bufferDesc.systemMemory = input.Pool == D3DDDIPOOL_SYSTEMMEM;
      bufferDesc.systemData = const_cast<void*>(info.pSysMem);
      if (info.pSysMem) {
        if (bufferDesc.bytes > UINTPTR_MAX - reinterpret_cast<uintptr_t>(info.pSysMem)) return E_INVALIDARG;
        const auto data = static_cast<const uint8_t*>(info.pSysMem);
        bufferInitialData.assign(data, data + bufferDesc.bytes);
      }
      // Height, depth, pitches and mip count are reserved for linear resources.
      return S_OK;
    }
    descriptions.reserve(input.SurfCount);
    for (UINT i = 0; i < input.SurfCount; ++i) {
      const auto info = input.pSurfList[i];
      dxvk::umd::D3D9SurfaceDesc desc;
      desc.width = info.Width; desc.height = info.Height; desc.format = format;
      desc.renderTarget = target; desc.systemMemory = input.Pool == D3DDDIPOOL_SYSTEMMEM;
      desc.depthStencil = depth;
      desc.dynamic = dynamic;
      desc.lockable = !depth && !input.Flags.NotLockable && (!texture || desc.systemMemory || dynamic);
      if (!desc.width || !desc.height || desc.width > UINT(INT_MAX / 4) || desc.height > UINT(INT_MAX))
        return E_INVALIDARG;
      if (texture && i) {
        const auto& previous = descriptions.back();
        if ((previous.width == 1 && previous.height == 1)
            || desc.width != std::max(1u, previous.width / 2)
            || desc.height != std::max(1u, previous.height / 2)) return E_INVALIDARG;
      }
      if (info.pSysMem) {
        if (!desc.systemMemory || info.SysMemPitch < desc.width * 4 || info.SysMemPitch > UINT(INT_MAX))
          return E_INVALIDARG;
        const uint64_t size = uint64_t(info.SysMemPitch) * (desc.height - 1) + uint64_t(desc.width) * 4;
        if (size > UINTPTR_MAX - reinterpret_cast<uintptr_t>(info.pSysMem)) return E_INVALIDARG;
        desc.systemData = const_cast<void*>(info.pSysMem); desc.systemPitch = info.SysMemPitch;
      }
      descriptions.push_back(desc);
    }
    return S_OK;
  }, [&](Device& device) {
      if (!device.runtimeResources.emplace(input.hResource).second) return E_INVALIDARG;
      struct Reservation {
        Device& device; HANDLE runtime; bool published = false;
        ~Reservation() { if (!published) device.runtimeResources.erase(runtime); }
      } reservation{device, input.hResource};
      auto resource = std::make_unique<Resource>();
      resource->runtime = input.hResource;
      resource->surfaces.reserve(descriptions.size());
      if (buffer) {
        resource->bufferDesc = bufferDesc;
        const HRESULT hr = result(device.backend->createBuffer(bufferDesc, resource->buffer,
          bufferInitialData.empty() ? nullptr : bufferInitialData.data()));
        if (FAILED(hr)) return hr;
        if (!resource->buffer) return E_FAIL;
      } else if (texture) {
        std::vector<std::unique_ptr<dxvk::umd::D3D9SurfaceResource>> levels;
        const HRESULT hr = result(device.backend->createTexture(descriptions.data(), input.MipLevels,
          resource->texture, levels));
        if (FAILED(hr)) return hr;
        if (!resource->texture || levels.size() != descriptions.size()) return E_FAIL;
        for (size_t i = 0; i < levels.size(); ++i) {
          if (!levels[i]) return E_FAIL;
          Surface entry; entry.desc = descriptions[i]; entry.backend = std::move(levels[i]);
          resource->surfaces.push_back(std::move(entry));
        }
      } else {
        for (const auto& desc : descriptions) {
          Surface entry; entry.desc = desc;
          const HRESULT hr = result(device.backend->createSurface(desc, entry.backend));
          if (FAILED(hr)) return hr;
          if (!entry.backend) return E_FAIL;
          resource->surfaces.push_back(std::move(entry));
        }
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
}

HRESULT APIENTRY present(HANDLE handle, const D3DDDIARG_PRESENT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  // Destination index and flip interval are reserved for this source-only
  // blit. A runtime resource token is never a kernel allocation handle.
  if (!input.hSrcResource || input.hDstResource || input.SrcSubResourceIndex
      || input.Flags.Value != 1) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    const auto entry = device.resources.find(input.hSrcResource);
    if (entry == device.resources.end()) return E_INVALIDARG;
    auto& resource = *entry->second;
    if (resource.surfaces.size() != 1 || resource.buffer) return D3DERR_NOTAVAILABLE;
    auto& surface = resource.surfaces[0];
    if (!surface.desc.renderTarget || surface.desc.depthStencil
        || surface.desc.systemMemory || surface.locked) return E_INVALIDARG;
    if (!device.memory.available9()) return D3DERR_NOTAVAILABLE;
    HRESULT hr = result(device.backend->flush());
    if (FAILED(hr)) return hr;
    std::vector<uint8_t> pixels;
    hr = result(device.backend->readSurface(*surface.backend, pixels));
    if (FAILED(hr)) return hr;
    const uint64_t size = uint64_t(surface.desc.width) * surface.desc.height * 4;
    if (size != pixels.size()) return E_FAIL;
    const auto runtime = device.gpu->backend();
    hr = result(runtime.create.callbacks->status(runtime.create.owner));
    if (FAILED(hr)) return hr;
    if (!surface.present) {
      auto allocation = std::make_unique<dxvk::umd::RuntimeAllocation>();
      const auto format = surface.desc.format == D3DFMT_X8R8G8B8
        ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
      hr = result(device.memory.allocate(*allocation, resource.runtime,
        surface.desc.width, surface.desc.height, format));
      if (FAILED(hr)) return hr;
      surface.present = std::move(allocation);
    }
    // Readback is complete and unmapped before any caller-thread publication
    // callback. The DDI owns these tightly packed bytes throughout the copy.
    hr = result(device.memory.upload(*surface.present, pixels.data(), surface.desc.width * 4));
    if (FAILED(hr)) return hr;
    hr = result(runtime.create.callbacks->status(runtime.create.owner));
    if (FAILED(hr)) return hr;
    return result(device.memory.present9(*surface.present, input,
      [&] { return !device.closing && !device.removed; }));
  });
}

HRESULT APIENTRY destroyResource(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    const auto entry = device.resources.find(token);
    if (entry == device.resources.end()) return E_INVALIDARG;
    if (entry->second->bufferLocked) return E_INVALIDARG;
    for (const auto& item : entry->second->surfaces)
      if (item.locked) return E_INVALIDARG;
    if (device.target == token) {
      const HRESULT hr = result(device.backend->setRenderTarget(nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.target = nullptr;
    }
    if (device.depthStencil == token) {
      const HRESULT hr = result(device.backend->setDepthStencil(nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.depthStencil = nullptr;
    }
    for (UINT i = 0; i < device.boundTextures.size(); ++i) {
      if (device.boundTextures[i] != token) continue;
      const HRESULT hr = result(device.backend->setTexture(i < 16 ? i : D3DVERTEXTEXTURESAMPLER0 + i - 16, nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.boundTextures[i] = nullptr;
    }
    for (UINT i = 0; i < device.streams.size(); ++i) {
      if (device.streams[i].buffer != token) continue;
      const HRESULT hr = result(device.backend->setStreamSource(i, nullptr, 0, 0));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.streams[i] = {};
    }
    if (device.indices == token) {
      const HRESULT hr = result(device.backend->setIndices(nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.indices = nullptr;
    }
    // The backend joins recording/submission before runtime backing expires.
    const HRESULT hr = result(device.backend->flush());
    if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
    for (auto& surface : entry->second->surfaces) {
      if (!surface.present) continue;
      const HRESULT cleanup = result(surface.present->release());
      if (FAILED(cleanup)) return cleanup;
    }
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

HRESULT APIENTRY setDepthStencil(HANDLE handle, const D3DDDIARG_SETDEPTHSTENCIL* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  return operation(handle, [&](Device& device) {
    auto depth = input.hZBuffer ? surface(device, input.hZBuffer, 0) : nullptr;
    if (input.hZBuffer && (!depth || !depth->desc.depthStencil || depth->locked)) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setDepthStencil(depth ? depth->backend.get() : nullptr));
    if (SUCCEEDED(hr)) device.depthStencil = input.hZBuffer;
    return hr;
  });
}

HRESULT APIENTRY clear(HANDLE handle, const D3DDDIARG_CLEAR* args, UINT count, const RECT* rects) {
  if (!args || (count && !rects)) return E_INVALIDARG;
  const auto input = *args;
  constexpr UINT computeRects = 8; // D3DCLEAR_COMPUTERECTS, native DDI only.
  constexpr UINT buffers = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
  if (!(input.Flags & buffers) || (input.Flags & ~(buffers | computeRects))) return E_INVALIDARG;
  if ((input.Flags & D3DCLEAR_ZBUFFER) && (!std::isfinite(input.FillDepth)
      || input.FillDepth < 0.0f || input.FillDepth > 1.0f)) return E_INVALIDARG;
  if ((input.Flags & D3DCLEAR_STENCIL) && input.FillStencil > 255) return E_INVALIDARG;
  std::vector<RECT> areas;
  return preparedOperation(handle, [&](Device& device) {
    auto target = (input.Flags & D3DCLEAR_TARGET) ? surface(device, device.target, device.targetIndex) : nullptr;
    auto depth = (input.Flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL))
      ? surface(device, device.depthStencil, 0) : nullptr;
    if ((input.Flags & D3DCLEAR_TARGET) && (!target || target->locked)) return E_INVALIDARG;
    if ((input.Flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) && (!depth || depth->locked)) return E_INVALIDARG;
    if ((input.Flags & D3DCLEAR_STENCIL) && depth->desc.format != D3DFMT_D24S8) return E_INVALIDARG;
    if (uint64_t(count) * sizeof(RECT) > UINTPTR_MAX - reinterpret_cast<uintptr_t>(rects)) return E_INVALIDARG;
    if (count) areas.assign(rects, rects + count);
    for (const auto& area : areas) {
      if (area.right < area.left || area.bottom < area.top) return E_INVALIDARG;
      if (!(input.Flags & computeRects)) {
        if (area.left < 0 || area.top < 0) return E_INVALIDARG;
        for (const auto item : {target, depth})
          if (item && (UINT(area.right) > item->desc.width || UINT(area.bottom) > item->desc.height))
            return E_INVALIDARG;
      }
    }
    return S_OK;
  }, [&](Device& device) {
    return device.backend->clear(input.Flags & buffers, input.FillColor, input.FillDepth,
      input.FillStencil, count, count ? areas.data() : nullptr, (input.Flags & computeRects) != 0);
  });
}

HRESULT APIENTRY blt(HANDLE handle, const D3DDDIARG_BLT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Flags.Value & ~UINT(1)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    auto src = surface(device, input.hSrcResource, input.SrcSubResourceIndex);
    auto dst = surface(device, input.hDstResource, input.DstSubResourceIndex);
    if (!src || !dst || src->locked || dst->locked || src->desc.depthStencil || dst->desc.depthStencil
        || src->desc.format != dst->desc.format
        || !validArea(input.SrcRect, src->desc) || !validArea(input.DstRect, dst->desc)
        || input.SrcRect.right - input.SrcRect.left != input.DstRect.right - input.DstRect.left
        || input.SrcRect.bottom - input.SrcRect.top != input.DstRect.bottom - input.DstRect.top)
      return E_INVALIDARG;
    return device.backend->copySurface(*dst->backend, input.DstRect, *src->backend, input.SrcRect);
  });
}

HRESULT APIENTRY bufferBlt(HANDLE handle, const D3DDDIARG_BUFFERBLT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  std::vector<uint8_t> upload;
  return preparedOperation(handle, [&](Device& device) {
    const auto src = device.resources.find(input.hSrcResource);
    const auto dst = device.resources.find(input.hDstResource);
    if (src == device.resources.end() || dst == device.resources.end()
        || !src->second->buffer || !dst->second->buffer
        || src->second->bufferLocked || dst->second->bufferLocked
        || uint64_t(input.SrcRange.Offset) + input.SrcRange.Size > src->second->bufferDesc.bytes
        || uint64_t(input.Offset) + input.SrcRange.Size > dst->second->bufferDesc.bytes)
      return E_INVALIDARG;
    if (input.SrcRange.Size && src->second->bufferDesc.systemData) {
      const auto data = static_cast<const uint8_t*>(src->second->bufferDesc.systemData) + input.SrcRange.Offset;
      upload.assign(data, data + input.SrcRange.Size);
    }
    return S_OK;
  }, [&](Device& device) {
    if (!input.SrcRange.Size) return S_OK;
    return device.backend->copyBuffer(*device.resources.at(input.hDstResource)->buffer, input.Offset,
      *device.resources.at(input.hSrcResource)->buffer, input.SrcRange.Offset, input.SrcRange.Size,
      upload.empty() ? nullptr : upload.data());
  });
}

HRESULT APIENTRY lockResource(HANDLE handle, D3DDDIARG_LOCK* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Flags.ReadOnly && input.Flags.WriteOnly) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    const auto resource = device.resources.find(input.hResource);
    if (resource != device.resources.end() && resource->second->buffer) {
      auto& item = *resource->second;
      if (input.SubResourceIndex || input.Flags.Value & ~UINT(0x29f) || item.bufferLocked
          || bool(input.Flags.NotifyOnly) != bool(item.bufferDesc.systemData)
          || !item.bufferDesc.lockable || (input.Flags.ReadOnly && item.bufferDesc.writeOnly)
          || (input.Flags.NoOverwrite && input.Flags.Discard) || (input.Flags.Discard && input.Flags.ReadOnly)
          || ((input.Flags.NoOverwrite || input.Flags.Discard) && !item.bufferDesc.dynamic)) return E_INVALIDARG;
      const UINT offset = input.Flags.RangeValid ? input.Range.Offset : 0;
      const UINT bytes = input.Flags.RangeValid ? input.Range.Size : item.bufferDesc.bytes;
      if (!bytes || uint64_t(offset) + bytes > item.bufferDesc.bytes) return E_INVALIDARG;
      const DWORD flags = (input.Flags.ReadOnly ? D3DLOCK_READONLY : 0)
        | (input.Flags.Discard ? D3DLOCK_DISCARD : 0) | (input.Flags.NoOverwrite ? D3DLOCK_NOOVERWRITE : 0)
        | (input.Flags.DoNotWait ? D3DLOCK_DONOTWAIT : 0);
      void* data = nullptr;
      const HRESULT hr = result(device.backend->lockBuffer(*item.buffer, offset, bytes, flags, data));
      if (FAILED(hr)) return hr;
      item.bufferLocked = true;
      item.bufferReadOnly = input.Flags.ReadOnly != 0;
      item.bufferLockOffset = offset; item.bufferLockBytes = bytes;
      if (!data) {
        if (result(device.backend->unlockBuffer(*item.buffer)) == S_OK) item.bufferLocked = false;
        return E_FAIL;
      }
      args->pSurfData = data; args->Pitch = 0; args->SlicePitch = 0;
      return S_OK;
    }
    if (input.Flags.Value & ~UINT(0x2ab)) return E_INVALIDARG;
    auto item = surface(device, input.hResource, input.SubResourceIndex);
    if (!item || !item->desc.lockable || item->locked
        || bool(input.Flags.NotifyOnly) != bool(item->desc.systemData)) return E_INVALIDARG;
    // A top-level DISCARD may replace the backing of the whole mip chain.
    // Reject it while any level is mapped, and never pass a partial/read-only
    // or lower-level discard to the renderer. NOOVERWRITE remains buffer-only.
    if (input.Flags.Discard && (!item->desc.dynamic || input.SubResourceIndex
        || input.Flags.AreaValid || input.Flags.ReadOnly
        || std::any_of(resource->second->surfaces.begin(), resource->second->surfaces.end(),
          [](const Surface& level) { return level.locked; }))) return E_INVALIDARG;
    if (input.Flags.AreaValid && !validArea(input.Area, item->desc)) return E_INVALIDARG;
    const DWORD flags = (input.Flags.ReadOnly ? D3DLOCK_READONLY : 0)
      | (input.Flags.Discard ? D3DLOCK_DISCARD : 0)
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
  std::vector<uint8_t> upload;
  return preparedOperation(handle, [&](Device& device) {
    const auto resource = device.resources.find(input.hResource);
    if (resource != device.resources.end() && resource->second->buffer) {
      const auto& item = *resource->second;
      if (input.SubResourceIndex || !item.bufferLocked
          || bool(input.Flags.NotifyOnly) != bool(item.bufferDesc.systemData)) return E_INVALIDARG;
      if (item.bufferDesc.systemData && !item.bufferReadOnly) {
        const auto data = static_cast<const uint8_t*>(item.bufferDesc.systemData) + item.bufferLockOffset;
        upload.assign(data, data + item.bufferLockBytes);
      }
    }
    return S_OK;
  }, [&](Device& device) {
    const auto resource = device.resources.find(input.hResource);
    if (resource != device.resources.end() && resource->second->buffer) {
      auto& item = *resource->second;
      if (input.SubResourceIndex || !item.bufferLocked
          || bool(input.Flags.NotifyOnly) != bool(item.bufferDesc.systemData)) return E_INVALIDARG;
      const HRESULT hr = result(device.backend->unlockBuffer(*item.buffer, upload.empty() ? nullptr : upload.data()));
      if (SUCCEEDED(hr)) item.bufferLocked = false;
      return hr;
    }
    auto item = surface(device, input.hResource, input.SubResourceIndex);
    if (!item || !item->locked || bool(input.Flags.NotifyOnly) != item->notifyOnly) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->unlockSurface(*item->backend));
    if (SUCCEEDED(hr)) item->locked = false;
    return hr;
  });
}

bool textureSlot(UINT stage, UINT& slot) {
  if (stage < 16) { slot = stage; return true; }
  if (stage >= D3DVERTEXTEXTURESAMPLER0 && stage <= D3DVERTEXTEXTURESAMPLER3) {
    slot = 16 + stage - D3DVERTEXTEXTURESAMPLER0;
    return true;
  }
  return false;
}

HRESULT APIENTRY setTexture(HANDLE handle, UINT stage, HANDLE token) {
  UINT slot = 0;
  if (!textureSlot(stage, slot)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    const auto entry = device.resources.find(token);
    if (token) {
      if (entry == device.resources.end() || !entry->second->texture
          || entry->second->surfaces[0].desc.systemMemory) return E_INVALIDARG;
      for (const auto& level : entry->second->surfaces)
        if (level.locked) return E_INVALIDARG;
    }
    const HRESULT hr = result(device.backend->setTexture(stage, token ? entry->second->texture.get() : nullptr));
    if (SUCCEEDED(hr)) device.boundTextures[slot] = token;
    return hr;
  });
}

bool textureStageState(D3DDDITEXTURESTAGESTATETYPE input, D3DTEXTURESTAGESTATETYPE& state) {
#define D3D9_TEXTURE_STATE(name) case D3DDDITSS_##name: state = D3DTSS_##name; return true
  switch (input) {
    D3D9_TEXTURE_STATE(COLOROP); D3D9_TEXTURE_STATE(COLORARG1); D3D9_TEXTURE_STATE(COLORARG2);
    D3D9_TEXTURE_STATE(ALPHAOP); D3D9_TEXTURE_STATE(ALPHAARG1); D3D9_TEXTURE_STATE(ALPHAARG2);
    D3D9_TEXTURE_STATE(BUMPENVMAT00); D3D9_TEXTURE_STATE(BUMPENVMAT01);
    D3D9_TEXTURE_STATE(BUMPENVMAT10); D3D9_TEXTURE_STATE(BUMPENVMAT11);
    D3D9_TEXTURE_STATE(TEXCOORDINDEX); D3D9_TEXTURE_STATE(BUMPENVLSCALE);
    D3D9_TEXTURE_STATE(BUMPENVLOFFSET); D3D9_TEXTURE_STATE(TEXTURETRANSFORMFLAGS);
    D3D9_TEXTURE_STATE(COLORARG0); D3D9_TEXTURE_STATE(ALPHAARG0);
    D3D9_TEXTURE_STATE(RESULTARG); D3D9_TEXTURE_STATE(CONSTANT);
    default: return false;
  }
#undef D3D9_TEXTURE_STATE
}
bool samplerState(D3DDDITEXTURESTAGESTATETYPE input, D3DSAMPLERSTATETYPE& state) {
#define D3D9_SAMPLER_STATE(name) case D3DDDITSS_##name: state = D3DSAMP_##name; return true
  switch (input) {
    D3D9_SAMPLER_STATE(ADDRESSU); D3D9_SAMPLER_STATE(ADDRESSV); D3D9_SAMPLER_STATE(ADDRESSW);
    D3D9_SAMPLER_STATE(BORDERCOLOR); D3D9_SAMPLER_STATE(MAGFILTER); D3D9_SAMPLER_STATE(MINFILTER);
    D3D9_SAMPLER_STATE(MIPFILTER); D3D9_SAMPLER_STATE(MIPMAPLODBIAS); D3D9_SAMPLER_STATE(MAXMIPLEVEL);
    D3D9_SAMPLER_STATE(MAXANISOTROPY); D3D9_SAMPLER_STATE(SRGBTEXTURE);
    D3D9_SAMPLER_STATE(ELEMENTINDEX); D3D9_SAMPLER_STATE(DMAPOFFSET);
    default: return false;
  }
#undef D3D9_SAMPLER_STATE
}
HRESULT APIENTRY setTextureStageState(HANDLE handle, const D3DDDIARG_TEXTURESTAGESTATE* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  D3DTEXTURESTAGESTATETYPE textureState = D3DTSS_COLOROP;
  D3DSAMPLERSTATETYPE sampleState = D3DSAMP_ADDRESSU;
  const bool sample = samplerState(input.State, sampleState);
  UINT slot = 0;
  if (sample ? !textureSlot(input.Stage, slot)
             : (input.Stage >= 8 || !textureStageState(input.State, textureState))) return E_INVALIDARG;
  // Native colorkey and TEXTUREMAP are not public API state enums. They need
  // dedicated semantics; never cast them to D3DTSS or D3DSAMP.
  return operation(handle, [&](Device& device) {
    return sample ? device.backend->setSamplerState(input.Stage, sampleState, input.Value)
                  : device.backend->setTextureStageState(input.Stage, textureState, input.Value);
  });
}

HRESULT APIENTRY texBlt(HANDLE handle, const D3DDDIARG_TEXBLT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  struct Copy {
    Surface* source;
    Surface* destination;
    RECT sourceRect, destinationRect;
    std::vector<uint8_t> upload;
  };
  std::vector<Copy> copies;
  return preparedOperation(handle, [&](Device& device) {
    const auto src = device.resources.find(input.hSrcResource);
    const auto dst = device.resources.find(input.hDstResource);
    if (src == device.resources.end() || dst == device.resources.end()
        || !src->second->texture || !dst->second->texture || input.DstPoint.x < 0 || input.DstPoint.y < 0)
      return E_INVALIDARG;
    auto& sources = src->second->surfaces;
    auto& destinations = dst->second->surfaces;
    if (sources[0].desc.format != destinations[0].desc.format || !validArea(input.SrcRect, sources[0].desc))
      return E_INVALIDARG;
    // Match dimensions, not level numbers: a smaller destination starts at a
    // corresponding source mip. CubeMapFace is reserved for these 2D textures.
    size_t first = 0;
    while (first < sources.size() && (sources[first].desc.width != destinations[0].desc.width
        || sources[first].desc.height != destinations[0].desc.height)) ++first;
    if (first == sources.size()) return E_INVALIDARG;
    const size_t count = std::min(sources.size() - first, destinations.size());
    copies.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      auto& source = sources[first + i];
      auto& destination = destinations[i];
      if (source.locked || destination.locked) return E_INVALIDARG;
      const uint64_t divisor = uint64_t(1) << (first + i);
      RECT from = {LONG(uint64_t(input.SrcRect.left) / divisor), LONG(uint64_t(input.SrcRect.top) / divisor),
        LONG(std::min<uint64_t>(source.desc.width, (uint64_t(input.SrcRect.right) + divisor - 1) / divisor)),
        LONG(std::min<uint64_t>(source.desc.height, (uint64_t(input.SrcRect.bottom) + divisor - 1) / divisor))};
      const LONG x = LONG(UINT(input.DstPoint.x) >> i), y = LONG(UINT(input.DstPoint.y) >> i);
      const int64_t right = int64_t(x) + from.right - from.left, bottom = int64_t(y) + from.bottom - from.top;
      if (!validArea(from, source.desc) || right > INT_MAX || bottom > INT_MAX) return E_INVALIDARG;
      RECT to = {x, y, LONG(right), LONG(bottom)};
      if (!validArea(to, destination.desc)) return E_INVALIDARG;
      Copy copy{&source, &destination, from, to, {}};
      if (source.desc.systemData && !destination.desc.systemMemory) {
        const size_t pitch = size_t(from.right - from.left) * 4;
        const size_t rows = size_t(from.bottom - from.top);
        if (rows > SIZE_MAX / pitch) return E_INVALIDARG;
        copy.upload.resize(pitch * rows);
        auto data = static_cast<const uint8_t*>(source.desc.systemData)
          + size_t(from.top) * source.desc.systemPitch + size_t(from.left) * 4;
        for (size_t row = 0; row < rows; ++row)
          std::memcpy(copy.upload.data() + row * pitch, data + row * source.desc.systemPitch, pitch);
      }
      copies.push_back(std::move(copy));
    }
    return S_OK;
  }, [&](Device& device) {
    for (const auto& copy : copies) {
      const dxvk::umd::D3D9SurfaceUpload upload{copy.upload.data(), UINT(copy.sourceRect.right - copy.sourceRect.left) * 4};
      const HRESULT hr = result(device.backend->copySurface(*copy.destination->backend, copy.destinationRect,
        *copy.source->backend, copy.sourceRect, copy.upload.empty() ? nullptr : &upload));
      if (FAILED(hr)) return hr;
    }
    return S_OK;
  });
}

HRESULT APIENTRY createVertexDeclaration(HANDLE handle, D3DDDIARG_CREATEVERTEXSHADERDECL* args,
    const D3DDDIVERTEXELEMENT* inputElements) {
  if (!args || !inputElements) return E_INVALIDARG;
  const UINT count = args->NumVertexElements;
  if (!count || count > 64) return E_INVALIDARG;
  try {
    std::vector<D3DVERTEXELEMENT9> elements;
    elements.reserve(size_t(count) + 1);
    UINT streams = 0, extent = 0;
    std::array<UINT, 16> streamSizes = {};
    constexpr UINT sizes[] = {4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8};
    for (UINT i = 0; i < count; ++i) {
      const auto item = inputElements[i];
      // Some runtimes include the terminator in the counted declaration.
      if (item.Stream == 0xff && item.Type == D3DDECLTYPE_UNUSED) {
        if (i + 1 != count || !i || item.Offset || item.Method || item.Usage || item.UsageIndex)
          return E_INVALIDARG;
        break;
      }
      if (item.Stream >= 16 || item.Type >= D3DDECLTYPE_UNUSED
          || item.Method != D3DDECLMETHOD_DEFAULT || item.Usage > D3DDECLUSAGE_SAMPLE
          || item.UsageIndex >= 16) return E_INVALIDARG;
      elements.push_back({item.Stream,item.Offset,item.Type,item.Method,item.Usage,item.UsageIndex});
      streams |= UINT(1) << item.Stream;
      streamSizes[item.Stream] = (std::max)(streamSizes[item.Stream], UINT(item.Offset) + sizes[item.Type]);
      if (!item.Stream) extent = (std::max)(extent, UINT(item.Offset) + sizes[item.Type]);
    }
    elements.push_back(D3DDECL_END());
    return operation(handle, [&](Device& device) {
      auto declaration = std::make_unique<Declaration>();
      declaration->streams = streams; declaration->streamZeroSize = extent;
      declaration->streamSizes = streamSizes;
      const HRESULT hr = result(device.backend->createVertexDeclaration(elements.data(), declaration->backend));
      if (FAILED(hr)) return hr;
      if (!declaration->backend) return E_FAIL;
      const auto runtime = device.gpu->backend();
      const HRESULT status = runtime.create.callbacks->status(runtime.create.owner);
      if (FAILED(status)) return status;
      std::lock_guard<std::mutex> lock(devicesMutex);
      if (!nextHandle) return E_OUTOFMEMORY;
      const HANDLE token = reinterpret_cast<HANDLE>(nextHandle++);
      device.declarations.emplace(token, std::move(declaration));
      args->ShaderHandle = token;
      return S_OK;
    });
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}

HRESULT APIENTRY setVertexDeclaration(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    auto entry = device.declarations.find(token);
    if (token && entry == device.declarations.end()) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setVertexDeclaration(token ? entry->second->backend.get() : nullptr));
    if (SUCCEEDED(hr)) device.declaration = token;
    return hr;
  });
}

HRESULT APIENTRY destroyVertexDeclaration(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    const auto entry = device.declarations.find(token);
    if (entry == device.declarations.end()) return E_INVALIDARG;
    if (device.declaration == token) {
      const HRESULT hr = result(device.backend->setVertexDeclaration(nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      device.declaration = nullptr;
    }
    const HRESULT hr = result(device.backend->flush());
    if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
    device.declarations.erase(entry);
    return hr;
  }, true);
}

bool queryDescription(D3DDDIQUERYTYPE input, D3DQUERYTYPE& type, UINT& bytes) {
  switch (input) {
    case D3DDDIQUERYTYPE_VCACHE: type = D3DQUERYTYPE_VCACHE; bytes = sizeof(D3DDEVINFO_VCACHE); break;
    case D3DDDIQUERYTYPE_EVENT: type = D3DQUERYTYPE_EVENT; bytes = sizeof(BOOL); break;
    case D3DDDIQUERYTYPE_OCCLUSION: type = D3DQUERYTYPE_OCCLUSION; bytes = sizeof(UINT); break;
    case D3DDDIQUERYTYPE_TIMESTAMP: type = D3DQUERYTYPE_TIMESTAMP; bytes = sizeof(UINT64); break;
    case D3DDDIQUERYTYPE_TIMESTAMPDISJOINT: type = D3DQUERYTYPE_TIMESTAMPDISJOINT; bytes = sizeof(BOOL); break;
    case D3DDDIQUERYTYPE_TIMESTAMPFREQ: type = D3DQUERYTYPE_TIMESTAMPFREQ; bytes = sizeof(UINT64); break;
    default: return false;
  }
  return true;
}

HRESULT APIENTRY createQuery(HANDLE handle, D3DDDIARG_CREATEQUERY* args) {
  if (!args) return E_INVALIDARG;
  const auto queryType = args->QueryType;
  D3DQUERYTYPE type = D3DQUERYTYPE_EVENT;
  UINT bytes = 0;
  if (!queryDescription(queryType, type, bytes)) return D3DERR_NOTAVAILABLE;
  return operation(handle, [&](Device& device) {
    auto query = std::make_unique<Query>();
    query->type = type; query->bytes = bytes;
    const HRESULT hr = result(device.backend->createQuery(type, query->backend));
    if (FAILED(hr)) return hr;
    if (!query->backend) return E_FAIL;
    const auto runtime = device.gpu->backend();
    const HRESULT status = result(runtime.create.callbacks->status(runtime.create.owner));
    if (FAILED(status)) return status;
    std::lock_guard<std::mutex> lock(devicesMutex);
    if (!nextHandle) return E_OUTOFMEMORY;
    const HANDLE token = reinterpret_cast<HANDLE>(nextHandle++);
    device.queries.emplace(token, std::move(query));
    args->hQuery = token;
    return S_OK;
  });
}

HRESULT APIENTRY issueQuery(HANDLE handle, const D3DDDIARG_ISSUEQUERY* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  // Native Begin/End bits have the opposite values to D3DISSUE_BEGIN/END.
  if (input.Flags.Value != 1 && input.Flags.Value != 2) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    const auto entry = device.queries.find(input.hQuery);
    if (entry == device.queries.end()) return E_INVALIDARG;
    auto& query = *entry->second;
    const bool begin = input.Flags.Begin != 0;
    if (begin && query.type != D3DQUERYTYPE_OCCLUSION && query.type != D3DQUERYTYPE_TIMESTAMPDISJOINT)
      return E_INVALIDARG;
    const HRESULT hr = result(device.backend->issueQuery(*query.backend, begin ? D3DISSUE_BEGIN : D3DISSUE_END));
    if (FAILED(hr)) return hr;
    const auto runtime = device.gpu->backend();
    const HRESULT status = result(runtime.create.callbacks->status(runtime.create.owner));
    if (FAILED(status)) return status;
    query.ended = !begin;
    return S_OK;
  });
}

HRESULT APIENTRY getQueryData(HANDLE handle, const D3DDDIARG_GETQUERYDATA* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  alignas(UINT64) std::array<uint8_t, sizeof(D3DDEVINFO_VCACHE)> data = {};
  bool pending = false;
  const HRESULT hr = preparedOperation(handle, [&](Device& device) {
    const auto entry = device.queries.find(input.hQuery);
    if (entry == device.queries.end()) return E_INVALIDARG;
    if (input.pData && entry->second->bytes > UINTPTR_MAX - reinterpret_cast<uintptr_t>(input.pData))
      return E_INVALIDARG;
    return S_OK;
  }, [&](Device& device) {
    auto& query = *device.queries.at(input.hQuery);
    // Avoid the core's implicit issue-on-first-GetData behavior at the DDI.
    if (!query.ended) { pending = true; return S_OK; }
    const HRESULT fetched = device.backend->getQueryData(*query.backend,
      input.pData ? data.data() : nullptr, input.pData ? query.bytes : 0);
    if (fetched != S_OK && fetched != S_FALSE) return result(fetched);
    const auto runtime = device.gpu->backend();
    const HRESULT status = result(runtime.create.callbacks->status(runtime.create.owner));
    if (FAILED(status)) return status;
    if (fetched == S_FALSE) { pending = true; return S_OK; }
    if (query.type == D3DQUERYTYPE_EVENT) {
      // Cached core EVENT data writes one bool byte; the native ABI requires
      // the complete four-byte BOOL TRUE, only after actual completion.
      const BOOL completed = TRUE;
      std::memcpy(data.data(), &completed, sizeof(completed));
    }
    if (input.pData) std::memcpy(input.pData, data.data(), query.bytes);
    return S_OK;
  });
  // S_FALSE is a query-completion result; all other DDIs retain strict S_OK.
  return hr == S_OK && pending ? S_FALSE : hr;
}

HRESULT APIENTRY destroyQuery(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    const auto entry = device.queries.find(token);
    if (entry == device.queries.end()) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->flush());
    if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
    device.queries.erase(entry);
    return hr;
  }, true);
}

HRESULT createShader(HANDLE handle, ShaderStage stage, UINT bytes,
                     const UINT* input, HANDLE* output) {
  if (!input || bytes < 8 || bytes % sizeof(DWORD) || bytes > 4u * 1024u * 1024u)
    return E_INVALIDARG;
  if (bytes > std::numeric_limits<uintptr_t>::max() - reinterpret_cast<uintptr_t>(input))
    return E_INVALIDARG;
  std::vector<DWORD> code;
  return preparedOperation(handle, [&](Device&) {
    code.resize(bytes / sizeof(DWORD));
    std::memcpy(code.data(), input, bytes);
    return dxvk::validateD3D9ShaderCode(code.data(), bytes, stage == ShaderStage::Vertex)
      ? S_OK : E_INVALIDARG;
  }, [&](Device& device) {
    auto shader = std::make_unique<Shader>();
    shader->stage = stage;
    const HRESULT hr = result(device.backend->createShader(stage, code.data(), bytes, shader->backend));
    if (FAILED(hr)) return hr;
    if (!shader->backend) return E_FAIL;
    const auto runtime = device.gpu->backend();
    const HRESULT status = runtime.create.callbacks->status(runtime.create.owner);
    if (FAILED(status)) return status;
    std::lock_guard<std::mutex> lock(devicesMutex);
    if (!nextHandle) return E_OUTOFMEMORY;
    const HANDLE token = reinterpret_cast<HANDLE>(nextHandle++);
    device.shaders.emplace(token, std::move(shader));
    *output = token;
    return S_OK;
  });
}
HRESULT APIENTRY createVertexShader(HANDLE handle, D3DDDIARG_CREATEVERTEXSHADERFUNC* args, const UINT* code) {
  if (!args) return E_INVALIDARG;
  const UINT bytes = args->Size;
  return createShader(handle, ShaderStage::Vertex, bytes, code, &args->ShaderHandle);
}
HRESULT APIENTRY createPixelShader(HANDLE handle, D3DDDIARG_CREATEPIXELSHADER* args, const UINT* code) {
  if (!args) return E_INVALIDARG;
  const UINT bytes = args->CodeSize;
  return createShader(handle, ShaderStage::Pixel, bytes, code, &args->ShaderHandle);
}

template<ShaderStage Stage>
HRESULT APIENTRY setShader(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    const auto entry = device.shaders.find(token);
    if (token && (entry == device.shaders.end() || entry->second->stage != Stage)) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setShader(Stage, token ? entry->second->backend.get() : nullptr));
    if (SUCCEEDED(hr)) device.boundShaders[size_t(Stage)] = token;
    return hr;
  });
}
template<ShaderStage Stage>
HRESULT APIENTRY destroyShader(HANDLE handle, HANDLE token) {
  return operation(handle, [&](Device& device) {
    const auto entry = device.shaders.find(token);
    if (entry == device.shaders.end() || entry->second->stage != Stage) return E_INVALIDARG;
    auto& binding = device.boundShaders[size_t(Stage)];
    if (binding == token) {
      const HRESULT hr = result(device.backend->setShader(Stage, nullptr));
      if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
      binding = nullptr;
    }
    const HRESULT hr = result(device.backend->flush());
    if (FAILED(hr) && hr != D3DERR_DEVICELOST) return hr;
    device.shaders.erase(entry);
    return hr;
  }, true);
}

enum class ConstantType { Float, Int, Bool };
template<ShaderStage Stage, ConstantType Type, typename Args, typename T>
HRESULT shaderConstants(HANDLE handle, const Args* args, const T* input) {
  if (!args) return E_INVALIDARG;
  const auto data = *args;
  constexpr UINT limit = Type == ConstantType::Float
    ? (Stage == ShaderStage::Vertex ? dxvk::caps::MaxFloatConstantsVS : dxvk::caps::MaxSM3FloatConstantsPS)
    : dxvk::caps::MaxOtherConstants;
  if (data.Register > limit || data.Count > limit - data.Register || (!input && data.Count))
    return E_INVALIDARG;
  constexpr UINT components = Type == ConstantType::Bool ? 1u : 4u;
  const size_t bytes = size_t(data.Count) * components * sizeof(T);
  if (bytes > std::numeric_limits<uintptr_t>::max() - reinterpret_cast<uintptr_t>(input))
    return E_INVALIDARG;
  std::vector<T> values;
  return preparedOperation(handle, [&](Device&) {
    if (data.Count) {
      values.resize(size_t(data.Count) * components);
      std::memcpy(values.data(), input, bytes);
    }
    return S_OK;
  }, [&](Device& device) {
    if (!data.Count) return S_OK;
    if constexpr (Type == ConstantType::Float)
      return device.backend->setShaderConstantF(Stage, data.Register, data.Count, values.data());
    else if constexpr (Type == ConstantType::Int)
      return device.backend->setShaderConstantI(Stage, data.Register, data.Count, values.data());
    else
      return device.backend->setShaderConstantB(Stage, data.Register, data.Count, values.data());
  });
}
HRESULT APIENTRY vertexConstantsF(HANDLE handle, const D3DDDIARG_SETVERTEXSHADERCONST* args, const void* values) {
  return shaderConstants<ShaderStage::Vertex, ConstantType::Float>(handle, args, static_cast<const float*>(values));
}
HRESULT APIENTRY pixelConstantsF(HANDLE handle, const D3DDDIARG_SETPIXELSHADERCONST* args, const float* values) {
  return shaderConstants<ShaderStage::Pixel, ConstantType::Float>(handle, args, values);
}
HRESULT APIENTRY vertexConstantsI(HANDLE handle, const D3DDDIARG_SETVERTEXSHADERCONSTI* args, const INT* values) {
  return shaderConstants<ShaderStage::Vertex, ConstantType::Int>(handle, args, values);
}
HRESULT APIENTRY pixelConstantsI(HANDLE handle, const D3DDDIARG_SETPIXELSHADERCONSTI* args, const INT* values) {
  return shaderConstants<ShaderStage::Pixel, ConstantType::Int>(handle, args, values);
}
HRESULT APIENTRY vertexConstantsB(HANDLE handle, const D3DDDIARG_SETVERTEXSHADERCONSTB* args, const BOOL* values) {
  return shaderConstants<ShaderStage::Vertex, ConstantType::Bool>(handle, args, values);
}
HRESULT APIENTRY pixelConstantsB(HANDLE handle, const D3DDDIARG_SETPIXELSHADERCONSTB* args, const BOOL* values) {
  return shaderConstants<ShaderStage::Pixel, ConstantType::Bool>(handle, args, values);
}

bool renderState(D3DDDIRENDERSTATETYPE input, D3DRENDERSTATETYPE& output) {
  // Native-only legacy commands must never become ignored public states.
#define D3D9_STATE(name) case D3DDDIRS_##name: output = D3DRS_##name; return true
  switch (input) {
    D3D9_STATE(ZENABLE); D3D9_STATE(FILLMODE); D3D9_STATE(SHADEMODE);
    D3D9_STATE(ZWRITEENABLE); D3D9_STATE(ALPHATESTENABLE); D3D9_STATE(LASTPIXEL);
    D3D9_STATE(SRCBLEND); D3D9_STATE(DESTBLEND); D3D9_STATE(CULLMODE);
    D3D9_STATE(ZFUNC); D3D9_STATE(ALPHAREF); D3D9_STATE(ALPHAFUNC);
    D3D9_STATE(DITHERENABLE); D3D9_STATE(ALPHABLENDENABLE); D3D9_STATE(FOGENABLE);
    D3D9_STATE(SPECULARENABLE); D3D9_STATE(FOGCOLOR); D3D9_STATE(FOGTABLEMODE);
    D3D9_STATE(FOGSTART); D3D9_STATE(FOGEND); D3D9_STATE(FOGDENSITY);
    D3D9_STATE(RANGEFOGENABLE); D3D9_STATE(STENCILENABLE); D3D9_STATE(STENCILFAIL);
    D3D9_STATE(STENCILZFAIL); D3D9_STATE(STENCILPASS); D3D9_STATE(STENCILFUNC);
    D3D9_STATE(STENCILREF); D3D9_STATE(STENCILMASK); D3D9_STATE(STENCILWRITEMASK);
    D3D9_STATE(TEXTUREFACTOR); D3D9_STATE(WRAP0); D3D9_STATE(WRAP1);
    D3D9_STATE(WRAP2); D3D9_STATE(WRAP3); D3D9_STATE(WRAP4); D3D9_STATE(WRAP5);
    D3D9_STATE(WRAP6); D3D9_STATE(WRAP7); D3D9_STATE(CLIPPING); D3D9_STATE(LIGHTING);
    D3D9_STATE(AMBIENT); D3D9_STATE(FOGVERTEXMODE); D3D9_STATE(COLORVERTEX);
    D3D9_STATE(LOCALVIEWER); D3D9_STATE(NORMALIZENORMALS);
    D3D9_STATE(DIFFUSEMATERIALSOURCE); D3D9_STATE(SPECULARMATERIALSOURCE);
    D3D9_STATE(AMBIENTMATERIALSOURCE); D3D9_STATE(EMISSIVEMATERIALSOURCE);
    D3D9_STATE(VERTEXBLEND); D3D9_STATE(CLIPPLANEENABLE); D3D9_STATE(POINTSIZE);
    D3D9_STATE(POINTSIZE_MIN); D3D9_STATE(POINTSPRITEENABLE); D3D9_STATE(POINTSCALEENABLE);
    D3D9_STATE(POINTSCALE_A); D3D9_STATE(POINTSCALE_B); D3D9_STATE(POINTSCALE_C);
    D3D9_STATE(MULTISAMPLEANTIALIAS); D3D9_STATE(MULTISAMPLEMASK);
    D3D9_STATE(PATCHEDGESTYLE); D3D9_STATE(DEBUGMONITORTOKEN); D3D9_STATE(POINTSIZE_MAX);
    D3D9_STATE(INDEXEDVERTEXBLENDENABLE); D3D9_STATE(COLORWRITEENABLE);
    D3D9_STATE(TWEENFACTOR); D3D9_STATE(BLENDOP); D3D9_STATE(POSITIONDEGREE);
    D3D9_STATE(NORMALDEGREE); D3D9_STATE(SCISSORTESTENABLE); D3D9_STATE(SLOPESCALEDEPTHBIAS);
    D3D9_STATE(ANTIALIASEDLINEENABLE); D3D9_STATE(MINTESSELLATIONLEVEL);
    D3D9_STATE(MAXTESSELLATIONLEVEL); D3D9_STATE(ADAPTIVETESS_X); D3D9_STATE(ADAPTIVETESS_Y);
    D3D9_STATE(ADAPTIVETESS_Z); D3D9_STATE(ADAPTIVETESS_W);
    D3D9_STATE(ENABLEADAPTIVETESSELLATION); D3D9_STATE(TWOSIDEDSTENCILMODE);
    D3D9_STATE(CCW_STENCILFAIL); D3D9_STATE(CCW_STENCILZFAIL); D3D9_STATE(CCW_STENCILPASS);
    D3D9_STATE(CCW_STENCILFUNC); D3D9_STATE(COLORWRITEENABLE1); D3D9_STATE(COLORWRITEENABLE2);
    D3D9_STATE(COLORWRITEENABLE3); D3D9_STATE(BLENDFACTOR); D3D9_STATE(SRGBWRITEENABLE);
    D3D9_STATE(DEPTHBIAS); D3D9_STATE(WRAP8); D3D9_STATE(WRAP9); D3D9_STATE(WRAP10);
    D3D9_STATE(WRAP11); D3D9_STATE(WRAP12); D3D9_STATE(WRAP13); D3D9_STATE(WRAP14);
    D3D9_STATE(WRAP15); D3D9_STATE(SEPARATEALPHABLENDENABLE); D3D9_STATE(SRCBLENDALPHA);
    D3D9_STATE(DESTBLENDALPHA); D3D9_STATE(BLENDOPALPHA);
    default: return false;
  }
#undef D3D9_STATE
}

HRESULT APIENTRY setRenderState(HANDLE handle, const D3DDDIARG_RENDERSTATE* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  D3DRENDERSTATETYPE mapped = D3DRS_ZENABLE;
  if (input.State == D3DDDIRS_SCENECAPTURE || input.State == D3DDDIRS_SOFTWAREVERTEXPROCESSING) {
    if (input.Value > 1) return E_INVALIDARG;
  } else if (!renderState(input.State, mapped)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    if (input.State == D3DDDIRS_SCENECAPTURE) return device.backend->setScene(input.Value != 0);
    if (input.State == D3DDDIRS_SOFTWAREVERTEXPROCESSING)
      return device.backend->setSoftwareVertexProcessing(input.Value != 0);
    return device.backend->setRenderState(mapped, input.Value);
  });
}

bool transformType(D3DTRANSFORMSTATETYPE type) {
  const UINT value = UINT(type);
  return type == D3DTS_VIEW || type == D3DTS_PROJECTION
    || (value >= UINT(D3DTS_TEXTURE0) && value <= UINT(D3DTS_TEXTURE7))
    || (value >= UINT(D3DTS_WORLD) && value < UINT(D3DTS_WORLD) + 256);
}
template<typename Args, bool Multiply>
HRESULT APIENTRY setTransform(HANDLE handle, const Args* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (!transformType(input.TransformType)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    return device.backend->setTransform(input.TransformType, input.Matrix, Multiply);
  });
}
HRESULT APIENTRY setMaterial(HANDLE handle, const D3DDDIARG_SETMATERIAL* args) {
  if (!args) return E_INVALIDARG;
  const D3DMATERIAL9 material = {args->Diffuse, args->Ambient, args->Specular, args->Emissive, args->Power};
  return operation(handle, [&](Device& device) { return device.backend->setMaterial(material); });
}
HRESULT APIENTRY setClipPlane(HANDLE handle, const D3DDDIARG_SETCLIPPLANE* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  // The public COM core caps invalid indices to its last plane. A typed DDI
  // must reject them before callback pumping or changing a valid plane.
  if (input.Index >= dxvk::caps::MaxClipPlanes) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    return device.backend->setClipPlane(input.Index, input.Plane);
  });
}
HRESULT APIENTRY createLight(HANDLE handle, const D3DDDIARG_CREATELIGHT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  return operation(handle, [&](Device& device) {
    if (device.lights.count(input.Index)) return E_INVALIDARG;
    UINT slot = 0;
    while (std::any_of(device.lights.begin(), device.lights.end(), [&](const auto& light) {
      return light.second.slot == slot;
    })) {
      if (slot == UINT_MAX) return E_OUTOFMEMORY;
      ++slot;
    }
    device.lights.emplace(input.Index, Device::Light{slot, false});
    try {
      D3DLIGHT9 light = {};
      light.Type = D3DLIGHT_DIRECTIONAL;
      light.Diffuse = {1.0f, 1.0f, 1.0f, 0.0f};
      light.Direction.z = 1.0f;
      const HRESULT hr = result(device.backend->setLight(slot, light));
      if (FAILED(hr)) device.lights.erase(input.Index);
      return hr;
    } catch (...) {
      device.lights.erase(input.Index);
      throw;
    }
  });
}
HRESULT APIENTRY setLight(HANDLE handle, const D3DDDIARG_SETLIGHT* args, const D3DDDI_LIGHT* properties) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  // The SDK defines three enum values (0/1/2), rather than independent bits.
  if (input.DataType != D3DDDI_SETLIGHT_ENABLE && input.DataType != D3DDDI_SETLIGHT_DISABLE
      && input.DataType != D3DDDI_SETLIGHT_DATA) return E_INVALIDARG;
  D3DLIGHT9 light = {};
  return preparedOperation(handle, [&](Device& device) {
    const auto entry = device.lights.find(input.Index);
    if (entry == device.lights.end()) return E_INVALIDARG;
    if (input.DataType == D3DDDI_SETLIGHT_DATA) {
      if (!properties || uintptr_t(properties) > UINTPTR_MAX - sizeof(*properties)) return E_INVALIDARG;
      const auto value = *properties;
      if (value.Type != D3DLIGHT_POINT && value.Type != D3DLIGHT_SPOT
          && value.Type != D3DLIGHT_DIRECTIONAL) return E_INVALIDARG;
      light = {value.Type, value.Diffuse, value.Specular, value.Ambient,
        value.Position, value.Direction, value.Range, value.Falloff,
        value.Attenuation0, value.Attenuation1, value.Attenuation2, value.Theta, value.Phi};
    } else if (input.DataType == D3DDDI_SETLIGHT_ENABLE && !entry->second.enabled
        && std::count_if(device.lights.begin(), device.lights.end(), [](const auto& item) {
          return item.second.enabled;
        }) >= dxvk::caps::MaxEnabledLights) return D3DERR_INVALIDCALL;
    return S_OK;
  }, [&](Device& device) {
    auto& owned = device.lights.at(input.Index);
    if (input.DataType == D3DDDI_SETLIGHT_DATA) return device.backend->setLight(owned.slot, light);
    const bool enable = input.DataType == D3DDDI_SETLIGHT_ENABLE;
    const HRESULT hr = result(device.backend->setLightEnabled(owned.slot, enable));
    if (SUCCEEDED(hr)) owned.enabled = enable;
    return hr;
  });
}
HRESULT APIENTRY destroyLight(HANDLE handle, const D3DDDIARG_DESTROYLIGHT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  return operation(handle, [&](Device& device) {
    const auto entry = device.lights.find(input.Index);
    if (entry == device.lights.end()) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setLightEnabled(entry->second.slot, false));
    if (SUCCEEDED(hr)) device.lights.erase(entry);
    return hr;
  }, true);
}

HRESULT APIENTRY setViewport(HANDLE handle, const D3DDDIARG_VIEWPORTINFO* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (!input.Width || !input.Height || uint64_t(input.X) + input.Width > INT_MAX
      || uint64_t(input.Y) + input.Height > INT_MAX) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    return device.backend->setViewport(input.X,input.Y,input.Width,input.Height);
  });
}
HRESULT APIENTRY setZRange(HANDLE handle, const D3DDDIARG_ZRANGE* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (!std::isfinite(input.MinZ) || !std::isfinite(input.MaxZ) || input.MinZ < 0.0f
      || input.MaxZ > 1.0f || input.MinZ > input.MaxZ) return E_INVALIDARG;
  return operation(handle, [&](Device& device) { return device.backend->setZRange(input.MinZ,input.MaxZ); });
}
HRESULT APIENTRY setScissorRect(HANDLE handle, const RECT* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.left > input.right || input.top > input.bottom) return E_INVALIDARG;
  return operation(handle, [&](Device& device) { return device.backend->setScissorRect(input); });
}

HRESULT APIENTRY setStreamSource(HANDLE handle, const D3DDDIARG_SETSTREAMSOURCE* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Stream >= 16 || (input.hVertexBuffer && !input.Stride)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    const auto entry = device.resources.find(input.hVertexBuffer);
    if (input.hVertexBuffer && (entry == device.resources.end() || !entry->second->buffer
        || entry->second->bufferDesc.index || entry->second->bufferLocked
        || input.Offset >= entry->second->bufferDesc.bytes)) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setStreamSource(input.Stream,
      input.hVertexBuffer ? entry->second->buffer.get() : nullptr,
      input.hVertexBuffer ? input.Offset : 0, input.hVertexBuffer ? input.Stride : 0));
    if (SUCCEEDED(hr)) {
      device.streams[input.Stream] = input.hVertexBuffer
        ? Device::Stream{input.hVertexBuffer,input.Offset,input.Stride} : Device::Stream{};
      if (!input.Stream) { device.userVertices = nullptr; device.userStride = 0; }
    }
    return hr;
  });
}

HRESULT APIENTRY setIndices(HANDLE handle, const D3DDDIARG_SETINDICES* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  return operation(handle, [&](Device& device) {
    const auto entry = device.resources.find(input.hIndexBuffer);
    if (input.hIndexBuffer && (entry == device.resources.end() || !entry->second->buffer
        || !entry->second->bufferDesc.index || entry->second->bufferLocked
        || input.Stride != (entry->second->bufferDesc.format == D3DFMT_INDEX16 ? 2u : 4u))) return E_INVALIDARG;
    const HRESULT hr = result(device.backend->setIndices(input.hIndexBuffer ? entry->second->buffer.get() : nullptr));
    if (SUCCEEDED(hr)) device.indices = input.hIndexBuffer;
    return hr;
  });
}

HRESULT APIENTRY setStreamSourceUm(HANDLE handle, const D3DDDIARG_SETSTREAMSOURCEUM* args,
    const void* vertices) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  if (input.Stream || (vertices && !input.Stride)) return E_INVALIDARG;
  return operation(handle, [&](Device& device) {
    if (device.streams[0].buffer) {
      const HRESULT hr = result(device.backend->setStreamSource(0, nullptr, 0, 0));
      if (FAILED(hr)) return hr;
      device.streams[0] = {};
    }
    device.userVertices = vertices;
    device.userStride = vertices ? input.Stride : 0;
    return S_OK;
  });
}

bool primitiveVertices(D3DPRIMITIVETYPE type, UINT primitives, uint64_t& count) {
  count = primitives;
  switch (type) {
    case D3DPT_POINTLIST: break;
    case D3DPT_LINELIST: count *= 2; break;
    case D3DPT_LINESTRIP: if (count) ++count; break;
    case D3DPT_TRIANGLELIST: count *= 3; break;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN: if (count) count += 2; break;
    default: return false;
  }
  return count <= UINT_MAX;
}

bool drawBindings(Device& device, const Declaration& declaration, uint64_t first, uint64_t count) {
  if (device.userVertices) return false; // Mixed user-memory/resource streams need a separate upload plan.
  if (first > UINT_MAX || count > uint64_t(UINT_MAX) + 1 - first) return false;
  for (UINT i = 0; i < device.streams.size(); ++i) {
    if (!(declaration.streams & (UINT(1) << i))) continue;
    const auto& stream = device.streams[i];
    const auto entry = device.resources.find(stream.buffer);
    if (entry == device.resources.end() || !entry->second->buffer || entry->second->bufferLocked
        || stream.stride < declaration.streamSizes[i]) return false;
    // Count complete declarations, rather than requiring unused final stride padding.
    if (count && uint64_t(stream.offset) + (first + count - 1) * stream.stride
        + declaration.streamSizes[i] > entry->second->bufferDesc.bytes) return false;
  }
  return true;
}

bool drawTarget(Device& device) {
  auto target = surface(device, device.target, device.targetIndex);
  if (!target || target->locked) return false;
  if (device.depthStencil) {
    auto depth = surface(device, device.depthStencil, 0);
    if (!depth || depth->locked || depth->desc.width < target->desc.width
        || depth->desc.height < target->desc.height) return false;
  }
  for (const auto binding : device.boundTextures) {
    if (!binding) continue;
    for (const auto& level : device.resources.at(binding)->surfaces)
      if (level.locked) return false;
  }
  return true;
}

HRESULT APIENTRY drawPrimitive(HANDLE handle, const D3DDDIARG_DRAWPRIMITIVE* args,
    const UINT* flags) {
  if (!args || flags) return E_INVALIDARG; // Per-edge line-fill flags need their own path.
  const auto input = *args;
  uint64_t count = 0;
  if (!primitiveVertices(input.PrimitiveType, input.PrimitiveCount, count)) return E_INVALIDARG;
  std::vector<uint8_t> vertices;
  UINT stride = 0;
  return preparedOperation(handle, [&](Device& device) {
    const auto declaration = device.declarations.find(device.declaration);
    if (declaration == device.declarations.end() || !drawTarget(device)) return E_INVALIDARG;
    if (!device.userVertices)
      return drawBindings(device, *declaration->second, input.VStart, count) ? S_OK : E_INVALIDARG;
    if (!device.userStride) return E_INVALIDARG;
    if (declaration->second->streams != 1
        || declaration->second->streamZeroSize > device.userStride) return E_INVALIDARG;
    for (const auto binding : device.boundTextures) {
      if (!binding) continue;
      for (const auto& level : device.resources.at(binding)->surfaces)
        if (level.locked) return E_INVALIDARG;
    }
    stride = device.userStride;
    if (!count) return S_OK;
    const uint64_t offset = uint64_t(input.VStart) * stride;
    const uint64_t size = count * stride;
    const auto base = reinterpret_cast<uintptr_t>(device.userVertices);
    // DXVK's UP upload uses 32-bit sizes. Check both upload and host ranges
    // before touching memory, including alignment headroom for its buffer.
    if (size > UINT_MAX - 255 || offset > UINTPTR_MAX - base
        || size > UINTPTR_MAX - base - offset) return E_INVALIDARG;
    vertices.resize(size_t(size));
    std::memcpy(vertices.data(),reinterpret_cast<const void*>(base + uintptr_t(offset)),vertices.size());
    return S_OK;
  }, [&](Device& device) {
    if (!count) return S_OK;
    return device.userVertices
      ? device.backend->drawPrimitive(input.PrimitiveType,input.PrimitiveCount,vertices.data(),stride)
      : device.backend->drawPrimitiveBuffers(input.PrimitiveType,input.VStart,input.PrimitiveCount);
  });
}

HRESULT APIENTRY drawIndexedPrimitive(HANDLE handle, const D3DDDIARG_DRAWINDEXEDPRIMITIVE* args) {
  if (!args) return E_INVALIDARG;
  const auto input = *args;
  uint64_t count = 0;
  if (!primitiveVertices(input.PrimitiveType, input.PrimitiveCount, count)) return E_INVALIDARG;
  return preparedOperation(handle, [&](Device& device) {
    const auto declaration = device.declarations.find(device.declaration);
    const auto indices = device.resources.find(device.indices);
    if (declaration == device.declarations.end() || !drawTarget(device)
        || indices == device.resources.end() || !indices->second->buffer || indices->second->bufferLocked)
      return E_INVALIDARG;
    const uint64_t stride = indices->second->bufferDesc.format == D3DFMT_INDEX16 ? 2 : 4;
    if ((uint64_t(input.StartIndex) + count) * stride > indices->second->bufferDesc.bytes
        || (count && !input.NumVertices)) return E_INVALIDARG;
    const int64_t first = int64_t(input.BaseVertexIndex) + input.MinIndex;
    // Negative base indices are valid when the referenced vertex range stays nonnegative.
    if (first < 0 || uint64_t(first) + input.NumVertices > uint64_t(UINT_MAX) + 1
        || !drawBindings(device, *declaration->second, uint64_t(first), input.NumVertices)) return E_INVALIDARG;
    return S_OK;
  }, [&](Device& device) {
    return count ? device.backend->drawIndexedPrimitive(input.PrimitiveType,input.BaseVertexIndex,
      input.MinIndex,input.NumVertices,input.StartIndex,input.PrimitiveCount) : S_OK;
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
  owner->memory.initialize9(owner->runtime, cb, identity, owner->service);
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
  table.pfnPresent = present;
  table.pfnCreateResource = createResource;
  table.pfnDestroyResource = destroyResource;
  table.pfnSetRenderTarget = setRenderTarget;
  table.pfnSetDepthStencil = setDepthStencil;
  table.pfnClear = clear;
  table.pfnBlt = blt;
  table.pfnBufBlt = bufferBlt;
  table.pfnLock = lockResource;
  table.pfnUnlock = unlockResource;
  table.pfnSetTexture = setTexture;
  table.pfnSetTextureStageState = setTextureStageState;
  table.pfnTexBlt = texBlt;
  table.pfnCreateVertexShaderDecl = createVertexDeclaration;
  table.pfnSetVertexShaderDecl = setVertexDeclaration;
  table.pfnDeleteVertexShaderDecl = destroyVertexDeclaration;
  table.pfnCreateVertexShaderFunc = createVertexShader;
  table.pfnSetVertexShaderFunc = setShader<ShaderStage::Vertex>;
  table.pfnDeleteVertexShaderFunc = destroyShader<ShaderStage::Vertex>;
  table.pfnCreatePixelShader = createPixelShader;
  table.pfnSetPixelShader = setShader<ShaderStage::Pixel>;
  table.pfnDeletePixelShader = destroyShader<ShaderStage::Pixel>;
  table.pfnSetVertexShaderConst = vertexConstantsF;
  table.pfnSetPixelShaderConst = pixelConstantsF;
  table.pfnSetVertexShaderConstI = vertexConstantsI;
  table.pfnSetPixelShaderConstI = pixelConstantsI;
  table.pfnSetVertexShaderConstB = vertexConstantsB;
  table.pfnSetPixelShaderConstB = pixelConstantsB;
  table.pfnSetRenderState = setRenderState;
  table.pfnSetTransform = setTransform<D3DDDIARG_SETTRANSFORM, false>;
  table.pfnMultiplyTransform = setTransform<D3DDDIARG_MULTIPLYTRANSFORM, true>;
  table.pfnSetMaterial = setMaterial;
  table.pfnSetClipPlane = setClipPlane;
  table.pfnCreateQuery = createQuery;
  table.pfnIssueQuery = issueQuery;
  table.pfnGetQueryData = getQueryData;
  table.pfnDestroyQuery = destroyQuery;
  table.pfnCreateLight = createLight;
  table.pfnSetLight = setLight;
  table.pfnDestroyLight = destroyLight;
  table.pfnSetViewport = setViewport;
  table.pfnSetZRange = setZRange;
  table.pfnSetScissorRect = setScissorRect;
  table.pfnSetStreamSourceUm = setStreamSourceUm;
  table.pfnSetStreamSource = setStreamSource;
  table.pfnSetIndices = setIndices;
  table.pfnDrawPrimitive = drawPrimitive;
  table.pfnDrawIndexedPrimitive = drawIndexedPrimitive;
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
