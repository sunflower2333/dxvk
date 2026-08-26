/*
 * Offscreen D3D11 smoke workload for the ARM64 WDDM bundle.
 *
 * This deliberately does not run in CI: it must execute beside the matching
 * DXVK and Turnip files on the target VM.  The workload clears a render target,
 * draws a full-screen triangle over a contrasting clear, copies it to a
 * staging resource, and verifies every pixel. That exercises device creation,
 * HLSL compilation, shader translation, graphics-pipeline submission,
 * synchronization, and GPU memory access instead of proving only that the
 * proxy DLLs load.
 */

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include <cstdio>

namespace {

constexpr char kShaderSource[] = R"(
struct VsOutput {
  float4 position : SV_Position;
};

VsOutput vs_main(uint vertex_id : SV_VertexID) {
  const float2 positions[3] = {
    float2(-1.0, -1.0),
    float2(-1.0,  3.0),
    float2( 3.0, -1.0),
  };
  VsOutput output;
  output.position = float4(positions[vertex_id], 0.0, 1.0);
  return output;
}

float4 ps_main() : SV_Target {
  return float4(1.0, 0.0, 0.0, 1.0);
}
)";

bool compile_shader(const char *entry_point, const char *target,
                    ID3DBlob **shader) {
  if (entry_point == nullptr || target == nullptr || shader == nullptr)
    return false;

  *shader = nullptr;
  ID3DBlob *errors = nullptr;
  const HRESULT status =
      D3DCompile(kShaderSource, sizeof(kShaderSource) - 1,
                 "wddm-d3d11-smoke.hlsl", nullptr, nullptr, entry_point, target,
                 D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
                 0, shader, &errors);
  if (errors != nullptr) {
    std::fwrite(errors->GetBufferPointer(), 1, errors->GetBufferSize(), stderr);
    errors->Release();
  }
  return SUCCEEDED(status) && *shader != nullptr;
}

bool verify_red_pixels(const D3D11_MAPPED_SUBRESOURCE &mapped) {
  if (mapped.pData == nullptr || mapped.RowPitch < 4 * 4)
    return false;

  const BYTE *base = static_cast<const BYTE *>(mapped.pData);
  for (UINT y = 0; y < 4; y++) {
    const BYTE *row = base + static_cast<SIZE_T>(y) * mapped.RowPitch;
    for (UINT x = 0; x < 4; x++) {
      const BYTE *pixel = row + x * 4;
      if (pixel[0] != 0xff || pixel[1] != 0x00 || pixel[2] != 0x00 ||
          pixel[3] != 0xff)
        return false;
    }
  }
  return true;
}

} // namespace

static bool try_adapter(IDXGIAdapter1 *adapter) {
  DXGI_ADAPTER_DESC1 description = {};
  if (FAILED(adapter->GetDesc1(&description)) ||
      (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
    return false;

  const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
  };
  ID3D11Device *device = nullptr;
  ID3D11DeviceContext *context = nullptr;
  ID3D11Texture2D *renderTarget = nullptr;
  ID3D11RenderTargetView *renderTargetView = nullptr;
  ID3D11Texture2D *staging = nullptr;
  ID3D11VertexShader *vertexShader = nullptr;
  ID3D11PixelShader *pixelShader = nullptr;
  ID3D11RasterizerState *rasterizer = nullptr;
  ID3DBlob *vertexBytecode = nullptr;
  ID3DBlob *pixelBytecode = nullptr;
  bool passed = false;
  HRESULT status = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                     D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                                     ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                     &device, nullptr, &context);

  if (SUCCEEDED(status) && device != nullptr && context != nullptr) {
    D3D11_TEXTURE2D_DESC renderDesc = {};
    renderDesc.Width = 4;
    renderDesc.Height = 4;
    renderDesc.MipLevels = 1;
    renderDesc.ArraySize = 1;
    renderDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    renderDesc.SampleDesc.Count = 1;
    renderDesc.Usage = D3D11_USAGE_DEFAULT;
    renderDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    status = device->CreateTexture2D(&renderDesc, nullptr, &renderTarget);
    if (SUCCEEDED(status))
      status = device->CreateRenderTargetView(renderTarget, nullptr,
                                              &renderTargetView);

    if (SUCCEEDED(status) &&
        (!compile_shader("vs_main", "vs_4_0", &vertexBytecode) ||
         !compile_shader("ps_main", "ps_4_0", &pixelBytecode)))
      status = E_FAIL;

    if (SUCCEEDED(status))
      status = device->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                                          vertexBytecode->GetBufferSize(),
                                          nullptr, &vertexShader);
    if (SUCCEEDED(status))
      status = device->CreatePixelShader(pixelBytecode->GetBufferPointer(),
                                         pixelBytecode->GetBufferSize(),
                                         nullptr, &pixelShader);

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    if (SUCCEEDED(status))
      status = device->CreateRasterizerState(&rasterizerDesc, &rasterizer);

    if (SUCCEEDED(status)) {
      const float clearColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};
      D3D11_VIEWPORT viewport = {};
      viewport.Width = 4.0f;
      viewport.Height = 4.0f;
      viewport.MaxDepth = 1.0f;
      context->OMSetRenderTargets(1, &renderTargetView, nullptr);
      context->ClearRenderTargetView(renderTargetView, clearColor);
      context->RSSetViewports(1, &viewport);
      context->RSSetState(rasterizer);
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      context->VSSetShader(vertexShader, nullptr, 0);
      context->PSSetShader(pixelShader, nullptr, 0);
      context->Draw(3, 0);

      D3D11_TEXTURE2D_DESC stagingDesc = renderDesc;
      stagingDesc.Usage = D3D11_USAGE_STAGING;
      stagingDesc.BindFlags = 0;
      stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      stagingDesc.MiscFlags = 0;
      status = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
    }

    if (SUCCEEDED(status)) {
      context->CopyResource(staging, renderTarget);
      context->Flush();

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      status = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
      if (SUCCEEDED(status)) {
        passed = verify_red_pixels(mapped);
        context->Unmap(staging, 0);
      }
    }
  }

  if (context != nullptr)
    context->ClearState();
  if (staging != nullptr)
    staging->Release();
  if (rasterizer != nullptr)
    rasterizer->Release();
  if (pixelShader != nullptr)
    pixelShader->Release();
  if (vertexShader != nullptr)
    vertexShader->Release();
  if (pixelBytecode != nullptr)
    pixelBytecode->Release();
  if (vertexBytecode != nullptr)
    vertexBytecode->Release();
  if (renderTargetView != nullptr)
    renderTargetView->Release();
  if (renderTarget != nullptr)
    renderTarget->Release();
  if (context != nullptr)
    context->Release();
  if (device != nullptr)
    device->Release();
  return SUCCEEDED(status) && passed;
}

int main() {
  IDXGIFactory1 *factory = nullptr;
  HRESULT status = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(status)) {
    std::fprintf(stderr, "CreateDXGIFactory1 failed: 0x%08lx\n",
                 static_cast<unsigned long>(status));
    return 1;
  }

  bool created = false;
  for (UINT index = 0; !created; index++) {
    IDXGIAdapter1 *adapter = nullptr;
    status = factory->EnumAdapters1(index, &adapter);
    if (status == DXGI_ERROR_NOT_FOUND)
      break;
    if (FAILED(status))
      break;
    created = try_adapter(adapter);
    adapter->Release();
  }

  factory->Release();
  if (!created)
    std::fprintf(
        stderr,
        "No hardware adapter completed the D3D11 offscreen clear/readback\n");
  return created ? 0 : 2;
}
