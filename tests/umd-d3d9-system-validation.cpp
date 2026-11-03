// SPDX-License-Identifier: MIT
// Ordinary Microsoft SYSTEM D3D8 or D3D9/9Ex. The owner manages persistent binding.
// Build with DXVK_SYSTEM_D3D8 for the genuine I386 D3D8 runtime; no API delegation.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201) // Anonymous SDK/WDK structures; source warnings remain strict.
#endif
#if defined(DXVK_SYSTEM_D3D8)
#include "umd-d3d8-api.h"
#define LEGACY_TAG "D8_SYSTEM"
#define LEGACY_SCHEMA "ordinary-system-d3d8"
using LegacyApi = IDirect3D8;
using LegacyNormal = IDirect3D8;
using LegacyDevice = IDirect3DDevice8;
using LegacyExtended = IDirect3DDevice8;
using LegacySurface = IDirect3DSurface8;
using LegacyIdentifier = D3DADAPTER_IDENTIFIER8;
using LegacyViewport = D3DVIEWPORT8;
#else
#include <d3d9.h>
#define LEGACY_TAG "D9_SYSTEM"
#define LEGACY_SCHEMA "ordinary-system-d3d9"
using LegacyApi = IDirect3D9Ex;
using LegacyNormal = IDirect3D9;
using LegacyDevice = IDirect3DDevice9;
using LegacyExtended = IDirect3DDevice9Ex;
using LegacySurface = IDirect3DSurface9;
using LegacyIdentifier = D3DADAPTER_IDENTIFIER9;
using LegacyViewport = D3DVIEWPORT9;
#endif
#include <d3dkmthk.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include <wrl/client.h>
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include "../src/umd/umd_runtime_identity.h"

namespace {
using Microsoft::WRL::ComPtr;
struct Failure { const char* stage; HRESULT hr; };
void require(bool value, const char* stage, HRESULT hr = E_FAIL) {
  if (!value) throw Failure{stage, hr};
}
void exact(HRESULT hr, const char* stage) {
  std::printf(LEGACY_TAG "_STEP stage=%s hr=%08lx\n", stage, static_cast<unsigned long>(hr));
  require(hr == S_OK, stage, hr);
}
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
std::wstring leaf(const std::wstring& path) { return path.substr(path.find_last_of(L"\\/") + 1); }
template<typename Function> Function symbol(HMODULE module, const char* name) {
  const FARPROC address = GetProcAddress(module, name);
  require(address != nullptr, name, HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND));
  Function result;
  static_assert(sizeof(result) == sizeof(address));
  std::memcpy(&result, &address, sizeof(result));
  return result;
}
std::string jsonString(const std::wstring& text) {
  std::string output = "\"";
  for (WCHAR value : text) {
    if (value >= 32 && value < 127 && value != L'\\' && value != L'\"') output += char(value);
    else { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", unsigned(value)); output += escaped; }
  }
  return output + '"';
}
struct SystemModules {
  std::vector<HMODULE> owned;
  ~SystemModules() { for (auto i = owned.rbegin(); i != owned.rend(); ++i) FreeLibrary(*i); }
  HMODULE load(const WCHAR* name) {
    std::vector<WCHAR> directory(32768);
    const UINT count = GetSystemDirectoryW(directory.data(), UINT(directory.size()));
    require(count && count < directory.size(), "system-directory");
    const std::wstring expected = std::wstring(directory.data(), count) + L"\\" + name;
    const HMODULE module = LoadLibraryExW(expected.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(module != nullptr, "load-system-module", HRESULT_FROM_WIN32(GetLastError()));
    owned.push_back(module);
    require(!_wcsicmp(loaded(module).c_str(), expected.c_str()), "exact-system-module");
    std::printf(LEGACY_TAG "_MODULE name=%ls path=%ls\n", name, expected.c_str());
    return module;
  }
};
void save(const std::wstring& directory, const WCHAR* name, const void* data, DWORD size) {
  const auto path = directory + L"\\" + name;
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file != INVALID_HANDLE_VALUE, "fresh-original-file", HRESULT_FROM_WIN32(GetLastError()));
  DWORD written = 0;
  const BOOL ok = WriteFile(file, data, size, &written, nullptr);
  const BOOL flushed = ok && written == size ? FlushFileBuffers(file) : FALSE;
  const DWORD error = ok && flushed ? ERROR_SUCCESS : GetLastError();
  const BOOL closed = CloseHandle(file);
  require(ok && written == size && flushed && closed, "write-flush-close-original", HRESULT_FROM_WIN32(error));
}
LUID parseLuid(const WCHAR* input) {
  require(input && std::wcslen(input) == 17 && input[8] == L':', "exact-luid");
  UINT high = 0, low = 0;
  for (UINT index = 0; index < 17; ++index) {
    if (index == 8) continue;
    const WCHAR token = input[index];
    const UINT digit = token >= L'0' && token <= L'9' ? UINT(token-L'0')
      : token >= L'a' && token <= L'f' ? UINT(token-L'a'+10)
      : token >= L'A' && token <= L'F' ? UINT(token-L'A'+10) : 16;
    require(digit < 16, "luid-hex");
    UINT& word = index < 8 ? high : low; word = (word << 4) | digit;
  }
  require(high || low, "nonzero-luid");
  return {low, LONG(high)};
}
bool equalLuid(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
struct Kmt {
  D3DKMT_HANDLE handle = 0;
  decltype(&D3DKMTCloseAdapter) close = nullptr;
  decltype(&D3DKMTQueryAdapterInfo) query = nullptr;
  ~Kmt() { if (handle && close) { D3DKMT_CLOSEADAPTER args{}; args.hAdapter = handle; close(&args); } }
  void info(KMTQUERYADAPTERINFOTYPE type, void* data, UINT size) {
    D3DKMT_QUERYADAPTERINFO args{}; args.hAdapter = handle; args.Type = type;
    args.pPrivateDriverData = data; args.PrivateDriverDataSize = size;
    const NTSTATUS status = query(&args);
    require(status == 0, "actual-kmt-query", HRESULT_FROM_NT(status));
  }
  std::wstring name() {
    D3DKMT_UMDFILENAMEINFO reply{}; reply.Version = KMTUMDVERSION_DX9;
    info(KMTQAITYPE_UMDRIVERNAME, &reply, sizeof(reply));
    UINT size = 0; while (size < MAX_PATH && reply.UmdFileName[size]) ++size;
    require(size && size < MAX_PATH, "bounded-kmt-native-name");
    return absolute(reply.UmdFileName);
  }
};
void verifyModules(const std::array<std::wstring, 4>& paths) {
  for (size_t role = 0; role < paths.size(); ++role) {
    const HMODULE module = GetModuleHandleW(leaf(paths[role]).c_str());
    require(module && !_wcsicmp(loaded(module).c_str(), paths[role].c_str()), "factory-loaded-exact-candidate-tuple");
    std::printf(LEGACY_TAG "_CANDIDATE_MODULE role=%zu path=%ls\n", role, paths[role].c_str());
  }
}
constexpr UINT Width = 16, Height = 16;
using Pixels = std::array<UINT, Width*Height>;
struct Region { UINT left, top, right, bottom, color; };
constexpr Region ClearRegions[]{{1,2,6,7,0xff9a4c23u},{10,8,15,13,0xff256ebau}};
constexpr Region DrawRegions[]{{2,1,9,6,0xffbd672du},{11,9,15,15,0xff36a4d1u},{0,13,3,16,0xff85c239u}};
Pixels expected(UINT frame) {
  Pixels result{}; result.fill(frame ? 0xff421a75u : 0xff173b61u);
  const auto* regions = frame ? DrawRegions : ClearRegions;
  const UINT count = frame ? UINT(std::size(DrawRegions)) : UINT(std::size(ClearRegions));
  for (UINT r = 0; r < count; ++r)
    for (UINT y = regions[r].top; y < regions[r].bottom; ++y)
      for (UINT x = regions[r].left; x < regions[r].right; ++x) result[y*Width+x] = regions[r].color;
  return result;
}
struct Vertex { float x, y, z, rhw; UINT color; };
struct Frame { UINT pitch = 0, screenAttempts = 0; HRESULT cooperative = E_FAIL, present = E_FAIL; };
std::array<Frame, 2> frameResults{};
void configure(LegacyDevice* device) {
#if defined(DXVK_SYSTEM_D3D8)
  exact(device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "fixed-function-fvf");
  exact(device->SetPixelShader(0), "fixed-function-ps");
  exact(device->SetTexture(0, nullptr), "fixed-function-untextured");
#else
  exact(device->SetVertexShader(nullptr), "fixed-function-vs");
  exact(device->SetPixelShader(nullptr), "fixed-function-ps");
  exact(device->SetTexture(0, nullptr), "fixed-function-untextured");
  exact(device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE), "fixed-function-fvf");
#endif
  const std::pair<D3DRENDERSTATETYPE, DWORD> states[]{
    {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},
    {D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},
    {D3DRS_DITHERENABLE,FALSE},
#if !defined(DXVK_SYSTEM_D3D8)
    {D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_SRGBWRITEENABLE,FALSE},
#endif
    {D3DRS_COLORWRITEENABLE,15},{D3DRS_MULTISAMPLEMASK,0xffffffffu}};
  for (const auto& state : states) exact(device->SetRenderState(state.first, state.second), "fixed-function-render-state");
  exact(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1), "fixed-function-color-op");
  exact(device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE), "fixed-function-color-arg");
  exact(device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1), "fixed-function-alpha-op");
  exact(device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE), "fixed-function-alpha-arg");
  exact(device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE), "fixed-function-next-stage");
  const LegacyViewport viewport{0,0,Width,Height,0.0f,1.0f};
  exact(device->SetViewport(&viewport), "exact-viewport");
}
void pump() {
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
}
void screen(HWND window, UINT frame, const std::wstring& directory) {
  POINT origin{}; require(ClientToScreen(window, &origin) != FALSE, "actual-client-screen-origin");
  Pixels pixels{};
  const auto wanted = expected(frame);
  const ULONGLONG deadline = GetTickCount64() + 2000;
  bool matched = false;
  do {
    pump(); Sleep(25);
    const HDC dc = GetDC(nullptr); require(dc != nullptr, "desktop-screen-dc");
    bool valid = true;
    for (UINT y = 0; y < Height; ++y) for (UINT x = 0; x < Width; ++x) {
      const COLORREF pixel = GetPixel(dc, origin.x + int(x), origin.y + int(y));
      pixels[y*Width+x] = pixel; valid = valid && pixel != CLR_INVALID;
    }
    require(ReleaseDC(nullptr, dc) == 1 && valid, "desktop-screen-read-release");
    ++frameResults[frame].screenAttempts;
    matched = true;
    for (size_t p = 0; p < pixels.size(); ++p) {
      const UINT argb = wanted[p];
      const UINT rgb = ((argb >> 16) & 255u) | (argb & 0xff00u) | ((argb & 255u) << 16);
      matched = matched && pixels[p] == rgb;
    }
  } while (!matched && GetTickCount64() < deadline);
  save(directory, frame ? L"draw-screen.raw" : L"clear-screen.raw", pixels.data(), DWORD(sizeof(pixels)));
  require(matched, "literal-visible-screen-rgb");
}
void render(LegacyDevice* device, LegacyExtended* extended, HWND window,
    bool present, const std::wstring& directory) {
  ComPtr<LegacySurface> target, staging;
#if defined(DXVK_SYSTEM_D3D8)
  exact(device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &target), "public-backbuffer");
#else
  exact(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &target), "public-backbuffer");
#endif
  D3DSURFACE_DESC desc{}; exact(target->GetDesc(&desc), "actual-backbuffer-description");
  require(desc.Width == Width && desc.Height == Height && desc.Format == D3DFMT_A8R8G8B8
    && desc.MultiSampleType == D3DMULTISAMPLE_NONE
#if !defined(DXVK_SYSTEM_D3D8)
    && desc.MultiSampleQuality == 0
#endif
    && desc.Pool == D3DPOOL_DEFAULT,
    "exact-backbuffer-geometry-format");
#if defined(DXVK_SYSTEM_D3D8)
  exact(device->CreateImageSurface(Width, Height, desc.Format, &staging), "public-systemmem-readback");
  exact(device->SetRenderTarget(target.Get(), nullptr), "public-render-target");
#else
  exact(device->CreateOffscreenPlainSurface(Width, Height, desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr), "public-systemmem-readback");
  exact(device->SetRenderTarget(0, target.Get()), "public-render-target");
#endif
  configure(device);
  for (UINT frame = 0; frame < 2; ++frame) {
    std::printf(LEGACY_TAG "_FRAME_BEGIN frame=%u\n", frame);
    exact(device->Clear(0, nullptr, D3DCLEAR_TARGET, frame ? 0xff421a75u : 0xff173b61u, 1.0f, 0), "public-clear-base");
    if (!frame) {
      for (const auto& region : ClearRegions) {
        const D3DRECT box{LONG(region.left),LONG(region.top),LONG(region.right),LONG(region.bottom)};
        exact(device->Clear(1, &box, D3DCLEAR_TARGET, region.color, 1.0f, 0), "public-clear-asymmetric-rectangle");
      }
    } else {
      exact(device->BeginScene(), "public-begin-scene");
      for (const auto& region : DrawRegions) {
        const float l = float(region.left)-0.5f, r = float(region.right)-0.5f;
        const float t = float(region.top)-0.5f, b = float(region.bottom)-0.5f;
        const Vertex vertices[]{{l,t,0,1,region.color},{r,t,0,1,region.color},{l,b,0,1,region.color},{r,b,0,1,region.color}};
        exact(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)), "public-fvf-draw");
      }
      exact(device->EndScene(), "public-end-scene");
    }
#if defined(DXVK_SYSTEM_D3D8)
    exact(device->CopyRects(target.Get(), nullptr, 0, staging.Get(), nullptr), "public-blocking-readback-copy");
#else
    exact(device->GetRenderTargetData(target.Get(), staging.Get()), "public-blocking-readback-copy");
#endif
    D3DLOCKED_RECT mapped{}; exact(staging->LockRect(&mapped, nullptr, D3DLOCK_READONLY), "public-readback-lock");
    const uint64_t span = mapped.Pitch >= LONG(Width*4) ? uint64_t(Height-1)*UINT(mapped.Pitch)+Width*4 : 0;
    const bool valid = mapped.pBits && span && span <= uint64_t(UINTPTR_MAX)-reinterpret_cast<uintptr_t>(mapped.pBits)+1;
    Pixels pixels{};
    if (valid) for (UINT y = 0; y < Height; ++y)
      std::memcpy(pixels.data()+y*Width, static_cast<const BYTE*>(mapped.pBits)+size_t(y)*UINT(mapped.Pitch), Width*4);
    frameResults[frame].pitch = mapped.Pitch > 0 ? UINT(mapped.Pitch) : 0;
    exact(staging->UnlockRect(), "public-readback-unlock");
    save(directory, frame ? L"draw.raw" : L"clear.raw", pixels.data(), DWORD(sizeof(pixels)));
    require(valid && pixels == expected(frame), "literal-clear-fvf-pixels");
    frameResults[frame].cooperative = device->TestCooperativeLevel();
    exact(frameResults[frame].cooperative, "actual-device-cooperative-state");
    if (present) {
#if defined(DXVK_SYSTEM_D3D8)
      frameResults[frame].present = device->Present(nullptr, nullptr, nullptr, nullptr);
#else
      frameResults[frame].present = extended ? extended->PresentEx(nullptr, nullptr, nullptr, nullptr, 0)
        : device->Present(nullptr, nullptr, nullptr, nullptr);
#endif
      exact(frameResults[frame].present, "ordinary-system-present");
      screen(window, frame, directory);
    }
    std::printf(LEGACY_TAG "_FRAME_DONE frame=%u pixels=256 presents=%u\n", frame, present ? 1u : 0u);
  }
}
}

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc != 11) {
    std::fputs("usage: probe <9|9ex> <offscreen|present> <high:low-LUID> <front> <core> <private-loader> <ICD> <fresh-output> <Local-hold-event> <hold-ms<=60000>\n", stderr);
    return 2;
  }
  SystemModules modules;
  ComPtr<LegacyNormal> normal;
  ComPtr<LegacyApi> api;
  ComPtr<LegacyDevice> device;
  ComPtr<LegacyExtended> extended;
  HWND window = nullptr;
  HANDLE hold = nullptr;
  DWORD holdMs = 0;
  std::wstring directory, holdEvent;
  int result = 1;
  bool outputCreated = false;
  try {
    require(SetProcessDPIAware() != FALSE, "physical-pixel-dpi-context");
#if defined(DXVK_SYSTEM_D3D8)
    static_assert(sizeof(void*) == 4, "SYSTEM D3D8 requires I386");
    const bool ex = false, present = !std::wcscmp(argv[2], L"present");
    require(!std::wcscmp(argv[1], L"8"), "explicit-api");
#else
    const bool ex = !std::wcscmp(argv[1], L"9ex"), present = !std::wcscmp(argv[2], L"present");
    require(ex || !std::wcscmp(argv[1], L"9"), "explicit-api");
#endif
    require(present || !std::wcscmp(argv[2], L"offscreen"), "explicit-phase");
    const LUID luid = parseLuid(argv[3]);
    const std::array<std::wstring, 4> tuple{absolute(argv[4]),absolute(argv[5]),absolute(argv[6]),absolute(argv[7])};
    directory = absolute(argv[8]);
    require(std::wcslen(argv[9]) > 6 && !std::wcsncmp(argv[9], L"Local\\", 6), "explicit-local-hold-event");
    WCHAR* end = nullptr; const unsigned long timeout = std::wcstoul(argv[10], &end, 10);
    require(end && !*end && timeout && timeout <= 60000, "bounded-hold-time"); holdMs = DWORD(timeout);
    DWORD session = 0; require(ProcessIdToSessionId(GetCurrentProcessId(), &session) && session != 0, "interactive-user-session");
    HANDLE token = nullptr; require(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != FALSE, "process-token");
    TOKEN_ELEVATION elevation{}; DWORD tokenBytes = 0;
    const BOOL tokenOk = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &tokenBytes);
    const BOOL tokenClosed = CloseHandle(token);
    require(tokenOk && tokenClosed && tokenBytes == sizeof(elevation) && !elevation.TokenIsElevated, "limited-user-token");
    holdEvent = argv[9];
    hold = CreateEventW(nullptr, TRUE, FALSE, holdEvent.c_str()); const DWORD eventError = GetLastError();
    if (!hold || eventError == ERROR_ALREADY_EXISTS) {
      if (hold) CloseHandle(hold); hold = nullptr;
      throw Failure{"fresh-hold-event", HRESULT_FROM_WIN32(eventError ? eventError : ERROR_INVALID_HANDLE)};
    }
    require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "fresh-output-directory", HRESULT_FROM_WIN32(GetLastError())); outputCreated = true;
    for (const auto& path : tuple) require(!GetModuleHandleW(leaf(path).c_str()), "candidate-tuple-not-preloaded");
#if defined(DXVK_SYSTEM_D3D8)
    const HMODULE runtime = modules.load(L"d3d8.dll"), gdi = modules.load(L"gdi32.dll");
#else
    const HMODULE runtime = modules.load(L"d3d9.dll"), gdi = modules.load(L"gdi32.dll");
#endif
    Kmt kmt; kmt.query = symbol<decltype(&D3DKMTQueryAdapterInfo)>(gdi, "D3DKMTQueryAdapterInfo");
    kmt.close = symbol<decltype(&D3DKMTCloseAdapter)>(gdi, "D3DKMTCloseAdapter");
    D3DKMT_OPENADAPTERFROMLUID opened{}; opened.AdapterLuid = luid;
    const NTSTATUS status = symbol<decltype(&D3DKMTOpenAdapterFromLuid)>(gdi, "D3DKMTOpenAdapterFromLuid")(&opened);
    kmt.handle = opened.hAdapter; require(!status && kmt.handle, "actual-kmt-open", HRESULT_FROM_NT(status));
    D3DKMT_ADAPTERTYPE type{}; kmt.info(KMTQAITYPE_ADAPTERTYPE, &type, sizeof(type));
    require(type.RenderSupported && !type.SoftwareDevice, "actual-hardware-render-adapter");
    std::array<unsigned char, dxvk::umd::RuntimeIdentityReplySize> identityBytes{};
    kmt.info(KMTQAITYPE_UMDRIVERPRIVATE, identityBytes.data(), UINT(identityBytes.size()));
    dxvk::umd::RuntimeIdentity identity;
    require(dxvk::umd::readRuntimeIdentity(identityBytes.data(), identityBytes.size(), identity)
      && !std::memcmp(identity.luid.data(), &luid, sizeof(luid)), "actual-runtime-luid-generation");
    const auto selected = kmt.name();
    std::printf(LEGACY_TAG "_SELECTED api=%ls phase=%ls luid=%08x:%08x generation=%llu effective_umd=%ls\n",
      argv[1], argv[2], UINT(luid.HighPart), UINT(luid.LowPart), static_cast<unsigned long long>(identity.generation), selected.c_str());
    require(!_wcsicmp(selected.c_str(), tuple[0].c_str()), "actual-native-kmt-candidate-name");
#if defined(DXVK_SYSTEM_D3D8)
    normal.Attach(symbol<decltype(&Direct3DCreate8)>(runtime, "Direct3DCreate8")(D3D_SDK_VERSION));
    require(normal.Get() != nullptr, "ordinary-system-factory-8");
    UINT ordinal = UINT_MAX;
    const UINT count = normal->GetAdapterCount(); require(count && count <= 16, "bounded-public-adapters");
    for (UINT i = 0; i < count; ++i) {
      const HMONITOR monitor = normal->GetAdapterMonitor(i);
      MONITORINFOEXW info{}; info.cbSize = sizeof(info);
      require(monitor && GetMonitorInfoW(monitor, &info), "public-adapter-monitor");
      HDC dc = CreateDCW(L"DISPLAY", info.szDevice, nullptr, nullptr); require(dc != nullptr, "adapter-monitor-dc");
      D3DKMT_OPENADAPTERFROMHDC current{}; current.hDc = dc;
      const NTSTATUS openedMonitor = symbol<decltype(&D3DKMTOpenAdapterFromHdc)>(gdi, "D3DKMTOpenAdapterFromHdc")(&current);
      const BOOL releasedDc = DeleteDC(dc);
      require(!openedMonitor && current.hAdapter && releasedDc, "public-monitor-kmt-identity");
      D3DKMT_CLOSEADAPTER closed{}; closed.hAdapter = current.hAdapter;
      require(!kmt.close(&closed), "public-monitor-kmt-close");
      if (equalLuid(luid, current.AdapterLuid)) { require(ordinal == UINT_MAX, "unique-public-selected-luid"); ordinal = i; }
    }
    require(ordinal != UINT_MAX, "public-selected-luid-present");
    LegacyNormal* factory = normal.Get();
#else
    exact(symbol<decltype(&Direct3DCreate9Ex)>(runtime, "Direct3DCreate9Ex")(D3D_SDK_VERSION, &api), "ordinary-system-factory-9ex");
    UINT ordinal = UINT_MAX;
    const UINT count = api->GetAdapterCount(); require(count && count <= 16, "bounded-public-adapters");
    for (UINT i = 0; i < count; ++i) {
      LUID current{}; exact(api->GetAdapterLUID(i, &current), "public-adapter-luid");
      if (equalLuid(luid, current)) { require(ordinal == UINT_MAX, "unique-public-selected-luid"); ordinal = i; }
    }
    require(ordinal != UINT_MAX, "public-selected-luid-present");
    LegacyNormal* factory = api.Get();
#endif
    LegacyIdentifier adapter{}; exact(factory->GetAdapterIdentifier(ordinal, 0, &adapter), "public-adapter-identifier");
    require(adapter.VendorId == 0x1af4 && adapter.DeviceId == 0x1050, "actual-viogpu-public-identity");
    std::printf(LEGACY_TAG "_IDENTITY ordinal=%u vendor=%08x device=%08x driver=%.*s description=%.*s\n",
      ordinal, UINT(adapter.VendorId), UINT(adapter.DeviceId), int(sizeof(adapter.Driver)), adapter.Driver, int(sizeof(adapter.Description)), adapter.Description);
#if !defined(DXVK_SYSTEM_D3D8)
    if (!ex) {
      normal.Attach(symbol<decltype(&Direct3DCreate9)>(runtime, "Direct3DCreate9")(D3D_SDK_VERSION)); require(normal.Get() != nullptr, "ordinary-system-factory-9");
      LegacyIdentifier same{}; exact(normal->GetAdapterIdentifier(ordinal, 0, &same), "normal-public-adapter-identity");
      require(normal->GetAdapterCount() == count && same.VendorId == adapter.VendorId && same.DeviceId == adapter.DeviceId
        && !std::memcmp(&same.DeviceIdentifier, &adapter.DeviceIdentifier, sizeof(GUID)), "normal-ex-selected-adapter-match");
    }
    factory = ex ? static_cast<LegacyNormal*>(api.Get()) : normal.Get();
#endif
    D3DDISPLAYMODE display{}; exact(factory->GetAdapterDisplayMode(ordinal, &display), "public-display-format");
    std::printf(LEGACY_TAG "_DISPLAY format=%u width=%u height=%u refresh=%u\n",
      unsigned(display.Format), display.Width, display.Height, display.RefreshRate);
#if defined(DXVK_SYSTEM_D3D8)
    D3DCAPS8 caps{}; exact(factory->GetDeviceCaps(ordinal, D3DDEVTYPE_HAL, &caps), "public-device-caps");
    std::printf(LEGACY_TAG "_CAPS caps2=%08lx devcaps=%08lx windowed=%u\n",
      static_cast<unsigned long>(caps.Caps2), static_cast<unsigned long>(caps.DevCaps),
      unsigned(bool(caps.Caps2 & D3DCAPS2_CANRENDERWINDOWED)));
    // These are read-only capability queries, not fullscreen transitions.
    for (const auto format : {D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8})
      for (const BOOL windowed : {FALSE, TRUE})
        std::printf(LEGACY_TAG "_FORMAT_QUERY target=%u windowed=%u hr=%08lx\n",
          unsigned(format), unsigned(windowed), static_cast<unsigned long>(
            factory->CheckDeviceType(ordinal, D3DDEVTYPE_HAL, display.Format, format, windowed)));
#endif
    exact(factory->CheckDeviceType(ordinal, D3DDEVTYPE_HAL, display.Format, D3DFMT_A8R8G8B8, TRUE), "public-a8-windowed-target-support");
    WNDCLASSW klass{}; klass.lpfnWndProc = DefWindowProcW; klass.hInstance = GetModuleHandleW(nullptr); klass.lpszClassName = L"VioGpuD9SystemLiteralWindow";
    require(RegisterClassW(&klass), "register-probe-window");
    window = CreateWindowW(klass.lpszClassName, L"VIOGPU ordinary D9", WS_POPUP, 64, 64, Width, Height, nullptr, nullptr, klass.hInstance, nullptr);
    require(window != nullptr, "create-probe-window");
    if (present) { require(SetWindowPos(window, HWND_TOPMOST, 64, 64, Width, Height, SWP_SHOWWINDOW), "visible-exact-client-window"); UpdateWindow(window); pump(); }
    require((IsWindowVisible(window) != FALSE) == present, "explicit-window-visibility");
    D3DPRESENT_PARAMETERS parameters{}; parameters.BackBufferWidth = Width; parameters.BackBufferHeight = Height;
    parameters.BackBufferFormat = D3DFMT_A8R8G8B8; parameters.BackBufferCount = 1; parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD; parameters.hDeviceWindow = window; parameters.Windowed = TRUE;
#if defined(DXVK_SYSTEM_D3D8)
    parameters.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    exact(factory->CreateDevice(ordinal, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &parameters, &device), "ordinary-system-create-device-8");
#else
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (ex) { exact(api->CreateDeviceEx(ordinal, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &parameters, nullptr, &extended), "ordinary-system-create-device-9ex"); device = extended; }
    else exact(normal->CreateDevice(ordinal, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &parameters, &device), "ordinary-system-create-device-9");
#endif
    D3DDEVICE_CREATION_PARAMETERS created{}; exact(device->GetCreationParameters(&created), "public-created-device-identity");
    require(created.AdapterOrdinal == ordinal && created.DeviceType == D3DDEVTYPE_HAL
      && created.BehaviorFlags == D3DCREATE_HARDWARE_VERTEXPROCESSING && created.hFocusWindow == window, "actual-hal-hardware-vertex-device");
    verifyModules(tuple);
    render(device.Get(), extended.Get(), window, present, directory);
    require(!_wcsicmp(kmt.name().c_str(), tuple[0].c_str()), "native-name-stable-through-render"); verifyModules(tuple);
    std::array<unsigned char, dxvk::umd::RuntimeIdentityReplySize> after{};
    kmt.info(KMTQAITYPE_UMDRIVERPRIVATE, after.data(), UINT(after.size())); require(after == identityBytes, "runtime-identity-stable-through-render");
    const std::string manifest = "{\"schema\":\"" LEGACY_SCHEMA "-literal-v1\",\"api\":" + jsonString(argv[1])
      + ",\"phase\":" + jsonString(argv[2]) + ",\"width\":16,\"height\":16,\"frames\":2,\"pixels\":512,\"presents\":" + std::to_string(present ? 2 : 0)
      + ",\"screenPixels\":" + std::to_string(present ? 512 : 0) + ",\"luidHigh\":" + std::to_string(UINT(luid.HighPart)) + ",\"luidLow\":" + std::to_string(luid.LowPart)
      + ",\"generation\":" + std::to_string(identity.generation) + ",\"capabilities\":" + std::to_string(identity.capabilities)
      + ",\"ordinal\":" + std::to_string(ordinal) + ",\"vendor\":6900,\"device\":4176,\"format\":21,\"session\":" + std::to_string(session)
      + ",\"frontend\":" + jsonString(tuple[0]) + ",\"core\":" + jsonString(tuple[1]) + ",\"loader\":" + jsonString(tuple[2]) + ",\"icd\":" + jsonString(tuple[3])
      + ",\"systemRuntime\":" + jsonString(loaded(runtime)) + ",\"hardwareVertexProcessing\":true,\"softwareFallback\":false,\"registrationChangedByProbe\":false,\"productionAdmission\":false,\"frameResults\":["
      + "{\"rowPitch\":" + std::to_string(frameResults[0].pitch) + ",\"cooperative\":0,\"present\":" + std::to_string(frameResults[0].present) + ",\"screenAttempts\":" + std::to_string(frameResults[0].screenAttempts) + "},"
      + "{\"rowPitch\":" + std::to_string(frameResults[1].pitch) + ",\"cooperative\":0,\"present\":" + std::to_string(frameResults[1].present) + ",\"screenAttempts\":" + std::to_string(frameResults[1].screenAttempts) + "}]}\n";
    save(directory, L"manifest.json", manifest.data(), DWORD(manifest.size()));
    result = 0;
    std::printf(LEGACY_TAG "_RESULTS_READY api=%ls phase=%ls pixels=512 presents=%u screen_pixels=%u production_admission=0\n", argv[1], argv[2], present ? 2u : 0u, present ? 512u : 0u);
  } catch (const Failure& failure) {
    std::fprintf(stderr, LEGACY_TAG "_FAIL stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(failure.hr));
  } catch (...) { std::fputs(LEGACY_TAG "_FAIL stage=unexpected-exception\n", stderr); }
  if (hold) {
    const std::string held = "{\"schema\":\"" LEGACY_SCHEMA "-held-v1\",\"pid\":" + std::to_string(GetCurrentProcessId())
      + ",\"timeout_ms\":" + std::to_string(holdMs) + ",\"pending_exit\":" + std::to_string(result) + ",\"hold_event\":" + jsonString(holdEvent) + ",\"restoration_proved_by_event\":false}\n";
    try { if (outputCreated) save(directory.substr(0, directory.find_last_of(L"\\")), (leaf(directory) + L".held.json").c_str(), held.data(), DWORD(held.size())); }
    catch (...) { std::fputs(LEGACY_TAG "_FAIL stage=hold-file-write\n", stderr); result = 1; }
    std::printf(LEGACY_TAG "_HELD pid=%lu timeout_ms=%lu pending_exit=%d restoration_not_proved_by_event=1\n", GetCurrentProcessId(), holdMs, result);
    if (WaitForSingleObject(hold, holdMs) != WAIT_OBJECT_0) { std::fputs(LEGACY_TAG "_FAIL stage=hold-release-timeout\n", stderr); result = 1; }
    CloseHandle(hold);
  }
  if (extended) device.Reset();
  LegacyDevice* released = extended ? static_cast<LegacyDevice*>(extended.Detach()) : device.Detach();
  if (released) { const ULONG refs = released->Release(); std::printf(LEGACY_TAG "_DEVICE_RELEASE remaining=%lu\n", refs); if (refs) result = 1; }
  normal.Reset(); api.Reset();
  if (window && !DestroyWindow(window)) result = 1;
  std::printf(LEGACY_TAG "_DONE exit=%d production_admission=0 registry_changes=0\n", result);
  return result;
}
