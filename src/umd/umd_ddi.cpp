#include "umd_ddi.h"
#include "umd_api.h"
#include "umd_adapter.h"
#include "umd_shader.h"
#include "umd_query.h"
#include "umd_allocation.h"
#include "umd_map.h"
#include "umd_view.h"

#include <wrl/client.h>
#include <memory>
#include <new>
#include <vector>
#include <array>

namespace {
using Microsoft::WRL::ComPtr;
struct Shader;
struct InputLayout;
struct Device {
  std::shared_ptr<const dxvk::umd::AdapterIdentity> adapter;
  ComPtr<ID3D11Device> backend;
  ComPtr<ID3D11DeviceContext> context;
  D3D10DDI_HRTCORELAYER runtime;
  D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks;
  dxvk::umd::RuntimeMemory memory;
  bool vertexBound = false;
  bool pixelBound = false;
  bool targetBound = false;
  bool viewportBound = false;
  bool triangleList = false;
  bool indexBound = false;
  Shader* vertexShader = nullptr;
  Shader* pixelShader = nullptr;
  InputLayout* inputLayout = nullptr;
  void error(HRESULT hr) {
    if (FAILED(hr)) callbacks.pfnSetErrorCb(runtime, dxvk::umd::ddiResult(hr));
  }
};
struct Resource {
  Device* owner = nullptr;
  ComPtr<ID3D11Resource> backend;
  ComPtr<ID3D11Texture2D> presentReadback;
  dxvk::umd::RuntimeAllocation allocation;
};
struct RenderTarget {
  Device* owner = nullptr;
  ComPtr<ID3D11RenderTargetView> backend;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};
struct ShaderView {
  Device* owner = nullptr;
  ComPtr<ID3D11ShaderResourceView> backend;
};
struct Sampler {
  Device* owner = nullptr;
  ComPtr<ID3D11SamplerState> backend;
};
struct Shader {
  Device* owner = nullptr;
  dxvk::umd::ShaderStage stage = dxvk::umd::ShaderStage::Vertex;
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11PixelShader> pixel;
  std::vector<UINT> code;
  std::vector<dxvk::umd::ShaderSignatureEntry> inputs;
  std::vector<dxvk::umd::ShaderSignatureEntry> outputs;
  std::array<dxvk::umd::ShaderScalar,32> compiledInputTypes = {};
  std::array<dxvk::umd::ShaderScalar,32> compiledOutputTypes = {};
  bool needsLayout = false;
  bool needsLinkage = false;
};
struct InputLayout {
  Device* owner = nullptr;
  ComPtr<ID3D11InputLayout> backend;
  std::array<dxvk::umd::ShaderScalar,32> inputTypes = {};
};
struct Rasterizer {
  Device* owner = nullptr;
  ComPtr<ID3D11RasterizerState> backend;
};
struct BlendState {
  Device* owner = nullptr;
  ComPtr<ID3D11BlendState> backend;
};
struct DepthView {
  Device* owner = nullptr;
  ComPtr<ID3D11DepthStencilView> backend;
};
struct DepthState {
  Device* owner = nullptr;
  ComPtr<ID3D11DepthStencilState> backend;
};
struct Query {
  Device* owner = nullptr;
  ComPtr<ID3D11Query> backend;
  dxvk::umd::QueryInfo info;
  bool begun = false;
  bool issued = false;
};
Device* get(D3D10DDI_HDEVICE h) { return static_cast<Device*>(h.pDrvPrivate); }
Resource* get(D3D10DDI_HRESOURCE h) { return static_cast<Resource*>(h.pDrvPrivate); }
RenderTarget* get(D3D10DDI_HRENDERTARGETVIEW h) { return static_cast<RenderTarget*>(h.pDrvPrivate); }
ShaderView* get(D3D10DDI_HSHADERRESOURCEVIEW h) { return static_cast<ShaderView*>(h.pDrvPrivate); }
Sampler* get(D3D10DDI_HSAMPLER h) { return static_cast<Sampler*>(h.pDrvPrivate); }
Shader* get(D3D10DDI_HSHADER h) { return static_cast<Shader*>(h.pDrvPrivate); }
InputLayout* get(D3D10DDI_HELEMENTLAYOUT h) { return static_cast<InputLayout*>(h.pDrvPrivate); }
Rasterizer* get(D3D10DDI_HRASTERIZERSTATE h) { return static_cast<Rasterizer*>(h.pDrvPrivate); }
BlendState* get(D3D10DDI_HBLENDSTATE h) { return static_cast<BlendState*>(h.pDrvPrivate); }
DepthView* get(D3D10DDI_HDEPTHSTENCILVIEW h) { return static_cast<DepthView*>(h.pDrvPrivate); }
DepthState* get(D3D10DDI_HDEPTHSTENCILSTATE h) { return static_cast<DepthState*>(h.pDrvPrivate); }
Query* get(D3D10DDI_HQUERY h) { return static_cast<Query*>(h.pDrvPrivate); }

bool owned(Device* device, Query* query) {
  if (!query || query->owner != device || !query->backend) {
    device->error(E_INVALIDARG);
    return false;
  }
  return true;
}

SIZE_T APIENTRY querySize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATEQUERY*) { return sizeof(Query); }
void APIENTRY createQuery(D3D10DDI_HDEVICE h, const D3D10DDIARG_CREATEQUERY* args,
    D3D10DDI_HQUERY out, D3D10DDI_HRTQUERY) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto query = new (out.pDrvPrivate) Query();
  query->owner = device;
  if (!args || !dxvk::umd::queryInfo(args->Query, args->MiscFlags, query->info)) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_QUERY_DESC desc = {query->info.type, 0};
  try { device->error(device->backend->CreateQuery(&desc, &query->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyQuery(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object) {
  auto query = get(object);
  if (!query || query->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  query->~Query();
}
void APIENTRY beginQuery(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object) {
  auto device = get(h); auto query = get(object);
  if (!owned(device, query)) return;
  if (!query->info.beginRequired || query->begun) { device->error(E_INVALIDARG); return; }
  try {
    device->context->Begin(query->backend.Get());
    query->begun = true; query->issued = false;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY endQuery(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object) {
  auto device = get(h); auto query = get(object);
  if (!owned(device, query)) return;
  if (query->info.beginRequired && !query->begun) { device->error(E_INVALIDARG); return; }
  try {
    device->context->End(query->backend.Get());
    query->begun = false; query->issued = true;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY getQueryData(D3D10DDI_HDEVICE h, D3D10DDI_HQUERY object, void* data, UINT size, UINT flags) {
  auto device = get(h); auto query = get(object);
  if (!owned(device, query)) return;
  if (!query->issued) { device->error(E_INVALIDARG); return; }
  try {
    device->error(dxvk::umd::readQueryData(query->info, data, size, flags,
      [&](void* output, UINT outputSize, UINT apiFlags) {
        return device->context->GetData(query->backend.Get(), output, outputSize, apiFlags);
      }));
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

bool owned(Device* device, Resource* resource) {
  if (!resource || resource->owner != device || !resource->backend) {
    device->error(E_INVALIDARG);
    return false;
  }
  return true;
}
bool owned(Device* device, RenderTarget* target) {
  if (!target || target->owner != device || !target->backend) {
    device->error(E_INVALIDARG);
    return false;
  }
  return true;
}

SIZE_T APIENTRY resourceSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATERESOURCE*) {
  return sizeof(Resource);
}
void APIENTRY createResource(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATERESOURCE* args, D3D10DDI_HRESOURCE out,
    D3D10DDI_HRTRESOURCE runtime) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto resource = new (out.pDrvPrivate) Resource();
  resource->owner = device;
  if (!args || !args->pMipInfoList || !args->MipLevels || !args->ArraySize ||
      args->MipLevels > D3D11_REQ_MIP_LEVELS || args->ArraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION ||
      args->pPrimaryDesc ||
      args->MiscFlags || (args->MapFlags & ~D3D10_DDI_CPU_ACCESS_MASK) ||
      (args->BindFlags & ~(D3D10_DDI_BIND_PIPELINE_MASK | D3D10_DDI_BIND_PRESENT))) {
    device->error(E_INVALIDARG); return;
  }
  const bool presentable = (args->BindFlags & D3D10_DDI_BIND_PRESENT) != 0;
  if (presentable && (!device->memory.available() || !runtime.handle
      || args->ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D
      || args->MipLevels != 1 || args->ArraySize != 1
      || args->SampleDesc.Count != 1 || args->SampleDesc.Quality
      || args->Usage != D3D10_DDI_USAGE_DEFAULT || args->MapFlags
      || !(args->BindFlags & D3D10_DDI_BIND_RENDER_TARGET)
      || (args->Format != DXGI_FORMAT_R8G8B8A8_UNORM && args->Format != DXGI_FORMAT_B8G8R8A8_UNORM))) {
    device->error(DXGI_ERROR_UNSUPPORTED); return;
  }
  try {
    std::vector<D3D11_SUBRESOURCE_DATA> initial;
    if (args->pInitialDataUP) {
      initial.resize(size_t(args->MipLevels) * args->ArraySize);
      for (size_t i = 0; i < initial.size(); i++) {
        initial[i].pSysMem = args->pInitialDataUP[i].pSysMem;
        initial[i].SysMemPitch = args->pInitialDataUP[i].SysMemPitch;
        initial[i].SysMemSlicePitch = args->pInitialDataUP[i].SysMemSlicePitch;
      }
    }
    auto data = initial.empty() ? nullptr : initial.data();
    HRESULT hr = E_INVALIDARG;
    if (args->ResourceDimension == D3D10DDIRESOURCE_BUFFER && args->MipLevels == 1 && args->ArraySize == 1) {
      D3D11_BUFFER_DESC desc = {};
      desc.ByteWidth = args->pMipInfoList[0].TexelWidth;
      desc.Usage = static_cast<D3D11_USAGE>(args->Usage);
      desc.BindFlags = args->BindFlags & D3D10_DDI_BIND_PIPELINE_MASK;
      desc.CPUAccessFlags = ((args->MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
                         | ((args->MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
      ComPtr<ID3D11Buffer> buffer;
      hr = device->backend->CreateBuffer(&desc, data, &buffer);
      resource->backend = buffer;
    } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
      D3D11_TEXTURE2D_DESC desc = {};
      desc.Width = args->pMipInfoList[0].TexelWidth;
      desc.Height = args->pMipInfoList[0].TexelHeight;
      desc.MipLevels = args->MipLevels;
      desc.ArraySize = args->ArraySize;
      desc.Format = args->Format;
      desc.SampleDesc = args->SampleDesc;
      desc.Usage = static_cast<D3D11_USAGE>(args->Usage);
      desc.BindFlags = args->BindFlags & D3D10_DDI_BIND_PIPELINE_MASK;
      desc.CPUAccessFlags = ((args->MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
                         | ((args->MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
      ComPtr<ID3D11Texture2D> texture;
      hr = device->backend->CreateTexture2D(&desc, data, &texture);
      resource->backend = texture;
    }
    if (SUCCEEDED(hr) && presentable) {
      hr = device->memory.allocate(resource->allocation, runtime.handle,
        args->pMipInfoList[0].TexelWidth, args->pMipInfoList[0].TexelHeight, args->Format);
      if (FAILED(hr)) resource->backend.Reset();
    }
    device->error(hr);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  auto object = get(resource);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  get(h)->error(object->allocation.release());
  object->~Resource();
}

SIZE_T APIENTRY shaderViewSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATESHADERRESOURCEVIEW*) {
  return sizeof(ShaderView);
}
void APIENTRY createShaderView(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATESHADERRESOURCEVIEW* args,
    D3D10DDI_HSHADERRESOURCEVIEW out, D3D10DDI_HRTSHADERRESOURCEVIEW) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto view = new (out.pDrvPrivate) ShaderView(); view->owner = device;
  if (!args || args->ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D) {
    device->error(E_INVALIDARG); return;
  }
  if (!owned(device, get(args->hDrvResource))) return;
  ComPtr<ID3D11Texture2D> texture;
  if (FAILED(get(args->hDrvResource)->backend.As(&texture))) { device->error(E_INVALIDARG); return; }
  D3D11_TEXTURE2D_DESC resource = {}; texture->GetDesc(&resource);
  D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
  if (!dxvk::umd::textureShaderView(*args, resource, desc)) {
    device->error(E_INVALIDARG); return;
  }
  try { device->error(device->backend->CreateShaderResourceView(texture.Get(), &desc, &view->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyShaderView(D3D10DDI_HDEVICE h, D3D10DDI_HSHADERRESOURCEVIEW object) {
  auto view = get(object);
  if (!view || view->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  view->~ShaderView();
}
template<bool Vertex>
void APIENTRY setShaderResources(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D10DDI_HSHADERRESOURCEVIEW* objects) {
  auto device = get(h);
  constexpr UINT slots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && !objects)) { device->error(E_INVALIDARG); return; }
  ID3D11ShaderResourceView* views[slots] = {};
  for (UINT i = 0; i < count; i++) {
    auto view = get(objects[i]);
    if (view && (view->owner != device || !view->backend)) { device->error(E_INVALIDARG); return; }
    views[i] = view ? view->backend.Get() : nullptr;
  }
  try {
    if (Vertex) device->context->VSSetShaderResources(start, count, views);
    else device->context->PSSetShaderResources(start, count, views);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY samplerSize(D3D10DDI_HDEVICE, const D3D10_DDI_SAMPLER_DESC*) { return sizeof(Sampler); }
void APIENTRY createSampler(D3D10DDI_HDEVICE h, const D3D10_DDI_SAMPLER_DESC* args,
    D3D10DDI_HSAMPLER out, D3D10DDI_HRTSAMPLER) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto sampler = new (out.pDrvPrivate) Sampler(); sampler->owner = device;
  if (!args) { device->error(E_INVALIDARG); return; }
  D3D11_SAMPLER_DESC desc = {};
  desc.Filter = static_cast<D3D11_FILTER>(args->Filter);
  desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(args->AddressU);
  desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(args->AddressV);
  desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(args->AddressW);
  desc.MipLODBias = args->MipLODBias; desc.MaxAnisotropy = args->MaxAnisotropy;
  desc.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(args->ComparisonFunc);
  for (unsigned i = 0; i < 4; i++) desc.BorderColor[i] = args->BorderColor[i];
  desc.MinLOD = args->MinLOD; desc.MaxLOD = args->MaxLOD;
  try { device->error(device->backend->CreateSamplerState(&desc, &sampler->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroySampler(D3D10DDI_HDEVICE h, D3D10DDI_HSAMPLER object) {
  auto sampler = get(object);
  if (!sampler || sampler->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  sampler->~Sampler();
}
template<bool Vertex>
void APIENTRY setSamplers(D3D10DDI_HDEVICE h, UINT start, UINT count, const D3D10DDI_HSAMPLER* objects) {
  auto device = get(h);
  constexpr UINT slots = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && !objects)) { device->error(E_INVALIDARG); return; }
  ID3D11SamplerState* samplers[slots] = {};
  for (UINT i = 0; i < count; i++) {
    auto sampler = get(objects[i]);
    if (sampler && (sampler->owner != device || !sampler->backend)) { device->error(E_INVALIDARG); return; }
    samplers[i] = sampler ? sampler->backend.Get() : nullptr;
  }
  try {
    if (Vertex) device->context->VSSetSamplers(start, count, samplers);
    else device->context->PSSetSamplers(start, count, samplers);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY targetSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATERENDERTARGETVIEW*) {
  return sizeof(RenderTarget);
}
void APIENTRY createTarget(D3D10DDI_HDEVICE h,
    const D3D10DDIARG_CREATERENDERTARGETVIEW* args,
    D3D10DDI_HRENDERTARGETVIEW out, D3D10DDI_HRTRENDERTARGETVIEW) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto target = new (out.pDrvPrivate) RenderTarget();
  target->owner = device;
  if (!args || args->ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D ||
      !owned(device, get(args->hDrvResource))) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_RENDER_TARGET_VIEW_DESC desc = {};
  ComPtr<ID3D11Texture2D> texture;
  if (FAILED(get(args->hDrvResource)->backend.As(&texture))) { device->error(E_INVALIDARG); return; }
  D3D11_TEXTURE2D_DESC resource = {}; texture->GetDesc(&resource);
  if (!dxvk::umd::textureTargetView(*args, resource, desc)) { device->error(E_INVALIDARG); return; }
  target->format = desc.Format;
  try { device->error(device->backend->CreateRenderTargetView(texture.Get(), &desc, &target->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyTarget(D3D10DDI_HDEVICE h, D3D10DDI_HRENDERTARGETVIEW target) {
  auto object = get(target);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  object->~RenderTarget();
}
void APIENTRY clearTarget(D3D10DDI_HDEVICE h, D3D10DDI_HRENDERTARGETVIEW target, FLOAT color[4]) {
  auto device = get(h);
  if (owned(device, get(target))) device->context->ClearRenderTargetView(get(target)->backend.Get(), color);
}
SIZE_T APIENTRY depthViewSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATEDEPTHSTENCILVIEW*) {
  return sizeof(DepthView);
}
void APIENTRY createDepthView(D3D10DDI_HDEVICE h, const D3D10DDIARG_CREATEDEPTHSTENCILVIEW* args,
    D3D10DDI_HDEPTHSTENCILVIEW out, D3D10DDI_HRTDEPTHSTENCILVIEW) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto view = new (out.pDrvPrivate) DepthView(); view->owner = device;
  if (!args || args->ResourceDimension != D3D10DDIRESOURCE_TEXTURE2D
      || !owned(device, get(args->hDrvResource))) { device->error(E_INVALIDARG); return; }
  ComPtr<ID3D11Texture2D> texture;
  if (FAILED(get(args->hDrvResource)->backend.As(&texture))) { device->error(E_INVALIDARG); return; }
  D3D11_TEXTURE2D_DESC resource = {}; texture->GetDesc(&resource);
  if (!(resource.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      || args->Tex2D.MipSlice >= resource.MipLevels || !args->Tex2D.ArraySize
      || args->Tex2D.FirstArraySlice >= resource.ArraySize
      || args->Tex2D.ArraySize > resource.ArraySize - args->Tex2D.FirstArraySlice) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_DEPTH_STENCIL_VIEW_DESC desc = {}; desc.Format = args->Format;
  if (resource.SampleDesc.Count > 1) {
    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
    desc.Texture2DMSArray.FirstArraySlice = args->Tex2D.FirstArraySlice;
    desc.Texture2DMSArray.ArraySize = args->Tex2D.ArraySize;
  } else {
    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray.MipSlice = args->Tex2D.MipSlice;
    desc.Texture2DArray.FirstArraySlice = args->Tex2D.FirstArraySlice;
    desc.Texture2DArray.ArraySize = args->Tex2D.ArraySize;
  }
  try { device->error(device->backend->CreateDepthStencilView(texture.Get(), &desc, &view->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyDepthView(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILVIEW object) {
  auto view = get(object);
  if (!view || view->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  view->~DepthView();
}
void APIENTRY clearDepthView(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILVIEW object,
    UINT flags, FLOAT depth, UINT8 stencil) {
  auto device = get(h); auto view = get(object);
  if (!view || view->owner != device || !view->backend
      || (flags & ~(D3D10_DDI_CLEAR_DEPTH | D3D10_DDI_CLEAR_STENCIL))) {
    device->error(E_INVALIDARG); return;
  }
  const UINT apiFlags = ((flags & D3D10_DDI_CLEAR_DEPTH) ? D3D11_CLEAR_DEPTH : 0)
                      | ((flags & D3D10_DDI_CLEAR_STENCIL) ? D3D11_CLEAR_STENCIL : 0);
  try { if (apiFlags) device->context->ClearDepthStencilView(view->backend.Get(), apiFlags, depth, stencil); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY copyResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, D3D10DDI_HRESOURCE src) {
  auto device = get(h);
  if (owned(device, get(dst)) && owned(device, get(src)))
    device->context->CopyResource(get(dst)->backend.Get(), get(src)->backend.Get());
}

void APIENTRY resolveResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT dstIndex,
    D3D10DDI_HRESOURCE src, UINT srcIndex, DXGI_FORMAT format) {
  auto device = get(h);
  if (!owned(device, get(dst)) || !owned(device, get(src))) return;
  ComPtr<ID3D11Texture2D> destination, source;
  if (FAILED(get(dst)->backend.As(&destination)) || FAILED(get(src)->backend.As(&source))) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_TEXTURE2D_DESC dstDesc = {}, srcDesc = {};
  destination->GetDesc(&dstDesc); source->GetDesc(&srcDesc);
  if (!dxvk::umd::resolveSubresources(dstDesc, dstIndex, srcDesc, srcIndex, format)) {
    device->error(E_INVALIDARG); return;
  }
  try {
    UINT support = 0;
    const HRESULT hr = device->backend->CheckFormatSupport(format, &support);
    if (FAILED(hr)) { device->error(hr); return; }
    if (!(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
      device->error(DXGI_ERROR_UNSUPPORTED); return;
    }
    device->context->ResolveSubresource(destination.Get(), dstIndex, source.Get(), srcIndex, format);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

void APIENTRY checkFormat(D3D10DDI_HDEVICE h, DXGI_FORMAT format, UINT* output) {
  auto device = get(h);
  if (!output) { device->error(E_INVALIDARG); return; }
  *output = 0;
  try {
    UINT support = 0;
    const HRESULT hr = device->backend->CheckFormatSupport(format, &support);
    if (FAILED(hr)) { device->error(hr == E_INVALIDARG ? E_FAIL : hr); return; }
    *output = dxvk::umd::nativeFormatCaps(support);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}

void APIENTRY checkMultisample(D3D10DDI_HDEVICE h, DXGI_FORMAT format, UINT count, UINT* output) {
  auto device = get(h);
  if (!output) { device->error(E_INVALIDARG); return; }
  *output = 0;
  if (!count || count > D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT) return;
  try { device->error(device->backend->CheckMultisampleQualityLevels(format, count, output)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}

struct SubresourceInfo {
  UINT width = 0, height = 1;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
  UINT bindings = 0;
};
bool subresourceInfo(Resource* resource, UINT index, SubresourceInfo& info) {
  resource->backend->GetType(&info.dimension);
  if (info.dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
    if (index) return false;
    ComPtr<ID3D11Buffer> buffer;
    if (FAILED(resource->backend.As(&buffer))) return false;
    D3D11_BUFFER_DESC desc = {}; buffer->GetDesc(&desc);
    info.width = desc.ByteWidth; info.usage = desc.Usage; info.bindings = desc.BindFlags;
    return true;
  }
  if (info.dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(resource->backend.As(&texture))) return false;
    D3D11_TEXTURE2D_DESC desc = {}; texture->GetDesc(&desc);
    if (!desc.MipLevels || index / desc.MipLevels >= desc.ArraySize || desc.SampleDesc.Count != 1)
      return false;
    const UINT mip = index % desc.MipLevels;
    info.width = desc.Width >> mip; if (!info.width) info.width = 1;
    info.height = desc.Height >> mip; if (!info.height) info.height = 1;
    info.format = desc.Format; info.usage = desc.Usage; info.bindings = desc.BindFlags;
    return info.format == DXGI_FORMAT_R8G8B8A8_UNORM || info.format == DXGI_FORMAT_B8G8R8A8_UNORM;
  }
  return false;
}
bool subresourceBox(const SubresourceInfo& info, const D3D10_DDI_BOX* input, D3D11_BOX& box) {
  if (input && (input->left < 0 || input->top < 0 || input->front < 0 ||
      input->right < 0 || input->bottom < 0 || input->back < 0)) return false;
  box = input ? D3D11_BOX{UINT(input->left), UINT(input->top), UINT(input->front),
                         UINT(input->right), UINT(input->bottom), UINT(input->back)}
              : D3D11_BOX{0, 0, 0, info.width, info.height, 1};
  return box.left <= box.right && box.top <= box.bottom && box.front <= box.back
      && box.right <= info.width && box.bottom <= info.height && box.back <= 1;
}
void APIENTRY copyRegion(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT dstIndex,
    UINT x, UINT y, UINT z, D3D10DDI_HRESOURCE src, UINT srcIndex, const D3D10_DDI_BOX* input) {
  auto device = get(h);
  if (!owned(device, get(dst)) || !owned(device, get(src))) return;
  SubresourceInfo source, destination; D3D11_BOX box;
  if (!subresourceInfo(get(src), srcIndex, source) || !subresourceInfo(get(dst), dstIndex, destination)
      || source.dimension != destination.dimension || source.format != destination.format
      || destination.usage == D3D11_USAGE_IMMUTABLE || !subresourceBox(source, input, box)) {
    device->error(E_INVALIDARG); return;
  }
  if (box.left == box.right || box.top == box.bottom || box.front == box.back) return;
  if ((get(src)->backend.Get() == get(dst)->backend.Get() && srcIndex == dstIndex)
      || x > destination.width || box.right - box.left > destination.width - x
      || y > destination.height || box.bottom - box.top > destination.height - y || z) {
    device->error(E_INVALIDARG); return;
  }
  try {
    device->context->CopySubresourceRegion(get(dst)->backend.Get(), dstIndex, x, y, z,
      get(src)->backend.Get(), srcIndex, input ? &box : nullptr);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY updateResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT index,
    const D3D10_DDI_BOX* input, const void* source, UINT rowPitch, UINT depthPitch) {
  auto device = get(h);
  if (!owned(device, get(dst))) return;
  SubresourceInfo destination; D3D11_BOX box;
  if (!subresourceInfo(get(dst), index, destination) || destination.usage != D3D11_USAGE_DEFAULT
      || !subresourceBox(destination, input, box)
      || (input && (destination.bindings & D3D11_BIND_CONSTANT_BUFFER))) {
    device->error(E_INVALIDARG); return;
  }
  if (box.left == box.right || box.top == box.bottom || box.front == box.back) return;
  if (!source || (destination.dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D
      && rowPitch < (box.right - box.left) * 4)) { device->error(E_INVALIDARG); return; }
  try {
    device->context->UpdateSubresource(get(dst)->backend.Get(), index, input ? &box : nullptr,
      source, rowPitch, depthPitch);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY mapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource,
    UINT subresource, D3D10_DDI_MAP type, UINT flags, D3D10DDI_MAPPED_SUBRESOURCE* out) {
  auto device = get(h);
  if (!out) { device->error(E_INVALIDARG); return; }
  *out = {};
  if (!owned(device, get(resource))) return;
  try {
    device->error(dxvk::umd::mapSubresource(type, flags, out,
      [&](D3D11_MAP apiType, UINT apiFlags, D3D11_MAPPED_SUBRESOURCE* mapped) {
        return device->context->Map(get(resource)->backend.Get(), subresource, apiType, apiFlags, mapped);
      }));
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
BOOL APIENTRY isStagingBusy(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return TRUE;
  BOOL busy = TRUE;
  const HRESULT removed = device->backend->GetDeviceRemovedReason();
  if (FAILED(removed)) { device->error(removed); return TRUE; }
  device->error(dxvk::umd::isStagingResourceBusy(device->context.Get(),
    get(resource)->backend.Get(), &busy));
  return busy;
}
void APIENTRY resourceHazard(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return;
  D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
  get(resource)->backend->GetType(&dimension);
  if (dimension != D3D11_RESOURCE_DIMENSION_BUFFER) {
    device->error(E_INVALIDARG); return;
  }
  // DXVK tracks Vulkan access transitions at the ensuing buffer bind/use.
  // This notification does not require a CPU/GPU synchronization operation.
}
void APIENTRY shaderViewHazard(D3D10DDI_HDEVICE h, D3D10DDI_HSHADERRESOURCEVIEW object,
    D3D10DDI_HRESOURCE resource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return;
  auto view = get(object);
  if (!view || view->owner != device || !view->backend) {
    device->error(E_INVALIDARG); return;
  }
  ComPtr<ID3D11Resource> viewed;
  view->backend->GetResource(&viewed);
  if (viewed.Get() != get(resource)->backend.Get()) {
    device->error(E_INVALIDARG); return;
  }
  // The embedded view retains its exact subresource range. DXVK's resource
  // tracking inserts the necessary Vulkan barriers when that view is used.
}
void APIENTRY unmapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT subresource) {
  auto device = get(h);
  if (!owned(device, get(resource))) return;
  try { device->context->Unmap(get(resource)->backend.Get(), subresource); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY shaderSize(D3D10DDI_HDEVICE, const UINT*, const D3D10DDIARG_STAGE_IO_SIGNATURES*) {
  return sizeof(Shader);
}
void createShader(D3D10DDI_HDEVICE h, const UINT* code, D3D10DDI_HSHADER out,
    const D3D10DDIARG_STAGE_IO_SIGNATURES* signature, dxvk::umd::ShaderStage stage) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto shader = new (out.pDrvPrivate) Shader();
  shader->owner = device;
  shader->stage = stage;
  if (!code || !signature || signature->NumInputSignatureEntries > 32 ||
      !signature->NumOutputSignatureEntries || signature->NumOutputSignatureEntries > 32 || !signature->pOutputSignature ||
      (signature->NumInputSignatureEntries && !signature->pInputSignature)) {
    device->error(E_INVALIDARG); return;
  }
  static_assert(D3D10_SB_NAME_POSITION == 1 && D3D10_SB_NAME_VERTEX_ID == 6);
  try {
    Shader candidate; candidate.owner = device; candidate.stage = stage;
    for (UINT i = 0; i < signature->NumInputSignatureEntries; i++) {
      const auto& entry = signature->pInputSignature[i];
      candidate.inputs.push_back({uint32_t(entry.SystemValue), entry.Register, entry.Mask});
      candidate.needsLayout |= stage == dxvk::umd::ShaderStage::Vertex && entry.SystemValue == D3D10_SB_NAME_UNDEFINED;
    }
    for (UINT i = 0; i < signature->NumOutputSignatureEntries; i++) {
      const auto& entry = signature->pOutputSignature[i];
      candidate.outputs.push_back({uint32_t(entry.SystemValue), entry.Register, entry.Mask});
      candidate.needsLinkage |= stage == dxvk::umd::ShaderStage::Vertex && entry.SystemValue == D3D10_SB_NAME_UNDEFINED;
    }
    if (stage == dxvk::umd::ShaderStage::Pixel) {
      std::vector<dxvk::umd::ShaderSignatureEntry> resolved;
      if (!dxvk::umd::resolvePixelInputs(code, code[1], candidate.inputs.data(), candidate.inputs.size(), resolved)) {
        device->error(E_INVALIDARG); return;
      }
      candidate.inputs = std::move(resolved);
    }
    auto validationInputs = candidate.inputs;
    auto validationOutputs = candidate.outputs;
    // Validate raw tokens and register structure now. These provisional
    // signature types are discarded and never enter the DXVK compiler.
    // The bound layout supplies actual types when the shader is first drawn.
    if (stage == dxvk::umd::ShaderStage::Vertex) {
      for (auto& input : validationInputs)
        if (!input.systemValue) input.scalar = dxvk::umd::ShaderScalar::Float32;
      for (auto& output : validationOutputs)
        if (!output.systemValue) output.scalar = dxvk::umd::ShaderScalar::Uint32;
    }
    std::vector<unsigned char> bytecode;
    if (!dxvk::umd::buildShaderContainer(stage, code, code[1], validationInputs.data(),
        validationInputs.size(), validationOutputs.data(), validationOutputs.size(), bytecode)) {
      device->error(E_INVALIDARG); return;
    }
    HRESULT hr = S_OK;
    if (candidate.needsLayout || candidate.needsLinkage) candidate.code.assign(code, code + code[1]);
    else if (stage == dxvk::umd::ShaderStage::Vertex)
      hr = device->backend->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &candidate.vertex);
    else hr = device->backend->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &candidate.pixel);
    if (FAILED(hr)) { device->error(hr); return; }
    *shader = std::move(candidate);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY createVertexShader(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER out, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* sig) {
  createShader(h, code, out, sig, dxvk::umd::ShaderStage::Vertex);
}
void APIENTRY createPixelShader(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER out, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* sig) {
  createShader(h, code, out, sig, dxvk::umd::ShaderStage::Pixel);
}
void APIENTRY destroyShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto object = get(shader);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  if (get(h)->vertexShader == object) {
    get(h)->context->VSSetShader(nullptr, nullptr, 0);
    get(h)->vertexShader = nullptr; get(h)->vertexBound = false;
  }
  if (get(h)->pixelShader == object) {
    get(h)->context->PSSetShader(nullptr, nullptr, 0);
    get(h)->pixelShader = nullptr; get(h)->pixelBound = false;
  }
  object->~Shader();
}
void APIENTRY setVertexShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h);
  auto object = get(shader);
  if (object && (object->owner != device || object->stage != dxvk::umd::ShaderStage::Vertex
      || (!object->vertex && !object->needsLayout && !object->needsLinkage))) { device->error(E_INVALIDARG); return; }
  device->context->VSSetShader(object ? object->vertex.Get() : nullptr, nullptr, 0);
  device->vertexShader = object;
  device->vertexBound = object != nullptr;
}
void APIENTRY setPixelShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h);
  auto object = get(shader);
  if (object && (object->owner != device || !object->pixel)) { device->error(E_INVALIDARG); return; }
  device->context->PSSetShader(object ? object->pixel.Get() : nullptr, nullptr, 0);
  device->pixelShader = object;
  device->pixelBound = object != nullptr;
}
template<bool Vertex>
void APIENTRY setConstantBuffers(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D10DDI_HRESOURCE* resources) {
  auto device = get(h);
  constexpr UINT slots = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && !resources)) {
    device->error(E_INVALIDARG); return;
  }
  ComPtr<ID3D11Buffer> ownedBuffers[slots];
  ID3D11Buffer* buffers[slots] = {};
  for (UINT i = 0; i < count; i++) {
    if (!resources[i].pDrvPrivate) continue;
    auto resource = get(resources[i]);
    if (!owned(device, resource)) return;
    if (FAILED(resource->backend.As(&ownedBuffers[i]))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc = {}; ownedBuffers[i]->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)) { device->error(E_INVALIDARG); return; }
    buffers[i] = ownedBuffers[i].Get();
  }
  // Validate every resource before changing state, so an invalid tail cannot
  // leave a partially updated binding range. Null entries explicitly unbind.
  try {
    if (Vertex) device->context->VSSetConstantBuffers(start, count, buffers);
    else device->context->PSSetConstantBuffers(start, count, buffers);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY setRenderTargets(D3D10DDI_HDEVICE h, const D3D10DDI_HRENDERTARGETVIEW* targets,
    UINT count, UINT clear, D3D10DDI_HDEPTHSTENCILVIEW depth) {
  auto device = get(h);
  if (count > 1 || clear > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT - count ||
      (count && !targets)) { device->error(E_INVALIDARG); return; }
  auto depthView = get(depth);
  if (depthView && (depthView->owner != device || !depthView->backend)) { device->error(E_INVALIDARG); return; }
  ID3D11RenderTargetView* target = nullptr;
  if (count && targets[0].pDrvPrivate) {
    auto object = get(targets[0]);
    if (!owned(device, object)) return;
    // The initial PS profile has float output. Integer targets need typed
    // output variants and are intentionally outside this development slice.
    if (object->format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        object->format != DXGI_FORMAT_B8G8R8A8_UNORM) { device->error(E_INVALIDARG); return; }
    target = object->backend.Get();
  }
  // ClearSlots is an optimization aid. A zero-color-target call must still
  // bind/unbind the depth view atomically and clear all color targets.
  try {
    device->context->OMSetRenderTargets(count, count ? &target : nullptr,
      depthView ? depthView->backend.Get() : nullptr);
    device->targetBound = target != nullptr;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY setViewports(D3D10DDI_HDEVICE h, UINT count, UINT clear, const D3D10_DDI_VIEWPORT* views) {
  auto device = get(h);
  if (count > 1 || clear > D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE - count ||
      (count && !views)) { device->error(E_INVALIDARG); return; }
  D3D11_VIEWPORT viewport = {};
  if (count) viewport = {views->TopLeftX, views->TopLeftY, views->Width, views->Height, views->MinDepth, views->MaxDepth};
  if (count || clear) {
    device->context->RSSetViewports(count, count ? &viewport : nullptr);
    device->viewportBound = count != 0;
  }
}
void APIENTRY setScissors(D3D10DDI_HDEVICE h, UINT count, UINT clear, const D3D10_DDI_RECT* input) {
  auto device = get(h);
  constexpr UINT slots = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  if (count > slots || clear > slots - count || (count && !input)) { device->error(E_INVALIDARG); return; }
  D3D11_RECT rects[slots] = {};
  for (UINT i = 0; i < count; i++)
    rects[i] = {input[i].left, input[i].top, input[i].right, input[i].bottom};
  try { if (count || clear) device->context->RSSetScissorRects(count, rects); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY rasterizerSize(D3D10DDI_HDEVICE, const D3D10_DDI_RASTERIZER_DESC*) { return sizeof(Rasterizer); }
void APIENTRY createRasterizer(D3D10DDI_HDEVICE h, const D3D10_DDI_RASTERIZER_DESC* args,
    D3D10DDI_HRASTERIZERSTATE out, D3D10DDI_HRTRASTERIZERSTATE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto object = new (out.pDrvPrivate) Rasterizer();
  object->owner = device;
  if (!args) { device->error(E_INVALIDARG); return; }
  D3D11_RASTERIZER_DESC desc = {};
  desc.FillMode = static_cast<D3D11_FILL_MODE>(args->FillMode);
  desc.CullMode = static_cast<D3D11_CULL_MODE>(args->CullMode);
  desc.FrontCounterClockwise = args->FrontCounterClockwise;
  desc.DepthBias = args->DepthBias; desc.DepthBiasClamp = args->DepthBiasClamp;
  desc.SlopeScaledDepthBias = args->SlopeScaledDepthBias;
  desc.DepthClipEnable = args->DepthClipEnable; desc.ScissorEnable = args->ScissorEnable;
  desc.MultisampleEnable = args->MultisampleEnable; desc.AntialiasedLineEnable = args->AntialiasedLineEnable;
  device->error(device->backend->CreateRasterizerState(&desc, &object->backend));
}
void APIENTRY destroyRasterizer(D3D10DDI_HDEVICE h, D3D10DDI_HRASTERIZERSTATE state) {
  auto object = get(state);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  object->~Rasterizer();
}
void APIENTRY setRasterizer(D3D10DDI_HDEVICE h, D3D10DDI_HRASTERIZERSTATE state) {
  auto device = get(h);
  auto object = get(state);
  if (object && (object->owner != device || !object->backend)) { device->error(E_INVALIDARG); return; }
  device->context->RSSetState(object ? object->backend.Get() : nullptr);
}
void APIENTRY setTopology(D3D10DDI_HDEVICE h, D3D10_DDI_PRIMITIVE_TOPOLOGY topology) {
  auto device = get(h);
  if (topology != D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST) { device->error(E_INVALIDARG); return; }
  device->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  device->triangleList = true;
}
SIZE_T APIENTRY blendSize(D3D10DDI_HDEVICE, const D3D10_DDI_BLEND_DESC*) { return sizeof(BlendState); }
void APIENTRY createBlend(D3D10DDI_HDEVICE h, const D3D10_DDI_BLEND_DESC* args,
    D3D10DDI_HBLENDSTATE out, D3D10DDI_HRTBLENDSTATE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto state = new (out.pDrvPrivate) BlendState(); state->owner = device;
  if (!args) { device->error(E_INVALIDARG); return; }
  D3D11_BLEND_DESC desc = {};
  desc.AlphaToCoverageEnable = args->AlphaToCoverageEnable;
  // Match DXVK's D3D10Device translation: enables and masks are per target,
  // while factors/equations are shared by the D3D10.0 descriptor.
  desc.IndependentBlendEnable = TRUE;
  for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
    auto& target = desc.RenderTarget[i];
    target.BlendEnable = args->BlendEnable[i];
    target.RenderTargetWriteMask = args->RenderTargetWriteMask[i];
    target.SrcBlend = static_cast<D3D11_BLEND>(args->SrcBlend);
    target.DestBlend = static_cast<D3D11_BLEND>(args->DestBlend);
    target.BlendOp = static_cast<D3D11_BLEND_OP>(args->BlendOp);
    target.SrcBlendAlpha = static_cast<D3D11_BLEND>(args->SrcBlendAlpha);
    target.DestBlendAlpha = static_cast<D3D11_BLEND>(args->DestBlendAlpha);
    target.BlendOpAlpha = static_cast<D3D11_BLEND_OP>(args->BlendOpAlpha);
  }
  try { device->error(device->backend->CreateBlendState(&desc, &state->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyBlend(D3D10DDI_HDEVICE h, D3D10DDI_HBLENDSTATE object) {
  auto state = get(object);
  if (!state || state->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  state->~BlendState();
}
void APIENTRY setBlend(D3D10DDI_HDEVICE h, D3D10DDI_HBLENDSTATE object, const FLOAT factor[4], UINT sampleMask) {
  auto device = get(h); auto state = get(object);
  if (state && (state->owner != device || !state->backend)) { device->error(E_INVALIDARG); return; }
  try { device->context->OMSetBlendState(state ? state->backend.Get() : nullptr, factor, sampleMask); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
SIZE_T APIENTRY depthStateSize(D3D10DDI_HDEVICE, const D3D10_DDI_DEPTH_STENCIL_DESC*) { return sizeof(DepthState); }
void APIENTRY createDepthState(D3D10DDI_HDEVICE h, const D3D10_DDI_DEPTH_STENCIL_DESC* args,
    D3D10DDI_HDEPTHSTENCILSTATE out, D3D10DDI_HRTDEPTHSTENCILSTATE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto state = new (out.pDrvPrivate) DepthState(); state->owner = device;
  if (!args || (args->StencilEnable && !args->FrontEnable && !args->BackEnable)) {
    device->error(E_INVALIDARG); return;
  }
  D3D11_DEPTH_STENCIL_DESC desc = {};
  desc.DepthEnable = args->DepthEnable;
  desc.DepthWriteMask = static_cast<D3D11_DEPTH_WRITE_MASK>(args->DepthWriteMask);
  desc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(args->DepthFunc);
  desc.StencilEnable = args->StencilEnable;
  desc.StencilReadMask = args->StencilReadMask; desc.StencilWriteMask = args->StencilWriteMask;
  auto face = [](const D3D10_DDI_DEPTH_STENCILOP_DESC& input, bool enabled) {
    // The native DDI has per-face enables, unlike the public D3D11 API.
    // A disabled face must not compare or modify stencil contents.
    if (!enabled) return D3D11_DEPTH_STENCILOP_DESC {
      D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS};
    return D3D11_DEPTH_STENCILOP_DESC {
      static_cast<D3D11_STENCIL_OP>(input.StencilFailOp),
      static_cast<D3D11_STENCIL_OP>(input.StencilDepthFailOp),
      static_cast<D3D11_STENCIL_OP>(input.StencilPassOp),
      static_cast<D3D11_COMPARISON_FUNC>(input.StencilFunc)};
  };
  desc.FrontFace = face(args->FrontFace, args->StencilEnable && args->FrontEnable);
  desc.BackFace = face(args->BackFace, args->StencilEnable && args->BackEnable);
  try { device->error(device->backend->CreateDepthStencilState(&desc, &state->backend)); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyDepthState(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILSTATE object) {
  auto state = get(object);
  if (!state || state->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  state->~DepthState();
}
void APIENTRY setDepthState(D3D10DDI_HDEVICE h, D3D10DDI_HDEPTHSTENCILSTATE object, UINT stencil) {
  auto device = get(h); auto state = get(object);
  if (state && (state->owner != device || !state->backend)) { device->error(E_INVALIDARG); return; }
  try { device->context->OMSetDepthStencilState(state ? state->backend.Get() : nullptr, stencil); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
bool inputFormat(DXGI_FORMAT format, dxvk::umd::ShaderScalar& scalar, uint8_t& mask) {
  using Scalar = dxvk::umd::ShaderScalar;
  switch (format) {
    case DXGI_FORMAT_R32_FLOAT: scalar = Scalar::Float32; mask = 1; return true;
    case DXGI_FORMAT_R32G32_FLOAT: scalar = Scalar::Float32; mask = 3; return true;
    case DXGI_FORMAT_R32G32B32_FLOAT: scalar = Scalar::Float32; mask = 7; return true;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: scalar = Scalar::Float32; mask = 15; return true;
    case DXGI_FORMAT_R32_UINT: scalar = Scalar::Uint32; mask = 1; return true;
    case DXGI_FORMAT_R32G32_UINT: scalar = Scalar::Uint32; mask = 3; return true;
    case DXGI_FORMAT_R32G32B32_UINT: scalar = Scalar::Uint32; mask = 7; return true;
    case DXGI_FORMAT_R32G32B32A32_UINT: scalar = Scalar::Uint32; mask = 15; return true;
    case DXGI_FORMAT_R32_SINT: scalar = Scalar::Sint32; mask = 1; return true;
    case DXGI_FORMAT_R32G32_SINT: scalar = Scalar::Sint32; mask = 3; return true;
    case DXGI_FORMAT_R32G32B32_SINT: scalar = Scalar::Sint32; mask = 7; return true;
    case DXGI_FORMAT_R32G32B32A32_SINT: scalar = Scalar::Sint32; mask = 15; return true;
    default: return false;
  }
}
SIZE_T APIENTRY layoutSize(D3D10DDI_HDEVICE, const D3D10DDIARG_CREATEELEMENTLAYOUT*) { return sizeof(InputLayout); }
void APIENTRY createLayout(D3D10DDI_HDEVICE h, const D3D10DDIARG_CREATEELEMENTLAYOUT* args,
    D3D10DDI_HELEMENTLAYOUT out, D3D10DDI_HRTELEMENTLAYOUT) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto layout = new (out.pDrvPrivate) InputLayout(); layout->owner = device;
  if (!args || args->NumElements > 32 || (args->NumElements && !args->pVertexElements)) {
    device->error(E_INVALIDARG); return;
  }
  try {
    InputLayout candidate; candidate.owner = device;
    D3D11_INPUT_ELEMENT_DESC elements[32] = {};
    dxvk::umd::ShaderSignatureEntry inputs[32] = {};
    for (UINT i = 0; i < args->NumElements; i++) {
      const auto& input = args->pVertexElements[i];
      if (input.InputRegister >= 32 || input.InputSlot >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT
          || input.AlignedByteOffset > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES
          || input.AlignedByteOffset % 4
          || (input.InputSlotClass != D3D10_DDI_INPUT_PER_VERTEX_DATA && input.InputSlotClass != D3D10_DDI_INPUT_PER_INSTANCE_DATA)
          || (input.InputSlotClass == D3D10_DDI_INPUT_PER_VERTEX_DATA && input.InstanceDataStepRate)
          || candidate.inputTypes[input.InputRegister] != dxvk::umd::ShaderScalar::Unknown
          || !inputFormat(input.Format, inputs[i].scalar, inputs[i].mask)) {
        device->error(E_INVALIDARG); return;
      }
      for (UINT j = 0; j < i; j++)
        if (elements[j].InputSlot == input.InputSlot
            && (elements[j].InputSlotClass != static_cast<D3D11_INPUT_CLASSIFICATION>(input.InputSlotClass)
                || elements[j].InstanceDataStepRate != input.InstanceDataStepRate)) {
          device->error(E_INVALIDARG); return;
        }
      inputs[i].registerIndex = input.InputRegister;
      candidate.inputTypes[input.InputRegister] = inputs[i].scalar;
      elements[i] = {dxvk::umd::inputRegisterSemantic, input.InputRegister, input.Format,
        input.InputSlot, input.AlignedByteOffset, static_cast<D3D11_INPUT_CLASSIFICATION>(input.InputSlotClass),
        input.InstanceDataStepRate};
    }
    // CreateInputLayout only consumes the signature. The minimal code chunk
    // is never executed; no synthetic shader is substituted for the app VS.
    const uint32_t code[] = {0x10040,3,0x0100003e};
    dxvk::umd::ShaderSignatureEntry output = {1,0,15};
    std::vector<unsigned char> binary;
    if (!dxvk::umd::buildShaderContainer(dxvk::umd::ShaderStage::Vertex, code, 3,
        inputs, args->NumElements, &output, 1, binary)) { device->error(E_INVALIDARG); return; }
    const HRESULT hr = device->backend->CreateInputLayout(elements, args->NumElements,
      binary.data(), binary.size(), &candidate.backend);
    if (FAILED(hr)) { device->error(hr); return; }
    *layout = std::move(candidate);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyLayout(D3D10DDI_HDEVICE h, D3D10DDI_HELEMENTLAYOUT object) {
  auto device = get(h); auto layout = get(object);
  if (!layout || layout->owner != device) { device->error(E_INVALIDARG); return; }
  if (device->inputLayout == layout) {
    device->context->IASetInputLayout(nullptr); device->inputLayout = nullptr;
  }
  layout->~InputLayout();
}
void APIENTRY setLayout(D3D10DDI_HDEVICE h, D3D10DDI_HELEMENTLAYOUT object) {
  auto device = get(h); auto layout = get(object);
  if (layout && (layout->owner != device || !layout->backend)) { device->error(E_INVALIDARG); return; }
  try {
    device->context->IASetInputLayout(layout ? layout->backend.Get() : nullptr);
    device->inputLayout = layout;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY setVertexBuffers(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D10DDI_HRESOURCE* objects, const UINT* strides, const UINT* offsets) {
  auto device = get(h);
  constexpr UINT slots = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
  if (start > slots || count > slots - start || (count && (!objects || !strides || !offsets))) {
    device->error(E_INVALIDARG); return;
  }
  ID3D11Buffer* buffers[slots] = {};
  ComPtr<ID3D11Buffer> references[slots];
  for (UINT i = 0; i < count; i++) {
    if (!objects[i].pDrvPrivate) continue;
    auto resource = get(objects[i]);
    if (!owned(device, resource)) return;
    if (FAILED(resource->backend.As(&references[i]))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc = {}; references[i]->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_VERTEX_BUFFER) || offsets[i] > desc.ByteWidth
        || strides[i] > D3D11_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES) { device->error(E_INVALIDARG); return; }
    buffers[i] = references[i].Get();
  }
  try { if (count) device->context->IASetVertexBuffers(start, count, buffers, strides, offsets); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
bool prepareVertexShader(Device* device) {
  auto shader = device->vertexShader;
  if (!shader || !device->pixelShader) return false;
  auto layout = device->inputLayout;
  if (shader->needsLayout && (!layout || !layout->backend)) { device->error(E_INVALIDARG); return false; }
  try {
    std::vector<dxvk::umd::ShaderSignatureEntry> outputs;
    if (!dxvk::umd::linkVertexOutputs(shader->outputs.data(), shader->outputs.size(),
        device->pixelShader->inputs.data(), device->pixelShader->inputs.size(), outputs)) {
      device->error(E_INVALIDARG); return false;
    }
    std::array<dxvk::umd::ShaderScalar,32> outputTypes = {}, inputTypes = {};
    for (const auto& output : outputs) outputTypes[output.registerIndex] = output.scalar;
    if (shader->needsLayout) inputTypes = layout->inputTypes;
    if (!shader->vertex || shader->compiledInputTypes != inputTypes ||
        (shader->needsLinkage && shader->compiledOutputTypes != outputTypes)) {
      auto inputs = shader->inputs;
      for (auto& input : inputs) if (!input.systemValue) {
        input.scalar = inputTypes[input.registerIndex];
        if (input.scalar == dxvk::umd::ShaderScalar::Unknown) { device->error(E_INVALIDARG); return false; }
      }
      std::vector<unsigned char> bytecode;
      if (!dxvk::umd::buildShaderContainer(shader->stage, shader->code.data(), shader->code.size(),
          inputs.data(), inputs.size(), outputs.data(), outputs.size(), bytecode)) { device->error(E_INVALIDARG); return false; }
      ComPtr<ID3D11VertexShader> compiled;
      const HRESULT hr = device->backend->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &compiled);
      if (FAILED(hr)) { device->error(hr); return false; }
      shader->vertex = std::move(compiled); shader->compiledInputTypes = inputTypes;
      shader->compiledOutputTypes = outputTypes;
    }
    device->context->VSSetShader(shader->vertex.Get(), nullptr, 0);
    return true;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
  return false;
}
void APIENTRY setIndexBuffer(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE object, DXGI_FORMAT format, UINT offset) {
  auto device = get(h);
  ComPtr<ID3D11Buffer> buffer;
  if (object.pDrvPrivate) {
    auto resource = get(object);
    if (!owned(device, resource)) return;
    if ((format != DXGI_FORMAT_R16_UINT && format != DXGI_FORMAT_R32_UINT)
        || FAILED(resource->backend.As(&buffer))) { device->error(E_INVALIDARG); return; }
    D3D11_BUFFER_DESC desc = {}; buffer->GetDesc(&desc);
    if (!(desc.BindFlags & D3D11_BIND_INDEX_BUFFER) || offset > desc.ByteWidth
        || offset % (format == DXGI_FORMAT_R16_UINT ? 2 : 4)) { device->error(E_INVALIDARG); return; }
  }
  try {
    device->context->IASetIndexBuffer(buffer.Get(), format, offset);
    device->indexBound = buffer.Get() != nullptr;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
bool drawReady(Device* device, bool indexed = false) {
  if (!device->vertexBound || !device->pixelBound || !device->targetBound ||
      !device->viewportBound || !device->triangleList || (indexed && !device->indexBound)) {
    device->error(E_INVALIDARG); return false;
  }
  return prepareVertexShader(device);
}
void APIENTRY draw(D3D10DDI_HDEVICE h, UINT count, UINT start) {
  auto device = get(h);
  if (!drawReady(device)) return;
  try { device->context->Draw(count, start); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY drawIndexed(D3D10DDI_HDEVICE h, UINT count, UINT start, INT base) {
  auto device = get(h);
  if (!drawReady(device, true)) return;
  try { device->context->DrawIndexed(count, start, base); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY drawInstanced(D3D10DDI_HDEVICE h, UINT count, UINT instances, UINT start, UINT firstInstance) {
  auto device = get(h);
  if (!drawReady(device)) return;
  try { device->context->DrawInstanced(count, instances, start, firstInstance); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY drawIndexedInstanced(D3D10DDI_HDEVICE h, UINT count, UINT instances,
    UINT start, INT base, UINT firstInstance) {
  auto device = get(h);
  if (!drawReady(device, true)) return;
  try { device->context->DrawIndexedInstanced(count, instances, start, base, firstInstance); }
  catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
  catch (...) { device->error(E_FAIL); }
}
void APIENTRY flush(D3D10DDI_HDEVICE h) { get(h)->context->Flush(); }
void APIENTRY relocateDeviceFunctions(D3D10DDI_HDEVICE h, D3D10DDI_DEVICEFUNCS* functions) {
  if (!functions) { get(h)->error(E_INVALIDARG); return; }
  // The runtime has already copied its table. No driver object caches a
  // pointer to that table, and none of our entrypoints require rebasing.
}
void APIENTRY counterInfo(D3D10DDI_HDEVICE h, D3D10DDI_COUNTER_INFO* info) {
  if (!info) { get(h)->error(E_INVALIDARG); return; }
  // The embedded DXVK device exposes no performance counters. This is the
  // cached creation-time capability, independent of later device removal.
  *info = {};
}
void APIENTRY checkCounter(D3D10DDI_HDEVICE h, D3D10DDI_QUERY query,
    D3D10DDI_COUNTER_TYPE*, UINT*, LPSTR, UINT*, LPSTR, UINT*, LPSTR, UINT*) {
  // Known optional counters are unsupported; no device-dependent range exists.
  get(h)->error(query >= D3D10DDI_COUNTER_GPU_IDLE && query <= D3D10DDI_COUNTER_TEXTURE_CACHE_HIT_RATE
    ? DXGI_DDI_ERR_UNSUPPORTED : E_INVALIDARG);
}
void APIENTRY destroyDevice(D3D10DDI_HDEVICE h) {
  get(h)->error(get(h)->memory.close());
  get(h)->~Device();
}

HRESULT APIENTRY present(DXGI_DDI_ARG_PRESENT* args) {
  if (!args || !args->hDevice || !args->hSurfaceToPresent)
    return E_INVALIDARG;
  auto device = reinterpret_cast<Device*>(args->hDevice);
  auto resource = reinterpret_cast<Resource*>(args->hSurfaceToPresent);
  if (resource->owner != device || !resource->backend || !resource->allocation.handle()
      || args->SrcSubResourceIndex || args->DstSubResourceIndex
      || args->hDstResource || !args->pDXGIContext || args->Flags.Value != 1)
    return E_INVALIDARG;
  try {
    ComPtr<ID3D11Texture2D> source;
    HRESULT hr = resource->backend.As(&source);
    if (FAILED(hr)) return hr;
    if (!resource->presentReadback) {
      D3D11_TEXTURE2D_DESC desc = {}; source->GetDesc(&desc);
      desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
      hr = device->backend->CreateTexture2D(&desc, nullptr, &resource->presentReadback);
      if (FAILED(hr)) return hr;
    }
    device->context->CopyResource(resource->presentReadback.Get(), source.Get());
    D3D11_MAPPED_SUBRESOURCE map = {};
    // Synchronous Map is the GPU completion barrier before any guest CPU
    // publication. Correctness checkpoint; this is not a zero-copy path.
    hr = device->context->Map(resource->presentReadback.Get(), 0, D3D11_MAP_READ, 0, &map);
    if (FAILED(hr)) return hr;
    struct Unmap {
      ID3D11DeviceContext* context;
      ID3D11Resource* resource;
      ~Unmap() { context->Unmap(resource, 0); }
    } unmap = {device->context.Get(), resource->presentReadback.Get()};
    hr = device->memory.upload(resource->allocation, map.pData, map.RowPitch);
    if (FAILED(hr)) return hr;
    return device->memory.present(resource->allocation, *args);
  } catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }
    catch (...) { return E_FAIL; }
}
}

extern "C" SIZE_T APIENTRY VioGpuDxvkPrivateDeviceSize() { return sizeof(Device); }

extern "C" HRESULT APIENTRY VioGpuDxvkCreateDdiTestDevice(
    const LUID* luid, D3D10DDI_HDEVICE h, D3D10DDI_HRTCORELAYER runtime,
    const D3D10DDI_CORELAYER_DEVICECALLBACKS* callbacks, D3D10DDI_DEVICEFUNCS* table) {
  if (!luid || !h.pDrvPrivate || !callbacks || !callbacks->pfnSetErrorCb || !table)
    return E_INVALIDARG;
  *table = {};
  auto device = new (h.pDrvPrivate) Device();
  device->runtime = runtime;
  device->callbacks = *callbacks;
  const HRESULT hr = dxvk::umd::createDevice(*luid, D3D_FEATURE_LEVEL_10_0,
    &device->backend, &device->context);
  if (FAILED(hr)) { device->~Device(); return hr; }
  table->pfnCalcPrivateResourceSize = resourceSize;
  table->pfnCreateResource = createResource;
  table->pfnDestroyResource = destroyResource;
  table->pfnCalcPrivateShaderResourceViewSize = shaderViewSize;
  table->pfnCreateShaderResourceView = createShaderView;
  table->pfnDestroyShaderResourceView = destroyShaderView;
  table->pfnVsSetShaderResources = setShaderResources<true>;
  table->pfnPsSetShaderResources = setShaderResources<false>;
  table->pfnCalcPrivateSamplerSize = samplerSize;
  table->pfnCreateSampler = createSampler;
  table->pfnDestroySampler = destroySampler;
  table->pfnVsSetSamplers = setSamplers<true>;
  table->pfnPsSetSamplers = setSamplers<false>;
  table->pfnCalcPrivateRenderTargetViewSize = targetSize;
  table->pfnCreateRenderTargetView = createTarget;
  table->pfnDestroyRenderTargetView = destroyTarget;
  table->pfnClearRenderTargetView = clearTarget;
  table->pfnCalcPrivateDepthStencilViewSize = depthViewSize;
  table->pfnCreateDepthStencilView = createDepthView;
  table->pfnDestroyDepthStencilView = destroyDepthView;
  table->pfnClearDepthStencilView = clearDepthView;
  table->pfnCalcPrivateDepthStencilStateSize = depthStateSize;
  table->pfnCreateDepthStencilState = createDepthState;
  table->pfnDestroyDepthStencilState = destroyDepthState;
  table->pfnSetDepthStencilState = setDepthState;
  table->pfnResourceCopy = copyResource;
  table->pfnResourceResolveSubresource = resolveResource;
  table->pfnCheckFormatSupport = checkFormat;
  table->pfnCheckMultisampleQualityLevels = checkMultisample;
  table->pfnResourceCopyRegion = copyRegion;
  table->pfnResourceUpdateSubresourceUP = updateResource;
  table->pfnDefaultConstantBufferUpdateSubresourceUP = updateResource;
  table->pfnCalcPrivateQuerySize = querySize;
  table->pfnCreateQuery = createQuery;
  table->pfnDestroyQuery = destroyQuery;
  table->pfnQueryBegin = beginQuery;
  table->pfnQueryEnd = endQuery;
  table->pfnQueryGetData = getQueryData;
  table->pfnResourceMap = mapResource;
  table->pfnResourceUnmap = unmapResource;
  table->pfnStagingResourceMap = mapResource;
  table->pfnStagingResourceUnmap = unmapResource;
  table->pfnResourceIsStagingBusy = isStagingBusy;
  table->pfnResourceReadAfterWriteHazard = resourceHazard;
  table->pfnShaderResourceViewReadAfterWriteHazard = shaderViewHazard;
  table->pfnDynamicIABufferMapDiscard = mapResource;
  table->pfnDynamicIABufferMapNoOverwrite = mapResource;
  table->pfnDynamicIABufferUnmap = unmapResource;
  table->pfnDynamicConstantBufferMapDiscard = mapResource;
  table->pfnDynamicConstantBufferUnmap = unmapResource;
  table->pfnDynamicResourceMapDiscard = mapResource;
  table->pfnDynamicResourceUnmap = unmapResource;
  table->pfnCalcPrivateShaderSize = shaderSize;
  table->pfnCreateVertexShader = createVertexShader;
  table->pfnCreatePixelShader = createPixelShader;
  table->pfnDestroyShader = destroyShader;
  table->pfnVsSetShader = setVertexShader;
  table->pfnPsSetShader = setPixelShader;
  table->pfnVsSetConstantBuffers = setConstantBuffers<true>;
  table->pfnPsSetConstantBuffers = setConstantBuffers<false>;
  table->pfnSetRenderTargets = setRenderTargets;
  table->pfnSetViewports = setViewports;
  table->pfnSetScissorRects = setScissors;
  table->pfnCalcPrivateRasterizerStateSize = rasterizerSize;
  table->pfnCreateRasterizerState = createRasterizer;
  table->pfnDestroyRasterizerState = destroyRasterizer;
  table->pfnSetRasterizerState = setRasterizer;
  table->pfnIaSetTopology = setTopology;
  table->pfnIaSetIndexBuffer = setIndexBuffer;
  table->pfnIaSetVertexBuffers = setVertexBuffers;
  table->pfnCalcPrivateElementLayoutSize = layoutSize;
  table->pfnCreateElementLayout = createLayout;
  table->pfnDestroyElementLayout = destroyLayout;
  table->pfnIaSetInputLayout = setLayout;
  table->pfnCalcPrivateBlendStateSize = blendSize;
  table->pfnCreateBlendState = createBlend;
  table->pfnDestroyBlendState = destroyBlend;
  table->pfnSetBlendState = setBlend;
  table->pfnDraw = draw;
  table->pfnDrawIndexed = drawIndexed;
  table->pfnDrawInstanced = drawInstanced;
  table->pfnDrawIndexedInstanced = drawIndexedInstanced;
  table->pfnFlush = flush;
  table->pfnRelocateDeviceFuncs = relocateDeviceFunctions;
  table->pfnCheckCounterInfo = counterInfo;
  table->pfnCheckCounter = checkCounter;
  table->pfnDestroyDevice = destroyDevice;
  return S_OK;
}

HRESULT dxvk::umd::createAdapterDevice(
    const std::shared_ptr<const AdapterIdentity>& identity, D3D10DDIARG_CREATEDEVICE* args) {
  const HRESULT hr = VioGpuDxvkCreateDdiTestDevice(&identity->luid, args->hDrvDevice,
    args->hRTCoreLayer, args->pUMCallbacks, args->pDeviceFuncs);
  if (SUCCEEDED(hr)) {
    auto device = get(args->hDrvDevice);
    device->adapter = identity;
    device->memory.initialize(args->hRTDevice.handle, *args->pKTCallbacks,
      args->DXGIBaseDDI.pDXGIBaseCallbacks);
    if (args->DXGIBaseDDI.pDXGIDDIBaseFunctions) {
      *args->DXGIBaseDDI.pDXGIDDIBaseFunctions = {};
      if (device->memory.available())
        args->DXGIBaseDDI.pDXGIDDIBaseFunctions->pfnPresent = present;
    }
  }
  return hr;
}
