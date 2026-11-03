#include "../src/umd/umd_api.h"
#include "../src/util/log/log.h"
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>

namespace dxvk { Logger Logger::s_instance("dxvk-native-image-probe.log"); }

HRESULT exerciseNativeD3D(const LUID& luid, uint32_t width, uint32_t height, uint32_t stride,
    dxvk::umd::RuntimeBackend runtime, const mwd_allocation& native,
    const mwd_allocation& unusedNative, uint32_t unusedStride, bool rotate) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  // Rotation must join its actual asynchronous CS commit before returning.
  runtime.disableExtraThreads = !rotate;
  HRESULT hr = dxvk::umd::createDevice(luid, D3D_FEATURE_LEVEL_11_0, &device, &context, &runtime);
  std::printf("NATIVE_D3D_DEVICE hr=%08lx native=%u\n", (unsigned long)hr,
    unsigned(hr == S_OK && dxvk::umd::nativeSurfacesSupported(device.Get())));
  if (hr != S_OK || !dxvk::umd::nativeSurfacesSupported(device.Get())) return E_FAIL;
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_TYPELESS; desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  ComPtr<ID3D11Texture2D> image, readback;
  hr = dxvk::umd::createNativeTexture2D(device.Get(), desc, runtime, native, stride, &image);
  std::printf("NATIVE_D3D_IMAGE hr=%08lx\n", (unsigned long)hr);
  if (hr != S_OK || !image) return E_FAIL;
  auto unusedDesc = desc;
  if (!rotate) unusedDesc.Width = unusedDesc.Height = 64;
  ComPtr<ID3D11Texture2D> unusedImage;
  hr = dxvk::umd::createNativeTexture2D(device.Get(), unusedDesc, runtime, unusedNative, unusedStride, &unusedImage);
  std::printf("NATIVE_D3D_UNUSED_IMAGE hr=%08lx\n", (unsigned long)hr);
  if (hr != S_OK || !unusedImage) return E_FAIL;
  D3D11_RENDER_TARGET_VIEW_DESC view = {}; view.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  view.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  ComPtr<ID3D11RenderTargetView> target;
  hr = device->CreateRenderTargetView(image.Get(), &view, &target);
  if (hr != S_OK) return hr;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  hr = device->CreateTexture2D(&desc, nullptr, &readback);
  if (hr != S_OK) return hr;
  ComPtr<ID3D11DeviceContext1> context1;
  hr = context.As(&context1);
  if (hr != S_OK) return hr;
  const D3D11_RECT rect = {LONG(width / 4), LONG(height / 4), LONG(width * 3 / 4), LONG(height * 3 / 4)};
  if (rotate) {
    // Model a runtime-owned destination write with a distinct preserved image
    // alias. The original destination has never been used by this renderer.
    hr = dxvk::umd::prepareNativeTextureForExternalWrite(context.Get(), unusedImage.Get());
    if (hr != S_OK) return hr;
    ComPtr<ID3D11Texture2D> writer;
    hr = dxvk::umd::createNativeTexture2D(device.Get(), unusedDesc, runtime,
      unusedNative, unusedStride, &writer, true);
    if (hr != S_OK) return hr;
    ComPtr<ID3D11RenderTargetView> writerView, destinationView;
    hr = device->CreateRenderTargetView(writer.Get(), &view, &writerView);
    if (hr == S_OK) hr = device->CreateRenderTargetView(unusedImage.Get(), &view, &destinationView);
    if (hr != S_OK) return hr;
    for (unsigned pass = 0; pass < 2; ++pass) {
      const float blue[4] = {0, 0, 1, 1}, red[4] = {1, 0, 0, 1};
      context->ClearRenderTargetView(pass ? destinationView.Get() : writerView.Get(), pass ? red : blue);
      hr = pass ? dxvk::umd::prepareNativeTextureForExternalWrite(context.Get(), unusedImage.Get())
        : dxvk::umd::flushRuntimeSubmission(context.Get());
      if (hr != S_OK) return hr;
      context->CopyResource(readback.Get(), pass ? writer.Get() : unusedImage.Get());
      D3D11_MAPPED_SUBRESOURCE map = {};
      hr = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &map);
      if (hr != S_OK) return hr;
      uint64_t mismatches = !map.pData || map.RowPitch < width * 4;
      if (!mismatches) for (uint32_t y = 0; y < height; ++y) {
        const auto row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(map.pData) + uint64_t(y) * map.RowPitch);
        for (uint32_t x = 0; x < width; ++x) mismatches += row[x] != (pass ? 0xffff0000u : 0xff0000ffu);
      }
      context->Unmap(readback.Get(), 0);
      std::printf("NATIVE_HANDOFF_PIXELS pass=%u checked=%llu mismatches=%llu\n", pass,
        (unsigned long long)(uint64_t(width) * height), (unsigned long long)mismatches);
      if (mismatches) return E_FAIL;
    }
  }
  for (uint32_t frame = 0; frame < 2; ++frame) {
    const float red[4] = {1, 0, 0, 1}, green[4] = {0, 1, 0, 1};
    if (!frame) context->ClearRenderTargetView(target.Get(), red);
    else context1->ClearView(target.Get(), green, &rect, 1);
    context->CopyResource(readback.Get(), image.Get());
    // The queued command/storage owner must survive releasing application
    // texture and view references before the final synchronized readback.
    if (frame && !rotate) { target.Reset(); image.Reset(); }
    D3D11_MAPPED_SUBRESOURCE map = {};
    hr = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &map);
    std::printf("NATIVE_D3D_MAP frame=%u hr=%08lx stride=%u\n", frame, (unsigned long)hr, map.RowPitch);
    if (hr != S_OK) return hr;
    uint64_t mismatches = 0;
    if (!map.pData || map.RowPitch < width * 4) mismatches = 1;
    else for (uint32_t y = 0; y < height; ++y) {
      const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(map.pData) + uint64_t(y) * map.RowPitch);
      for (uint32_t x = 0; x < width; ++x) {
        const bool inside = frame && x >= uint32_t(rect.left) && x < uint32_t(rect.right)
          && y >= uint32_t(rect.top) && y < uint32_t(rect.bottom);
        const uint32_t expected = inside ? 0xff00ff00 : 0xffff0000;
        if (row[x] != expected) {
          if (mismatches < 4) std::printf("NATIVE_D3D_MISMATCH frame=%u x=%u y=%u actual=%08x expected=%08x\n",
            frame, x, y, row[x], expected);
          ++mismatches;
        }
      }
    }
    context->Unmap(readback.Get(), 0);
    std::printf("NATIVE_D3D_PIXELS frame=%u checked=%llu mismatches=%llu partial=%u\n", frame,
      (unsigned long long)(uint64_t(width) * height), (unsigned long long)mismatches, frame);
    if (mismatches) return E_FAIL;
  }
  if (rotate) {
    ComPtr<ID3D11RenderTargetView> secondTarget;
    hr = device->CreateRenderTargetView(unusedImage.Get(), &view, &secondTarget);
    if (hr != S_OK) return hr;
    const float blue[4] = {0, 0, 1, 1};
    context->ClearRenderTargetView(secondTarget.Get(), blue);
    ID3D11Texture2D* textures[] = {image.Get(), unusedImage.Get()};
    hr = dxvk::umd::rotateNativeTextureStorage(context.Get(), 2, textures);
    std::printf("NATIVE_D3D_ROTATE hr=%08lx\n", (unsigned long)hr);
    if (hr != S_OK) return hr;
    // Reopen the original AHB after both frames. It must retain the first
    // texture's red/green pixels, which now belong to textures[1].
    ComPtr<ID3D11Texture2D> reopened;
    auto reopenDesc = unusedDesc;
    hr = dxvk::umd::createNativeTexture2D(device.Get(), reopenDesc, runtime, native, stride, &reopened, true);
    if (hr != S_OK) return hr;
    auto verify = [&](ID3D11Texture2D* texture, unsigned pattern) -> HRESULT {
      context->CopyResource(readback.Get(), texture);
      D3D11_MAPPED_SUBRESOURCE map = {};
      const HRESULT mapped = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &map);
      if (mapped != S_OK) return mapped;
      uint64_t mismatches = !map.pData || map.RowPitch < width * 4;
      if (!mismatches) for (uint32_t y = 0; y < height; ++y) {
        const auto row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(map.pData) + uint64_t(y) * map.RowPitch);
        for (uint32_t x = 0; x < width; ++x) {
          const bool inside = x >= uint32_t(rect.left) && x < uint32_t(rect.right)
            && y >= uint32_t(rect.top) && y < uint32_t(rect.bottom);
          const uint32_t expected = pattern == 0 ? 0xff0000ff
            : pattern == 1 ? (inside ? 0xff00ff00 : 0xffff0000)
            : inside ? 0xff00ffff : 0xff0000ff;
          if (row[x] != expected) {
            if (mismatches < 4) std::printf("NATIVE_ROTATION_MISMATCH pattern=%u x=%u y=%u actual=%08x expected=%08x\n",
              pattern, x, y, row[x], expected);
            ++mismatches;
          }
        }
      }
      context->Unmap(readback.Get(), 0);
      std::printf("NATIVE_ROTATION_PIXELS pattern=%u checked=%llu mismatches=%llu\n",
        pattern, (unsigned long long)(uint64_t(width) * height), (unsigned long long)mismatches);
      return mismatches ? E_FAIL : S_OK;
    };
    for (unsigned i = 0; i < 2; ++i) if ((hr = verify(textures[i], i)) != S_OK) return hr;
    if ((hr = verify(reopened.Get(), 1)) != S_OK) return hr;
    const float cyan[4] = {0, 1, 1, 1};
    // Use the RTV created BEFORE storage rotation. It must follow its texture
    // to the new AHB, retaining blue outside the updated rectangle.
    context1->ClearView(target.Get(), cyan, &rect, 1);
    if ((hr = verify(image.Get(), 2)) != S_OK) return hr;
    if ((hr = verify(reopened.Get(), 1)) != S_OK) return hr;
    // Ordinary optimal storage and imported linear AHB storage have different
    // layouts and ownership. Preserve existing RTV/SRV objects through a full
    // three-way cycle and render via the already-used SRVs after every change.
    ComPtr<ID3D11Texture2D> ordinary, output;
    if ((hr = device->CreateTexture2D(&unusedDesc, nullptr, &ordinary)) != S_OK) return hr;
    if ((hr = device->CreateTexture2D(&unusedDesc, nullptr, &output)) != S_OK) return hr;
    ID3D11Texture2D* mixed[] = {image.Get(), ordinary.Get(), unusedImage.Get()};
    ComPtr<ID3D11RenderTargetView> views[3], outputView;
    ComPtr<ID3D11ShaderResourceView> sources[3];
    D3D11_SHADER_RESOURCE_VIEW_DESC srv = {}; srv.Format = view.Format;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srv.Texture2D.MipLevels = 1;
    for (unsigned i = 0; i < 3; ++i) {
      if ((hr = device->CreateRenderTargetView(mixed[i], &view, &views[i])) != S_OK) return hr;
      if ((hr = device->CreateShaderResourceView(mixed[i], &srv, &sources[i])) != S_OK) return hr;
    }
    if ((hr = device->CreateRenderTargetView(output.Get(), &view, &outputView)) != S_OK) return hr;
    HMODULE compiler = LoadLibraryExW(L"d3dcompiler_47.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!compiler) return HRESULT_FROM_WIN32(GetLastError());
    struct CompilerOwner { HMODULE module; ~CompilerOwner() { FreeLibrary(module); } } compilerOwner{compiler};
    const auto compile = reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(compiler, "D3DCompile"));
    if (!compile) return E_NOINTERFACE;
    const char vs[] = "float4 main(uint id:SV_VertexID):SV_Position { return float4(id==1?3:-1,id==2?-3:1,0,1); }";
    const char ps[] = "Texture2D<float4> t:register(t0); float4 main(float4 p:SV_Position):SV_Target { return t.Load(int3(p.xy,0)); }";
    ComPtr<ID3DBlob> vcode, pcode, errors;
    if ((hr = compile(vs, sizeof(vs)-1, "rotation-vs", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vcode, &errors)) != S_OK) return hr;
    errors.Reset();
    if ((hr = compile(ps, sizeof(ps)-1, "rotation-ps", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pcode, &errors)) != S_OK) return hr;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> pixel;
    if ((hr = device->CreateVertexShader(vcode->GetBufferPointer(), vcode->GetBufferSize(), nullptr, &vertex)) != S_OK) return hr;
    if ((hr = device->CreatePixelShader(pcode->GetBufferPointer(), pcode->GetBufferSize(), nullptr, &pixel)) != S_OK) return hr;
    D3D11_RASTERIZER_DESC raster = {}; raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rasterState;
    if ((hr = device->CreateRasterizerState(&raster, &rasterState)) != S_OK) return hr;
    context->RSSetState(rasterState.Get());
    const D3D11_VIEWPORT viewport = {0, 0, float(width), float(height), 0, 1};
    context->RSSetViewports(1, &viewport); context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(), nullptr, 0); context->PSSetShader(pixel.Get(), nullptr, 0);
    auto checkMixed = [&](ID3D11Texture2D* texture, uint32_t expected, bool partial) -> HRESULT {
      context->CopyResource(readback.Get(), texture);
      D3D11_MAPPED_SUBRESOURCE map = {};
      HRESULT result = context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &map);
      if (result != S_OK) return result;
      uint64_t bad = !map.pData || map.RowPitch < width * 4;
      if (!bad) for (uint32_t y = 0; y < height; ++y) {
        auto row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(map.pData) + uint64_t(y) * map.RowPitch);
        for (uint32_t x = 0; x < width; ++x) {
          bool inside = partial && x >= uint32_t(rect.left) && x < uint32_t(rect.right)
            && y >= uint32_t(rect.top) && y < uint32_t(rect.bottom);
          bad += row[x] != (inside ? 0xff00ffffu : expected);
        }
      }
      context->Unmap(readback.Get(), 0);
      std::printf("MIXED_IMAGE_PIXELS expected=%08x partial=%u checked=%llu mismatches=%llu\n",
        expected, unsigned(partial), (unsigned long long)(uint64_t(width) * height), (unsigned long long)bad);
      return bad ? E_FAIL : S_OK;
    };
    auto sampleOldView = [&](unsigned i, uint32_t expected, bool partial) -> HRESULT {
      auto rt = outputView.Get(); auto source = sources[i].Get();
      context->OMSetRenderTargets(1, &rt, nullptr); context->PSSetShaderResources(0, 1, &source);
      context->Draw(3, 0);
      source = nullptr; context->PSSetShaderResources(0, 1, &source); context->OMSetRenderTargets(0, nullptr, nullptr);
      return checkMixed(output.Get(), expected, partial);
    };
    const float colors[3][4] = {{1,0,0,1}, {0,0,1,1}, {0,1,0,1}};
    uint32_t expected[3] = {0xffff0000u, 0xff0000ffu, 0xff00ff00u};
    for (unsigned i = 0; i < 3; ++i) {
      context->ClearRenderTargetView(views[i].Get(), colors[i]);
      if ((hr = sampleOldView(i, expected[i], false)) != S_OK) return hr;
    }
    // Reject the owner commit after preparation. No image identity may move.
    unsigned rejected = 0;
    hr = dxvk::umd::rotateNativeTextureStorage(context.Get(), 3, mixed,
      [&](const std::function<void()>&) { Sleep(75); ++rejected; return E_OUTOFMEMORY; });
    if (hr != E_OUTOFMEMORY || rejected != 1) return E_FAIL;
    for (unsigned i = 0; i < 3; ++i)
      if ((hr = checkMixed(mixed[i], expected[i], false)) != S_OK) return hr;
    for (unsigned pass = 0; pass < 3; ++pass) {
      // Unbound clears stay deferred until rotation prepares each old image.
      for (unsigned i = 0; i < 3; ++i) context->ClearRenderTargetView(views[i].Get(), colors[i]);
      hr = dxvk::umd::rotateNativeTextureStorage(context.Get(), 3, mixed);
      std::printf("MIXED_IMAGE_ROTATION pass=%u hr=%08lx\n", pass, (unsigned long)hr);
      if (hr != S_OK) return hr;
      for (unsigned i = 0; i < 3; ++i) {
        if ((hr = checkMixed(mixed[i], expected[(i+1)%3], false)) != S_OK) return hr;
        if ((hr = sampleOldView(i, expected[(i+1)%3], false)) != S_OK) return hr;
        context1->ClearView(views[i].Get(), cyan, &rect, 1);
        if ((hr = sampleOldView(i, expected[(i+1)%3], true)) != S_OK) return hr;
      }
    }
    context->ClearState();
    if ((hr = dxvk::umd::flushRuntimeSubmission(context.Get())) != S_OK) return hr;
  }
  return device->GetDeviceRemovedReason();
}
