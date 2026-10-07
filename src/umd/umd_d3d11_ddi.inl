// Included in umd_ddi.cpp's private namespace. All interfaces share Device,
// Resource and the callback dispatcher; this file owns only typed newer DDIs.

SIZE_T APIENTRY resourceSize11(D3D10DDI_HDEVICE, const D3D11DDIARG_CREATERESOURCE*) {
  return sizeof(Resource);
}
HRESULT createResourceData11(Device* device, const D3D11DDIARG_CREATERESOURCE* args,
    Resource* resource, D3D10DDI_HRTRESOURCE runtime) {
  if (!args || !args->pMipInfoList || !args->MipLevels || !args->ArraySize
      || args->MipLevels > D3D11_REQ_MIP_LEVELS
      || args->ArraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) return E_INVALIDARG;
  UINT misc = 0, bindings = 0;
  if (!dxvk::umd::resource11Flags(*args, misc, bindings)) return E_INVALIDARG;
  if ((args->BindFlags & D3D10_DDI_BIND_PRESENT)
      || (args->MiscFlags & D3D10_DDI_RESOURCE_MISC_SHARED)) {
    // Paired KMD allocations currently describe only a plain 2D image. Keep
    // the existing shared/present ownership path and reject extra semantics.
    constexpr UINT legacyMisc = D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP | D3D10_DDI_RESOURCE_MISC_SHARED;
    if (args->ByteStride || (args->MiscFlags & ~legacyMisc)
        || (args->BindFlags & D3D11_DDI_BIND_UNORDERED_ACCESS)) return DXGI_ERROR_UNSUPPORTED;
    const auto legacy = dxvk::umd::resource10Fields(*args);
    return createResourceData(device, &legacy, resource, runtime);
  }
  if (args->pPrimaryDesc) return DXGI_ERROR_UNSUPPORTED;
  D3D11_TEXTURE3D_DESC volume = {};
  if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE3D) {
    const auto legacy = dxvk::umd::resource10Fields(*args);
    if (args->ByteStride || !dxvk::umd::texture3DDesc(legacy, misc, volume, true)
        || !dxvk::umd::texture3DInitialData(legacy)) return E_INVALIDARG;
  }
  resource->owner = device;
  resource->retirement = std::make_unique<ResourceRetirement>();
  std::vector<D3D11_SUBRESOURCE_DATA> initial;
  if (args->pInitialDataUP) {
    const size_t count = args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE3D
      ? args->MipLevels : size_t(args->MipLevels) * args->ArraySize;
    initial.resize(count);
    for (size_t i = 0; i < count; ++i) {
      if (!args->pInitialDataUP[i].pSysMem) return E_INVALIDARG;
      initial[i] = {args->pInitialDataUP[i].pSysMem, args->pInitialDataUP[i].SysMemPitch,
        args->pInitialDataUP[i].SysMemSlicePitch};
    }
  }
  const auto data = initial.empty() ? nullptr : initial.data();
  const auto& shape = args->pMipInfoList[0];
  const auto usage = static_cast<D3D11_USAGE>(args->Usage);
  const UINT cpu = dxvk::umd::resource11CpuAccess(args->MapFlags);
  HRESULT hr = E_INVALIDARG;
  if (args->ResourceDimension == D3D10DDIRESOURCE_BUFFER) {
    D3D11_BUFFER_DESC desc = {};
    if (!dxvk::umd::buffer11Desc(*args, desc)) return E_INVALIDARG;
    ComPtr<ID3D11Buffer> buffer;
    hr = device->backend->CreateBuffer(&desc, data, &buffer); resource->backend = buffer;
  } else {
    constexpr UINT bufferFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
      | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    if (args->ByteStride || (misc & bufferFlags)) return E_INVALIDARG;
    if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
      if (args->SampleDesc.Count != 1 || args->SampleDesc.Quality) return E_INVALIDARG;
      D3D11_TEXTURE1D_DESC desc = {shape.TexelWidth, args->MipLevels, args->ArraySize,
        args->Format, usage, bindings, cpu, misc};
      ComPtr<ID3D11Texture1D> texture;
      hr = device->backend->CreateTexture1D(&desc, data, &texture); resource->backend = texture;
    } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D
        || args->ResourceDimension == D3D10DDIRESOURCE_TEXTURECUBE) {
      if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURECUBE
          && (shape.TexelWidth != shape.TexelHeight || args->ArraySize % 6
          || args->SampleDesc.Count != 1 || args->SampleDesc.Quality)) return E_INVALIDARG;
      D3D11_TEXTURE2D_DESC desc = {shape.TexelWidth, shape.TexelHeight, args->MipLevels,
        args->ArraySize, args->Format, args->SampleDesc, usage, bindings, cpu, misc};
      ComPtr<ID3D11Texture2D> texture;
      hr = device->backend->CreateTexture2D(&desc, data, &texture); resource->backend = texture;
    } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE3D) {
      ComPtr<ID3D11Texture3D> texture;
      hr = device->backend->CreateTexture3D(&volume, data, &texture); resource->backend = texture;
    }
  }
  if (hr == S_OK && !resource->backend) return E_FAIL;
  return hr == S_OK || FAILED(hr) ? hr : E_FAIL;
}
void APIENTRY createResource11(D3D10DDI_HDEVICE h, const D3D11DDIARG_CREATERESOURCE* args,
    D3D10DDI_HRESOURCE out, D3D10DDI_HRTRESOURCE runtime) {
  auto device = get(h);
  publishNewResource(device, out, [&](Resource* staged) { return createResourceData11(device, args, staged, runtime); });
}

SIZE_T APIENTRY blendSize10_1(D3D10DDI_HDEVICE, const D3D10_1_DDI_BLEND_DESC*) { return sizeof(BlendState); }
void APIENTRY createBlend10_1(D3D10DDI_HDEVICE h, const D3D10_1_DDI_BLEND_DESC* args,
    D3D10DDI_HBLENDSTATE out, D3D10DDI_HRTBLENDSTATE) {
  auto device = get(h);
  if (!args) { device->error(E_INVALIDARG); return; }
  const auto desc = dxvk::umd::blend11Desc(*args);
  const HRESULT hr = createViewStorage<BlendState>(device, out.pDrvPrivate, [&](BlendState& blend) {
    return device->backend->CreateBlendState(&desc, &blend.backend);
  });
  device->error(hr);
}

template<typename Native> D3D10DDIARG_CREATESHADERRESOURCEVIEW shaderView10Fields(const Native& args) {
  D3D10DDIARG_CREATESHADERRESOURCEVIEW out = {};
  out.hDrvResource = args.hDrvResource; out.Format = args.Format; out.ResourceDimension = args.ResourceDimension;
  switch (args.ResourceDimension) {
    case D3D10DDIRESOURCE_BUFFER: out.Buffer = args.Buffer; break;
    case D3D10DDIRESOURCE_TEXTURE1D: out.Tex1D = args.Tex1D; break;
    case D3D10DDIRESOURCE_TEXTURE2D: out.Tex2D = args.Tex2D; break;
    case D3D10DDIRESOURCE_TEXTURE3D: out.Tex3D = args.Tex3D; break;
    default: break;
  }
  return out;
}
SIZE_T APIENTRY shaderViewSize11(D3D10DDI_HDEVICE, const D3D11DDIARG_CREATESHADERRESOURCEVIEW*) { return sizeof(ShaderView); }
SIZE_T APIENTRY shaderViewSize10_1(D3D10DDI_HDEVICE, const D3D10_1DDIARG_CREATESHADERRESOURCEVIEW*) { return sizeof(ShaderView); }
void APIENTRY createShaderView11(D3D10DDI_HDEVICE h, const D3D11DDIARG_CREATESHADERRESOURCEVIEW* args,
    D3D10DDI_HSHADERRESOURCEVIEW out, D3D10DDI_HRTSHADERRESOURCEVIEW runtime) {
  auto device = get(h);
  if (!args) { device->error(E_INVALIDARG); return; }
  if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D
      || args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D
      || args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE3D) {
    const auto legacy = shaderView10Fields(*args); createShaderView(h, &legacy, out, runtime); return;
  }
  if (!owned(device, get(args->hDrvResource))) return;
  HRESULT hr;
  {
    auto resource = get(args->hDrvResource)->backend;
    hr = createViewStorage<ShaderView>(device, out.pDrvPrivate, [&](ShaderView& view) {
      D3D11_SHADER_RESOURCE_VIEW_DESC desc = {}; desc.Format = args->Format;
      if (args->ResourceDimension == D3D10DDIRESOURCE_BUFFER || args->ResourceDimension == D3D11DDIRESOURCE_BUFFEREX) {
        ComPtr<ID3D11Buffer> buffer;
        if (FAILED(resource.As(&buffer))) return E_INVALIDARG;
        D3D11_BUFFER_DESC info = {}; buffer->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_SHADER_RESOURCE)) return E_INVALIDARG;
        const UINT first = args->ResourceDimension == D3D11DDIRESOURCE_BUFFEREX ? args->BufferEx.FirstElement : args->Buffer.FirstElement;
        const UINT count = args->ResourceDimension == D3D11DDIRESOURCE_BUFFEREX ? args->BufferEx.NumElements : args->Buffer.NumElements;
        const UINT flags = args->ResourceDimension == D3D11DDIRESOURCE_BUFFEREX ? args->BufferEx.Flags : 0;
        if (!count || (flags & ~D3D11_DDI_BUFFEREX_SRV_FLAG_RAW)) return E_INVALIDARG;
        if (flags) {
          if (!(info.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
              || args->Format != DXGI_FORMAT_R32_TYPELESS || !dxvk::umd::viewRange(first, count, info.ByteWidth / 4)) return E_INVALIDARG;
          desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
          desc.BufferEx = {first, count, D3D11_BUFFEREX_SRV_FLAG_RAW};
        } else {
          if ((info.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
              && (args->Format != DXGI_FORMAT_UNKNOWN || !info.StructureByteStride
              || !dxvk::umd::viewRange(first, count, info.ByteWidth / info.StructureByteStride))) return E_INVALIDARG;
          desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; desc.Buffer.FirstElement = first; desc.Buffer.NumElements = count;
        }
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURECUBE) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_SHADER_RESOURCE) || !(info.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
            || args->TexCube.First2DArrayFace % 6 || !args->TexCube.NumCubes
            || args->TexCube.NumCubes > info.ArraySize / 6
            || !dxvk::umd::viewRange(args->TexCube.First2DArrayFace / 6, args->TexCube.NumCubes, info.ArraySize / 6)
            || !dxvk::umd::viewRange(args->TexCube.MostDetailedMip, args->TexCube.MipLevels, info.MipLevels)) return E_INVALIDARG;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
        desc.TextureCubeArray = {args->TexCube.MostDetailedMip, args->TexCube.MipLevels,
          args->TexCube.First2DArrayFace, args->TexCube.NumCubes};
      } else return E_INVALIDARG;
      return device->backend->CreateShaderResourceView(resource.Get(), &desc, &view.backend);
    });
  }
  device->error(hr);
}
void APIENTRY createShaderView10_1(D3D10DDI_HDEVICE h, const D3D10_1DDIARG_CREATESHADERRESOURCEVIEW* args,
    D3D10DDI_HSHADERRESOURCEVIEW out, D3D10DDI_HRTSHADERRESOURCEVIEW runtime) {
  if (!args) { get(h)->error(E_INVALIDARG); return; }
  D3D11DDIARG_CREATESHADERRESOURCEVIEW translated = {};
  translated.hDrvResource = args->hDrvResource; translated.Format = args->Format;
  translated.ResourceDimension = args->ResourceDimension;
  switch (args->ResourceDimension) {
    case D3D10DDIRESOURCE_BUFFER: translated.Buffer = args->Buffer; break;
    case D3D10DDIRESOURCE_TEXTURE1D: translated.Tex1D = args->Tex1D; break;
    case D3D10DDIRESOURCE_TEXTURE2D: translated.Tex2D = args->Tex2D; break;
    case D3D10DDIRESOURCE_TEXTURE3D: translated.Tex3D = args->Tex3D; break;
    case D3D10DDIRESOURCE_TEXTURECUBE: translated.TexCube = args->TexCube; break;
    default: get(h)->error(E_INVALIDARG); return;
  }
  createShaderView11(h, &translated, out, runtime);
}

SIZE_T APIENTRY depthViewSize11(D3D10DDI_HDEVICE, const D3D11DDIARG_CREATEDEPTHSTENCILVIEW*) { return sizeof(DepthView); }
void APIENTRY createDepthView11(D3D10DDI_HDEVICE h, const D3D11DDIARG_CREATEDEPTHSTENCILVIEW* args,
    D3D10DDI_HDEPTHSTENCILVIEW out, D3D10DDI_HRTDEPTHSTENCILVIEW) {
  auto device = get(h);
  if (!args || (args->Flags & ~(D3D11_DDI_CREATE_DSV_READ_ONLY_DEPTH | D3D11_DDI_CREATE_DSV_READ_ONLY_STENCIL))) {
    device->error(E_INVALIDARG); return;
  }
  if (!owned(device, get(args->hDrvResource))) return;
  HRESULT hr;
  {
    auto resource = get(args->hDrvResource)->backend;
    hr = createViewStorage<DepthView>(device, out.pDrvPrivate, [&](DepthView& view) {
      D3D11_DEPTH_STENCIL_VIEW_DESC desc = {}; desc.Format = args->Format;
      if (args->Flags & D3D11_DDI_CREATE_DSV_READ_ONLY_DEPTH) desc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
      if (args->Flags & D3D11_DDI_CREATE_DSV_READ_ONLY_STENCIL) desc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
      if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
        ComPtr<ID3D11Texture1D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE1D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_DEPTH_STENCIL) || args->Tex1D.MipSlice >= info.MipLevels
            || !dxvk::umd::viewRange(args->Tex1D.FirstArraySlice, args->Tex1D.ArraySize, info.ArraySize)) return E_INVALIDARG;
        desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
        desc.Texture1DArray = {args->Tex1D.MipSlice, args->Tex1D.FirstArraySlice, args->Tex1D.ArraySize};
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_DEPTH_STENCIL) || args->Tex2D.MipSlice >= info.MipLevels
            || !dxvk::umd::viewRange(args->Tex2D.FirstArraySlice, args->Tex2D.ArraySize, info.ArraySize)) return E_INVALIDARG;
        if (info.SampleDesc.Count > 1) {
          if (args->Tex2D.MipSlice) return E_INVALIDARG;
          desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
          desc.Texture2DMSArray = {args->Tex2D.FirstArraySlice, args->Tex2D.ArraySize};
        } else {
          desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
          desc.Texture2DArray = {args->Tex2D.MipSlice, args->Tex2D.FirstArraySlice, args->Tex2D.ArraySize};
        }
      } else return E_INVALIDARG;
      return device->backend->CreateDepthStencilView(resource.Get(), &desc, &view.backend);
    });
  }
  device->error(hr);
}

struct UnorderedView : Child {
  Device* owner = nullptr;
  ComPtr<ID3D11UnorderedAccessView> backend;
  bool counter = false;
};
UnorderedView* get(D3D11DDI_HUNORDEREDACCESSVIEW h) { return static_cast<UnorderedView*>(h.pDrvPrivate); }
SIZE_T APIENTRY unorderedViewSize(D3D10DDI_HDEVICE, const D3D11DDIARG_CREATEUNORDEREDACCESSVIEW*) { return sizeof(UnorderedView); }
void APIENTRY createUnorderedView(D3D10DDI_HDEVICE h, const D3D11DDIARG_CREATEUNORDEREDACCESSVIEW* args,
    D3D11DDI_HUNORDEREDACCESSVIEW out, D3D11DDI_HRTUNORDEREDACCESSVIEW) {
  auto device = get(h);
  if (!args) { device->error(E_INVALIDARG); return; }
  if (device->featureLevel < D3D_FEATURE_LEVEL_11_0) { device->error(DXGI_ERROR_UNSUPPORTED); return; }
  if (!owned(device, get(args->hDrvResource))) return;
  HRESULT hr;
  {
    auto resource = get(args->hDrvResource)->backend;
    hr = createViewStorage<UnorderedView>(device, out.pDrvPrivate, [&](UnorderedView& view) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {}; desc.Format = args->Format;
      if (args->ResourceDimension == D3D10DDIRESOURCE_BUFFER) {
        ComPtr<ID3D11Buffer> buffer;
        if (FAILED(resource.As(&buffer))) return E_INVALIDARG;
        D3D11_BUFFER_DESC info = {}; buffer->GetDesc(&info);
        const UINT flags = args->Buffer.Flags;
        constexpr UINT known = D3D11_DDI_BUFFER_UAV_FLAG_RAW | D3D11_DDI_BUFFER_UAV_FLAG_APPEND | D3D11_DDI_BUFFER_UAV_FLAG_COUNTER;
        if (!(info.BindFlags & D3D11_BIND_UNORDERED_ACCESS) || !args->Buffer.NumElements
            || (flags & ~known) || ((flags & D3D11_DDI_BUFFER_UAV_FLAG_APPEND) && (flags & D3D11_DDI_BUFFER_UAV_FLAG_COUNTER))) return E_INVALIDARG;
        if (flags & D3D11_DDI_BUFFER_UAV_FLAG_RAW) {
          if (flags != D3D11_DDI_BUFFER_UAV_FLAG_RAW || args->Format != DXGI_FORMAT_R32_TYPELESS
              || !(info.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
              || !dxvk::umd::viewRange(args->Buffer.FirstElement, args->Buffer.NumElements, info.ByteWidth / 4)) return E_INVALIDARG;
        } else if (info.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
          if (args->Format != DXGI_FORMAT_UNKNOWN || !info.StructureByteStride
              || !dxvk::umd::viewRange(args->Buffer.FirstElement, args->Buffer.NumElements, info.ByteWidth / info.StructureByteStride)) return E_INVALIDARG;
        } else if (flags) return E_INVALIDARG;
        desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        desc.Buffer = {args->Buffer.FirstElement, args->Buffer.NumElements, flags};
        view.counter = (flags & (D3D11_DDI_BUFFER_UAV_FLAG_APPEND | D3D11_DDI_BUFFER_UAV_FLAG_COUNTER)) != 0;
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE1D) {
        ComPtr<ID3D11Texture1D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE1D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_UNORDERED_ACCESS) || args->Tex1D.MipSlice >= info.MipLevels
            || !dxvk::umd::viewRange(args->Tex1D.FirstArraySlice, args->Tex1D.ArraySize, info.ArraySize)) return E_INVALIDARG;
        desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
        desc.Texture1DArray = {args->Tex1D.MipSlice, args->Tex1D.FirstArraySlice, args->Tex1D.ArraySize};
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE2D) {
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE2D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_UNORDERED_ACCESS) || info.SampleDesc.Count != 1
            || args->Tex2D.MipSlice >= info.MipLevels
            || !dxvk::umd::viewRange(args->Tex2D.FirstArraySlice, args->Tex2D.ArraySize, info.ArraySize)) return E_INVALIDARG;
        desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {args->Tex2D.MipSlice, args->Tex2D.FirstArraySlice, args->Tex2D.ArraySize};
      } else if (args->ResourceDimension == D3D10DDIRESOURCE_TEXTURE3D) {
        ComPtr<ID3D11Texture3D> texture;
        if (FAILED(resource.As(&texture))) return E_INVALIDARG;
        D3D11_TEXTURE3D_DESC info = {}; texture->GetDesc(&info);
        if (!(info.BindFlags & D3D11_BIND_UNORDERED_ACCESS) || args->Tex3D.MipSlice >= info.MipLevels
            || !dxvk::umd::viewRange(args->Tex3D.FirstW, args->Tex3D.WSize, std::max(1u, info.Depth >> args->Tex3D.MipSlice))) return E_INVALIDARG;
        desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {args->Tex3D.MipSlice, args->Tex3D.FirstW, args->Tex3D.WSize};
      } else return E_INVALIDARG;
      return device->backend->CreateUnorderedAccessView(resource.Get(), &desc, &view.backend);
    });
  }
  device->error(hr);
}
void APIENTRY destroyUnorderedView(D3D10DDI_HDEVICE h, D3D11DDI_HUNORDEREDACCESSVIEW object) {
  auto view = get(object);
  if (!view || view->owner != get(h)) { get(h)->error(E_INVALIDARG); return; }
  retireChild(get(h), view, view->backend);
}
void APIENTRY clearUnorderedUint(D3D10DDI_HDEVICE h, D3D11DDI_HUNORDEREDACCESSVIEW object, const UINT values[4]) {
  auto device = get(h); auto view = get(object);
  if (!view || view->owner != device || !view->backend || !values) { device->error(E_INVALIDARG); return; }
  device->context->ClearUnorderedAccessViewUint(view->backend.Get(), values);
}
void APIENTRY clearUnorderedFloat(D3D10DDI_HDEVICE h, D3D11DDI_HUNORDEREDACCESSVIEW object, const FLOAT values[4]) {
  auto device = get(h); auto view = get(object);
  if (!view || view->owner != device || !view->backend || !values) { device->error(E_INVALIDARG); return; }
  device->context->ClearUnorderedAccessViewFloat(view->backend.Get(), values);
}
bool unorderedBindings(Device* device, UINT start, UINT count, const D3D11DDI_HUNORDEREDACCESSVIEW* objects,
    const UINT* initialCounts, std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT>& output) {
  constexpr UINT slots = D3D11_PS_CS_UAV_REGISTER_COUNT;
  if (start > slots || count > slots - start || (count && !objects)) return false;
  for (UINT i = 0; i < count; ++i) {
    auto view = get(objects[i]);
    if (!view) continue;
    if (view->owner != device || !view->backend || (view->counter && !initialCounts)) return false;
    output[start + i] = view->backend.Get();
  }
  return true;
}
void APIENTRY setUnorderedViews(D3D10DDI_HDEVICE h, UINT start, UINT count,
    const D3D11DDI_HUNORDEREDACCESSVIEW* objects, const UINT* initialCounts) {
  auto device = get(h);
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> views{};
  if (!unorderedBindings(device, start, count, objects, initialCounts, views)) { device->error(E_INVALIDARG); return; }
  device->context->CSSetUnorderedAccessViews(start, count, views.data() + start, initialCounts);
}
void APIENTRY setRenderTargets11(D3D10DDI_HDEVICE h, const D3D10DDI_HRENDERTARGETVIEW* targets,
    UINT count, UINT clear, D3D10DDI_HDEPTHSTENCILVIEW depth,
    const D3D11DDI_HUNORDEREDACCESSVIEW* unordered, const UINT* initialCounts,
    UINT start, UINT unorderedCount, UINT rangeStart, UINT rangeSize) {
  auto device = get(h); RenderTargetBindings bindings;
  std::array<ID3D11UnorderedAccessView*, D3D11_PS_CS_UAV_REGISTER_COUNT> views{};
  constexpr UINT slots = D3D11_PS_CS_UAV_REGISTER_COUNT;
  const HRESULT hr = renderTargetBindings(device, targets, count, clear, depth, bindings);
  if (FAILED(hr) || start < count || rangeStart > slots || rangeSize > slots - rangeStart
      || !unorderedBindings(device, start, unorderedCount, unordered, initialCounts, views)) {
    device->error(E_INVALIDARG); return;
  }
  std::array<UINT, slots> counters; counters.fill(UINT(-1));
  if (initialCounts) for (UINT i = 0; i < unorderedCount; ++i) counters[start + i] = initialCounts[i];
  // DDI binds the whole state. RangeStart/RangeSize are optimization hints;
  // omitted bindings are NULL, including a tail cleared with zero hints.
  device->context->OMSetRenderTargetsAndUnorderedAccessViews(count, count ? bindings.targets.data() : nullptr,
    bindings.depth, count, slots - count, views.data() + count, counters.data() + count);
  device->targetShared = std::move(bindings.shared); device->targetBound = bindings.anyColor;
  device->targetTypes = bindings.types;
}
bool computeReady(Device* device) {
  if (device->featureLevel < D3D_FEATURE_LEVEL_11_0 || !device->computeShader) {
    device->error(E_INVALIDARG); return false;
  }
  if (device->computeShader->native11 && device->classBindings[unsigned(dxvk::umd::ShaderStage::Compute)].pointers.size()
      != device->computeShader->native11->interfaceSlots) { device->error(E_INVALIDARG); return false; }
  const auto stage = unsigned(dxvk::umd::ShaderStage::Compute);
  for (UINT slot = 0; slot < device->boundSharedHigh[stage]; ++slot)
    if (!readSharedSurface(device, device->boundShared[stage][slot])) return false;
  return true;
}
void APIENTRY dispatch(D3D10DDI_HDEVICE h, UINT x, UINT y, UINT z) {
  auto device = get(h);
  if (x > D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION || y > D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION
      || z > D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION) { device->error(E_INVALIDARG); return; }
  if (!x || !y || !z) return;
  if (computeReady(device)) device->context->Dispatch(x, y, z);
}
bool indirectBuffer(Device* device, D3D10DDI_HRESOURCE resource, UINT offset, UINT bytes,
    ComPtr<ID3D11Buffer>& output) {
  if (!owned(device, get(resource))) return false;
  if (FAILED(get(resource)->backend.As(&output))) { device->error(E_INVALIDARG); return false; }
  D3D11_BUFFER_DESC info = {}; output->GetDesc(&info);
  if (!(info.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) || offset % 4
      || offset > info.ByteWidth || bytes > info.ByteWidth - offset) { device->error(E_INVALIDARG); return false; }
  return true;
}
void APIENTRY dispatchIndirect(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT offset) {
  auto device = get(h); ComPtr<ID3D11Buffer> buffer;
  if (indirectBuffer(device, resource, offset, 12, buffer) && computeReady(device)) device->context->DispatchIndirect(buffer.Get(), offset);
}
void APIENTRY drawInstancedIndirect(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT offset) {
  auto device = get(h); ComPtr<ID3D11Buffer> buffer;
  if (indirectBuffer(device, resource, offset, 16, buffer) && drawReady(device)) device->context->DrawInstancedIndirect(buffer.Get(), offset);
}
void APIENTRY drawIndexedInstancedIndirect(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT offset) {
  auto device = get(h); ComPtr<ID3D11Buffer> buffer;
  if (indirectBuffer(device, resource, offset, 20, buffer) && drawReady(device, true)) device->context->DrawIndexedInstancedIndirect(buffer.Get(), offset);
}
void APIENTRY copyStructureCount(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE resource, UINT offset,
    D3D11DDI_HUNORDEREDACCESSVIEW object) {
  auto device = get(h); auto view = get(object);
  if (!owned(device, get(resource))) return;
  ComPtr<ID3D11Buffer> buffer;
  if (!view || view->owner != device || !view->backend || !view->counter
      || FAILED(get(resource)->backend.As(&buffer))) { device->error(E_INVALIDARG); return; }
  D3D11_BUFFER_DESC info = {}; buffer->GetDesc(&info);
  if (info.Usage != D3D11_USAGE_DEFAULT || offset % 4 || offset > info.ByteWidth || 4 > info.ByteWidth - offset) {
    device->error(E_INVALIDARG); return;
  }
  device->context->CopyStructureCount(buffer.Get(), offset, view->backend.Get());
}
void APIENTRY setResourceMinLod(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE object, FLOAT lod) {
  auto device = get(h);
  if (!owned(device, get(object))) return;
  auto resource = get(object)->backend.Get();
  UINT misc = 0, mips = 0;
  ComPtr<ID3D11Texture1D> texture1;
  ComPtr<ID3D11Texture2D> texture2;
  ComPtr<ID3D11Texture3D> texture3;
  if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture1)))) {
    D3D11_TEXTURE1D_DESC info = {}; texture1->GetDesc(&info); misc = info.MiscFlags; mips = info.MipLevels;
  } else if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture2)))) {
    D3D11_TEXTURE2D_DESC info = {}; texture2->GetDesc(&info); misc = info.MiscFlags; mips = info.MipLevels;
  } else if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture3)))) {
    D3D11_TEXTURE3D_DESC info = {}; texture3->GetDesc(&info); misc = info.MiscFlags; mips = info.MipLevels;
  }
  if (!(misc & D3D11_RESOURCE_MISC_RESOURCE_CLAMP) || !std::isfinite(lod)
      || lod < 0 || lod > static_cast<FLOAT>(mips)) { device->error(E_INVALIDARG); return; }
  device->context->SetResourceMinLOD(resource, lod);
}

SIZE_T APIENTRY tessellationSize(D3D10DDI_HDEVICE, const UINT*, const D3D11DDIARG_TESSELLATION_IO_SIGNATURES*) { return sizeof(Shader); }
SIZE_T APIENTRY geometryStreamSize11(D3D10DDI_HDEVICE,
    const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT*, const D3D10DDIARG_STAGE_IO_SIGNATURES*) { return sizeof(Shader); }
void APIENTRY relocateDeviceFunctions10_1(D3D10DDI_HDEVICE h, D3D10_1DDI_DEVICEFUNCS* functions) {
  if (!functions) get(h)->error(E_INVALIDARG);
}
void APIENTRY relocateDeviceFunctions11(D3D10DDI_HDEVICE h, D3D11DDI_DEVICEFUNCS* functions) {
  if (!functions) get(h)->error(E_INVALIDARG);
}
// A real bit-copy implements identity conversion. Format conversion still
// needs a rendering pass; reject it explicitly instead of issuing an invalid
// public CopyResource call whose void return could look like success.
bool conversionIdentity(Device* device, D3D10DDI_HRESOURCE dst, UINT dstIndex,
    D3D10DDI_HRESOURCE src, UINT srcIndex) {
  if (!owned(device, get(dst)) || !owned(device, get(src))) return false;
  SubresourceInfo destination, source;
  if (!subresourceInfo(get(dst), dstIndex, destination) || !subresourceInfo(get(src), srcIndex, source)
      || destination.format != source.format || destination.dimension != source.dimension) {
    device->error(DXGI_ERROR_UNSUPPORTED); return false;
  }
  return true;
}
void APIENTRY convertResource(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, D3D10DDI_HRESOURCE src) {
  if (conversionIdentity(get(h), dst, 0, src, 0)) copyResource(h, dst, src);
}
void APIENTRY convertResourceRegion(D3D10DDI_HDEVICE h, D3D10DDI_HRESOURCE dst, UINT dstIndex,
    UINT x, UINT y, UINT z, D3D10DDI_HRESOURCE src, UINT srcIndex, const D3D10_DDI_BOX* box) {
  if (conversionIdentity(get(h), dst, dstIndex, src, srcIndex)) copyRegion(h, dst, dstIndex, x, y, z, src, srcIndex, box);
}
