// SPDX-License-Identifier: MIT
// Diagnostic reference calls only; never a substitute for the strict DDI fixtures.
#include "umd-probe-shaders.h"
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <algorithm>
#include <stdexcept>
using Microsoft::WRL::ComPtr;

// Report infrastructure errors without letting a partial diagnostic look complete.
static void require(HRESULT hr, const char* operation) {
  if (FAILED(hr)) {
    std::fprintf(stderr, "CONTROL failure\noperation=%s\nHRESULT=%08lx\n", operation, (unsigned long)hr);
    throw std::runtime_error(operation);
  }
}
// The optional debug layer adds messages, not a different graphics backend.
static void dumpMessages(ID3D11Device* device) {
  ComPtr<ID3D11InfoQueue> queue;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(&queue)))) return;
  for (UINT64 i = 0; i < queue->GetNumStoredMessages(); ++i) {
    SIZE_T size = 0; queue->GetMessage(i, nullptr, &size);
    std::vector<unsigned char> storage(size);
    auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
    if (SUCCEEDED(queue->GetMessage(i, message, &size)))
      std::printf("D3D11_MESSAGE\nseverity=%u\nid=%u\ntext=%s\n", message->Severity, message->ID, message->pDescription);
  }
  queue->ClearStoredMessages();
}
// Independently recreate the GS container and compare native shader validation.
static void geometryControl(ID3D11Device* device) {
  const char* source = R"(
struct Output { float4 position : SV_Position; uint4 payload : TEXCOORD0; };
[maxvertexcount(3)]
void gs(triangle float4 positions[3] : SV_Position, inout TriangleStream<Output> stream) {
  [unroll] for (uint i = 0; i < 3; ++i) {
    Output value; value.position = positions[i];
    value.payload = uint4(0x11223344,0x87654321,0x7fc01234,0x80000000);
    stream.Append(value);
  }
  stream.RestartStrip();
})";
  std::vector<uint32_t> tokens; ComPtr<ID3DBlob> original;
  if (!compileHlslTokens(source, "gs", "gs_4_0", tokens, &original)) throw std::runtime_error("compile GS");
  ComPtr<ID3D11ShaderReflection> reflection;
  require(D3DReflect(original->GetBufferPointer(), original->GetBufferSize(), IID_PPV_ARGS(&reflection)), "reflect GS");
  D3D11_SHADER_DESC desc{}; require(reflection->GetDesc(&desc), "GS description");
  std::vector<dxvk::umd::ShaderSignatureEntry> inputs, outputs, resolved, linked;
  for (bool input : {true, false}) {
    for (UINT i = 0; i < (input ? desc.InputParameters : desc.OutputParameters); ++i) {
      D3D11_SIGNATURE_PARAMETER_DESC entry{};
      require(input ? reflection->GetInputParameterDesc(i, &entry) : reflection->GetOutputParameterDesc(i, &entry), "GS signature");
      (input ? inputs : outputs).push_back({uint32_t(entry.SystemValueType), entry.Register, entry.Mask});
    }
  }
  if (!dxvk::umd::resolveGeometryInputs(tokens.data(), tokens.size(), inputs.data(), inputs.size(), resolved) ||
      !dxvk::umd::linkVertexOutputs(outputs.data(), outputs.size(), nullptr, 0, linked)) throw std::runtime_error("resolve GS");
  std::vector<unsigned char> binary;
  if (!dxvk::umd::buildShaderContainer(dxvk::umd::ShaderStage::Geometry, tokens.data(), tokens.size(),
      resolved.data(), resolved.size(), linked.data(), linked.size(), binary)) throw std::runtime_error("rebuild GS");
  for (bool rebuilt : {false, true}) {
    D3D11_SO_DECLARATION_ENTRY so[] = {
      {0, "SV_Position", 0, 0, 4, 0},
      {0, rebuilt ? dxvk::umd::varyingRegisterSemantic : "TEXCOORD", rebuilt ? 1u : 0u, 0, 4, 0}};
    for (UINT raster : {D3D11_SO_NO_RASTERIZED_STREAM, 0u}) {
      ComPtr<ID3D11GeometryShader> shader; UINT stride = 32;
      const HRESULT hr = device->CreateGeometryShaderWithStreamOutput(
        rebuilt ? binary.data() : original->GetBufferPointer(),
        rebuilt ? binary.size() : original->GetBufferSize(), so, 2, &stride, 1, raster, nullptr, &shader);
      std::printf("SO_CONTROL\nrebuilt=%u\nrasterized_stream=%u\nHRESULT=%08lx\n", rebuilt, raster, (unsigned long)hr);
      dumpMessages(device);
    }
  }
}
// Read every mip's first texel without using native UMD resource or mapping DDIs.
static void readControl(
    ID3D11Device* device, ID3D11DeviceContext* context,
    ID3D11Texture1D* texture, const char* phase) {
  D3D11_TEXTURE1D_DESC desc{}; texture->GetDesc(&desc);
  desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.BindFlags = desc.MiscFlags = 0;
  ComPtr<ID3D11Texture1D> staging;
  require(device->CreateTexture1D(&desc, nullptr, &staging), "control staging");
  context->CopyResource(staging.Get(), texture);
  std::printf("MIP_CONTROL\nphase=%s\n", phase);
  for (UINT sub = 0; sub < desc.MipLevels * desc.ArraySize; ++sub) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    require(context->Map(staging.Get(), sub, D3D11_MAP_READ, 0, &mapped), "control map");
    uint32_t value; std::memcpy(&value, mapped.pData, sizeof(value));
    std::printf("subresource=%u first=%08x\n", sub, value);
    context->Unmap(staging.Get(), sub);
  }
}
// Distinguish creation effects from the documented SRV-scoped GenerateMips call.
static void mipControl(ID3D11Device* device, ID3D11DeviceContext* context) {
  D3D11_TEXTURE1D_DESC desc{};
  desc.Width = 16; desc.MipLevels = 5; desc.ArraySize = 3;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  std::vector<std::vector<UINT>> pixels(15); D3D11_SUBRESOURCE_DATA data[15]{};
  for (UINT slice = 0; slice < 3; ++slice) for (UINT mip = 0; mip < 5; ++mip) {
    UINT sub = slice * 5 + mip; pixels[sub].resize(std::max(1u, 16u >> mip));
    for (UINT x = 0; x < pixels[sub].size(); ++x)
      pixels[sub][x] = slice && mip == 1 ? (slice == 1 ? 0xff0000ffu : 0xff00ff00u) :
        0xff000000u | (slice << 16) | (mip << 8) | (x + 1);
    data[sub] = {pixels[sub].data(), UINT(pixels[sub].size() * 4), UINT(pixels[sub].size() * 4)};
  }
  ComPtr<ID3D11Texture1D> texture;
  require(device->CreateTexture1D(&desc, data, &texture), "control Texture1D");
  readControl(device, context, texture.Get(), "before GenerateMips");
  D3D11_SHADER_RESOURCE_VIEW_DESC vd{}; vd.Format = desc.Format;
  vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
  vd.Texture1DArray = {1, 4, 1, 2};
  ComPtr<ID3D11ShaderResourceView> view;
  require(device->CreateShaderResourceView(texture.Get(), &vd, &view), "control SRV");
  D3D11_SHADER_RESOURCE_VIEW_DESC actual{}; view->GetDesc(&actual);
  std::printf("MIP_VIEW\nfirst_mip=%u\nmips=%u\nfirst_slice=%u\nslices=%u\n",
    actual.Texture1DArray.MostDetailedMip, actual.Texture1DArray.MipLevels,
    actual.Texture1DArray.FirstArraySlice, actual.Texture1DArray.ArraySize);
  context->GenerateMips(view.Get()); dumpMessages(device);
  readControl(device, context, texture.Get(), "after GenerateMips");
}
// Observations do not decide regression success: the strict production-DDI tests still run.
int main() {
  try {
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
      &level, 1, D3D11_SDK_VERSION, &device, nullptr, &context);
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
      std::puts("REFERENCE_DEBUG_LAYER=unavailable");
      hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        &level, 1, D3D11_SDK_VERSION, &device, nullptr, &context);
    }
    require(hr, "reference device");
    std::puts("REFERENCE_ONLY=WARP; no native admission or hardware assertion");
    geometryControl(device.Get()); mipControl(device.Get(), context.Get());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "REFERENCE_ERROR\nreason=%s\n", error.what()); return 1;
  }
}
