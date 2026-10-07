// Included in umd_ddi.cpp's private namespace. Every entry uses the exact SDK
// D3D11 callback type and the same callable Device owner as the other DDIs.
bool signatureRange11(const D3D10DDIARG_SIGNATURE_ENTRY* entries, UINT count) {
  if (count > 128 || (count && !entries)) return false;
  for (UINT i = 0; i < count; ++i)
    if (!entries[i].Mask || (entries[i].Mask & ~15u) || UINT(entries[i].SystemValue) > 22
        || (entries[i].Register >= 32 && entries[i].Register != UINT(-1))) return false;
  return true;
}
void signatureOrdinals11(std::vector<dxvk::umd::ShaderIo11>& decoded,
    const D3D10DDIARG_SIGNATURE_ENTRY* entries, UINT count) {
  // The runtime union includes unused clip/cull registers. Its ordinal must
  // survive when a consumer uses only a later array element.
  for (auto& entry : decoded) if (entry.systemValue == 2 || entry.systemValue == 3) {
    entry.semanticIndex = 0;
    for (UINT i = 0; i < count; ++i) {
      if (UINT(entries[i].SystemValue) != entry.systemValue || entries[i].Register >= entry.registerIndex) continue;
      bool earlier = false;
      for (UINT j = 0; j < i; ++j)
        earlier |= entries[j].SystemValue == entries[i].SystemValue && entries[j].Register == entries[i].Register;
      entry.semanticIndex += !earlier;
    }
  }
}
HRESULT compileNativeShader11(Device* device, Shader& shader, const dxvk::umd::ShaderCode11& code) {
  using dxvk::umd::ShaderStage;
  std::vector<unsigned char> binary;
  if (!dxvk::umd::buildShader11Container(code, binary)) return E_INVALIDARG;
  if (shader.compiledNative11 == binary) return S_OK;
  const auto linkage = shader.linkage.Get();
  HRESULT hr = E_INVALIDARG;
  switch (shader.stage) {
    case ShaderStage::Vertex: {
      ComPtr<ID3D11VertexShader> compiled;
      hr = device->backend->CreateVertexShader(binary.data(), binary.size(), linkage, &compiled);
      if (hr == S_OK && compiled) shader.vertex = std::move(compiled);
      else if (!FAILED(hr)) hr = E_FAIL;
    } break;
    case ShaderStage::Pixel: {
      ComPtr<ID3D11PixelShader> compiled;
      hr = device->backend->CreatePixelShader(binary.data(), binary.size(), linkage, &compiled);
      if (hr == S_OK && compiled) shader.pixel = std::move(compiled);
      else if (!FAILED(hr)) hr = E_FAIL;
    } break;
    case ShaderStage::Geometry: {
      ComPtr<ID3D11GeometryShader> compiled;
      if (shader.withStreamOutput) {
        std::vector<D3D11_SO_DECLARATION_ENTRY> entries;
        entries.reserve(shader.nativeStream.entries.size());
        for (const auto& entry : shader.nativeStream.entries)
          entries.push_back({entry.stream, entry.semantic.empty() ? nullptr : entry.semantic.c_str(),
            entry.semanticIndex, entry.start, entry.count, entry.slot});
        hr = device->backend->CreateGeometryShaderWithStreamOutput(binary.data(), binary.size(),
          entries.empty() ? nullptr : entries.data(), UINT(entries.size()), shader.nativeStream.strides.data(),
          shader.nativeStream.strideCount, shader.nativeStream.rasterizedStream, linkage, &compiled);
      } else hr = device->backend->CreateGeometryShader(binary.data(), binary.size(), linkage, &compiled);
      if (hr == S_OK && compiled) shader.geometry = std::move(compiled);
      else if (!FAILED(hr)) hr = E_FAIL;
    } break;
    case ShaderStage::Hull: {
      ComPtr<ID3D11HullShader> compiled;
      hr = device->backend->CreateHullShader(binary.data(), binary.size(), linkage, &compiled);
      if (hr == S_OK && compiled) shader.hull = std::move(compiled);
      else if (!FAILED(hr)) hr = E_FAIL;
    } break;
    case ShaderStage::Domain: {
      ComPtr<ID3D11DomainShader> compiled;
      hr = device->backend->CreateDomainShader(binary.data(), binary.size(), linkage, &compiled);
      if (hr == S_OK && compiled) shader.domain = std::move(compiled);
      else if (!FAILED(hr)) hr = E_FAIL;
    } break;
    case ShaderStage::Compute: {
      ComPtr<ID3D11ComputeShader> compiled;
      hr = device->backend->CreateComputeShader(binary.data(), binary.size(), linkage, &compiled);
      if (hr == S_OK && compiled) shader.compute = std::move(compiled);
      else if (!FAILED(hr)) hr = E_FAIL;
    } break;
  }
  if (hr == S_OK) shader.compiledNative11 = std::move(binary);
  return hr;
}
HRESULT createNativeShader11(Device* device, const UINT* code, D3D10DDI_HSHADER output,
    dxvk::umd::ShaderStage stage, const D3D10DDIARG_STAGE_IO_SIGNATURES* signature = nullptr,
    const D3D11DDIARG_TESSELLATION_IO_SIGNATURES* tessellation = nullptr,
    const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* stream = nullptr) {
  using dxvk::umd::ShaderStage;
  if (stage >= ShaderStage::Hull && device->featureLevel < D3D_FEATURE_LEVEL_11_0) return DXGI_ERROR_UNSUPPORTED;
  if (!output.pDrvPrivate || uintptr_t(output.pDrvPrivate) % alignof(Shader) || !code) return E_INVALIDARG;
  if ((stage == ShaderStage::Hull || stage == ShaderStage::Domain) && !tessellation) return E_INVALIDARG;
  if ((signature && (!signatureRange11(signature->pInputSignature, signature->NumInputSignatureEntries)
      || !signatureRange11(signature->pOutputSignature, signature->NumOutputSignatureEntries)))
      || (tessellation && (!signatureRange11(tessellation->pInputSignature, tessellation->NumInputSignatureEntries)
      || !signatureRange11(tessellation->pOutputSignature, tessellation->NumOutputSignatureEntries)
      || !signatureRange11(tessellation->pPatchConstantSignature, tessellation->NumPatchConstantSignatureEntries)))) return E_INVALIDARG;
  const UINT maximum = device->featureLevel >= D3D_FEATURE_LEVEL_11_0 ? 0x50
    : device->featureLevel >= D3D_FEATURE_LEVEL_10_1 ? 0x41 : 0x40;
  if ((code[0] & 0xffff) > maximum) return DXGI_ERROR_UNSUPPORTED;
  auto codeStage = stage;
  if (stream) {
    codeStage = ShaderStage(code[0] >> 16);
    if (codeStage != ShaderStage::Geometry && codeStage != ShaderStage::Vertex && codeStage != ShaderStage::Domain) return E_INVALIDARG;
    if (stream->NumEntries > 512 || (stream->NumEntries && !stream->pOutputStreamDecl)
        || stream->NumStrides > 4 || (stream->NumStrides && !stream->BufferStridesInBytes)) return E_INVALIDARG;
  }
  HRESULT hr = E_INVALIDARG;
  {
    Shader staged; staged.owner = device; staged.stage = stage;
    try {
      dxvk::umd::ShaderCode11 decoded;
      if (!dxvk::umd::decodeShader11(codeStage, code, code[1], decoded)) return E_INVALIDARG;
      if (signature) {
        signatureOrdinals11(decoded.inputs, signature->pInputSignature, signature->NumInputSignatureEntries);
        signatureOrdinals11(decoded.outputs, signature->pOutputSignature, signature->NumOutputSignatureEntries);
      }
      if (tessellation) {
        // A hull shader may omit its identity control-point phase. The union
        // carries those implicit I/O declarations which have no raw tokens.
        if (stage == ShaderStage::Hull && !decoded.hullControlPhase) {
          decoded.inputs.clear(); decoded.outputs.clear();
          for (UINT i = 0; i < tessellation->NumOutputSignatureEntries; ++i) {
            const auto& entry = tessellation->pOutputSignature[i];
            if (entry.Register >= 32) return E_INVALIDARG;
            const auto scalar = entry.SystemValue == D3D10_SB_NAME_POSITION || entry.SystemValue == D3D10_SB_NAME_CLIP_DISTANCE
              || entry.SystemValue == D3D10_SB_NAME_CULL_DISTANCE ? dxvk::umd::ShaderScalar::Float32 : dxvk::umd::ShaderScalar::Uint32;
            decoded.outputs.push_back({UINT(entry.SystemValue), entry.Register, entry.Mask, scalar});
          }
          // Preserve registers used only by a patch function as well.
          for (UINT i = 0; i < tessellation->NumInputSignatureEntries; ++i) {
            const auto& entry = tessellation->pInputSignature[i];
            if (entry.Register >= 32) continue;
            const auto scalar = entry.SystemValue == D3D10_SB_NAME_POSITION || entry.SystemValue == D3D10_SB_NAME_CLIP_DISTANCE
              || entry.SystemValue == D3D10_SB_NAME_CULL_DISTANCE ? dxvk::umd::ShaderScalar::Float32 : dxvk::umd::ShaderScalar::Uint32;
            decoded.inputs.push_back({UINT(entry.SystemValue), entry.Register, entry.Mask, scalar});
          }
        }
        signatureOrdinals11(decoded.inputs, tessellation->pInputSignature, tessellation->NumInputSignatureEntries);
        signatureOrdinals11(decoded.outputs, tessellation->pOutputSignature, tessellation->NumOutputSignatureEntries);
      }
      staged.retirement = std::make_unique<ComRetirement>();
      if (decoded.interfaceSlots) {
        hr = device->backend->CreateClassLinkage(&staged.linkage);
        if (hr != S_OK || !staged.linkage) return FAILED(hr) ? hr : E_FAIL;
      }
      if (stream) {
        std::vector<dxvk::umd::ShaderStreamDeclaration11> entries;
        entries.reserve(stream->NumEntries);
        for (UINT i = 0; i < stream->NumEntries; ++i) {
          const auto& entry = stream->pOutputStreamDecl[i];
          entries.push_back({entry.Stream, entry.OutputSlot, entry.RegisterIndex, entry.RegisterMask});
        }
        if (!dxvk::umd::shader11StreamOutput(decoded, entries.data(), entries.size(), stream->BufferStridesInBytes,
            stream->NumStrides, stream->RasterizedStream, staged.nativeStream)) return E_INVALIDARG;
        staged.withStreamOutput = true;
      }
      hr = compileNativeShader11(device, staged, decoded);
      if (hr == S_OK && device->retired) hr = DXGI_ERROR_DEVICE_REMOVED;
      if (hr == S_OK) {
        staged.native11 = std::move(decoded);
        new (output.pDrvPrivate) Shader(std::move(staged));
      }
    } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
      catch (...) { hr = E_FAIL; }
  }
  return hr;
}
void APIENTRY createVertexShader11(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER output, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* signature) {
  get(h)->error(createNativeShader11(get(h), code, output, dxvk::umd::ShaderStage::Vertex, signature));
}
void APIENTRY createPixelShader11(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER output, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* signature) {
  get(h)->error(createNativeShader11(get(h), code, output, dxvk::umd::ShaderStage::Pixel, signature));
}
void APIENTRY createGeometryShader11(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER output, D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* signature) {
  get(h)->error(createNativeShader11(get(h), code, output, dxvk::umd::ShaderStage::Geometry, signature));
}
void APIENTRY createGeometryStream11(D3D10DDI_HDEVICE h,
    const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT* stream, D3D10DDI_HSHADER output,
    D3D10DDI_HRTSHADER, const D3D10DDIARG_STAGE_IO_SIGNATURES* signature) {
  if (!stream) { get(h)->error(E_INVALIDARG); return; }
  get(h)->error(createNativeShader11(get(h), stream->pShaderCode, output, dxvk::umd::ShaderStage::Geometry, signature, nullptr, stream));
}
void APIENTRY createHullShader11(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER output, D3D10DDI_HRTSHADER, const D3D11DDIARG_TESSELLATION_IO_SIGNATURES* signature) {
  get(h)->error(createNativeShader11(get(h), code, output, dxvk::umd::ShaderStage::Hull, nullptr, signature));
}
void APIENTRY createDomainShader11(D3D10DDI_HDEVICE h, const UINT* code,
    D3D10DDI_HSHADER output, D3D10DDI_HRTSHADER, const D3D11DDIARG_TESSELLATION_IO_SIGNATURES* signature) {
  get(h)->error(createNativeShader11(get(h), code, output, dxvk::umd::ShaderStage::Domain, nullptr, signature));
}
void APIENTRY createComputeShader11(D3D10DDI_HDEVICE h, const UINT* code, D3D10DDI_HSHADER output, D3D10DDI_HRTSHADER) {
  get(h)->error(createNativeShader11(get(h), code, output, dxvk::umd::ShaderStage::Compute));
}
void bindNativeShader11(Device* device, dxvk::umd::ShaderStage stage, Shader* shader, const NativeClassBindings11& bindings) {
  using dxvk::umd::ShaderStage;
  auto classes = bindings.pointers.empty() ? nullptr : bindings.pointers.data();
  const UINT count = UINT(bindings.pointers.size());
  switch (stage) {
    case ShaderStage::Vertex: device->context->VSSetShader(shader ? shader->vertex.Get() : nullptr, classes, count); break;
    case ShaderStage::Pixel: device->context->PSSetShader(shader ? shader->pixel.Get() : nullptr, classes, count); break;
    case ShaderStage::Geometry: device->context->GSSetShader(shader ? shader->geometry.Get() : nullptr, classes, count); break;
    case ShaderStage::Hull: device->context->HSSetShader(shader ? shader->hull.Get() : nullptr, classes, count); break;
    case ShaderStage::Domain: device->context->DSSetShader(shader ? shader->domain.Get() : nullptr, classes, count); break;
    case ShaderStage::Compute: device->context->CSSetShader(shader ? shader->compute.Get() : nullptr, classes, count); break;
  }
}
void recordNativeShader11(Device* device, dxvk::umd::ShaderStage stage, Shader* shader) {
  using dxvk::umd::ShaderStage;
  switch (stage) {
    case ShaderStage::Vertex: device->vertexShader = shader; device->vertexBound = !!shader; break;
    case ShaderStage::Pixel: device->pixelShader = shader; device->pixelBound = !!shader; break;
    case ShaderStage::Geometry: device->geometryShader = shader; break;
    case ShaderStage::Hull: device->hullShader = shader; break;
    case ShaderStage::Domain: device->domainShader = shader; break;
    case ShaderStage::Compute: device->computeShader = shader; break;
  }
}
template<dxvk::umd::ShaderStage Stage>
void APIENTRY setShaderWithInterfaces11(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER handle,
    UINT count, const UINT* tables, const D3D11DDIARG_POINTERDATA* locations) {
  auto device = get(h); auto shader = get(handle);
  HRESULT hr = E_INVALIDARG;
  const bool ownedShader = shader && shader->owner == device && shader->native11;
  {
    NativeClassBindings11 staged;
    try {
      if ((!shader && !count) || (ownedShader && shader->stage == Stage && count == shader->native11->interfaceSlots
          && count <= 253 && (!count || (tables && locations)))) {
        hr = S_OK;
        staged.owners.resize(count); staged.pointers.resize(count);
        for (UINT i = 0; i < count; ++i) {
          bool active = false;
          for (const auto& iface : shader->native11->interfaces) active |= i >= iface.first && i - iface.first < iface.count;
          if (!active) continue;
          const auto& location = locations[i]; UINT offset = 0;
          if (location.uReserved || !dxvk::umd::shader11InterfaceTable(*shader->native11, i, tables[i])
              || !dxvk::umd::shader11ClassPointer(location.uCBID, location.uCBOffset, location.uBaseTex,
                  location.uBaseSamp, offset)) { hr = E_INVALIDARG; break; }
          const auto name = dxvk::umd::shader11ClassName(tables[i]);
          hr = shader->linkage->CreateClassInstance(name.c_str(), location.uCBID, offset,
            location.uBaseTex, location.uBaseSamp, &staged.owners[i]);
          if (hr != S_OK || !staged.owners[i]) { if (!FAILED(hr)) hr = E_FAIL; break; }
          staged.pointers[i] = staged.owners[i].Get();
        }
        if (hr == S_OK && device->retired) hr = DXGI_ERROR_DEVICE_REMOVED;
        if (hr == S_OK) {
          bindNativeShader11(device, Stage, shader, staged);
          device->classBindings[UINT(Stage)] = std::move(staged);
          recordNativeShader11(device, Stage, shader);
        }
      }
    } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
      catch (...) { hr = DXGI_ERROR_DEVICE_REMOVED; }
  }
  // WDK: after any error from SetShaderWithIfaces the runtime invalidates
  // this handle and will never call DestroyShader. Retire our own child now.
  if (FAILED(hr) && ownedShader) destroyShader(h, handle);
  device->error(hr);
}
template<dxvk::umd::ShaderStage Stage>
void APIENTRY setShader11(D3D10DDI_HDEVICE h, D3D10DDI_HSHADER handle) {
  setShaderWithInterfaces11<Stage>(h, handle, 0, nullptr, nullptr);
}
void APIENTRY setTopology11(D3D10DDI_HDEVICE h, D3D10_DDI_PRIMITIVE_TOPOLOGY native) {
  auto device = get(h); D3D11_PRIMITIVE_TOPOLOGY topology;
  const UINT value = UINT(native);
  if (value >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST && value <= D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST) {
    if (device->featureLevel < D3D_FEATURE_LEVEL_11_0) { device->error(DXGI_ERROR_UNSUPPORTED); return; }
    topology = D3D11_PRIMITIVE_TOPOLOGY(value);
  } else if (!dxvk::umd::primitiveTopology(native, topology)) { device->error(E_INVALIDARG); return; }
  device->context->IASetPrimitiveTopology(topology);
  device->topology = topology; device->topologyBound = topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}
bool prepareNativeGraphics11(Device* device) {
  using dxvk::umd::ShaderStage;
  using dxvk::umd::ShaderScalar;
  const bool patch = device->topology >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
    && device->topology <= D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;
  if (!!device->hullShader != !!device->domainShader || patch != !!device->hullShader) {
    device->error(E_INVALIDARG); return false;
  }
  const std::array<Shader*, 5> shaders = {device->vertexShader, device->hullShader, device->domainShader,
    device->geometryShader, device->pixelShader};
  std::array<std::optional<dxvk::umd::ShaderCode11>, 5> codes;
  try {
    for (size_t i = 0; i < shaders.size(); ++i) if (shaders[i]) {
      if (!shaders[i]->native11 || device->classBindings[UINT(shaders[i]->stage)].pointers.size() != shaders[i]->native11->interfaceSlots) {
        device->error(E_INVALIDARG); return false;
      }
      codes[i] = *shaders[i]->native11;
    }
    for (auto& input : codes[0]->inputs) if (!input.systemValue) {
      const auto layout = device->inputLayout;
      if (!layout || !layout->backend || input.registerIndex >= layout->inputTypes.size()
          || layout->inputTypes[input.registerIndex] == ShaderScalar::Unknown) { device->error(E_INVALIDARG); return false; }
      input.scalar = layout->inputTypes[input.registerIndex];
    }
    if (codes[4]) for (auto& output : codes[4]->outputs) if (output.systemValue == 64) {
      if (output.registerIndex >= device->targetTypes.size()) { device->error(E_INVALIDARG); return false; }
      if (device->targetTypes[output.registerIndex] != ShaderScalar::Unknown) output.scalar = device->targetTypes[output.registerIndex];
    }
    for (size_t i = 0; i + 1 < shaders.size(); ++i) if (shaders[i]) {
      size_t next = i + 1; while (next < shaders.size() && !shaders[next]) ++next;
      if (next == shaders.size()) continue;
      UINT stream = 0;
      if (shaders[i]->stage == ShaderStage::Geometry && shaders[i]->withStreamOutput) {
        stream = shaders[i]->nativeStream.rasterizedStream;
        if (stream == D3D11_SO_NO_RASTERIZED_STREAM) continue;
      }
      // A public GS made from VS/DS tokens is stream-output passthrough. The
      // backend consumes the upstream output signature as its input signature.
      const auto& inputs = shaders[next]->stage == ShaderStage::Geometry && codes[next]->stage != ShaderStage::Geometry
        ? codes[next]->outputs : codes[next]->inputs;
      std::vector<dxvk::umd::ShaderIo11> linked;
      if (!dxvk::umd::linkShader11Outputs(codes[i]->outputs, inputs, stream, linked)) { device->error(E_INVALIDARG); return false; }
      codes[i]->outputs = std::move(linked);
    }
    if (codes[1] && codes[2]) {
      std::vector<dxvk::umd::ShaderIo11> linked;
      if (!dxvk::umd::linkShader11Outputs(codes[1]->patch, codes[2]->patch, 0, linked)) { device->error(E_INVALIDARG); return false; }
      codes[1]->patch = std::move(linked);
    }
    // Compile every changed signature before committing the first binding.
    for (size_t i = 0; i < shaders.size(); ++i) if (shaders[i]) {
      const HRESULT hr = compileNativeShader11(device, *shaders[i], *codes[i]);
      if (FAILED(hr)) { device->error(hr); return false; }
    }
    for (auto shader : shaders) if (shader)
      bindNativeShader11(device, shader->stage, shader, device->classBindings[UINT(shader->stage)]);
    return true;
  } catch (const std::bad_alloc&) { device->error(E_OUTOFMEMORY); }
    catch (...) { device->error(E_FAIL); }
  return false;
}
