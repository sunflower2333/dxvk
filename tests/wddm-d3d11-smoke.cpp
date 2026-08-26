/*
 * Compile/link smoke workload for the ARM64 WDDM bundle.
 *
 * This deliberately does not run in CI: it must execute beside the matching
 * DXVK and Turnip files on the target VM.  Keeping the workload here ensures
 * the bundle is consumable by a native D3D11 process, rather than proving only
 * that the five proxy DLLs have the expected PE machine type.
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

  if (context != nullptr)
    context->Release();
  if (device != nullptr)
    device->Release();
  return SUCCEEDED(status);
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
    std::fprintf(stderr, "No hardware adapter created a D3D11 device\n");
  return created ? 0 : 2;
}
