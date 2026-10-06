#include "umd_d3d9_backend.h"
#include "umd_d3d9_api.h"
#include "umd_gpu_backend.h"
#include "umd_runtime_validation.h"
#include "../d3d9/d3d9_interface.h"
#include "../d3d9/d3d9_device.h"
#include <dxgi.h>
#include <new>
#include <vector>

namespace dxvk::umd {

struct D3D9Backend::State : GpuBackend {
  Com<D3D9InterfaceEx> parent;
  Com<D3D9DeviceEx> d3d;
  Com<IDirect3DSurface9> target;
};

struct D3D9SurfaceResource::State {
  Com<IDirect3DSurface9> surface;
  D3D9SurfaceDesc desc;
  D3DLOCKED_RECT mapping = {};
  RECT area = {};
  bool locked = false, readOnly = false;
};
D3D9SurfaceResource::D3D9SurfaceResource() : m_state(std::make_unique<State>()) { }
D3D9SurfaceResource::~D3D9SurfaceResource() = default;

struct D3D9TextureResource::State {
  Com<IDirect3DTexture9> texture;
};
D3D9TextureResource::D3D9TextureResource() : m_state(std::make_unique<State>()) { }
D3D9TextureResource::~D3D9TextureResource() = default;

struct D3D9VertexDeclaration::State {
  Com<IDirect3DVertexDeclaration9> declaration;
};
D3D9VertexDeclaration::D3D9VertexDeclaration() : m_state(std::make_unique<State>()) { }
D3D9VertexDeclaration::~D3D9VertexDeclaration() = default;

struct D3D9Shader::State {
  Com<IDirect3DVertexShader9> vertex;
  Com<IDirect3DPixelShader9> pixel;
};
D3D9Shader::D3D9Shader() : m_state(std::make_unique<State>()) { }
D3D9Shader::~D3D9Shader() = default;

struct D3D9BufferResource::State {
  Com<IDirect3DVertexBuffer9> vertex;
  Com<IDirect3DIndexBuffer9> index;
};
D3D9BufferResource::D3D9BufferResource() : m_state(std::make_unique<State>()) { }
D3D9BufferResource::~D3D9BufferResource() = default;

D3D9Backend::D3D9Backend() : m_state(std::make_unique<State>()) { }
D3D9Backend::~D3D9Backend() = default;
IDirect3DDevice9Ex* D3D9Backend::device() const noexcept { return m_state->d3d.ptr(); }
HRESULT D3D9Backend::flush() noexcept {
  try { return m_state->d3d->FlushRuntimeSubmission(); }
  catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
  catch (...) { return D3DERR_DEVICELOST; }
}

HRESULT D3D9Backend::createBuffer(const D3D9BufferDesc& desc,
    std::unique_ptr<D3D9BufferResource>& output) {
  auto buffer = std::make_unique<D3D9BufferResource>();
  const DWORD usage = (desc.dynamic ? D3DUSAGE_DYNAMIC : 0) | (desc.writeOnly ? D3DUSAGE_WRITEONLY : 0);
  const HRESULT hr = desc.index
    ? m_state->d3d->CreateIndexBuffer(desc.bytes, usage, desc.format, D3DPOOL_DEFAULT, &buffer->m_state->index, nullptr)
    : m_state->d3d->CreateVertexBuffer(desc.bytes, usage, desc.fvf, D3DPOOL_DEFAULT, &buffer->m_state->vertex, nullptr);
  if (hr != S_OK || !(desc.index ? bool(buffer->m_state->index) : bool(buffer->m_state->vertex)))
    return FAILED(hr) ? hr : E_FAIL;
  output = std::move(buffer);
  return S_OK;
}
HRESULT D3D9Backend::lockBuffer(D3D9BufferResource& buffer, UINT offset, UINT bytes, DWORD flags, void*& data) {
  return buffer.m_state->index ? buffer.m_state->index->Lock(offset, bytes, &data, flags)
                              : buffer.m_state->vertex->Lock(offset, bytes, &data, flags);
}
HRESULT D3D9Backend::unlockBuffer(D3D9BufferResource& buffer) {
  return buffer.m_state->index ? buffer.m_state->index->Unlock() : buffer.m_state->vertex->Unlock();
}
HRESULT D3D9Backend::setStreamSource(UINT stream, D3D9BufferResource* buffer, UINT offset, UINT stride) {
  return m_state->d3d->SetStreamSource(stream, buffer ? buffer->m_state->vertex.ptr() : nullptr, offset, stride);
}
HRESULT D3D9Backend::setIndices(D3D9BufferResource* buffer) {
  return m_state->d3d->SetIndices(buffer ? buffer->m_state->index.ptr() : nullptr);
}
HRESULT D3D9Backend::drawPrimitiveBuffers(D3DPRIMITIVETYPE type, UINT start, UINT count) {
  return m_state->d3d->DrawPrimitive(type, start, count);
}
HRESULT D3D9Backend::drawIndexedPrimitive(D3DPRIMITIVETYPE type, INT base, UINT minimum,
    UINT vertices, UINT start, UINT count) {
  return m_state->d3d->DrawIndexedPrimitive(type, base, minimum, vertices, start, count);
}

HRESULT D3D9Backend::createVertexDeclaration(const D3DVERTEXELEMENT9* elements,
    std::unique_ptr<D3D9VertexDeclaration>& output) {
  auto declaration = std::make_unique<D3D9VertexDeclaration>();
  const HRESULT hr = m_state->d3d->CreateVertexDeclaration(elements, &declaration->m_state->declaration);
  if (SUCCEEDED(hr)) output = std::move(declaration);
  return hr;
}
HRESULT D3D9Backend::setVertexDeclaration(D3D9VertexDeclaration* declaration) {
  return m_state->d3d->SetVertexDeclaration(declaration ? declaration->m_state->declaration.ptr() : nullptr);
}
HRESULT D3D9Backend::createShader(D3D9ShaderStage stage, const DWORD* code, UINT bytes,
    std::unique_ptr<D3D9Shader>& output) {
  auto shader = std::make_unique<D3D9Shader>();
  const HRESULT hr = stage == D3D9ShaderStage::Vertex
    ? m_state->d3d->CreateNativeVertexShader(code, bytes, &shader->m_state->vertex)
    : m_state->d3d->CreateNativePixelShader(code, bytes, &shader->m_state->pixel);
  if (hr != S_OK || !(stage == D3D9ShaderStage::Vertex ? bool(shader->m_state->vertex) : bool(shader->m_state->pixel)))
    return FAILED(hr) ? hr : E_FAIL;
  output = std::move(shader);
  return S_OK;
}
HRESULT D3D9Backend::setShader(D3D9ShaderStage stage, D3D9Shader* shader) {
  return stage == D3D9ShaderStage::Vertex
    ? m_state->d3d->SetVertexShader(shader ? shader->m_state->vertex.ptr() : nullptr)
    : m_state->d3d->SetPixelShader(shader ? shader->m_state->pixel.ptr() : nullptr);
}
HRESULT D3D9Backend::setShaderConstantF(D3D9ShaderStage stage, UINT first, UINT count, const float* values) {
  return stage == D3D9ShaderStage::Vertex ? m_state->d3d->SetVertexShaderConstantF(first, values, count)
                                        : m_state->d3d->SetPixelShaderConstantF(first, values, count);
}
HRESULT D3D9Backend::setShaderConstantI(D3D9ShaderStage stage, UINT first, UINT count, const INT* values) {
  return stage == D3D9ShaderStage::Vertex ? m_state->d3d->SetVertexShaderConstantI(first, values, count)
                                        : m_state->d3d->SetPixelShaderConstantI(first, values, count);
}
HRESULT D3D9Backend::setShaderConstantB(D3D9ShaderStage stage, UINT first, UINT count, const BOOL* values) {
  return stage == D3D9ShaderStage::Vertex ? m_state->d3d->SetVertexShaderConstantB(first, values, count)
                                        : m_state->d3d->SetPixelShaderConstantB(first, values, count);
}
HRESULT D3D9Backend::setTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX& matrix, bool multiply) {
  return multiply ? m_state->d3d->MultiplyTransform(state, &matrix)
                  : m_state->d3d->SetTransform(state, &matrix);
}
HRESULT D3D9Backend::setMaterial(const D3DMATERIAL9& material) {
  return m_state->d3d->SetMaterial(&material);
}
HRESULT D3D9Backend::setLight(UINT index, const D3DLIGHT9& light) {
  return m_state->d3d->SetLight(index, &light);
}
HRESULT D3D9Backend::setLightEnabled(UINT index, bool enable) {
  return m_state->d3d->LightEnable(index, enable);
}
HRESULT D3D9Backend::setRenderState(D3DRENDERSTATETYPE state, DWORD value) {
  return m_state->d3d->SetRenderState(state, value);
}
HRESULT D3D9Backend::setScene(bool capture) {
  return capture ? m_state->d3d->BeginScene() : m_state->d3d->EndScene();
}
HRESULT D3D9Backend::setSoftwareVertexProcessing(bool enable) {
  return m_state->d3d->SetSoftwareVertexProcessing(enable);
}
HRESULT D3D9Backend::setViewport(UINT x, UINT y, UINT width, UINT height) {
  D3DVIEWPORT9 viewport = {};
  const HRESULT hr = m_state->d3d->GetViewport(&viewport);
  if (FAILED(hr)) return hr;
  viewport.X = x; viewport.Y = y; viewport.Width = width; viewport.Height = height;
  return m_state->d3d->SetViewport(&viewport);
}
HRESULT D3D9Backend::setZRange(float minimum, float maximum) {
  D3DVIEWPORT9 viewport = {};
  const HRESULT hr = m_state->d3d->GetViewport(&viewport);
  if (FAILED(hr)) return hr;
  viewport.MinZ = minimum; viewport.MaxZ = maximum;
  return m_state->d3d->SetViewport(&viewport);
}
HRESULT D3D9Backend::setScissorRect(const RECT& area) {
  return m_state->d3d->SetScissorRect(&area);
}
HRESULT D3D9Backend::drawPrimitive(D3DPRIMITIVETYPE type, UINT count,
    const void* vertices, UINT stride) {
  // The native DDI owns the user-memory binding. The public UP call's
  // implicit stream-zero unbind must not retire that native binding.
  return m_state->d3d->DrawPrimitiveUP(type, count, vertices, stride);
}

HRESULT D3D9Backend::createSurface(const D3D9SurfaceDesc& desc,
                                 std::unique_ptr<D3D9SurfaceResource>& result) {
  auto resource = std::make_unique<D3D9SurfaceResource>();
  auto& state = *resource->m_state;
  state.desc = desc;
  const HRESULT hr = desc.depthStencil
    ? m_state->d3d->CreateDepthStencilSurface(desc.width, desc.height, desc.format,
        D3DMULTISAMPLE_NONE, 0, FALSE, &state.surface, nullptr)
    : desc.renderTarget
    ? m_state->d3d->CreateRenderTarget(desc.width, desc.height, desc.format,
        D3DMULTISAMPLE_NONE, 0, desc.lockable, &state.surface, nullptr)
    : m_state->d3d->CreateOffscreenPlainSurface(desc.width, desc.height, desc.format,
        desc.systemMemory ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT, &state.surface, nullptr);
  if (hr != S_OK || !state.surface) return FAILED(hr) ? hr : E_FAIL;
  result = std::move(resource);
  return S_OK;
}

HRESULT D3D9Backend::setRenderTarget(D3D9SurfaceResource* target) {
  auto surface = target ? target->m_state->surface.ptr() : nullptr;
  const HRESULT hr = m_state->d3d->SetNativeRenderTarget(surface);
  if (SUCCEEDED(hr)) m_state->target = surface;
  return hr;
}

HRESULT D3D9Backend::setDepthStencil(D3D9SurfaceResource* depth) {
  return m_state->d3d->SetDepthStencilSurface(depth ? depth->m_state->surface.ptr() : nullptr);
}

HRESULT D3D9Backend::createTexture(const D3D9SurfaceDesc* levels, UINT count,
    std::unique_ptr<D3D9TextureResource>& output,
    std::vector<std::unique_ptr<D3D9SurfaceResource>>& outputSurfaces) {
  auto texture = std::make_unique<D3D9TextureResource>();
  const auto& desc = levels[0];
  HRESULT hr = m_state->d3d->CreateTexture(desc.width, desc.height, count,
    desc.renderTarget ? D3DUSAGE_RENDERTARGET : 0, desc.format,
    desc.systemMemory ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT,
    &texture->m_state->texture, nullptr);
  if (hr != S_OK || !texture->m_state->texture) return FAILED(hr) ? hr : E_FAIL;
  if (texture->m_state->texture->GetLevelCount() != count) return E_FAIL;
  std::vector<std::unique_ptr<D3D9SurfaceResource>> surfaces;
  surfaces.reserve(count);
  for (UINT i = 0; i < count; ++i) {
    auto surface = std::make_unique<D3D9SurfaceResource>();
    surface->m_state->desc = levels[i];
    hr = texture->m_state->texture->GetSurfaceLevel(i, &surface->m_state->surface);
    if (hr != S_OK || !surface->m_state->surface) return FAILED(hr) ? hr : E_FAIL;
    surfaces.push_back(std::move(surface));
  }
  outputSurfaces = std::move(surfaces);
  output = std::move(texture);
  return S_OK;
}
HRESULT D3D9Backend::setTexture(UINT stage, D3D9TextureResource* texture) {
  return m_state->d3d->SetTexture(stage, texture ? texture->m_state->texture.ptr() : nullptr);
}
HRESULT D3D9Backend::setTextureStageState(UINT stage, D3DTEXTURESTAGESTATETYPE state, DWORD value) {
  return m_state->d3d->SetTextureStageState(stage, state, value);
}
HRESULT D3D9Backend::setSamplerState(UINT stage, D3DSAMPLERSTATETYPE state, DWORD value) {
  return m_state->d3d->SetSamplerState(stage, state, value);
}

HRESULT D3D9Backend::clear(DWORD flags, D3DCOLOR color, float depth, DWORD stencil,
    UINT count, const RECT* rects, bool computeRects) {
  std::vector<D3DRECT> areas;
  areas.reserve(count);
  for (UINT i = 0; i < count; ++i)
    areas.push_back({rects[i].left, rects[i].top, rects[i].right, rects[i].bottom});
  return m_state->d3d->ClearNative(count, count ? areas.data() : nullptr,
                                  flags, color, depth, stencil, computeRects);
}

static void copyRows(void* destination, UINT destinationPitch, const void* source,
                     UINT sourcePitch, UINT bytes, UINT rows) {
  auto dst = static_cast<uint8_t*>(destination);
  auto src = static_cast<const uint8_t*>(source);
  for (UINT row = 0; row < rows; ++row)
    std::memcpy(dst + size_t(row) * destinationPitch, src + size_t(row) * sourcePitch, bytes);
}

HRESULT D3D9Backend::lockSurface(D3D9SurfaceResource& resource, const RECT* area,
                               DWORD flags, D3DLOCKED_RECT& output) {
  auto& state = *resource.m_state;
  D3DLOCKED_RECT mapping = {};
  const HRESULT hr = state.surface->LockRect(&mapping, area, flags);
  if (FAILED(hr)) return hr;
  state.mapping = mapping;
  state.area = area ? *area : RECT{0, 0, LONG(state.desc.width), LONG(state.desc.height)};
  state.locked = true;
  state.readOnly = (flags & D3DLOCK_READONLY) != 0;
  output = mapping;
  if (state.desc.systemData) {
    auto bytes = static_cast<uint8_t*>(state.desc.systemData);
    output.pBits = bytes + size_t(state.area.top) * state.desc.systemPitch + size_t(state.area.left) * 4;
    output.Pitch = INT(state.desc.systemPitch);
  }
  return S_OK;
}

HRESULT D3D9Backend::unlockSurface(D3D9SurfaceResource& resource, bool upload) {
  auto& state = *resource.m_state;
  if (upload && state.desc.systemData && !state.readOnly) {
    auto bytes = static_cast<const uint8_t*>(state.desc.systemData)
      + size_t(state.area.top) * state.desc.systemPitch + size_t(state.area.left) * 4;
    copyRows(state.mapping.pBits, UINT(state.mapping.Pitch), bytes, state.desc.systemPitch,
             UINT(state.area.right - state.area.left) * 4, UINT(state.area.bottom - state.area.top));
  }
  const HRESULT hr = state.surface->UnlockRect();
  if (SUCCEEDED(hr)) state.locked = false;
  return hr;
}

HRESULT D3D9Backend::copySurface(D3D9SurfaceResource& destination, const RECT& destinationRect,
                               D3D9SurfaceResource& source, const RECT& sourceRect,
                               const D3D9SurfaceUpload* upload) {
  auto& dst = *destination.m_state;
  auto& src = *source.m_state;
  if (src.desc.systemMemory && dst.desc.systemMemory)
    return flush(); // The runtime performs the system-to-system copy itself.
  if (src.desc.systemMemory) {
    if (upload || src.desc.systemData) {
      D3DLOCKED_RECT mapping = {};
      HRESULT hr = src.surface->LockRect(&mapping, &sourceRect, 0);
      if (FAILED(hr)) return hr;
      const auto data = upload ? upload->data : static_cast<const uint8_t*>(src.desc.systemData)
        + size_t(sourceRect.top) * src.desc.systemPitch + size_t(sourceRect.left) * 4;
      const UINT pitch = upload ? upload->pitch : src.desc.systemPitch;
      if (!mapping.pBits || mapping.Pitch <= 0) {
        src.surface->UnlockRect();
        return E_FAIL;
      }
      copyRows(mapping.pBits, UINT(mapping.Pitch), data, pitch,
               UINT(sourceRect.right - sourceRect.left) * 4, UINT(sourceRect.bottom - sourceRect.top));
      hr = src.surface->UnlockRect();
      if (FAILED(hr)) return hr;
    }
    const POINT point = {destinationRect.left, destinationRect.top};
    return m_state->d3d->UpdateSurface(src.surface.ptr(), &sourceRect, dst.surface.ptr(), &point);
  }
  if (!dst.desc.systemMemory && &destination == &source) {
    if (!std::memcmp(&sourceRect, &destinationRect, sizeof(RECT))) return S_OK;
    Com<IDirect3DSurface9> temporary;
    HRESULT hr = m_state->d3d->CreateRenderTarget(UINT(sourceRect.right - sourceRect.left),
      UINT(sourceRect.bottom - sourceRect.top), src.desc.format,
      D3DMULTISAMPLE_NONE, 0, FALSE, &temporary, nullptr);
    if (FAILED(hr)) return hr;
    hr = m_state->d3d->StretchRect(src.surface.ptr(), &sourceRect, temporary.ptr(), nullptr, D3DTEXF_NONE);
    if (FAILED(hr)) return hr;
    return m_state->d3d->StretchRect(temporary.ptr(), nullptr, dst.surface.ptr(), &destinationRect, D3DTEXF_NONE);
  }
  if (!dst.desc.systemMemory)
    return m_state->d3d->StretchRect(src.surface.ptr(), &sourceRect,
                                    dst.surface.ptr(), &destinationRect, D3DTEXF_NONE);

  // Read back the actual GPU surface before copying the requested region.
  // This also handles caller-owned system memory with an arbitrary row pitch.
  Com<IDirect3DSurface9> staging;
  HRESULT hr = m_state->d3d->CreateOffscreenPlainSurface(src.desc.width, src.desc.height,
    src.desc.format, D3DPOOL_SYSTEMMEM, &staging, nullptr);
  if (FAILED(hr)) return hr;
  hr = m_state->d3d->GetRenderTargetData(src.surface.ptr(), staging.ptr());
  if (FAILED(hr)) return hr;
  D3DLOCKED_RECT from = {}, to = {};
  hr = staging->LockRect(&from, &sourceRect, D3DLOCK_READONLY);
  if (FAILED(hr)) return hr;
  hr = dst.surface->LockRect(&to, &destinationRect, 0);
  if (SUCCEEDED(hr)) {
    const UINT bytes = UINT(sourceRect.right - sourceRect.left) * 4;
    const UINT rows = UINT(sourceRect.bottom - sourceRect.top);
    copyRows(to.pBits, UINT(to.Pitch), from.pBits, UINT(from.Pitch), bytes, rows);
    if (dst.desc.systemData) {
      auto external = static_cast<uint8_t*>(dst.desc.systemData)
        + size_t(destinationRect.top) * dst.desc.systemPitch + size_t(destinationRect.left) * 4;
      copyRows(external, dst.desc.systemPitch, from.pBits, UINT(from.Pitch), bytes, rows);
    }
    hr = dst.surface->UnlockRect();
  }
  const HRESULT unlocked = staging->UnlockRect();
  return FAILED(hr) ? hr : unlocked;
}

static HRESULT d3d9Error(HRESULT hr) {
  switch (hr) {
    case DXGI_ERROR_UNSUPPORTED:
    case DXGI_ERROR_NOT_FOUND: return D3DERR_NOTAVAILABLE;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET: return D3DERR_DEVICELOST;
    default: return hr;
  }
}

HRESULT D3D9Backend::create(const AdapterLuid& luid, const RuntimeBackend* runtime,
                          std::unique_ptr<D3D9Backend>& result) noexcept {
  result.reset();
  if (luid == AdapterLuid{} || !runtime || !validRuntimeBackend(*runtime))
    return E_INVALIDARG;
  try {
    auto backend = std::make_unique<D3D9Backend>();
    auto& state = *backend->m_state;
    HRESULT hr = state.initialize(luid, DxvkInstanceFlag::ClientApiIsD3D9, runtime);
    if (FAILED(hr)) return d3d9Error(hr);
    state.parent = new D3D9InterfaceEx(state.instance, state.adapter);
    state.d3d = new D3D9DeviceEx(state.parent.ptr(), state.parent->GetAdapter(0),
      D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING
      | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, state.device);
    hr = state.d3d->InitializeNativeOffscreen();
    if (FAILED(hr)) return hr;
    result = std::move(backend);
    return S_OK;
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (const DxvkError& e) {
      Logger::err(str::format("VIOGPU UMD D3D9 backend: ", e.message()));
      return D3DERR_NOTAVAILABLE;
    } catch (...) { return E_FAIL; }
}

}

extern "C" HRESULT APIENTRY VioGpuDxvkProbeD3D9BackendForTest(const LUID* luid,
    const dxvk::umd::RuntimeBackend* runtime, UINT* swapchainCount) {
  if (!luid || !swapchainCount) return E_INVALIDARG;
  dxvk::umd::AdapterLuid requested;
  static_assert(sizeof(requested) == sizeof(*luid));
  std::memcpy(requested.data(), luid, requested.size());
  std::unique_ptr<dxvk::umd::D3D9Backend> backend;
  const HRESULT hr = dxvk::umd::D3D9Backend::create(requested, runtime, backend);
  if (FAILED(hr)) return hr;
  const UINT count = backend->device()->GetNumberOfSwapChains();
  backend.reset();
  *swapchainCount = count;
  return S_OK;
}
