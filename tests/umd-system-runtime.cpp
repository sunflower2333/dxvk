// Actual Microsoft D3D11/DXGI runtime control, separate from direct DDI
// fixtures. CI has no VIOGPU KMD: the candidate must not activate there.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using Microsoft::WRL::ComPtr;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "system runtime line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

int main(int argc, char** argv) {
  const bool warpOnly = argc == 2 && std::strcmp(argv[1], "--warp-only") == 0;
  CHECK(argc == 1 || warpOnly);
  const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
  if (!warpOnly) {
  wchar_t executable[32768];
  DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
  CHECK(length && length < 32768);
  std::wstring dll(executable, length);
  dll.resize(dll.find_last_of(L"\\/") + 1);
  dll += L"viogpudxvk.dll";
  HMODULE candidate = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  CHECK(candidate);
  {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_SOFTWARE,
      candidate, 0, &level, 1, D3D11_SDK_VERSION, &device, nullptr, &context);
    CHECK(FAILED(result) && !device && !context);
    std::printf("system candidate software-module attempt hr=0x%08lx; native DDI entry/activation NOT proved\n", static_cast<unsigned long>(result));
  }
  CHECK(FreeLibrary(candidate));
  }

  WNDCLASSW klass{};
  klass.lpfnWndProc = DefWindowProcW;
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpszClassName = L"DxvkUmdRuntimeControl";
  CHECK(RegisterClassW(&klass));
  HWND window = CreateWindowW(klass.lpszClassName, L"runtime control", WS_OVERLAPPEDWINDOW,
    0, 0, 64, 64, nullptr, nullptr, klass.hInstance, nullptr);
  CHECK(window);
  if (warpOnly) {
    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
  }
  HRESULT presented = E_FAIL;
  {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = 2; desc.BufferDesc.Height = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1; desc.OutputWindow = window; desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapchain;
    CHECK(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
      0, &level, 1, D3D11_SDK_VERSION, &desc, &swapchain, &device, nullptr, &context) == S_OK);
    {
      ComPtr<ID3D11Texture2D> target, staging;
      CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&target)) == S_OK);
      ComPtr<ID3D11RenderTargetView> view;
      CHECK(device->CreateRenderTargetView(target.Get(), nullptr, &view) == S_OK);
      ID3D11RenderTargetView* raw = view.Get(); context->OMSetRenderTargets(1, &raw, nullptr);
      D3D11_VIEWPORT viewport{0, 0, 2, 2, 0, 1}; context->RSSetViewports(1, &viewport);
      D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID;
      raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
      ComPtr<ID3D11RasterizerState> state;
      CHECK(device->CreateRasterizerState(&raster, &state) == S_OK); context->RSSetState(state.Get());
      const char* source =
        "float4 vs(uint i:SV_VertexID):SV_Position { float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)}; return float4(p[i],0,1); }"
        "float4 ps():SV_Target { return float4(0.25,0.5,0.75,1); }";
      ComPtr<ID3DBlob> vsCode, psCode;
      CHECK(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
        "vs", "vs_4_0", 0, 0, &vsCode, nullptr) == S_OK);
      CHECK(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
        "ps", "ps_4_0", 0, 0, &psCode, nullptr) == S_OK);
      ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11PixelShader> ps;
      CHECK(device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &vs) == S_OK);
      CHECK(device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &ps) == S_OK);
      context->VSSetShader(vs.Get(), nullptr, 0); context->PSSetShader(ps.Get(), nullptr, 0);
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      context->Draw(3, 0);
      D3D11_TEXTURE2D_DESC readback{}; target->GetDesc(&readback);
      readback.Usage = D3D11_USAGE_STAGING; readback.BindFlags = 0;
      readback.MiscFlags = 0; readback.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      CHECK(device->CreateTexture2D(&readback, nullptr, &staging) == S_OK);
      context->CopyResource(staging.Get(), target.Get()); context->Flush();
      D3D11_MAPPED_SUBRESOURCE map{};
      CHECK(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &map) == S_OK);
      for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 2; ++x) {
        auto pixel = static_cast<const unsigned char*>(map.pData) + y * map.RowPitch + x * 4;
        CHECK(pixel[0] >= 63 && pixel[0] <= 64 && pixel[1] >= 127 && pixel[1] <= 128
          && pixel[2] >= 191 && pixel[2] <= 192 && pixel[3] == 255);
      }
      context->Unmap(staging.Get(), 0);
      const ULONGLONG deadline = GetTickCount64() + 2000;
      do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
          TranslateMessage(&message); DispatchMessageW(&message);
        }
        context->Draw(3, 0); context->Flush();
        presented = swapchain->Present(0, 0);
        if (!warpOnly || presented == S_OK || FAILED(presented)) break;
        Sleep(16);
      } while (GetTickCount64() < deadline);
      std::printf("WARP Present final=0x%08lx window-shown=%d startup-budget-ms=2000\n",
        static_cast<unsigned long>(presented), warpOnly);
      CHECK(warpOnly ? presented == S_OK : SUCCEEDED(presented));
      context->ClearState();
    }
    // Children are gone; no later application call is used to finish teardown.
    context.Reset(); swapchain.Reset(); device.Reset();
  }
  CHECK(DestroyWindow(window));
  CHECK(UnregisterClassW(klass.lpszClassName, klass.hInstance));
  std::printf("system runtime control PASS: WARP Draw/readback/Present/immediate teardown; Present=0x%08lx; this is NOT candidate rendering\n", static_cast<unsigned long>(presented));
}
