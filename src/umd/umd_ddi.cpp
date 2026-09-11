#include "umd_ddi.h"
#include "umd_api.h"
#include "umd_adapter.h"
#include "umd_shader.h"
#include "umd_query.h"
#include "umd_allocation.h"

#include <wrl/client.h>
#include <memory>
#include <new>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
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
  void error(HRESULT hr) {
    if (FAILED(hr)) callbacks.pfnSetErrorCb(runtime, hr);
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
struct Shader {
  Device* owner = nullptr;
  dxvk::umd::ShaderStage stage = dxvk::umd::ShaderStage::Vertex;
  ComPtr<ID3D11VertexShader> vertex;
  ComPtr<ID3D11PixelShader> pixel;
};
struct Rasterizer {
  Device* owner = nullptr;
  ComPtr<ID3D11RasterizerState> backend;
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
Shader* get(D3D10DDI_HSHADER h) { return static_cast<Shader*>(h.pDrvPrivate); }
Rasterizer* get(D3D10DDI_HRASTERIZERSTATE h) { return static_cast<Rasterizer*>(h.pDrvPrivate); }
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
  desc.Format = args->Format;
  // Array view covers the non-array one-slice case too.
  desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
  desc.Texture2DArray.MipSlice = args->Tex2D.MipSlice;
  desc.Texture2DArray.FirstArraySlice = args->Tex2D.FirstArraySlice;
  desc.Texture2DArray.ArraySize = args->Tex2D.ArraySize;
  target->format = desc.Format;
  device->error(device->backend->CreateRenderTargetView(
    get(args->hDrvResource)->backend.Get(), &desc, &target->backend));
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
void APIENTRY copyResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, D3D10DDI_HRESOURCE src) {
  auto device = get(h);
  if (owned(device, get(dst)) && owned(device, get(src)))
    device->context->CopyResource(get(dst)->backend.Get(), get(src)->backend.Get());
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
  if (flags & ~D3D10_DDI_MAP_FLAG_MASK) { device->error(E_INVALIDARG); return; }
  if (!owned(device, get(resource))) return;
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  const UINT apiFlags = (flags & D3D10_DDI_MAP_FLAG_DONOTWAIT) ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0;
  const HRESULT hr = device->context->Map(get(resource)->backend.Get(), subresource,
    static_cast<D3D11_MAP>(type), apiFlags, &mapped);
  if (SUCCEEDED(hr)) {
    out->pData = mapped.pData; out->RowPitch = mapped.RowPitch; out->DepthPitch = mapped.DepthPitch;
  }
  device->error(hr);
}
void APIENTRY unmapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT subresource) {
  auto device = get(h);
  if (owned(device, get(resource))) device->context->Unmap(get(resource)->backend.Get(), subresource);
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
  if (!code || !signature || signature->NumInputSignatureEntries > 1 ||
      signature->NumOutputSignatureEntries != 1 || !signature->pOutputSignature ||
      (signature->NumInputSignatureEntries && !signature->pInputSignature)) {
    device->error(E_INVALIDARG); return;
  }
  static_assert(D3D10_SB_NAME_POSITION == 1 && D3D10_SB_NAME_VERTEX_ID == 6);
  dxvk::umd::ShaderSignatureEntry input = {};
  if (signature->NumInputSignatureEntries) {
    const auto& entry = signature->pInputSignature[0];
    input = {uint32_t(entry.SystemValue), entry.Register, entry.Mask};
  }
  const auto& entry = signature->pOutputSignature[0];
  dxvk::umd::ShaderSignatureEntry output = {uint32_t(entry.SystemValue), entry.Register, entry.Mask};
  try {
    std::vector<unsigned char> bytecode;
    if (!dxvk::umd::buildShaderContainer(stage, code, code[1], &input,
        signature->NumInputSignatureEntries, &output, 1, bytecode)) {
      device->error(E_INVALIDARG); return;
    }
    if (stage == dxvk::umd::ShaderStage::Vertex)
      device->error(device->backend->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &shader->vertex));
    else
      device->error(device->backend->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &shader->pixel));
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
  object->~Shader();
}
void APIENTRY setVertexShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h);
  auto object = get(shader);
  if (object && (object->owner != device || !object->vertex)) { device->error(E_INVALIDARG); return; }
  device->context->VSSetShader(object ? object->vertex.Get() : nullptr, nullptr, 0);
  device->vertexBound = object != nullptr;
}
void APIENTRY setPixelShader(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER shader) {
  auto device = get(h);
  auto object = get(shader);
  if (object && (object->owner != device || !object->pixel)) { device->error(E_INVALIDARG); return; }
  device->context->PSSetShader(object ? object->pixel.Get() : nullptr, nullptr, 0);
  device->pixelBound = object != nullptr;
}
void APIENTRY setRenderTargets(D3D10DDI_HDEVICE h, const D3D10DDI_HRENDERTARGETVIEW* targets,
    UINT count, UINT clear, D3D10DDI_HDEPTHSTENCILVIEW depth) {
  auto device = get(h);
  if (count > 1 || clear > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT - count ||
      depth.pDrvPrivate || (count && !targets)) { device->error(E_INVALIDARG); return; }
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
  if (count || clear) {
    device->context->OMSetRenderTargets(count, count ? &target : nullptr, nullptr);
    device->targetBound = target != nullptr;
  }
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
void APIENTRY draw(D3D10DDI_HDEVICE h, UINT count, UINT start) {
  auto device = get(h);
  if (!device->vertexBound || !device->pixelBound || !device->targetBound ||
      !device->viewportBound || !device->triangleList) { device->error(E_INVALIDARG); return; }
  device->context->Draw(count, start);
}
void APIENTRY flush(D3D10DDI_HDEVICE h) { get(h)->context->Flush(); }
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
  table->pfnCalcPrivateRenderTargetViewSize = targetSize;
  table->pfnCreateRenderTargetView = createTarget;
  table->pfnDestroyRenderTargetView = destroyTarget;
  table->pfnClearRenderTargetView = clearTarget;
  table->pfnResourceCopy = copyResource;
  table->pfnResourceCopyRegion = copyRegion;
  table->pfnResourceUpdateSubresourceUP = updateResource;
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
  table->pfnCalcPrivateShaderSize = shaderSize;
  table->pfnCreateVertexShader = createVertexShader;
  table->pfnCreatePixelShader = createPixelShader;
  table->pfnDestroyShader = destroyShader;
  table->pfnVsSetShader = setVertexShader;
  table->pfnPsSetShader = setPixelShader;
  table->pfnSetRenderTargets = setRenderTargets;
  table->pfnSetViewports = setViewports;
  table->pfnCalcPrivateRasterizerStateSize = rasterizerSize;
  table->pfnCreateRasterizerState = createRasterizer;
  table->pfnDestroyRasterizerState = destroyRasterizer;
  table->pfnSetRasterizerState = setRasterizer;
  table->pfnIaSetTopology = setTopology;
  table->pfnDraw = draw;
  table->pfnFlush = flush;
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
