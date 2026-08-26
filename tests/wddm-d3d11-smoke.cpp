/*
 * Offscreen D3D11 smoke workload for the ARM64 WDDM bundle.
 *
 * This deliberately does not run in CI: it must execute beside the matching
 * DXVK and Turnip files on the target VM.  The workload clears a render target,
 * copies it to a staging resource, and verifies the readback.  That exercises
 * device creation, shader-free command submission, synchronization, and GPU
 * memory access instead of proving only that the proxy DLLs load.
 */

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>

#include <cstdio>

static bool
try_adapter(IDXGIAdapter1 *adapter)
{
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
  bool passed = false;
  const HRESULT status = D3D11CreateDevice(
    adapter,
    D3D_DRIVER_TYPE_UNKNOWN,
    nullptr,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
    levels,
    ARRAYSIZE(levels),
    D3D11_SDK_VERSION,
    &device,
    nullptr,
    &context);

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

    HRESULT step = device->CreateTexture2D(&renderDesc, nullptr, &renderTarget);
    if (SUCCEEDED(step))
      step = device->CreateRenderTargetView(renderTarget, nullptr, &renderTargetView);

    if (SUCCEEDED(step)) {
      const float clearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
      context->OMSetRenderTargets(1, &renderTargetView, nullptr);
      context->ClearRenderTargetView(renderTargetView, clearColor);
      context->Flush();

      D3D11_TEXTURE2D_DESC stagingDesc = renderDesc;
      stagingDesc.Usage = D3D11_USAGE_STAGING;
      stagingDesc.BindFlags = 0;
      stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      stagingDesc.MiscFlags = 0;
      step = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
    }

    if (SUCCEEDED(step)) {
      context->CopyResource(staging, renderTarget);
      context->Flush();

      D3D11_MAPPED_SUBRESOURCE mapped = {};
      step = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
      if (SUCCEEDED(step)) {
        if (mapped.pData != nullptr && mapped.RowPitch >= 4 * sizeof(BYTE)) {
          const BYTE *pixel = static_cast<const BYTE *>(mapped.pData);
          passed = pixel[0] == 0xff && pixel[1] == 0x00 && pixel[2] == 0x00 && pixel[3] == 0xff;
        }
        context->Unmap(staging, 0);
      }
    }
  }

  if (staging != nullptr)
    staging->Release();
  if (renderTargetView != nullptr)
    renderTargetView->Release();
  if (renderTarget != nullptr)
    renderTarget->Release();
  if (context != nullptr)
    context->Release();
  if (device != nullptr)
    device->Release();
  if (FAILED(status))
    return false;
  return passed;
}

int
main()
{
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
    std::fprintf(stderr, "No hardware adapter completed the D3D11 offscreen clear/readback\n");
  return created ? 0 : 2;
}
