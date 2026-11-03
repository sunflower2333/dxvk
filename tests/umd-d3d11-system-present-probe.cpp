// SPDX-License-Identifier: MIT
// SYSTEM D3D10/10.1/11 through typed D3D11 DDI plus windowed DXGI Present.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#pragma warning(push)
#pragma warning(disable: 4201)
#include <d3d10umddi.h>
#pragma warning(pop)
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
#include <unordered_map>
#include <vector>
#include "umd-d3d11-system-entry.h"
#include "../src/umd/umd_runtime_identity.h"

namespace {
using Microsoft::WRL::ComPtr;
UINT publicApi = 11;
D3D_FEATURE_LEVEL requestedFeature = D3D_FEATURE_LEVEL_10_0;
struct Failure { const char* stage; HRESULT result; };
void require(bool value, const char* stage, HRESULT result = E_FAIL) {
  if (!value) throw Failure{stage, result};
}
void exact(HRESULT result, const char* stage) { require(result == S_OK, stage, result); }
unsigned dxvkWorkerCount() {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  require(snapshot != INVALID_HANDLE_VALUE, "thread-snapshot");
  unsigned count = 0;
  THREADENTRY32 entry{}; entry.dwSize = sizeof(entry);
  try {
    require(Thread32First(snapshot, &entry) != FALSE, "thread-first");
    do {
      if (entry.th32OwnerProcessID != GetCurrentProcessId()) continue;
      HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
      if (!thread && GetLastError() == ERROR_INVALID_PARAMETER) continue; // Already exited.
      require(thread != nullptr, "owned-thread-query");
      PWSTR name = nullptr;
      const HRESULT hr = GetThreadDescription(thread, &name);
      CloseHandle(thread);
      if (SUCCEEDED(hr) && name && !std::wcsncmp(name, L"dxvk-", 5)) ++count;
      if (name) LocalFree(name);
      require(SUCCEEDED(hr), "owned-thread-description", hr);
    } while (Thread32Next(snapshot, &entry));
    require(GetLastError() == ERROR_NO_MORE_FILES, "thread-enumeration-complete");
  } catch (...) { CloseHandle(snapshot); throw; }
  CloseHandle(snapshot);
  return count;
}
template<typename Function> Function symbol(HMODULE module, const char* name) {
  FARPROC value = GetProcAddress(module, name);
  require(value != nullptr, name, HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
  Function result; static_assert(sizeof(result) == sizeof(value));
  std::memcpy(&result, &value, sizeof(result)); return result;
}
std::wstring absolute(const WCHAR* input) {
  require(input && std::wcslen(input) > 2 && input[1] == L':' && input[2] == L'\\', "absolute-drive-path");
  std::vector<WCHAR> buffer(32768);
  const DWORD size = GetFullPathNameW(input, DWORD(buffer.size()), buffer.data(), nullptr);
  require(size && size < buffer.size(), "canonical-path", HRESULT_FROM_WIN32(GetLastError()));
  return {buffer.data(), size};
}
std::wstring basename(const std::wstring& path) { return path.substr(path.find_last_of(L'\\') + 1); }
std::wstring loaded(HMODULE module) {
  require(module != nullptr, "actual-module-loaded");
  std::vector<WCHAR> buffer(32768);
  const DWORD size = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()));
  require(size && size < buffer.size(), "actual-module-path", HRESULT_FROM_WIN32(GetLastError()));
  return {buffer.data(), size};
}
std::string json(const std::wstring& value) {
  std::string result = "\"";
  for (WCHAR token : value) {
    if (token >= 32 && token < 127 && token != L'\\' && token != L'\"') result += char(token);
    else { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", unsigned(token)); result += escaped; }
  }
  return result + '"';
}
std::string jsonAscii(const char* value) {
  std::wstring text; for (; *value; ++value) text += WCHAR(static_cast<unsigned char>(*value));
  return json(text);
}
void save(const std::wstring& directory, const WCHAR* name, const void* data, DWORD size, bool flush = false) {
  HANDLE file = CreateFileW((directory + L"\\" + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file != INVALID_HANDLE_VALUE, "fresh-original-file", HRESULT_FROM_WIN32(GetLastError()));
  DWORD written = 0; BOOL success = WriteFile(file, data, size, &written, nullptr);
  if (success && flush) success = FlushFileBuffers(file);
  const DWORD error = success ? ERROR_SUCCESS : GetLastError(); const BOOL closed = CloseHandle(file);
  require(success && written == size && closed, "original-write-close", HRESULT_FROM_WIN32(error));
}
void saveText(const std::wstring& directory, const WCHAR* name, const std::string& text) {
  require(text.size() <= MAXDWORD, "bounded-original-text"); save(directory, name, text.data(), DWORD(text.size()));
}
std::vector<std::pair<std::wstring, std::wstring>> systemPaths;
std::vector<std::pair<std::wstring, std::wstring>> privatePaths;
HMODULE systemModule(const WCHAR* name) {
  std::vector<WCHAR> buffer(32768);
  const UINT size = GetSystemDirectoryW(buffer.data(), UINT(buffer.size()));
  require(size && size < buffer.size(), "system-directory");
  const std::wstring expected = std::wstring(buffer.data(), size) + L"\\" + name;
  HMODULE module = LoadLibraryExW(expected.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  require(module != nullptr, "load-system-module", HRESULT_FROM_WIN32(GetLastError()));
  const auto actual = loaded(module);
  require(!_wcsicmp(expected.c_str(), actual.c_str()), "exact-system-module-path");
  systemPaths.emplace_back(name, actual); return module; // Held through all COM/KMT teardown.
}
void exactLoaded(const std::wstring& expected, const char* stage, const WCHAR* role, bool record = true) {
  HMODULE module = GetModuleHandleW(basename(expected).c_str());
  require(module != nullptr, stage);
  const auto actual = loaded(module);
  require(!_wcsicmp(expected.c_str(), actual.c_str()), stage);
  if (record) privatePaths.emplace_back(role, actual);
}
void exactEnvironment(const WCHAR* name, const std::wstring& expected) {
  std::vector<WCHAR> buffer(32768);
  const DWORD size = GetEnvironmentVariableW(name, buffer.data(), DWORD(buffer.size()));
  require(size && size < buffer.size() && !_wcsicmp(expected.c_str(), absolute(buffer.data()).c_str()), "exact-private-icd-environment");
}
LUID parseLuid(const WCHAR* text) {
  require(text && std::wcslen(text) == 17 && text[8] == L':', "exact-adapter-luid");
  UINT high = 0, low = 0;
  for (UINT i = 0; i < 17; ++i) {
    if (i == 8) continue;
    const WCHAR token = text[i];
    const UINT digit = token >= L'0' && token <= L'9' ? UINT(token - L'0')
      : token >= L'a' && token <= L'f' ? UINT(token - L'a' + 10)
      : token >= L'A' && token <= L'F' ? UINT(token - L'A' + 10) : 16;
    require(digit < 16, "luid-hex"); UINT& part = i < 8 ? high : low; part = (part << 4) | digit;
  }
  require(high || low, "nonzero-luid"); return {low, LONG(high)};
}
bool sameLuid(LUID a, LUID b) { return a.HighPart == b.HighPart && a.LowPart == b.LowPart; }
struct KmtAdapter {
  D3DKMT_HANDLE handle = 0;
  decltype(&D3DKMTQueryAdapterInfo) query = nullptr;
  decltype(&D3DKMTCloseAdapter) closer = nullptr;
  NTSTATUS close() {
    D3DKMT_CLOSEADAPTER args{}; args.hAdapter = handle;
    const NTSTATUS status = closer(&args); if (status == 0) handle = 0; return status;
  }
  void info(KMTQUERYADAPTERINFOTYPE type, void* output, UINT size) {
    D3DKMT_QUERYADAPTERINFO args{}; args.hAdapter = handle; args.Type = type;
    args.pPrivateDriverData = output; args.PrivateDriverDataSize = size;
    const NTSTATUS result = query(&args); require(result == 0, "actual-kmt-query", HRESULT_FROM_NT(result));
  }
};
std::wstring effectiveName(KmtAdapter& adapter) {
  D3DKMT_UMDFILENAMEINFO result{}; result.Version = KMTUMDVERSION_DX11;
  adapter.info(KMTQAITYPE_UMDRIVERNAME, &result, sizeof(result));
  UINT size = 0; while (size < MAX_PATH && result.UmdFileName[size]) ++size;
  require(size && size < MAX_PATH, "bounded-effective-d11-name"); return absolute(result.UmdFileName);
}
bool compatiblePipeline(UINT flags, UINT advertised, D3D_FEATURE_LEVEL requested) {
  const auto level = D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(flags);
  // API feature level is checked exactly on the public device below. The
  // runtime may create its internal DDI device at the adapter's higher level.
  // Accept only the documented profiles, and only if this same adapter
  // advertised the selected profile before CreateDevice.
  if (level != D3D11DDI_3DPIPELINELEVEL_10_0 && level != D3D11DDI_3DPIPELINELEVEL_10_1
      && level != D3D11DDI_3DPIPELINELEVEL_11_0)
    return false;
  if (!(advertised & D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(level))) return false;
  return requested == D3D_FEATURE_LEVEL_10_0
    || (requested == D3D_FEATURE_LEVEL_10_1 && level >= D3D11DDI_3DPIPELINELEVEL_10_1)
    || (requested == D3D_FEATURE_LEVEL_11_0 && level == D3D11DDI_3DPIPELINELEVEL_11_0);
}
std::string diagnostics(const std::wstring& frontend, const std::wstring& core, bool required) {
  HMODULE module = GetModuleHandleW(basename(frontend).c_str());
  if (!module) { require(!required, "runtime-loaded-validation-front"); return "null"; }
  require(!_wcsicmp(frontend.c_str(), loaded(module).c_str()), "runtime-exact-validation-front");
  auto info = std::make_unique<VioGpuD11EntryInfo>(); info->size = sizeof(*info);
  exact(symbol<VioGpuD11ReadEntry>(module, "VioGpuDxvkD11ValidationInfo")(info.get()), "readonly-negotiation-info");
  require(info->schema == 1 && !info->overflow && info->eventCount <= VioGpuD11EventCapacity, "bounded-complete-negotiation-info");
  if (required) {
    require(!_wcsicmp(info->corePath, core.c_str()), "forwarder-exact-core-path");
    exactLoaded(frontend, "runtime-loaded-exact-frontend", L"frontend");
    exactLoaded(core, "runtime-loaded-exact-core", L"core");
  }
  bool created11 = false;
  std::unordered_map<UINT64, UINT> pipelineCaps;
  std::string text = "{\"schema\":1,\"core\":" + json(info->corePath) + ",\"liveAdapters\":" + std::to_string(info->liveAdapters) + ",\"events\":[";
  for (UINT i = 0; i < info->eventCount; ++i) {
    const auto& e = info->events[i]; require(e.completed && e.sequence == i + 1, "completed-ordered-negotiation");
    if (e.call == VioGpuD11Call::Caps && e.result == S_OK
        && e.type == D3D11DDICAPS_3DPIPELINESUPPORT && e.dataSize == sizeof(UINT))
      pipelineCaps[e.adapter] = e.caps;
    if (e.call == VioGpuD11Call::Create && e.result == S_OK
        && (e.interfaceVersion == D3D11_0_DDI_INTERFACE_VERSION || e.interfaceVersion == D3D11_0_7_DDI_INTERFACE_VERSION)
        && compatiblePipeline(e.flags, pipelineCaps[e.adapter], requestedFeature)
        && e.kernelCallbacks && e.coreCallbacks) created11 = true;
    if (i) text += ',';
    text += "{\"sequence\":" + std::to_string(e.sequence) + ",\"call\":" + std::to_string(UINT(e.call))
      + ",\"result\":" + std::to_string(e.result) + ",\"interface\":" + std::to_string(e.interfaceVersion)
      + ",\"version\":" + std::to_string(e.version) + ",\"flags\":" + std::to_string(e.flags)
      + ",\"type\":" + std::to_string(e.type) + ",\"dataSize\":" + std::to_string(e.dataSize)
      + ",\"capacity\":" + std::to_string(e.capacity) + ",\"count\":" + std::to_string(e.count)
      + ",\"caps\":" + std::to_string(e.caps) + ",\"argument\":" + std::to_string(e.argument)
      + ",\"adapter\":" + std::to_string(e.adapter) + ",\"runtimeAdapter\":" + std::to_string(e.runtimeAdapter)
      + ",\"kernelCallbacks\":" + std::to_string(e.kernelCallbacks) + ",\"coreCallbacks\":" + std::to_string(e.coreCallbacks)
      + ",\"returnSize\":" + std::to_string(e.returnSize) + ",\"versions\":[";
    require(e.capturedVersions <= 8, "bounded-version-originals");
    for (UINT j = 0; j < e.capturedVersions; ++j) { if (j) text += ','; text += std::to_string(e.versions[j]); }
    text += "]}";
  }
  require(!required || created11, "runtime-created-typed11-requested-feature-level"); return text + "]}";
}
constexpr UINT Width = 16, Height = 16;
struct Readback { HRESULT map = E_FAIL, removed = E_FAIL, present = E_FAIL, presentRemoved = E_FAIL; UINT rowPitch = 0, screenAttempts = 0; bool windowedBefore = false, windowedAfter = false, screenMatched = false; };
std::array<Readback, 2> readbacks{};
UINT presents = 0;
struct WindowObservation { UINT64 handle = 0; DWORD pid = 0, thread = 0, session = MAXDWORD; };
WindowObservation windowObservation{};
DXGI_SWAP_CHAIN_DESC actualSwapchain{};
bool swapchainDescriptorCaptured = false, windowDestroyed = false, classUnregistered = false;
constexpr UINT WindowAssociationFlags = DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER;
HRESULT windowAssociation = E_FAIL;
constexpr WCHAR WindowClassPrefix[] = L"VioGpuD11PresentOwned-";
void readyWindow(HWND window) {
  MSG message{};
  UINT count = 0;
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
    require(++count <= 256 && message.message != WM_QUIT, "bounded-owned-window-pump");
    TranslateMessage(&message); DispatchMessageW(&message);
  }
  DWORD pid = 0; const DWORD thread = GetWindowThreadProcessId(window, &pid);
  require(IsWindow(window) && IsWindowVisible(window) && !IsIconic(window)
    && pid == GetCurrentProcessId() && thread == GetCurrentThreadId(), "visible-owned-current-thread-window");
}
void waitVisibleSwapchain(IDXGISwapChain* swapchain, HWND window) {
  const ULONGLONG deadline = GetTickCount64() + 5000;
  UINT attempts = 0;
  HRESULT previous = E_UNEXPECTED;
  for (;;) {
    readyWindow(window);
    const HRESULT hr = swapchain->Present(0, DXGI_PRESENT_TEST);
    ++attempts;
    if (hr != previous || GetTickCount64() >= deadline) {
      RECT outer{}, client{};
      GetWindowRect(window, &outer); GetClientRect(window, &client);
      const HWND foreground = GetForegroundWindow();
      DWORD foregroundPid = 0; GetWindowThreadProcessId(foreground, &foregroundPid);
      WCHAR desktop[128] = {}, inputName[128] = {}; DWORD needed = 0;
      GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
        desktop, sizeof(desktop), &needed);
      HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
      const DWORD inputError = input ? ERROR_SUCCESS : GetLastError();
      if (input) {
        GetUserObjectInformationW(input, UOI_NAME, inputName, sizeof(inputName), &needed);
        CloseDesktop(input);
      }
      std::fprintf(stderr, "DXGI_VISIBILITY hr=%08lx attempts=%u outer=%ld,%ld,%ld,%ld client=%ld,%ld,%ld,%ld foreground=%p foreground_pid=%lu desktop=%ls input_desktop=%ls input_error=%lu\n",
        static_cast<unsigned long>(hr), attempts, outer.left, outer.top, outer.right, outer.bottom,
        client.left, client.top, client.right, client.bottom, foreground, foregroundPid,
        desktop, inputName, inputError);
      std::fflush(stderr);
      previous = hr;
    }
    if (hr == S_OK) return;
    // DXGI can still be processing a newly shown window. An occluded status
    // is never counted as a presented frame; test visibility for a finite time.
    if (hr != DXGI_STATUS_OCCLUDED || GetTickCount64() >= deadline)
      exact(hr, "bounded-swapchain-visibility");
    MsgWaitForMultipleObjectsEx(0, nullptr, 50, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  }
}
void visiblePixels(HWND window, UINT frame, const std::wstring& directory) {
  RECT client{}; POINT origin{};
  require(GetClientRect(window, &client) && client.right == Width && client.bottom == Height
    && ClientToScreen(window, &origin), "exact-unscaled-visible-client");
  std::array<COLORREF, Width * Height> pixels{};
  const COLORREF expected = frame ? RGB(255, 0, 0) : RGB(0, 0, 0);
  const ULONGLONG deadline = GetTickCount64() + 5000;
  do {
    readyWindow(window);
    HDC dc = GetDC(nullptr); require(dc != nullptr, "visible-desktop-dc");
    bool valid = true, matched = true;
    for (UINT y = 0; y < Height; ++y) for (UINT x = 0; x < Width; ++x) {
      const auto color = GetPixel(dc, origin.x + int(x), origin.y + int(y));
      pixels[y * Width + x] = color;
      valid = valid && color != CLR_INVALID;
      matched = matched && color == expected;
    }
    require(ReleaseDC(nullptr, dc) == 1 && valid, "visible-desktop-read-release");
    ++readbacks[frame].screenAttempts;
    readbacks[frame].screenMatched = matched;
    if (matched) break;
    MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  } while (GetTickCount64() < deadline);
  save(directory, frame ? L"draw-screen.raw" : L"clear-screen.raw", pixels.data(), DWORD(sizeof(pixels)));
  require(readbacks[frame].screenMatched, "literal-512-visible-desktop-pixels");
}
struct Api10 {
  using Device = ID3D10Device; using Context = ID3D10Device; using Texture = ID3D10Texture2D;
  using View = ID3D10RenderTargetView; using Vertex = ID3D10VertexShader; using Pixel = ID3D10PixelShader;
  using Raster = ID3D10RasterizerState; using RasterDesc = D3D10_RASTERIZER_DESC;
  using TextureDesc = D3D10_TEXTURE2D_DESC; using Viewport = D3D10_VIEWPORT; using Mapped = D3D10_MAPPED_TEXTURE2D;
  static constexpr auto Default = D3D10_USAGE_DEFAULT, Staging = D3D10_USAGE_STAGING;
  static constexpr auto Fill = D3D10_FILL_SOLID; static constexpr auto Cull = D3D10_CULL_NONE;
  static void shaders(Device* device, ID3DBlob* vs, ID3DBlob* ps, Vertex** vertex, Pixel** pixel) {
    exact(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), vertex), "public-create-vs");
    exact(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), pixel), "public-create-ps");
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
  static constexpr auto Default = D3D11_USAGE_DEFAULT, Staging = D3D11_USAGE_STAGING;
  static constexpr auto Fill = D3D11_FILL_SOLID; static constexpr auto Cull = D3D11_CULL_NONE;
  static void shaders(Device* device, ID3DBlob* vs, ID3DBlob* ps, Vertex** vertex, Pixel** pixel) {
    exact(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, vertex), "public-create-vs");
    exact(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, pixel), "public-create-ps");
  }
  static void bind(Context* context, Vertex* vertex, Pixel* pixel) {
    context->VSSetShader(vertex, nullptr, 0); context->PSSetShader(pixel, nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  }
  static HRESULT map(Context* context, Texture* texture, Mapped* mapped) { return context->Map(texture, 0, D3D11_MAP_READ, 0, mapped); }
  static void unmap(Context* context, Texture* texture) { context->Unmap(texture, 0); }
};
template<typename Api> void render(typename Api::Device* device, typename Api::Context* context, IDXGISwapChain* swapchain,
    HWND window, HMODULE compiler, const std::wstring& directory) {
  ComPtr<typename Api::Texture> target, staging;
  exact(swapchain->GetBuffer(0, IID_PPV_ARGS(&target)), "ordinary-swapchain-backbuffer");
  typename Api::TextureDesc desc{}; target->GetDesc(&desc);
  require(desc.Width == Width && desc.Height == Height && desc.MipLevels == 1 && desc.ArraySize == 1
    && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM && desc.SampleDesc.Count == 1 && !desc.SampleDesc.Quality
    && desc.Usage == Api::Default && (desc.BindFlags & D3D11_BIND_RENDER_TARGET), "exact-swapchain-backbuffer-descriptor");
  desc.Usage = Api::Staging; desc.BindFlags = 0; desc.MiscFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  exact(device->CreateTexture2D(&desc, nullptr, &staging), "public-create-staging");
  ComPtr<typename Api::View> view; exact(device->CreateRenderTargetView(target.Get(), nullptr, &view), "public-create-rtv");
  auto rawView = view.Get(); context->OMSetRenderTargets(1, &rawView, nullptr);
  typename Api::Viewport viewport{}; viewport.Width = Width; viewport.Height = Height; viewport.MaxDepth = 1;
  context->RSSetViewports(1, &viewport);
  typename Api::RasterDesc raster{}; raster.FillMode = Api::Fill; raster.CullMode = Api::Cull; raster.DepthClipEnable = TRUE;
  ComPtr<typename Api::Raster> state; exact(device->CreateRasterizerState(&raster, &state), "public-create-rasterizer");
  context->RSSetState(state.Get());
  const char hlsl[] = "float4 vs(uint i:SV_VertexID):SV_Position{float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[i],0,1);}float4 ps():SV_Target{return float4(1,0,0,1);}";
  const auto compile = symbol<decltype(&D3DCompile)>(compiler, "D3DCompile");
  ComPtr<ID3DBlob> vs, ps;
  exact(compile(hlsl, sizeof(hlsl)-1, nullptr, nullptr, nullptr, "vs", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, nullptr), "system-fxc-vs");
  exact(compile(hlsl, sizeof(hlsl)-1, nullptr, nullptr, nullptr, "ps", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, nullptr), "system-fxc-ps");
  save(directory, L"vs.dxbc", vs->GetBufferPointer(), DWORD(vs->GetBufferSize()));
  save(directory, L"ps.dxbc", ps->GetBufferPointer(), DWORD(ps->GetBufferSize()));
  ComPtr<typename Api::Vertex> vertex; ComPtr<typename Api::Pixel> pixel;
  Api::shaders(device, vs.Get(), ps.Get(), &vertex, &pixel);
  Api::bind(context, vertex.Get(), pixel.Get());
  const float black[4]{0, 0, 0, 1};
  for (UINT frame = 0; frame < 2; ++frame) {
    // DISCARD permits contents to change after Present. Rebind and fully
    // overwrite each frame rather than relying on the previous buffer.
    context->OMSetRenderTargets(1, &rawView, nullptr);
    context->ClearRenderTargetView(view.Get(), black);
    if (frame) context->Draw(3, 0);
    context->CopyResource(staging.Get(), target.Get()); context->Flush();
    typename Api::Mapped mapped{}; readbacks[frame].map = Api::map(context, staging.Get(), &mapped);
    exact(readbacks[frame].map, "blocking-public-map");
    readbacks[frame].rowPitch = mapped.RowPitch;
    const UINT64 span = UINT64(Height - 1) * mapped.RowPitch + Width * 4;
    if (!mapped.pData || mapped.RowPitch < Width*4 || span > UINTPTR_MAX
        || reinterpret_cast<UINT_PTR>(mapped.pData) > UINTPTR_MAX - (span - 1)) {
      Api::unmap(context, staging.Get()); throw Failure{"bounded-readback-pitch", E_FAIL};
    }
    std::array<unsigned char, Width*Height*4> original{};
    for (UINT y = 0; y < Height; ++y) std::memcpy(original.data() + y*Width*4,
      static_cast<const unsigned char*>(mapped.pData) + size_t(y)*mapped.RowPitch, Width*4);
    Api::unmap(context, staging.Get());
    save(directory, frame ? L"draw.raw" : L"clear.raw", original.data(), DWORD(original.size()));
    for (UINT p = 0; p < Width*Height; ++p) require(original[p*4] == (frame ? 255 : 0)
      && original[p*4+1] == 0 && original[p*4+2] == 0 && original[p*4+3] == 255, "literal-512-pixel-oracle");
    readbacks[frame].removed = device->GetDeviceRemovedReason(); exact(readbacks[frame].removed, "actual-device-removal-readback");
    waitVisibleSwapchain(swapchain, window);
    BOOL fullscreen = TRUE;
    exact(swapchain->GetFullscreenState(&fullscreen, nullptr), "actual-windowed-before-present");
    readbacks[frame].windowedBefore = fullscreen == FALSE;
    require(readbacks[frame].windowedBefore, "no-fullscreen-before-present");
    readbacks[frame].present = swapchain->Present(1, 0);
    exact(readbacks[frame].present, "ordinary-dxgi-present-exact-s-ok"); ++presents;
    readbacks[frame].presentRemoved = device->GetDeviceRemovedReason();
    exact(readbacks[frame].presentRemoved, "actual-device-removal-present");
    fullscreen = TRUE;
    exact(swapchain->GetFullscreenState(&fullscreen, nullptr), "actual-windowed-after-present");
    readbacks[frame].windowedAfter = fullscreen == FALSE;
    require(readbacks[frame].windowedAfter, "no-fullscreen-after-present");
    visiblePixels(window, frame, directory);
  }
  context->ClearState(); context->Flush();
}
#include "umd-d3d11-display-cycle.h"

UINT sm5ComputeWords = 0, sm5TessellationPixels = 0;
D3D11_QUERY_DATA_PIPELINE_STATISTICS sm5ComputeStats{}, sm5TessellationStats{};
void validateSm5(ID3D11Device* device, ID3D11DeviceContext* context, HMODULE compiler,
    const std::wstring& directory) {
  require(device->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0, "sm5-exact-fl11-device");
  const auto compile = symbol<decltype(&D3DCompile)>(compiler, "D3DCompile");
  const auto shader = [&](const char* source, const char* entry, const char* target) {
    ComPtr<ID3DBlob> code, errors;
    const HRESULT hr = compile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target,
      D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors);
    const std::wstring name = L"sm5-" + std::wstring(entry, entry + std::strlen(entry));
    if (errors) save(directory, (name + L"-compiler.txt").c_str(), errors->GetBufferPointer(), DWORD(errors->GetBufferSize()));
    exact(hr, "system-fxc-sm5");
    save(directory, (name + L".dxbc").c_str(), code->GetBufferPointer(), DWORD(code->GetBufferSize()));
    return code;
  };
  const auto query = [&]() {
    D3D11_QUERY_DESC desc{D3D11_QUERY_PIPELINE_STATISTICS, 0};
    ComPtr<ID3D11Query> value;
    exact(device->CreateQuery(&desc, &value), "sm5-create-statistics-query");
    return value;
  };
  const auto statistics = [&](ID3D11Query* value, D3D11_QUERY_DATA_PIPELINE_STATISTICS& data) {
    context->Flush();
    const ULONGLONG deadline = GetTickCount64() + 5000;
    HRESULT hr;
    do {
      hr = context->GetData(value, &data, sizeof(data), 0);
      if (hr != S_FALSE) break;
      Sleep(1);
    } while (GetTickCount64() < deadline);
    exact(hr, "bounded-sm5-pipeline-statistics");
  };
  {
    // Guard words surround the GPU-written interval; a dispatch that never
    // executes, writes an incorrect index, or overwrites the tail must fail.
    std::array<UINT, 272> initial{}; initial.fill(0xcdcdcdcd);
    D3D11_BUFFER_DESC desc{}; desc.ByteWidth = sizeof(initial);
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; desc.StructureByteStride = sizeof(UINT);
    D3D11_SUBRESOURCE_DATA data{initial.data(), 0, 0};
    ComPtr<ID3D11Buffer> buffer, readback;
    exact(device->CreateBuffer(&desc, &data, &buffer), "sm5-create-structured-uav-buffer");
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
    desc.MiscFlags = 0; desc.StructureByteStride = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    exact(device->CreateBuffer(&desc, nullptr, &readback), "sm5-create-buffer-staging");
    D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc{}; viewDesc.Format = DXGI_FORMAT_UNKNOWN;
    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; viewDesc.Buffer.NumElements = UINT(initial.size());
    ComPtr<ID3D11UnorderedAccessView> view;
    exact(device->CreateUnorderedAccessView(buffer.Get(), &viewDesc, &view), "sm5-create-buffer-uav");
    auto code = shader("RWStructuredBuffer<uint> output:register(u0);"
      "[numthreads(64,1,1)]void cs(uint3 id:SV_DispatchThreadID){output[id.x+4]=(id.x*17+3)^0x5a5aa5a5;}", "cs", "cs_5_0");
    ComPtr<ID3D11ComputeShader> compute;
    exact(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &compute), "sm5-create-compute-shader");
    context->CSSetShader(compute.Get(), nullptr, 0);
    auto rawView = view.Get(); context->CSSetUnorderedAccessViews(0, 1, &rawView, nullptr);
    auto stats = query(); context->Begin(stats.Get());
    context->Dispatch(4, 1, 1); context->End(stats.Get());
    rawView = nullptr; context->CSSetUnorderedAccessViews(0, 1, &rawView, nullptr);
    context->CopyResource(readback.Get(), buffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    exact(context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "sm5-map-compute-output");
    if (!mapped.pData) { context->Unmap(readback.Get(), 0); throw Failure{"sm5-null-compute-mapping", E_FAIL}; }
    std::array<UINT, 272> actual{}; std::memcpy(actual.data(), mapped.pData, sizeof(actual));
    context->Unmap(readback.Get(), 0);
    save(directory, L"sm5-compute.raw", actual.data(), DWORD(sizeof(actual)));
    for (UINT i = 0; i < actual.size(); ++i) {
      const UINT expected = i >= 4 && i < 260 ? (((i - 4) * 17 + 3) ^ 0x5a5aa5a5u) : 0xcdcdcdcdu;
      require(actual[i] == expected, "literal-sm5-compute-data-and-guards");
    }
    statistics(stats.Get(), sm5ComputeStats);
    require(sm5ComputeStats.CSInvocations == 256, "sm5-compute-invocation-count");
    sm5ComputeWords = UINT(actual.size());
    context->ClearState();
  }
  {
    const char source[] = R"(
struct CP { float4 p : POSITION; };
struct Factors { float edges[3] : SV_TessFactor; float inside : SV_InsideTessFactor; };
CP vs(uint id : SV_VertexID) {
  float2 points[3] = {float2(-1,-1),float2(-1,3),float2(3,-1)};
  CP result; result.p=float4(points[id],0,1); return result;
}
Factors patch(InputPatch<CP,3> points, uint id : SV_PrimitiveID) {
  Factors f; f.edges[0]=2; f.edges[1]=2; f.edges[2]=2; f.inside=2; return f;
}
[domain("tri")][partitioning("integer")][outputtopology("triangle_cw")]
[outputcontrolpoints(3)][patchconstantfunc("patch")]
CP hs(InputPatch<CP,3> points, uint id : SV_OutputControlPointID) { return points[id]; }
[domain("tri")]
float4 ds(Factors f, float3 bary : SV_DomainLocation, const OutputPatch<CP,3> points) : SV_Position {
  return bary.x*points[0].p+bary.y*points[1].p+bary.z*points[2].p;
}
float4 ps() : SV_Target { return float4(0,1,0,1); }
)";
    auto vs = shader(source, "vs", "vs_5_0"), hs = shader(source, "hs", "hs_5_0");
    auto ds = shader(source, "ds", "ds_5_0"), ps = shader(source, "ps", "ps_5_0");
    ComPtr<ID3D11VertexShader> vertex; ComPtr<ID3D11HullShader> hull;
    ComPtr<ID3D11DomainShader> domain; ComPtr<ID3D11PixelShader> pixel;
    exact(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex), "sm5-create-tessellation-vs");
    exact(device->CreateHullShader(hs->GetBufferPointer(), hs->GetBufferSize(), nullptr, &hull), "sm5-create-hull-shader");
    exact(device->CreateDomainShader(ds->GetBufferPointer(), ds->GetBufferSize(), nullptr, &domain), "sm5-create-domain-shader");
    exact(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel), "sm5-create-tessellation-ps");
    D3D11_TEXTURE2D_DESC desc{}; desc.Width = Width; desc.Height = Height;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target, staging;
    exact(device->CreateTexture2D(&desc, nullptr, &target), "sm5-create-tessellation-target");
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    exact(device->CreateTexture2D(&desc, nullptr, &staging), "sm5-create-tessellation-staging");
    ComPtr<ID3D11RenderTargetView> view;
    exact(device->CreateRenderTargetView(target.Get(), nullptr, &view), "sm5-create-tessellation-rtv");
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> state;
    exact(device->CreateRasterizerState(&raster, &state), "sm5-create-tessellation-rasterizer");
    const D3D11_VIEWPORT viewport{0, 0, float(Width), float(Height), 0, 1};
    context->RSSetState(state.Get()); context->RSSetViewports(1, &viewport);
    auto rawView = view.Get(); context->OMSetRenderTargets(1, &rawView, nullptr);
    const float black[4]{0, 0, 0, 1}; context->ClearRenderTargetView(view.Get(), black);
    context->VSSetShader(vertex.Get(), nullptr, 0); context->HSSetShader(hull.Get(), nullptr, 0);
    context->DSSetShader(domain.Get(), nullptr, 0); context->PSSetShader(pixel.Get(), nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
    auto stats = query(); context->Begin(stats.Get());
    context->Draw(3, 0); context->End(stats.Get());
    context->CopyResource(staging.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    exact(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "sm5-map-tessellation-output");
    const UINT64 span = UINT64(Height - 1) * mapped.RowPitch + Width * 4;
    if (!mapped.pData || mapped.RowPitch < Width * 4 || span > UINTPTR_MAX
        || reinterpret_cast<UINT_PTR>(mapped.pData) > UINTPTR_MAX - (span - 1)) {
      context->Unmap(staging.Get(), 0); throw Failure{"sm5-bounded-tessellation-pitch", E_FAIL};
    }
    std::array<unsigned char, Width * Height * 4> actual{};
    for (UINT y = 0; y < Height; ++y) std::memcpy(actual.data() + y * Width * 4,
      static_cast<const unsigned char*>(mapped.pData) + size_t(y) * mapped.RowPitch, Width * 4);
    context->Unmap(staging.Get(), 0);
    save(directory, L"sm5-tessellation.raw", actual.data(), DWORD(actual.size()));
    for (UINT i = 0; i < Width * Height; ++i) require(actual[i*4] == 0 && actual[i*4+1] == 255
      && actual[i*4+2] == 0 && actual[i*4+3] == 255, "literal-sm5-tessellation-pixels");
    statistics(stats.Get(), sm5TessellationStats);
    require(sm5TessellationStats.HSInvocations > 0 && sm5TessellationStats.DSInvocations >= 3,
      "actual-sm5-hull-and-domain-invocations");
    sm5TessellationPixels = Width * Height;
    context->ClearState(); context->Flush();
  }
  exact(device->GetDeviceRemovedReason(), "sm5-device-still-healthy");
}
void negative(const WCHAR* input) {
  const auto expected = absolute(input);
  HMODULE module = LoadLibraryExW(expected.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  require(module && !_wcsicmp(loaded(module).c_str(), expected.c_str()), "negative-exact-front");
  const auto entry = symbol<PFND3D10DDI_OPENADAPTER>(module, "OpenAdapter10_2");
  UINT checks = 0;
  const auto check = [&](bool value, const char* stage) { ++checks; require(value, stage); };
  displayCycleControls(check);
  check(compatiblePipeline(0x13, 3, D3D_FEATURE_LEVEL_10_0), "runtime-higher-advertised-ddi-profile");
  check(compatiblePipeline(0x13, 3, D3D_FEATURE_LEVEL_10_1), "runtime-matching-ddi-profile");
  check(compatiblePipeline(0x11, 1, D3D_FEATURE_LEVEL_10_0), "runtime-advertised-ddi10-profile");
  check(!compatiblePipeline(0x11, 3, D3D_FEATURE_LEVEL_10_1), "reject-insufficient-ddi-profile");
  check(!compatiblePipeline(0x13, 1, D3D_FEATURE_LEVEL_10_0), "reject-unadvertised-ddi-profile");
  check(!compatiblePipeline(0x13, 0, D3D_FEATURE_LEVEL_10_0), "reject-missing-adapter-caps");
  check(compatiblePipeline(0x15, 7, D3D_FEATURE_LEVEL_10_0), "runtime-higher-advertised-ddi11-profile");
  check(compatiblePipeline(0x15, 7, D3D_FEATURE_LEVEL_10_1), "runtime-ddi11-for-public-10-1");
  check(compatiblePipeline(0x15, 7, D3D_FEATURE_LEVEL_11_0), "runtime-matching-ddi11-profile");
  check(!compatiblePipeline(0x13, 7, D3D_FEATURE_LEVEL_11_0), "reject-lower-created-ddi-profile");
  check(!compatiblePipeline(0x15, 3, D3D_FEATURE_LEVEL_11_0), "reject-unadvertised-ddi11-profile");
  check(!compatiblePipeline(0x13, 3, D3D_FEATURE_LEVEL_11_0), "reject-unavailable-public-feature");
  check(!compatiblePipeline(0x17, 15, D3D_FEATURE_LEVEL_11_0), "reject-reserved-ddi-profile");
  check(!compatiblePipeline(0x15, 7, D3D_FEATURE_LEVEL_11_1), "reject-unvalidated-public-feature");
  check(dxvkWorkerCount() == 0, "no-initial-dxvk-worker");
  PWSTR oldName = nullptr;
  check(SUCCEEDED(GetThreadDescription(GetCurrentThread(), &oldName)), "save-current-thread-name");
  struct NameScope {
    PWSTR name;
    ~NameScope() { SetThreadDescription(GetCurrentThread(), name ? name : L""); if (name) LocalFree(name); }
  } nameScope{oldName};
  check(SUCCEEDED(SetThreadDescription(GetCurrentThread(), L"dxvk-probe-control")), "name-actual-owned-thread");
  check(dxvkWorkerCount() == 1, "detect-actual-named-worker");
  check(SUCCEEDED(SetThreadDescription(GetCurrentThread(), oldName ? oldName : L"")), "restore-current-thread-name");
  check(dxvkWorkerCount() == 0, "no-worker-after-restore");
  check(entry(nullptr) == E_INVALIDARG, "typed-null-modern-entry");
  struct Guarded { UINT64 before; D3D10_2DDI_ADAPTERFUNCS functions; UINT64 after; } storage{};
  storage.before = storage.after = 0x53797374656d4431ull; std::memset(&storage.functions, 0xa5, sizeof(storage.functions));
  D3D10DDIARG_OPENADAPTER args{}; args.pAdapterFuncs_2 = &storage.functions; args.Interface = 0xffffffff;
  args.hAdapter.pDrvPrivate = reinterpret_cast<void*>(UINT_PTR(0xa5));
  check(entry(&args) == E_INVALIDARG && !args.hAdapter.pDrvPrivate, "missing-callback-modern-guard");
  const D3D10_2DDI_ADAPTERFUNCS zero{};
  check(!std::memcmp(&storage.functions, &zero, sizeof(zero)) && storage.before == storage.after
    && storage.before == 0x53797374656d4431ull, "bounded-modern-table-clear");
  const auto read = symbol<VioGpuD11ReadEntry>(module, "VioGpuDxvkD11ValidationInfo");
  check(read(nullptr) == E_INVALIDARG, "readonly-null-guard");
  auto info = std::make_unique<VioGpuD11EntryInfo>();
  check(read(info.get()) == E_INVALIDARG, "readonly-size-guard"); info->size = sizeof(*info);
  check(read(info.get()) == S_OK, "readonly-info");
  check(info->schema == 1 && info->eventCount == 1 && !info->overflow && !info->liveAdapters
    && !info->corePath[0] && info->events[0].completed && info->events[0].result == E_INVALIDARG, "negative-no-core-no-adapter");
  check(GetProcAddress(module, "OpenAdapter10") == nullptr, "no-legacy-fallback-export");
  check(FreeLibrary(module) != FALSE, "negative-front-release");
  std::printf("SYSTEM_D3D11_VALIDATION_NEGATIVE_PASS checks=%u core_loaded=0 registry_changes=0 gpu_calls=0\n", checks);
}
}

int wmain(int argc, WCHAR** argv) {
  if (argc == 3 && !std::wcscmp(argv[1], L"--entry-negative")) {
    try { negative(argv[2]); return 0; }
    catch (const Failure& error) { std::fprintf(stderr, "SYSTEM_D3D11_FAIL stage=%s hr=%08lx\n", error.stage, static_cast<unsigned long>(error.result)); return 1; }
  }
  if (argc < 10 || argc > 15) {
    std::fputs("usage: probe <high:low-LUID> <absolute-front> <absolute-core> <absolute-private-loader> <absolute-ICD-DLL> <absolute-owned-ICD-JSON> <fresh-output-dir> <Local-hold-event> <hold-ms> [10|10.1|11] [pinned|discovery] [default|inline] [10_0|10_1|11_0] [display-cycle|display-blt-cycle]\n", stderr); return 2;
  }
  ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D10Device> device10; ComPtr<ID3D10Device1> device10_1;
  ComPtr<IDXGISwapChain> swapchain;
  KmtAdapter kmt; HANDLE hold = nullptr; HWND window = nullptr;
  HINSTANCE instance = GetModuleHandleW(nullptr); std::wstring windowClass;
  ATOM classAtom = 0;
  std::wstring front, core, directory, privateLoader, icd, icdJson, effective;
  bool outputCreated = false, factoryCalled = false, pixelsPassed = false, heldReleased = false;
  bool discoverIcd = false;
  bool inlineWork = false;
  UINT creationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
  unsigned rendererWorkers = UINT_MAX;
  HRESULT factoryResult = E_FAIL, failureResult = E_FAIL; const char* failureStage = "unstarted";
  LUID expected{}; dxvk::umd::RuntimeIdentity identity{}; D3D_FEATURE_LEVEL obtained{};
  DWORD holdMs = 0; NTSTATUS closeResult = NTSTATUS(-1073741823L);
  try {
    if (argc >= 11) publicApi = !std::wcscmp(argv[10], L"10") ? 10u : !std::wcscmp(argv[10], L"10.1") ? 101u : !std::wcscmp(argv[10], L"11") ? 11u : 0u;
    require(publicApi != 0, "explicit-system-api");
    if (argc >= 12) {
      require(!std::wcscmp(argv[11], L"pinned") || !std::wcscmp(argv[11], L"discovery"), "explicit-icd-selection");
      discoverIcd = !std::wcscmp(argv[11], L"discovery");
    }
    if (argc >= 13) {
      require(!std::wcscmp(argv[12], L"default") || !std::wcscmp(argv[12], L"inline"), "explicit-threading-selection");
      inlineWork = !std::wcscmp(argv[12], L"inline");
    }
    static_assert(D3D10_CREATE_DEVICE_SINGLETHREADED == UINT(D3D11_CREATE_DEVICE_SINGLETHREADED));
    static_assert(D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
      == UINT(D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS));
    if (inlineWork) creationFlags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
    requestedFeature = publicApi == 101 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_10_0;
    if (argc >= 14) {
      require(publicApi == 11, "explicit-feature-requires-d3d11-api");
      requestedFeature = !std::wcscmp(argv[13], L"10_0") ? D3D_FEATURE_LEVEL_10_0
        : !std::wcscmp(argv[13], L"10_1") ? D3D_FEATURE_LEVEL_10_1
        : !std::wcscmp(argv[13], L"11_0") ? D3D_FEATURE_LEVEL_11_0 : D3D_FEATURE_LEVEL(0);
      require(requestedFeature != D3D_FEATURE_LEVEL(0), "explicit-feature-level");
    }
    if (argc == 15) {
      displayCyclePreserve = !std::wcscmp(argv[14], L"display-blt-cycle");
      require(publicApi == 11 && (displayCyclePreserve || !std::wcscmp(argv[14], L"display-cycle")), "explicit-display-cycle");
      displayCycleRequested = true;
    }
    expected = parseLuid(argv[1]); front = absolute(argv[2]); core = absolute(argv[3]);
    privateLoader = absolute(argv[4]); icd = absolute(argv[5]); icdJson = absolute(argv[6]); directory = absolute(argv[7]);
    WCHAR* tail = nullptr; const unsigned long duration = std::wcstoul(argv[9], &tail, 10);
    require(tail && !*tail && duration && duration <= 60000, "bounded-hold-ms"); holdMs = DWORD(duration);
    require(!std::wcsncmp(argv[8], L"Local\\", 6) && argv[8][6], "session-local-hold-name");
    hold = CreateEventW(nullptr, TRUE, FALSE, argv[8]); const DWORD eventError = GetLastError();
    if (!hold || eventError == ERROR_ALREADY_EXISTS) {
      if (hold) CloseHandle(hold); hold = nullptr;
      throw Failure{"fresh-hold-event", HRESULT_FROM_WIN32(eventError ? eventError : ERROR_INVALID_HANDLE)};
    }
    require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "fresh-output-directory", HRESULT_FROM_WIN32(GetLastError())); outputCreated = true;
    for (const auto& path : {front, core, privateLoader, icd}) require(!GetModuleHandleW(basename(path).c_str()), "no-candidate-manual-preload");
    if (discoverIcd) {
      // Exercise an ordinary application environment. Exact loaded-module
      // checks and the pixel/Present oracle remain identical to pinned mode.
      for (const WCHAR* name : {L"VK_DRIVER_FILES", L"VK_ICD_FILENAMES", L"VK_ADD_DRIVER_FILES"}) {
        SetLastError(ERROR_SUCCESS);
        require(!GetEnvironmentVariableW(name, nullptr, 0) && GetLastError() == ERROR_ENVVAR_NOT_FOUND,
          "no-icd-environment-override");
      }
    } else {
      exactEnvironment(L"VK_DRIVER_FILES", icdJson); exactEnvironment(L"VK_ICD_FILENAMES", icdJson);
    }
    HMODULE dxgi = systemModule(L"dxgi.dll"), runtime = systemModule(publicApi == 10 ? L"d3d10.dll" : publicApi == 101 ? L"d3d10_1.dll" : L"d3d11.dll");
    HMODULE gdi = systemModule(L"gdi32.dll"), compiler = systemModule(L"d3dcompiler_47.dll");
    systemModule(L"user32.dll"); // Verify the window API module against System32 too.
    ComPtr<IDXGIFactory1> factory;
    exact(symbol<decltype(&CreateDXGIFactory1)>(dxgi, "CreateDXGIFactory1")(IID_PPV_ARGS(&factory)), "system-dxgi-factory");
    ComPtr<IDXGIAdapter1> selected;
    for (UINT index = 0;; ++index) {
      ComPtr<IDXGIAdapter1> candidate; const HRESULT hr = factory->EnumAdapters1(index, &candidate);
      if (hr == DXGI_ERROR_NOT_FOUND) break; exact(hr, "actual-adapter-enumeration");
      DXGI_ADAPTER_DESC1 desc{}; exact(candidate->GetDesc1(&desc), "actual-adapter-description");
      if (sameLuid(desc.AdapterLuid, expected)) {
        require(!selected && desc.VendorId == 0x1af4 && desc.DeviceId == 0x1050 && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE), "unique-selected-hardware-viogpu"); selected = candidate;
      }
    }
    require(selected != nullptr, "selected-luid-present");
    kmt.query = symbol<decltype(&D3DKMTQueryAdapterInfo)>(gdi, "D3DKMTQueryAdapterInfo");
    kmt.closer = symbol<decltype(&D3DKMTCloseAdapter)>(gdi, "D3DKMTCloseAdapter");
    D3DKMT_OPENADAPTERFROMLUID open{}; open.AdapterLuid = expected;
    const NTSTATUS status = symbol<decltype(&D3DKMTOpenAdapterFromLuid)>(gdi, "D3DKMTOpenAdapterFromLuid")(&open);
    kmt.handle = open.hAdapter; require(status == 0 && kmt.handle, "actual-kmt-open-luid", HRESULT_FROM_NT(status));
    D3DKMT_ADAPTERTYPE type{}; kmt.info(KMTQAITYPE_ADAPTERTYPE, &type, sizeof(type));
    require(type.RenderSupported && !type.SoftwareDevice, "actual-kmt-nonsoftware-render");
    std::array<unsigned char, dxvk::umd::RuntimeIdentityReplySize> raw{};
    kmt.info(KMTQAITYPE_UMDRIVERPRIVATE, raw.data(), UINT(raw.size())); save(directory, L"identity.raw", raw.data(), UINT(raw.size()));
    require(dxvk::umd::readRuntimeIdentity(raw.data(), raw.size(), identity)
      && !std::memcmp(identity.luid.data(), &expected, sizeof(expected)), "actual-kmt-viogpu-private-identity");
    effective = effectiveName(kmt);
    std::printf("SYSTEM_D3D11_SELECTED luid=%08x:%08x effective=%s frontend=%s\n", UINT(expected.HighPart), UINT(expected.LowPart), json(effective).c_str(), json(front).c_str()); std::fflush(stdout);
    require(!_wcsicmp(effective.c_str(), front.c_str()), "effective-modern-validation-name");
    factoryCalled = true;
    const char* factoryStage = publicApi == 10 ? "ordinary-system-d3d10-create-device"
      : publicApi == 101 ? "ordinary-system-d3d10_1-create-device" : "ordinary-system-d3d11-create-device";
    if (publicApi == 10) {
      factoryResult = symbol<decltype(&D3D10CreateDevice)>(runtime, "D3D10CreateDevice")(selected.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr,
        creationFlags, D3D10_SDK_VERSION, &device10);
      if (factoryResult == S_OK && device10) obtained = D3D_FEATURE_LEVEL_10_0; // D3D10CreateDevice has a fixed 10.0 contract.
    } else if (publicApi == 101) {
      factoryResult = symbol<decltype(&D3D10CreateDevice1)>(runtime, "D3D10CreateDevice1")(selected.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr,
        creationFlags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &device10_1);
      if (factoryResult == S_OK && device10_1) {
        obtained = D3D_FEATURE_LEVEL(device10_1->GetFeatureLevel());
        exact(device10_1.As(&device10), "public-d3d10_1-base-device");
      }
    } else {
      factoryResult = symbol<decltype(&D3D11CreateDevice)>(runtime, "D3D11CreateDevice")(selected.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        creationFlags, &requestedFeature, 1, D3D11_SDK_VERSION, &device, &obtained, &context);
    }
    // Save the actual negotiation even if the SYSTEM factory failed.
    saveText(directory, L"negotiation.json", diagnostics(front, core, false) + '\n');
    exact(factoryResult, factoryStage);
    require(obtained == requestedFeature, "exact-requested-feature-level");
    if (publicApi == 11) require(device && context && device->GetFeatureLevel() == requestedFeature && device->GetCreationFlags() == creationFlags, "exact-d3d11-creation-flags");
    else require(device10 && device10->GetCreationFlags() == creationFlags, "exact-d3d10-creation-flags");
    diagnostics(front, core, true); exactLoaded(privateLoader, "actual-private-vulkan-loader", L"privateLoader");
    exactLoaded(icd, "actual-private-mesa-icd", L"icd");
    require(!GetModuleHandleW(L"warp.dll") && !GetModuleHandleW(L"d3d10warp.dll"), "no-software-runtime-modules");
    // The approved private Khronos loader may genuinely be named winevulkan.
    // Accept only its exact owned path; an unrelated loader is never adopted.
    for (const WCHAR* name : {L"winevulkan.dll", L"vulkan-1.dll"}) {
      HMODULE module = GetModuleHandleW(name);
      require(!module || !_wcsicmp(loaded(module).c_str(), privateLoader.c_str()), "no-unapproved-vulkan-loader");
    }
    ComPtr<IDXGIDevice> dxgiDevice; exact(publicApi == 11 ? device.As(&dxgiDevice) : device10.As(&dxgiDevice), "device-dxgi-interface");
    ComPtr<IDXGIAdapter> actual; exact(dxgiDevice->GetAdapter(&actual), "created-device-adapter");
    DXGI_ADAPTER_DESC actualDesc{}; exact(actual->GetDesc(&actualDesc), "created-device-description");
    require(sameLuid(actualDesc.AdapterLuid, expected) && actualDesc.VendorId == 0x1af4 && actualDesc.DeviceId == 0x1050, "created-device-exact-selected-luid");
    require(ProcessIdToSessionId(GetCurrentProcessId(), &windowObservation.session) && windowObservation.session != 0,
      "interactive-nonzero-session-required");
    windowClass = WindowClassPrefix + std::to_wstring(GetCurrentProcessId());
    require(SetProcessDPIAware() != FALSE, "physical-client-pixel-coordinates");
    WNDCLASSW klass{}; klass.lpfnWndProc = DefWindowProcW; klass.hInstance = instance; klass.lpszClassName = windowClass.c_str();
    classAtom = RegisterClassW(&klass); require(classAtom != 0, "fresh-owned-window-class", HRESULT_FROM_WIN32(GetLastError()));
    window = CreateWindowW(windowClass.c_str(), L"VIOGPU ordinary Direct3D Present validation", WS_POPUP,
      64, 64, Width, Height, nullptr, nullptr, instance, nullptr);
    require(window != nullptr, "create-owned-present-window", HRESULT_FROM_WIN32(GetLastError()));
    require(SetWindowPos(window, HWND_TOPMOST, 64, 64, Width, Height, SWP_SHOWWINDOW), "visible-owned-topmost-window");
    ShowWindow(window, SW_SHOWNORMAL); UpdateWindow(window); readyWindow(window);
    windowObservation.handle = reinterpret_cast<UINT_PTR>(window);
    windowObservation.thread = GetWindowThreadProcessId(window, &windowObservation.pid);
    // Obtain the parent factory from the actual created device adapter.
    ComPtr<IDXGIFactory> deviceFactory; exact(actual->GetParent(IID_PPV_ARGS(&deviceFactory)), "actual-device-parent-factory");
    windowAssociation = deviceFactory->MakeWindowAssociation(window, WindowAssociationFlags);
    exact(windowAssociation, "disable-owned-window-alt-enter-and-mode-changes");
    DXGI_SWAP_CHAIN_DESC requestedSwapchain{};
    requestedSwapchain.BufferDesc.Width = Width; requestedSwapchain.BufferDesc.Height = Height;
    requestedSwapchain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; requestedSwapchain.SampleDesc.Count = 1;
    requestedSwapchain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; requestedSwapchain.BufferCount = 1;
    requestedSwapchain.OutputWindow = window; requestedSwapchain.Windowed = TRUE; requestedSwapchain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IUnknown* createdDevice = publicApi == 11 ? static_cast<IUnknown*>(device.Get()) : static_cast<IUnknown*>(device10.Get());
    exact(deviceFactory->CreateSwapChain(createdDevice, &requestedSwapchain, &swapchain), "ordinary-system-create-swapchain");
    exact(swapchain->GetDesc(&actualSwapchain), "actual-swapchain-descriptor"); swapchainDescriptorCaptured = true;
    require(actualSwapchain.BufferDesc.Width == Width && actualSwapchain.BufferDesc.Height == Height
      && actualSwapchain.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM && actualSwapchain.SampleDesc.Count == 1
      && !actualSwapchain.SampleDesc.Quality && actualSwapchain.BufferUsage == DXGI_USAGE_RENDER_TARGET_OUTPUT
      && actualSwapchain.BufferCount == 1 && actualSwapchain.OutputWindow == window && actualSwapchain.Windowed == TRUE
      && actualSwapchain.SwapEffect == DXGI_SWAP_EFFECT_DISCARD && !actualSwapchain.Flags, "exact-windowed-swapchain");
    ComPtr<IUnknown> createdIdentity, swapIdentity;
    exact(createdDevice->QueryInterface(IID_PPV_ARGS(&createdIdentity)), "created-com-identity");
    exact(swapchain->GetDevice(IID_PPV_ARGS(&swapIdentity)), "swapchain-com-identity");
    require(createdIdentity.Get() == swapIdentity.Get(), "same-created-device-swapchain");
    if (publicApi == 11 && requestedFeature == D3D_FEATURE_LEVEL_11_0)
      validateSm5(device.Get(), context.Get(), compiler, directory);
    if (publicApi == 11) render<Api11>(device.Get(), context.Get(), swapchain.Get(), window, compiler, directory);
    else render<Api10>(device10.Get(), device10.Get(), swapchain.Get(), window, compiler, directory);
    if (displayCycleRequested) {
      swapchain.Reset();
      displayCycle(device.Get(), context.Get(), deviceFactory.Get(), window, directory);
      require(displayCycleCompleted && displayCycleFrames == 4 && displayCycleRestore == S_OK,
        "display-cycle-completed-and-restored", displayCycleRestore);
    }
    rendererWorkers = dxvkWorkerCount();
    if (inlineWork) require(rendererWorkers == 0, "inline-renderer-has-no-workers");
    pixelsPassed = true;
    for (const auto& pair : {std::make_pair(front, L"frontend"), std::make_pair(core, L"core"),
        std::make_pair(privateLoader, L"privateLoader"), std::make_pair(icd, L"icd")})
      exactLoaded(pair.first, "exact-private-module-after-present", pair.second, false);
    require(!_wcsicmp(effectiveName(kmt).c_str(), front.c_str()), "effective-modern-name-stable");
    failureStage = ""; failureResult = S_OK;
  } catch (const Failure& error) { failureStage = error.stage; failureResult = error.result; }
    catch (...) { failureStage = "unexpected-exception"; failureResult = E_FAIL; }
  if (hold) {
    if (outputCreated) {
      try {
        const std::string checkpoint = "{\"schema\":1,\"api\":" + std::to_string(publicApi) + ",\"pid\":" + std::to_string(GetCurrentProcessId())
          + ",\"event\":" + json(argv[8]) + ",\"timeout_ms\":" + std::to_string(holdMs)
          + ",\"output\":" + json(directory) + ",\"factoryCalled\":" + std::string(factoryCalled ? "true" : "false")
          + ",\"factoryResult\":" + std::to_string(factoryResult) + ",\"pixelsPassed\":" + std::string(pixelsPassed ? "true" : "false")
          + ",\"stage\":" + jsonAscii(failureStage) + ",\"result\":" + std::to_string(failureResult)
          + ",\"deviceRemovedReason\":" + std::to_string(device ? device->GetDeviceRemovedReason() : E_NOINTERFACE)
          + ",\"displayCycleRestore\":" + std::to_string(displayCycleRestore) + "}\n";
        const size_t slash = directory.find_last_of(L'\\');
        const std::wstring parent = directory.substr(0, slash), name = directory.substr(slash + 1) + L".held.json";
        save(parent, name.c_str(), checkpoint.data(), DWORD(checkpoint.size()), true);
      } catch (const Failure& error) { failureStage = error.stage; failureResult = error.result; }
    }
    std::printf("SYSTEM_D3D11_HELD pid=%lu timeout_ms=%lu pixels_passed=%u stage=%s hr=%08lx\n",
      static_cast<unsigned long>(GetCurrentProcessId()), static_cast<unsigned long>(holdMs), pixelsPassed ? 1u : 0u,
      failureStage, static_cast<unsigned long>(failureResult)); std::fflush(stdout);
    heldReleased = WaitForSingleObject(hold, holdMs) == WAIT_OBJECT_0;
    if (!heldReleased) { failureStage = "hold-release-timeout"; failureResult = E_FAIL; }
    if (!CloseHandle(hold)) { failureStage = "hold-handle-close"; failureResult = HRESULT_FROM_WIN32(GetLastError()); }
  }
  // The owner inspects and releases the held device/swapchain/window lifetime.
  // Drop every device owner before capturing adapter retirement.
  if (displayCycleRequested) { std::fputs("DISPLAY_CYCLE_CLEANUP swapchain\n", stderr); std::fflush(stderr); }
  swapchain.Reset();
  if (displayCycleRequested) { std::fputs("DISPLAY_CYCLE_CLEANUP context\n", stderr); std::fflush(stderr); }
  context.Reset();
  if (displayCycleRequested) { std::fputs("DISPLAY_CYCLE_CLEANUP device\n", stderr); std::fflush(stderr); }
  device.Reset(); device10.Reset(); device10_1.Reset();
  if (displayCycleRequested) { std::fputs("DISPLAY_CYCLE_CLEANUP device-released\n", stderr); std::fflush(stderr); }
  if (window) {
    windowDestroyed = DestroyWindow(window) != FALSE;
    if (!windowDestroyed) { failureStage = "owned-window-destroy"; failureResult = HRESULT_FROM_WIN32(GetLastError()); }
    window = nullptr;
  }
  if (classAtom) {
    classUnregistered = UnregisterClassW(windowClass.c_str(), instance) != FALSE;
    if (!classUnregistered) { failureStage = "owned-window-class-unregister"; failureResult = HRESULT_FROM_WIN32(GetLastError()); }
    classAtom = 0;
  }
  if (kmt.handle) { closeResult = kmt.close(); if (closeResult != 0) { failureStage = "owned-kmt-adapter-close"; failureResult = HRESULT_FROM_NT(closeResult); } }
  const bool passed = pixelsPassed && presents == 2 && windowDestroyed && classUnregistered
    && heldReleased && factoryResult == S_OK && failureResult == S_OK && closeResult == 0
    && (!displayCycleRequested || (displayCycleCompleted && displayCycleFrames == 4 && displayCycleRestore == S_OK));
  if (outputCreated) {
    try {
      if (factoryCalled) saveText(directory, L"closed-negotiation.json", diagnostics(front, core, false) + '\n');
      std::string result = "{\"schema\":1,\"api\":" + std::to_string(publicApi) + ",\"passed\":" + std::string(passed ? "true" : "false")
        + ",\"icdSelection\":" + jsonAscii(discoverIcd ? "discovery" : "pinned")
        + ",\"threading\":" + jsonAscii(inlineWork ? "inline" : "default")
        + ",\"displayCycleRequested\":" + std::string(displayCycleRequested ? "true" : "false")
        + ",\"displayCyclePreserve\":" + std::string(displayCyclePreserve ? "true" : "false")
        + ",\"displayCycleCompleted\":" + std::string(displayCycleCompleted ? "true" : "false")
        + ",\"displayCycleRestore\":" + std::to_string(displayCycleRestore)
        + ",\"displayCyclePhases\":" + displayCyclePhases + ']'
        + ",\"creationFlags\":" + std::to_string(creationFlags) + ",\"rendererWorkers\":" + std::to_string(rendererWorkers)
        + ",\"failure\":" + jsonAscii(failureStage) + ",\"failureResult\":" + std::to_string(failureResult)
        + ",\"factoryCalled\":" + std::string(factoryCalled ? "true" : "false") + ",\"factoryResult\":" + std::to_string(factoryResult)
        + ",\"requestedFeatureLevel\":" + std::to_string(UINT(requestedFeature))
        + ",\"featureLevel\":" + std::to_string(UINT(obtained)) + ",\"luidHigh\":" + std::to_string(UINT(expected.HighPart))
        + ",\"sm5ComputeWords\":" + std::to_string(sm5ComputeWords)
        + ",\"sm5TessellationPixels\":" + std::to_string(sm5TessellationPixels)
        + ",\"sm5ComputeInvocations\":" + std::to_string(sm5ComputeStats.CSInvocations)
        + ",\"sm5HullInvocations\":" + std::to_string(sm5TessellationStats.HSInvocations)
        + ",\"sm5DomainInvocations\":" + std::to_string(sm5TessellationStats.DSInvocations)
        + ",\"luidLow\":" + std::to_string(expected.LowPart) + ",\"generation\":" + std::to_string(identity.generation)
        + ",\"capabilities\":" + std::to_string(identity.capabilities) + ",\"frontend\":" + json(front) + ",\"core\":" + json(core)
        + ",\"privateLoader\":" + json(privateLoader) + ",\"icd\":" + json(icd) + ",\"icdJson\":" + json(icdJson)
        + ",\"effective\":" + json(effective) + ",\"heldReleased\":" + std::string(heldReleased ? "true" : "false")
        + ",\"kmtClose\":" + std::to_string(closeResult) + ",\"pixels\":" + std::to_string(pixelsPassed ? 512 : 0)
        + ",\"visibleDesktopPixels\":" + std::to_string((readbacks[0].screenMatched ? 256 : 0) + (readbacks[1].screenMatched ? 256 : 0))
        + ",\"softwareFallback\":false,\"productionAdmission\":false,\"registrationChangedByProbe\":false,\"presents\":" + std::to_string(presents)
        + ",\"window\":{\"handle\":" + std::to_string(windowObservation.handle) + ",\"pid\":" + std::to_string(windowObservation.pid)
        + ",\"thread\":" + std::to_string(windowObservation.thread) + ",\"session\":" + std::to_string(windowObservation.session)
        + ",\"destroyed\":" + std::string(windowDestroyed ? "true" : "false") + ",\"classUnregistered\":" + std::string(classUnregistered ? "true" : "false")
        + ",\"associationResult\":" + std::to_string(windowAssociation) + ",\"associationFlags\":" + std::to_string(WindowAssociationFlags)
        + "},\"swapchain\":{\"captured\":" + std::string(swapchainDescriptorCaptured ? "true" : "false")
        + ",\"width\":" + std::to_string(actualSwapchain.BufferDesc.Width) + ",\"height\":" + std::to_string(actualSwapchain.BufferDesc.Height)
        + ",\"format\":" + std::to_string(UINT(actualSwapchain.BufferDesc.Format)) + ",\"sampleCount\":" + std::to_string(actualSwapchain.SampleDesc.Count)
        + ",\"sampleQuality\":" + std::to_string(actualSwapchain.SampleDesc.Quality) + ",\"usage\":" + std::to_string(actualSwapchain.BufferUsage)
        + ",\"bufferCount\":" + std::to_string(actualSwapchain.BufferCount) + ",\"outputWindow\":" + std::to_string(reinterpret_cast<UINT_PTR>(actualSwapchain.OutputWindow))
        + ",\"windowed\":" + std::string(actualSwapchain.Windowed ? "true" : "false") + ",\"effect\":" + std::to_string(UINT(actualSwapchain.SwapEffect))
        + ",\"flags\":" + std::to_string(actualSwapchain.Flags) + "},\"readbacks\":[";
      for (UINT i = 0; i < readbacks.size(); ++i) {
        if (i) result += ',';
        result += "{\"map\":" + std::to_string(readbacks[i].map) + ",\"removed\":" + std::to_string(readbacks[i].removed)
          + ",\"rowPitch\":" + std::to_string(readbacks[i].rowPitch) + ",\"present\":" + std::to_string(readbacks[i].present)
          + ",\"presentRemoved\":" + std::to_string(readbacks[i].presentRemoved) + ",\"syncInterval\":1,\"presentFlags\":0"
          + ",\"windowedBefore\":" + std::string(readbacks[i].windowedBefore ? "true" : "false")
          + ",\"screenAttempts\":" + std::to_string(readbacks[i].screenAttempts)
          + ",\"screenMatched\":" + std::string(readbacks[i].screenMatched ? "true" : "false")
          + ",\"windowedAfter\":" + std::string(readbacks[i].windowedAfter ? "true" : "false") + '}';
      }
      result += "],\"systemModules\":{";
      for (size_t i = 0; i < systemPaths.size(); ++i) { if (i) result += ','; result += json(systemPaths[i].first) + ':' + json(systemPaths[i].second); }
      result += "},\"loadedModules\":{";
      for (size_t i = 0; i < privatePaths.size(); ++i) { if (i) result += ','; result += json(privatePaths[i].first) + ':' + json(privatePaths[i].second); }
      saveText(directory, L"manifest.json", result + "}}\n");
    } catch (const Failure& error) { std::fprintf(stderr, "SYSTEM_D3D11_EVIDENCE_FAIL stage=%s hr=%08lx\n", error.stage, static_cast<unsigned long>(error.result)); return 1; }
  }
  if (!passed) { std::fprintf(stderr, "SYSTEM_D3D11_FAIL stage=%s hr=%08lx\n", failureStage, static_cast<unsigned long>(failureResult)); return 1; }
  std::printf("SYSTEM_D3D11_PRESENT_VALIDATION_PASS api=%u feature_level=%04x typed_ddi=11 pixels=512 visible_desktop_pixels=512 presents=2 software_fallback=0 production_admission=0 registry_changes=0\n", publicApi, UINT(obtained)); return 0;
}
