#include "umd_ddi.h"
#include "umd_api.h"

#include <wrl/client.h>
#include <memory>
#include <new>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
struct Device {
  ComPtr<ID3D11Device> backend;
  ComPtr<ID3D11DeviceContext> context;
  D3D10DDI_HRTCORELAYER runtime;
  D3D10DDI_CORELAYER_DEVICECALLBACKS callbacks;
  void error(HRESULT hr) {
    if (FAILED(hr)) callbacks.pfnSetErrorCb(runtime, hr);
  }
};
struct Resource {
  Device* owner = nullptr;
  ComPtr<ID3D11Resource> backend;
};
struct RenderTarget {
  Device* owner = nullptr;
  ComPtr<ID3D11RenderTargetView> backend;
};
Device* get(D3D10DDI_HDEVICE h) { return static_cast<Device*>(h.pDrvPrivate); }
Resource* get(D3D10DDI_HRESOURCE h) { return static_cast<Resource*>(h.pDrvPrivate); }
RenderTarget* get(D3D10DDI_HRENDERTARGETVIEW h) { return static_cast<RenderTarget*>(h.pDrvPrivate); }

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
    D3D10DDI_HRTRESOURCE) {
  auto device = get(h);
  if (!out.pDrvPrivate) { device->error(E_INVALIDARG); return; }
  auto resource = new (out.pDrvPrivate) Resource();
  resource->owner = device;
  if (!args || !args->pMipInfoList || !args->MipLevels || !args->ArraySize ||
      args->MipLevels > D3D11_REQ_MIP_LEVELS || args->ArraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION ||
      args->pPrimaryDesc || (args->BindFlags & D3D10_DDI_BIND_PRESENT) ||
      args->MiscFlags || (args->MapFlags & ~D3D10_DDI_CPU_ACCESS_MASK) ||
      (args->BindFlags & ~D3D10_DDI_BIND_PIPELINE_MASK)) {
    device->error(E_INVALIDARG); return;
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
      desc.BindFlags = args->BindFlags;
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
      desc.BindFlags = args->BindFlags;
      desc.CPUAccessFlags = ((args->MapFlags & D3D10_DDI_CPU_ACCESS_READ) ? D3D11_CPU_ACCESS_READ : 0)
                         | ((args->MapFlags & D3D10_DDI_CPU_ACCESS_WRITE) ? D3D11_CPU_ACCESS_WRITE : 0);
      ComPtr<ID3D11Texture2D> texture;
      hr = device->backend->CreateTexture2D(&desc, data, &texture);
      resource->backend = texture;
    }
    device->error(hr);
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
}
void APIENTRY destroyResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource) {
  auto object = get(resource);
  if (!object || object->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
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
void APIENTRY mapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource,
    UINT subresource, D3D10_DDI_MAP type, UINT flags, D3D10DDI_MAPPED_SUBRESOURCE* out) {
  auto device = get(h);
  if (!out) { device->error(E_INVALIDARG); return; }
  *out = {};
  if (!owned(device, get(resource))) return;
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  const HRESULT hr = device->context->Map(get(resource)->backend.Get(), subresource,
    static_cast<D3D11_MAP>(type), flags, &mapped);
  if (SUCCEEDED(hr)) {
    out->pData = mapped.pData; out->RowPitch = mapped.RowPitch; out->DepthPitch = mapped.DepthPitch;
  }
  device->error(hr);
}
void APIENTRY unmapResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT subresource) {
  auto device = get(h);
  if (owned(device, get(resource))) device->context->Unmap(get(resource)->backend.Get(), subresource);
}
void APIENTRY flush(D3D10DDI_HDEVICE h) { get(h)->context->Flush(); }
void APIENTRY destroyDevice(D3D10DDI_HDEVICE h) { get(h)->~Device(); }
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
  table->pfnResourceMap = mapResource;
  table->pfnResourceUnmap = unmapResource;
  table->pfnStagingResourceMap = mapResource;
  table->pfnStagingResourceUnmap = unmapResource;
  table->pfnFlush = flush;
  table->pfnDestroyDevice = destroyDevice;
  return S_OK;
}
