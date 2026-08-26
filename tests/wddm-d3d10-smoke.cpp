/*
 * Offscreen D3D10 smoke workload for the ARM64 WDDM bundle.
 *
 * The workload clears a render target, copies it to a staging resource, and
 * verifies the readback. It must execute beside the matching DXVK and Turnip
 * files on the target VM; CI only links and validates the ARM64 image.
 */

#include <d3d10_1.h>
#include <dxgi.h>
#include <windows.h>

#include <cstdio>

static bool try_adapter(IDXGIAdapter1 *adapter) {
  if (adapter == nullptr)
    return false;

  DXGI_ADAPTER_DESC1 description = {};
  if (FAILED(adapter->GetDesc1(&description)) ||
      (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
    return false;

  ID3D10Device1 *device = nullptr;
  ID3D10Texture2D *render_target = nullptr;
  ID3D10RenderTargetView *render_target_view = nullptr;
  ID3D10Texture2D *staging = nullptr;
  bool passed = false;

  HRESULT status = D3D10CreateDevice1(adapter, D3D10_DRIVER_TYPE_UNKNOWN,
                                      nullptr, D3D10_CREATE_DEVICE_BGRA_SUPPORT,
                                      D3D10_FEATURE_LEVEL_10_0,
                                      D3D10_1_SDK_VERSION, &device);
  if (SUCCEEDED(status) && device != nullptr) {
    D3D10_TEXTURE2D_DESC render_desc = {};
    render_desc.Width = 4;
    render_desc.Height = 4;
    render_desc.MipLevels = 1;
    render_desc.ArraySize = 1;
    render_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    render_desc.SampleDesc.Count = 1;
    render_desc.Usage = D3D10_USAGE_DEFAULT;
    render_desc.BindFlags = D3D10_BIND_RENDER_TARGET;

    status = device->CreateTexture2D(&render_desc, nullptr, &render_target);
    if (SUCCEEDED(status))
      status = device->CreateRenderTargetView(render_target, nullptr,
                                              &render_target_view);

    if (SUCCEEDED(status)) {
      const float clear_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      device->OMSetRenderTargets(1, &render_target_view, nullptr);
      device->ClearRenderTargetView(render_target_view, clear_color);

      D3D10_TEXTURE2D_DESC staging_desc = render_desc;
      staging_desc.Usage = D3D10_USAGE_STAGING;
      staging_desc.BindFlags = 0;
      staging_desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
      staging_desc.MiscFlags = 0;
      status = device->CreateTexture2D(&staging_desc, nullptr, &staging);
    }

    if (SUCCEEDED(status)) {
      device->CopyResource(staging, render_target);
      device->Flush();

      D3D10_MAPPED_TEXTURE2D mapped = {};
      status = staging->Map(0, D3D10_MAP_READ, 0, &mapped);
      if (SUCCEEDED(status)) {
        if (mapped.pData != nullptr && mapped.RowPitch >= 4 * sizeof(BYTE)) {
          const BYTE *pixel = static_cast<const BYTE *>(mapped.pData);
          passed = pixel[0] == 0xff && pixel[1] == 0x00 && pixel[2] == 0x00 &&
                   pixel[3] == 0xff;
        }
        staging->Unmap(0);
      }
    }
  }

  if (staging != nullptr)
    staging->Release();
  if (render_target_view != nullptr)
    render_target_view->Release();
  if (render_target != nullptr)
    render_target->Release();
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

  bool passed = false;
  for (UINT index = 0; !passed; index++) {
    IDXGIAdapter1 *adapter = nullptr;
    status = factory->EnumAdapters1(index, &adapter);
    if (status == DXGI_ERROR_NOT_FOUND || FAILED(status))
      break;
    passed = try_adapter(adapter);
    adapter->Release();
  }

  factory->Release();
  if (!passed)
    std::fprintf(
        stderr,
        "No hardware adapter completed the D3D10 offscreen clear/readback\n");
  return passed ? 0 : 2;
}
