// SPDX-License-Identifier: MIT
// Explicit ordinary SYSTEM runtime validation. No registration or software fallback.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d10umddi.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3dkmthk.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "umd-system-validation-entry.h"
#include "../src/umd/umd_runtime_identity.h"

namespace {
using Microsoft::WRL::ComPtr;
struct Failure { const char* stage; HRESULT hr; };
void require(bool value, const char* stage, HRESULT hr = E_FAIL) {
  if (!value) throw Failure{stage, hr};
}
void exact(HRESULT result, const char* stage) { require(result == S_OK, stage, result); }
std::wstring absolute(const WCHAR* input) {
  require(input && std::wcslen(input) > 2 && input[1] == L':' && input[2] == L'\\', "absolute-drive-path");
  std::vector<WCHAR> buffer(32768);
  const DWORD size = GetFullPathNameW(input, DWORD(buffer.size()), buffer.data(), nullptr);
  require(size && size < buffer.size(), "canonical-path", HRESULT_FROM_WIN32(GetLastError()));
  return {buffer.data(), size};
}
std::wstring loaded(HMODULE module) {
  require(module != nullptr, "module-loaded");
  std::vector<WCHAR> buffer(32768);
  const DWORD size = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()));
  require(size && size < buffer.size(), "actual-module-path", HRESULT_FROM_WIN32(GetLastError()));
  return {buffer.data(), size};
}
std::wstring basename(const std::wstring& name) { return name.substr(name.find_last_of(L"\\/") + 1); }
template<typename Function> Function symbol(HMODULE module, const char* name) {
  FARPROC original = GetProcAddress(module, name);
  require(original != nullptr, name, HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
  Function function;
  static_assert(sizeof(function) == sizeof(original));
  std::memcpy(&function, &original, sizeof(function));
  return function;
}
std::vector<std::pair<std::wstring, std::wstring>> systemPaths;
HMODULE systemModule(const WCHAR* name) {
  std::vector<WCHAR> buffer(32768);
  const UINT size = GetSystemDirectoryW(buffer.data(), UINT(buffer.size()));
  require(size && size < buffer.size(), "system-directory");
  const std::wstring expected = std::wstring(buffer.data(), size) + L"\\" + name;
  HMODULE module = LoadLibraryExW(expected.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  require(module != nullptr, "load-system-module", HRESULT_FROM_WIN32(GetLastError()));
  const auto actual = loaded(module);
  require(!_wcsicmp(expected.c_str(), actual.c_str()), "system-module-path-equality");
  systemPaths.emplace_back(name, actual);
  return module; // Retain SYSTEM modules through all COM/KMT cleanup.
}
void save(const std::wstring& directory, const WCHAR* name, const void* data, DWORD size) {
  const auto path = directory + L"\\" + name;
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file != INVALID_HANDLE_VALUE, "create-original", HRESULT_FROM_WIN32(GetLastError()));
  DWORD written = 0;
  const BOOL success = WriteFile(file, data, size, &written, nullptr);
  const DWORD error = success ? ERROR_SUCCESS : GetLastError();
  const BOOL closed = CloseHandle(file);
  require(success && written == size && closed, "write-close-original", HRESULT_FROM_WIN32(error));
}
std::string jsonString(const std::wstring& text) {
  std::string result = "\"";
  for (WCHAR value : text) {
    if (value >= 32 && value < 127 && value != L'\\' && value != L'\"') result += char(value);
    else {
      char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", unsigned(value)); result += escaped;
    }
  }
  return result + '"';
}
LUID parseLuid(const WCHAR* input) {
  require(input && std::wcslen(input) == 17 && input[8] == L':', "exact-adapter-luid");
  unsigned high = 0, low = 0;
  for (UINT position = 0; position < 17; ++position) {
    if (position == 8) continue;
    const WCHAR token = input[position];
    const UINT digit = token >= L'0' && token <= L'9' ? UINT(token-L'0')
      : token >= L'a' && token <= L'f' ? UINT(token-L'a'+10)
      : token >= L'A' && token <= L'F' ? UINT(token-L'A'+10) : 16;
    require(digit < 16, "exact-adapter-luid-hex");
    UINT& value = position < 8 ? high : low; value = (value << 4) | digit;
  }
  require(high || low, "nonzero-adapter-luid");
  return {low, LONG(high)};
}
bool sameLuid(LUID first, LUID second) { return first.LowPart == second.LowPart && first.HighPart == second.HighPart; }

struct KmtAdapter {
  D3DKMT_HANDLE handle = 0;
  decltype(&D3DKMTCloseAdapter) close = nullptr;
  decltype(&D3DKMTQueryAdapterInfo) query = nullptr;
  ~KmtAdapter() { if (handle && close) { D3DKMT_CLOSEADAPTER args{}; args.hAdapter = handle; close(&args); } }
  void info(KMTQUERYADAPTERINFOTYPE type, void* data, UINT size) {
    D3DKMT_QUERYADAPTERINFO args{}; args.hAdapter = handle; args.Type = type;
    args.pPrivateDriverData = data; args.PrivateDriverDataSize = size;
    const NTSTATUS result = query(&args);
    require(result == 0, "actual-kmt-query", HRESULT_FROM_NT(result));
  }
};
std::wstring effectiveName(KmtAdapter& adapter, UINT api) {
  D3DKMT_UMDFILENAMEINFO name{};
  name.Version = api == 10 ? KMTUMDVERSION_DX10 : KMTUMDVERSION_DX11;
  adapter.info(KMTQAITYPE_UMDRIVERNAME, &name, sizeof(name));
  UINT size = 0;
  while (size < MAX_PATH && name.UmdFileName[size]) ++size;
  require(size && size < MAX_PATH, "bounded-kmt-umd-name");
  return absolute(name.UmdFileName);
}
std::unique_ptr<VioGpuSystemValidationEntryInfo> frontendInfo(const std::wstring& front, const std::wstring& core) {
  HMODULE module = GetModuleHandleW(basename(front).c_str());
  require(module && !_wcsicmp(loaded(module).c_str(), front.c_str()), "factory-loaded-exact-frontend");
  auto info = std::make_unique<VioGpuSystemValidationEntryInfo>(); info->size = sizeof(*info);
  exact(symbol<VioGpuSystemValidationReadEntry>(module, "VioGpuDxvkValidationEntryInfo")(info.get()), "frontend-readonly-info");
  require(info->legacyInterface == 0x000a0001 && info->calls > 0 && info->successfulCalls > 0
    && info->lastInterface == 0x000a0001 && info->lastResult == S_OK, "actual-private-entry-forwarding");
  require(!_wcsicmp(info->corePath, core.c_str()), "frontend-exact-core-path");
  HMODULE backend = GetModuleHandleW(basename(core).c_str());
  require(backend && !_wcsicmp(loaded(backend).c_str(), core.c_str()), "actual-loaded-core-path");
  return info;
}
constexpr UINT Width = 16, Height = 16;
struct FrameResult { HRESULT map, readbackRemoved, present, presentRemoved; UINT rowPitch; };
std::array<FrameResult, 2> frameResults{};
struct Api10 {
  using Device = ID3D10Device; using Context = ID3D10Device; using Texture = ID3D10Texture2D;
  using View = ID3D10RenderTargetView; using Vertex = ID3D10VertexShader; using Pixel = ID3D10PixelShader;
  using Raster = ID3D10RasterizerState; using RasterDesc = D3D10_RASTERIZER_DESC;
  using TextureDesc = D3D10_TEXTURE2D_DESC; using Viewport = D3D10_VIEWPORT; using Mapped = D3D10_MAPPED_TEXTURE2D;
  static constexpr auto Staging = D3D10_USAGE_STAGING;
  static constexpr auto Fill = D3D10_FILL_SOLID; static constexpr auto Cull = D3D10_CULL_NONE;
  static HRESULT shaders(Device* device, ID3DBlob* vs, ID3DBlob* ps, Vertex** vertex, Pixel** pixel) {
    exact(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), vertex), "public-create-vs");
    return device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), pixel);
  }
  static void bind(Context* context, Vertex* vertex, Pixel* pixel) {
    context->VSSetShader(vertex); context->PSSetShader(pixel);
    context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }
  static HRESULT map(Context*, Texture* texture, Mapped* mapped) { return texture->Map(0, D3D10_MAP_READ, 0, mapped); }
  static void unmap(Context*, Texture* texture) { texture->Unmap(0); }
};
struct Api11 {
  using Device = ID3D11Device; using Context = ID3D11DeviceContext; using Texture = ID3D11Texture2D;
  using View = ID3D11RenderTargetView; using Vertex = ID3D11VertexShader; using Pixel = ID3D11PixelShader;
  using Raster = ID3D11RasterizerState; using RasterDesc = D3D11_RASTERIZER_DESC;
  using TextureDesc = D3D11_TEXTURE2D_DESC; using Viewport = D3D11_VIEWPORT; using Mapped = D3D11_MAPPED_SUBRESOURCE;
  static constexpr auto Staging = D3D11_USAGE_STAGING;
  static constexpr auto Fill = D3D11_FILL_SOLID; static constexpr auto Cull = D3D11_CULL_NONE;
  static HRESULT shaders(Device* device, ID3DBlob* vs, ID3DBlob* ps, Vertex** vertex, Pixel** pixel) {
    exact(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, vertex), "public-create-vs");
    return device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, pixel);
  }
  static void bind(Context* context, Vertex* vertex, Pixel* pixel) {
    context->VSSetShader(vertex, nullptr, 0); context->PSSetShader(pixel, nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }
  static HRESULT map(Context* context, Texture* texture, Mapped* mapped) { return context->Map(texture, 0, D3D11_MAP_READ, 0, mapped); }
  static void unmap(Context* context, Texture* texture) { context->Unmap(texture, 0); }
};

template<typename Api> void render(typename Api::Device* device, typename Api::Context* context,
    IDXGISwapChain* swapchain, HMODULE compiler, const std::wstring& directory) {
  ComPtr<typename Api::Texture> target, staging;
  exact(swapchain->GetBuffer(0, IID_PPV_ARGS(&target)), "system-swapchain-buffer");
  typename Api::TextureDesc desc{}; target->GetDesc(&desc);
  require(desc.Width == Width && desc.Height == Height && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM
    && desc.MipLevels == 1 && desc.ArraySize == 1 && desc.SampleDesc.Count == 1, "original-swapchain-descriptor");
  desc.Usage = Api::Staging; desc.BindFlags = 0; desc.MiscFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  exact(device->CreateTexture2D(&desc, nullptr, &staging), "public-create-staging");
  ComPtr<typename Api::View> view;
  exact(device->CreateRenderTargetView(target.Get(), nullptr, &view), "public-create-rtv");
  auto rawView = view.Get(); context->OMSetRenderTargets(1, &rawView, nullptr);
  typename Api::Viewport viewport{}; viewport.Width = Width; viewport.Height = Height; viewport.MaxDepth = 1;
  context->RSSetViewports(1, &viewport);
  typename Api::RasterDesc raster{}; raster.FillMode = Api::Fill; raster.CullMode = Api::Cull; raster.DepthClipEnable = TRUE;
  ComPtr<typename Api::Raster> state;
  exact(device->CreateRasterizerState(&raster, &state), "public-create-raster"); context->RSSetState(state.Get());
  const char hlsl[] = "float4 vs(uint i:SV_VertexID):SV_Position{float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[i],0,1);}float4 ps():SV_Target{return float4(1,0,0,1);}";
  auto compile = symbol<decltype(&D3DCompile)>(compiler, "D3DCompile");
  ComPtr<ID3DBlob> vs, ps;
  exact(compile(hlsl, sizeof(hlsl)-1, nullptr, nullptr, nullptr, "vs", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, nullptr), "system-fxc-vs");
  exact(compile(hlsl, sizeof(hlsl)-1, nullptr, nullptr, nullptr, "ps", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, nullptr), "system-fxc-ps");
  ComPtr<typename Api::Vertex> vertex; ComPtr<typename Api::Pixel> pixel;
  exact(Api::shaders(device, vs.Get(), ps.Get(), &vertex, &pixel), "public-create-ps");
  Api::bind(context, vertex.Get(), pixel.Get());
  const float black[4]{0,0,0,1}; context->ClearRenderTargetView(view.Get(), black);
  for (UINT frame = 0; frame < 2; ++frame) {
    if (frame) context->Draw(3, 0);
    context->CopyResource(staging.Get(), target.Get()); context->Flush();
    typename Api::Mapped mapped{};
    auto& result = frameResults[frame];
    result.map = Api::map(context, staging.Get(), &mapped);
    exact(result.map, "blocking-public-readback-map");
    result.rowPitch = mapped.RowPitch;
    const uint64_t mappedSpan = uint64_t(Height-1)*mapped.RowPitch + Width*4;
    require(mapped.pData && mapped.RowPitch >= Width*4
      && mappedSpan <= uint64_t(UINTPTR_MAX) - reinterpret_cast<uintptr_t>(mapped.pData) + 1, "bounded-readback-row-pitch");
    std::array<unsigned char, Width*Height*4> original{};
    for (UINT y = 0; y < Height; ++y)
      std::memcpy(original.data()+y*Width*4, static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch, Width*4);
    Api::unmap(context, staging.Get());
    save(directory, frame ? L"draw.raw" : L"clear.raw", original.data(), DWORD(original.size()));
    for (UINT p = 0; p < Width*Height; ++p)
      require(original[4*p] == (frame ? 255 : 0) && original[4*p+1] == 0 && original[4*p+2] == 0 && original[4*p+3] == 255, "literal-clear-draw-pixel-oracle");
    result.readbackRemoved = device->GetDeviceRemovedReason(); exact(result.readbackRemoved, "actual-device-removal-readback");
    result.present = swapchain->Present(1, 0); exact(result.present, "ordinary-dxgi-present");
    result.presentRemoved = device->GetDeviceRemovedReason(); exact(result.presentRemoved, "actual-device-removal-present");
  }
  context->OMSetRenderTargets(0, nullptr, nullptr);
  std::printf("SYSTEM_RUNTIME_VALIDATION_DRAW_READBACK_PRESENT api=%u pixels=512 presents=2 software_fallback=0\n", std::is_same_v<Api, Api10> ? 10u : 11u);
}

void entryNegative(const WCHAR* path) {
  const auto front = absolute(path);
  HMODULE module = LoadLibraryExW(front.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  require(module && !_wcsicmp(front.c_str(), loaded(module).c_str()), "entry-negative-exact-module");
  auto entry = symbol<PFND3D10DDI_OPENADAPTER>(module, "OpenAdapter10");
  require(entry(nullptr) == E_INVALIDARG, "typed-null-entry-guard");
  D3D10DDIARG_OPENADAPTER unsupported{}; unsupported.Interface = D3D11_0_DDI_INTERFACE_VERSION;
  const auto original = unsupported;
  require(entry(&unsupported) == DXGI_ERROR_UNSUPPORTED && !std::memcmp(&unsupported, &original, sizeof(original)), "typed-unsupported-entry-guard");
  auto info = std::make_unique<VioGpuSystemValidationEntryInfo>(); info->size = sizeof(*info);
  auto read = symbol<VioGpuSystemValidationReadEntry>(module, "VioGpuDxvkValidationEntryInfo");
  require(read(nullptr) == E_INVALIDARG, "null-info-guard");
  exact(read(info.get()), "negative-entry-info");
  require(info->calls == 1 && info->successfulCalls == 0 && !info->corePath[0], "no-core-load-in-negative-controls");
  std::puts("SYSTEM_RUNTIME_VALIDATION_ENTRY_NEGATIVE_PASS checks=5 core_loaded=0 registry_changes=0 gpu_calls=0");
  FreeLibrary(module);
}
}

int wmain(int argc, WCHAR** argv) {
  if (argc == 3 && !std::wcscmp(argv[1], L"--entry-negative")) {
    try { entryNegative(argv[2]); return 0; }
    catch (const Failure& failure) { std::fprintf(stderr, "SYSTEM_VALIDATION_FAIL stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(failure.hr)); return 1; }
  }
  // Required operands prevent implicit/default-adapter or module selection.
  if (argc != 8) { std::fputs("usage: probe <10|11> <high:low-LUID> <absolute-frontend> <absolute-core> <fresh-output-dir> <hold-event-name> <hold-ms>\n", stderr); return 2; }
  ComPtr<ID3D10Device> device10;
  ComPtr<ID3D11Device> device11;
  ComPtr<ID3D11DeviceContext> context11;
  ComPtr<IDXGISwapChain> swapchain;
  HWND window = nullptr;
  HANDLE hold = nullptr;
  int exit = 1;
  DWORD holdMs = 0;
  try {
    const UINT api = !std::wcscmp(argv[1], L"10") ? 10u : !std::wcscmp(argv[1], L"11") ? 11u : 0u;
    require(api != 0, "explicit-system-api");
    const LUID expectedLuid = parseLuid(argv[2]);
    const auto front = absolute(argv[3]), core = absolute(argv[4]), directory = absolute(argv[5]);
    WCHAR* tail = nullptr; const unsigned long requestedMs = std::wcstoul(argv[7], &tail, 10);
    require(tail && !*tail && requestedMs && requestedMs <= 60000, "bounded-hold-time"); holdMs = DWORD(requestedMs);
    require(argv[6][0] && !std::wcsncmp(argv[6], L"Local\\", 6), "explicit-local-hold-event");
    hold = CreateEventW(nullptr, TRUE, FALSE, argv[6]);
    const DWORD eventError = GetLastError();
    if (!hold || eventError == ERROR_ALREADY_EXISTS) {
      if (hold) CloseHandle(hold);
      hold = nullptr;
      throw Failure{"fresh-hold-event", HRESULT_FROM_WIN32(eventError ? eventError : ERROR_INVALID_HANDLE)};
    }
    require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "fresh-original-directory", HRESULT_FROM_WIN32(GetLastError()));
    require(!GetModuleHandleW(basename(front).c_str()) && !GetModuleHandleW(basename(core).c_str()), "candidate-not-manually-preloaded");
    HMODULE dxgi = systemModule(L"dxgi.dll");
    HMODULE runtime = systemModule(api == 10 ? L"d3d10.dll" : L"d3d11.dll");
    HMODULE gdi = systemModule(L"gdi32.dll");
    HMODULE compiler = systemModule(L"d3dcompiler_47.dll");
    ComPtr<IDXGIFactory1> factory;
    exact(symbol<decltype(&CreateDXGIFactory1)>(dxgi, "CreateDXGIFactory1")(IID_PPV_ARGS(&factory)), "system-dxgi-factory");
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 selected{};
    for (UINT index = 0;; ++index) {
      ComPtr<IDXGIAdapter1> candidate;
      const HRESULT hr = factory->EnumAdapters1(index, &candidate);
      if (hr == DXGI_ERROR_NOT_FOUND) break;
      exact(hr, "actual-adapter-enumeration");
      DXGI_ADAPTER_DESC1 desc{}; exact(candidate->GetDesc1(&desc), "actual-adapter-desc");
      if (sameLuid(desc.AdapterLuid, expectedLuid)) {
        require(desc.VendorId == 0x1af4 && desc.DeviceId == 0x1050 && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE), "exact-hardware-viogpu-adapter");
        require(!adapter, "unique-selected-luid"); adapter = candidate; selected = desc;
      }
    }
    require(adapter != nullptr, "selected-luid-present");
    KmtAdapter kmt;
    kmt.query = symbol<decltype(&D3DKMTQueryAdapterInfo)>(gdi, "D3DKMTQueryAdapterInfo");
    kmt.close = symbol<decltype(&D3DKMTCloseAdapter)>(gdi, "D3DKMTCloseAdapter");
    D3DKMT_OPENADAPTERFROMLUID opened{}; opened.AdapterLuid = expectedLuid;
    const auto openResult = symbol<decltype(&D3DKMTOpenAdapterFromLuid)>(gdi, "D3DKMTOpenAdapterFromLuid")(&opened);
    kmt.handle = opened.hAdapter; require(openResult == 0 && kmt.handle, "actual-kmt-open-luid", HRESULT_FROM_NT(openResult));
    D3DKMT_ADAPTERTYPE type{}; kmt.info(KMTQAITYPE_ADAPTERTYPE, &type, sizeof(type));
    require(type.RenderSupported && !type.SoftwareDevice, "actual-kmt-hardware-render-support");
    std::array<unsigned char, dxvk::umd::RuntimeIdentityReplySize> identityBytes{};
    kmt.info(KMTQAITYPE_UMDRIVERPRIVATE, identityBytes.data(), UINT(identityBytes.size()));
    dxvk::umd::RuntimeIdentity identity;
    require(dxvk::umd::readRuntimeIdentity(identityBytes.data(), identityBytes.size(), identity)
      && !std::memcmp(identity.luid.data(), &expectedLuid, sizeof(expectedLuid)), "actual-kmt-viogpu-identity");
    const auto effective = effectiveName(kmt, api);
    std::printf("SYSTEM_VALIDATION_SELECTED api=%u luid=%08x:%08x effective_umd=%s frontend=%s\n", api,
      UINT(expectedLuid.HighPart), UINT(expectedLuid.LowPart), jsonString(effective).c_str(), jsonString(front).c_str()); std::fflush(stdout);
    require(!_wcsicmp(effective.c_str(), front.c_str()), "effective-kmt-frontend-selection-required");
    if (api == 10) {
      exact(symbol<decltype(&D3D10CreateDevice)>(runtime, "D3D10CreateDevice")(adapter.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION, &device10), "ordinary-system-d3d10-create-device");
    } else {
      const D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_10_0; D3D_FEATURE_LEVEL obtained{};
      exact(symbol<decltype(&D3D11CreateDevice)>(runtime, "D3D11CreateDevice")(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &requested, 1, D3D11_SDK_VERSION, &device11, &obtained, &context11), "ordinary-system-d3d11-create-device");
      require(obtained == requested, "exact-d3d11-feature-level-10_0");
    }
    auto info = frontendInfo(front, core);
    ComPtr<IDXGIDevice> dxgiDevice;
    exact(api == 10 ? device10.As(&dxgiDevice) : device11.As(&dxgiDevice), "actual-device-dxgi-interface");
    ComPtr<IDXGIAdapter> actualAdapter; exact(dxgiDevice->GetAdapter(&actualAdapter), "actual-device-adapter");
    DXGI_ADAPTER_DESC actualDesc{}; exact(actualAdapter->GetDesc(&actualDesc), "actual-device-adapter-desc");
    require(sameLuid(actualDesc.AdapterLuid, selected.AdapterLuid) && actualDesc.VendorId == 0x1af4 && actualDesc.DeviceId == 0x1050, "created-device-selected-luid");
    WNDCLASSW klass{}; klass.lpfnWndProc = DefWindowProcW; klass.hInstance = GetModuleHandleW(nullptr); klass.lpszClassName = L"VioGpuSystemValidationUnregistered";
    require(RegisterClassW(&klass), "register-probe-window");
    window = CreateWindowW(klass.lpszClassName, L"VIOGPU explicit system runtime validation", WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr, klass.hInstance, nullptr);
    require(window != nullptr, "create-visible-probe-window"); ShowWindow(window, SW_SHOWNORMAL); UpdateWindow(window);
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Width = Width; desc.BufferDesc.Height = Height;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount = 1; desc.OutputWindow = window; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    exact(factory->CreateSwapChain(api == 10 ? static_cast<IUnknown*>(device10.Get()) : static_cast<IUnknown*>(device11.Get()), &desc, &swapchain), "ordinary-system-create-swapchain");
    if (api == 10) render<Api10>(device10.Get(), device10.Get(), swapchain.Get(), compiler, directory);
    else render<Api11>(device11.Get(), context11.Get(), swapchain.Get(), compiler, directory);
    require(!_wcsicmp(effectiveName(kmt, api).c_str(), front.c_str()), "effective-name-stable-through-render");
    std::string manifest = "{\"schema\":1,\"api\":" + std::to_string(api) + ",\"width\":16,\"height\":16,\"frames\":2,\"pixels\":512,\"presents\":2,\"vendor\":6900,\"device\":4176,\"luidHigh\":" + std::to_string(UINT(expectedLuid.HighPart))
      + ",\"luidLow\":" + std::to_string(expectedLuid.LowPart) + ",\"generation\":" + std::to_string(identity.generation)
      + ",\"capabilities\":" + std::to_string(identity.capabilities) + ",\"frontend\":" + jsonString(front) + ",\"core\":" + jsonString(core)
      + ",\"entryCalls\":" + std::to_string(info->calls) + ",\"successfulEntryCalls\":" + std::to_string(info->successfulCalls)
      + ",\"entryInterface\":" + std::to_string(info->lastInterface) + ",\"entryVersion\":" + std::to_string(info->lastVersion)
      + ",\"entryResult\":0,\"softwareFallback\":false,\"unregisteredValidationCandidate\":true,\"productionAdmission\":false,\"registrationChangedByProbe\":false,\"frameResults\":[";
    for (size_t index = 0; index < frameResults.size(); ++index) {
      if (index) manifest += ',';
      const auto& result = frameResults[index];
      manifest += "{\"map\":" + std::to_string(result.map) + ",\"readbackRemoved\":" + std::to_string(result.readbackRemoved)
        + ",\"present\":" + std::to_string(result.present) + ",\"presentRemoved\":" + std::to_string(result.presentRemoved)
        + ",\"rowPitch\":" + std::to_string(result.rowPitch) + '}';
    }
    manifest += "],\"systemModules\":{ ";
    for (size_t index = 0; index < systemPaths.size(); ++index) {
      if (index) manifest += ',';
      manifest += jsonString(systemPaths[index].first) + ':' + jsonString(systemPaths[index].second);
    }
    manifest += "}}\n";
    save(directory, L"manifest.json", manifest.data(), DWORD(manifest.size()));
    std::printf("SYSTEM_RUNTIME_VALIDATION_PASS api=%u pixels=512 presents=2 production_admission=0 registry_changes=0\n", api); std::fflush(stdout);
    exit = 0;
  } catch (const Failure& failure) {
    std::fprintf(stderr, "SYSTEM_VALIDATION_FAIL stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(failure.hr)); std::fflush(stderr);
  } catch (...) { std::fputs("SYSTEM_VALIDATION_FAIL stage=unexpected-exception\n", stderr); }
  if (hold) {
    std::printf("SYSTEM_RUNTIME_VALIDATION_HELD pid=%lu timeout_ms=%lu pending_exit=%d registry_restoration_not_proved_by_event=1\n", static_cast<unsigned long>(GetCurrentProcessId()), static_cast<unsigned long>(holdMs), exit); std::fflush(stdout);
    if (WaitForSingleObject(hold, holdMs) != WAIT_OBJECT_0) { std::fputs("SYSTEM_VALIDATION_FAIL stage=hold-release-timeout\n", stderr); exit = 1; }
    CloseHandle(hold);
  }
  swapchain.Reset(); context11.Reset(); device11.Reset(); device10.Reset();
  if (window) DestroyWindow(window);
  return exit;
}
