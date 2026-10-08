// SPDX-License-Identifier: MIT
// SYSTEM D3D11 FL10_0, exact selected hardware adapter; no software fallback.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#pragma warning(push)
#pragma warning(disable: 4201)
#include <d3d10umddi.h>
#pragma warning(pop)
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
#include <vector>
#include "umd-d3d11-system-entry.h"
#include "../src/umd/umd_runtime_identity.h"

namespace {
using Microsoft::WRL::ComPtr;
struct Failure { const char* stage; HRESULT result; };
void require(bool value, const char* stage, HRESULT result = E_FAIL) {
  if (!value) throw Failure{stage, result};
}
void exact(HRESULT result, const char* stage) { require(result == S_OK, stage, result); }
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
void exactLoaded(const std::wstring& expected, const char* stage, const WCHAR* role) {
  HMODULE module = GetModuleHandleW(basename(expected).c_str());
  require(module != nullptr, stage);
  const auto actual = loaded(module);
  require(!_wcsicmp(expected.c_str(), actual.c_str()), stage);
  privatePaths.emplace_back(role, actual);
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
  std::string text = "{\"schema\":1,\"core\":" + json(info->corePath) + ",\"liveAdapters\":" + std::to_string(info->liveAdapters) + ",\"events\":[";
  for (UINT i = 0; i < info->eventCount; ++i) {
    const auto& e = info->events[i]; require(e.completed && e.sequence == i + 1, "completed-ordered-negotiation");
    if (e.call == VioGpuD11Call::Create && e.result == S_OK && e.interfaceVersion == D3D11_0_DDI_INTERFACE_VERSION
        && D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(e.flags) == D3D11DDI_3DPIPELINELEVEL_10_0
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
  require(!required || created11, "runtime-created-typed11-fl10_0"); return text + "]}";
}
constexpr UINT Width = 16, Height = 16;
struct Readback { HRESULT map = E_FAIL, removed = E_FAIL; UINT rowPitch = 0; };
std::array<Readback, 2> readbacks{};
void render(ID3D11Device* device, ID3D11DeviceContext* context, HMODULE compiler, const std::wstring& directory) {
  D3D11_TEXTURE2D_DESC desc{}; desc.Width = Width; desc.Height = Height; desc.MipLevels = 1;
  desc.ArraySize = 1; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> target, staging;
  exact(device->CreateTexture2D(&desc, nullptr, &target), "public-create-offscreen-target");
  desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  exact(device->CreateTexture2D(&desc, nullptr, &staging), "public-create-staging");
  ComPtr<ID3D11RenderTargetView> view; exact(device->CreateRenderTargetView(target.Get(), nullptr, &view), "public-create-rtv");
  ID3D11RenderTargetView* rawView = view.Get(); context->OMSetRenderTargets(1, &rawView, nullptr);
  D3D11_VIEWPORT viewport{}; viewport.Width = Width; viewport.Height = Height; viewport.MaxDepth = 1;
  context->RSSetViewports(1, &viewport);
  D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
  ComPtr<ID3D11RasterizerState> state; exact(device->CreateRasterizerState(&raster, &state), "public-create-rasterizer");
  context->RSSetState(state.Get());
  const char hlsl[] = "float4 vs(uint i:SV_VertexID):SV_Position{float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[i],0,1);}float4 ps():SV_Target{return float4(1,0,0,1);}";
  const auto compile = symbol<decltype(&D3DCompile)>(compiler, "D3DCompile");
  ComPtr<ID3DBlob> vs, ps;
  exact(compile(hlsl, sizeof(hlsl)-1, nullptr, nullptr, nullptr, "vs", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, nullptr), "system-fxc-vs");
  exact(compile(hlsl, sizeof(hlsl)-1, nullptr, nullptr, nullptr, "ps", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, nullptr), "system-fxc-ps");
  save(directory, L"vs.dxbc", vs->GetBufferPointer(), DWORD(vs->GetBufferSize()));
  save(directory, L"ps.dxbc", ps->GetBufferPointer(), DWORD(ps->GetBufferSize()));
  ComPtr<ID3D11VertexShader> vertex; ComPtr<ID3D11PixelShader> pixel;
  exact(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex), "public-create-vs");
  exact(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel), "public-create-ps");
  context->VSSetShader(vertex.Get(), nullptr, 0); context->PSSetShader(pixel.Get(), nullptr, 0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const float black[4]{0, 0, 0, 1}; context->ClearRenderTargetView(view.Get(), black);
  for (UINT frame = 0; frame < 2; ++frame) {
    if (frame) context->Draw(3, 0);
    context->CopyResource(staging.Get(), target.Get()); context->Flush();
    D3D11_MAPPED_SUBRESOURCE mapped{}; readbacks[frame].map = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    exact(readbacks[frame].map, "blocking-public-map");
    readbacks[frame].rowPitch = mapped.RowPitch;
    const UINT64 span = UINT64(Height - 1) * mapped.RowPitch + Width * 4;
    if (!mapped.pData || mapped.RowPitch < Width*4 || span > UINTPTR_MAX
        || reinterpret_cast<UINT_PTR>(mapped.pData) > UINTPTR_MAX - (span - 1)) {
      context->Unmap(staging.Get(), 0); throw Failure{"bounded-readback-pitch", E_FAIL};
    }
    std::array<unsigned char, Width*Height*4> original{};
    for (UINT y = 0; y < Height; ++y) std::memcpy(original.data() + y*Width*4,
      static_cast<const unsigned char*>(mapped.pData) + size_t(y)*mapped.RowPitch, Width*4);
    context->Unmap(staging.Get(), 0);
    save(directory, frame ? L"draw.raw" : L"clear.raw", original.data(), DWORD(original.size()));
    for (UINT p = 0; p < Width*Height; ++p) require(original[p*4] == (frame ? 255 : 0)
      && original[p*4+1] == 0 && original[p*4+2] == 0 && original[p*4+3] == 255, "literal-512-pixel-oracle");
    readbacks[frame].removed = device->GetDeviceRemovedReason(); exact(readbacks[frame].removed, "actual-device-removal-readback");
  }
  context->ClearState(); context->Flush();
}
void negative(const WCHAR* input) {
  const auto expected = absolute(input);
  HMODULE module = LoadLibraryExW(expected.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  require(module && !_wcsicmp(loaded(module).c_str(), expected.c_str()), "negative-exact-front");
  const auto entry = symbol<PFND3D10DDI_OPENADAPTER>(module, "OpenAdapter10_2");
  UINT checks = 0;
  const auto check = [&](bool value, const char* stage) { ++checks; require(value, stage); };
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
  if (argc != 10) {
    std::fputs("usage: probe <high:low-LUID> <absolute-front> <absolute-core> <absolute-private-loader> <absolute-ICD-DLL> <absolute-owned-ICD-JSON> <fresh-output-dir> <Local-hold-event> <hold-ms>\n", stderr); return 2;
  }
  ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
  KmtAdapter kmt; HANDLE hold = nullptr;
  std::wstring front, core, directory, privateLoader, icd, icdJson, effective;
  bool outputCreated = false, factoryCalled = false, pixelsPassed = false, heldReleased = false;
  HRESULT factoryResult = E_FAIL, failureResult = E_FAIL; const char* failureStage = "unstarted";
  LUID expected{}; dxvk::umd::RuntimeIdentity identity{}; D3D_FEATURE_LEVEL obtained{};
  DWORD holdMs = 0; NTSTATUS closeResult = NTSTATUS(-1073741823L);
  try {
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
    exactEnvironment(L"VK_DRIVER_FILES", icdJson); exactEnvironment(L"VK_ICD_FILENAMES", icdJson);
    HMODULE dxgi = systemModule(L"dxgi.dll"), runtime = systemModule(L"d3d11.dll");
    HMODULE gdi = systemModule(L"gdi32.dll"), compiler = systemModule(L"d3dcompiler_47.dll");
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
    const D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_10_0;
    factoryCalled = true;
    factoryResult = symbol<decltype(&D3D11CreateDevice)>(runtime, "D3D11CreateDevice")(selected.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
      D3D11_CREATE_DEVICE_SINGLETHREADED, &requested, 1, D3D11_SDK_VERSION, &device, &obtained, &context);
    // Save the actual negotiation even if the SYSTEM factory failed.
    saveText(directory, L"negotiation.json", diagnostics(front, core, false) + '\n');
    exact(factoryResult, "ordinary-system-d3d11-create-device");
    require(obtained == requested && device->GetFeatureLevel() == requested && device->GetCreationFlags() == D3D11_CREATE_DEVICE_SINGLETHREADED, "exact-logical-fl10_0-singlethreaded");
    diagnostics(front, core, true); exactLoaded(privateLoader, "actual-private-vulkan-loader", L"privateLoader");
    exactLoaded(icd, "actual-private-mesa-icd", L"icd");
    require(!GetModuleHandleW(L"warp.dll") && !GetModuleHandleW(L"d3d10warp.dll") && !GetModuleHandleW(L"winevulkan.dll"), "no-software-runtime-modules");
    ComPtr<IDXGIDevice> dxgiDevice; exact(device.As(&dxgiDevice), "device-dxgi-interface");
    ComPtr<IDXGIAdapter> actual; exact(dxgiDevice->GetAdapter(&actual), "created-device-adapter");
    DXGI_ADAPTER_DESC actualDesc{}; exact(actual->GetDesc(&actualDesc), "created-device-description");
    require(sameLuid(actualDesc.AdapterLuid, expected) && actualDesc.VendorId == 0x1af4 && actualDesc.DeviceId == 0x1050, "created-device-exact-selected-luid");
    render(device.Get(), context.Get(), compiler, directory); pixelsPassed = true;
    require(!_wcsicmp(effectiveName(kmt).c_str(), front.c_str()), "effective-modern-name-stable");
    failureStage = ""; failureResult = S_OK;
  } catch (const Failure& error) { failureStage = error.stage; failureResult = error.result; }
    catch (...) { failureStage = "unexpected-exception"; failureResult = E_FAIL; }
  if (hold) {
    if (outputCreated) {
      try {
        const std::string checkpoint = "{\"schema\":1,\"api\":11,\"pid\":" + std::to_string(GetCurrentProcessId())
          + ",\"event\":" + json(argv[8]) + ",\"timeout_ms\":" + std::to_string(holdMs)
          + ",\"output\":" + json(directory) + ",\"factoryCalled\":" + std::string(factoryCalled ? "true" : "false")
          + ",\"factoryResult\":" + std::to_string(factoryResult) + ",\"pixelsPassed\":" + std::string(pixelsPassed ? "true" : "false")
          + ",\"stage\":" + jsonAscii(failureStage) + ",\"result\":" + std::to_string(failureResult) + "}\n";
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
  context.Reset(); device.Reset();
  if (kmt.handle) { closeResult = kmt.close(); if (closeResult != 0) { failureStage = "owned-kmt-adapter-close"; failureResult = HRESULT_FROM_NT(closeResult); } }
  const bool passed = pixelsPassed && heldReleased && factoryResult == S_OK && failureResult == S_OK && closeResult == 0;
  if (outputCreated) {
    try {
      if (factoryCalled) saveText(directory, L"closed-negotiation.json", diagnostics(front, core, false) + '\n');
      std::string result = "{\"schema\":1,\"api\":11,\"passed\":" + std::string(passed ? "true" : "false")
        + ",\"failure\":" + jsonAscii(failureStage) + ",\"failureResult\":" + std::to_string(failureResult)
        + ",\"factoryCalled\":" + std::string(factoryCalled ? "true" : "false") + ",\"factoryResult\":" + std::to_string(factoryResult)
        + ",\"featureLevel\":" + std::to_string(UINT(obtained)) + ",\"luidHigh\":" + std::to_string(UINT(expected.HighPart))
        + ",\"luidLow\":" + std::to_string(expected.LowPart) + ",\"generation\":" + std::to_string(identity.generation)
        + ",\"capabilities\":" + std::to_string(identity.capabilities) + ",\"frontend\":" + json(front) + ",\"core\":" + json(core)
        + ",\"privateLoader\":" + json(privateLoader) + ",\"icd\":" + json(icd) + ",\"icdJson\":" + json(icdJson)
        + ",\"effective\":" + json(effective) + ",\"heldReleased\":" + std::string(heldReleased ? "true" : "false")
        + ",\"kmtClose\":" + std::to_string(closeResult) + ",\"pixels\":" + std::to_string(pixelsPassed ? 512 : 0)
        + ",\"softwareFallback\":false,\"productionAdmission\":false,\"registrationChangedByProbe\":false,\"presents\":0,\"readbacks\":[";
      for (UINT i = 0; i < readbacks.size(); ++i) {
        if (i) result += ',';
        result += "{\"map\":" + std::to_string(readbacks[i].map) + ",\"removed\":" + std::to_string(readbacks[i].removed)
          + ",\"rowPitch\":" + std::to_string(readbacks[i].rowPitch) + '}';
      }
      result += "],\"systemModules\":{";
      for (size_t i = 0; i < systemPaths.size(); ++i) { if (i) result += ','; result += json(systemPaths[i].first) + ':' + json(systemPaths[i].second); }
      result += "},\"loadedModules\":{";
      for (size_t i = 0; i < privatePaths.size(); ++i) { if (i) result += ','; result += json(privatePaths[i].first) + ':' + json(privatePaths[i].second); }
      saveText(directory, L"manifest.json", result + "}}\n");
    } catch (const Failure& error) { std::fprintf(stderr, "SYSTEM_D3D11_EVIDENCE_FAIL stage=%s hr=%08lx\n", error.stage, static_cast<unsigned long>(error.result)); return 1; }
  }
  if (!passed) { std::fprintf(stderr, "SYSTEM_D3D11_FAIL stage=%s hr=%08lx\n", failureStage, static_cast<unsigned long>(failureResult)); return 1; }
  std::puts("SYSTEM_D3D11_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=0 software_fallback=0 production_admission=0 registry_changes=0"); return 0;
}
