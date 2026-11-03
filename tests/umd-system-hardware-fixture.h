#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

// The outer fixture still exercises source-linked DDIs with controlled
// callbacks. This optional backend sends its public D3D11 operations through
// the installed SYSTEM runtime and exact hardware UMD. It is separate from
// the ordinary SYSTEM factory/Present probe and never implies full admission.
namespace system_fixture {
inline bool hardware = false;
inline Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
inline LUID adapterLuid{};
inline std::array<std::wstring, 4> modulePaths;

inline void require(bool value, const char* label) {
  if (value) return;
  std::fprintf(stderr, "SYSTEM_SEMANTICS_IDENTITY_FAILURE %s\n", label);
  std::fflush(stderr);
  std::exit(2);
}

inline std::wstring path(const char* input) {
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, nullptr, 0);
  require(size > 1 && size < 32768, "bounded-utf8-path");
  std::wstring value(size, L'\0');
  require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, value.data(), size) == size,
    "utf8-path-conversion");
  WCHAR full[32768];
  const DWORD count = GetFullPathNameW(value.c_str(), 32768, full, nullptr);
  require(count && count < 32768 && GetFileAttributesW(full) != INVALID_FILE_ATTRIBUTES,
    "existing-absolute-module");
  return full;
}

inline void checkModules() {
  require(!GetModuleHandleW(L"warp.dll") && !GetModuleHandleW(L"d3d10warp.dll"),
    "no-software-runtime");
  for (const auto& expected : modulePaths) {
    const auto slash = std::wcsrchr(expected.c_str(), L'\\');
    require(slash && slash[1], "module-basename");
    const HMODULE module = GetModuleHandleW(slash + 1);
    WCHAR actual[32768];
    const DWORD count = module ? GetModuleFileNameW(module, actual, 32768) : 0;
    require(count && count < 32768 && !_wcsicmp(actual, expected.c_str()),
      "exact-loaded-frontend-core-loader-icd");
  }
  const HMODULE runtime = GetModuleHandleW(L"d3d11.dll");
  WCHAR actual[32768], system[32768];
  const UINT systemSize = GetSystemDirectoryW(system, 32768);
  const DWORD count = runtime ? GetModuleFileNameW(runtime, actual, 32768) : 0;
  require(systemSize && systemSize < 32768 && count && count < 32768
    && !_wcsicmp(actual, (std::wstring(system) + L"\\d3d11.dll").c_str()), "system-d3d11-runtime");
}

inline void configure(int argc, char** argv) {
  if (argc == 1) return;
  require(argc == 6 && std::string(argv[1]) == "--system-hardware",
    "usage---system-hardware-front-core-loader-icd");
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  for (size_t i = 0; i < modulePaths.size(); ++i) modulePaths[i] = path(argv[i + 2]);
  // The controller checks the installed binding and file hashes. This process
  // independently selects exactly one non-software VirtIO hardware adapter.
  const HMODULE dxgi = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  require(dxgi != nullptr, "system-dxgi-runtime");
  const auto create = reinterpret_cast<HRESULT (WINAPI*)(REFIID, void**)>(GetProcAddress(dxgi, "CreateDXGIFactory1"));
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  require(create && create(IID_PPV_ARGS(&factory)) == S_OK, "system-dxgi-factory");
  for (UINT index = 0; ; ++index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
    const HRESULT hr = factory->EnumAdapters1(index, &candidate);
    if (hr == DXGI_ERROR_NOT_FOUND) break;
    require(hr == S_OK && candidate, "enumerate-adapter");
    DXGI_ADAPTER_DESC1 desc{};
    require(candidate->GetDesc1(&desc) == S_OK, "adapter-description");
    if (desc.VendorId != 0x1af4 || desc.DeviceId != 0x1050) continue;
    require(!adapter && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE), "unique-hardware-adapter");
    adapter = candidate; adapterLuid = desc.AdapterLuid;
  }
  require(adapter != nullptr, "virtio-hardware-adapter-found");
  hardware = true;
  std::printf("SYSTEM_SEMANTICS_START luid=%08lx:%08lx vendor=1af4 device=1050 controlled_outer_ddi=1\n",
    static_cast<unsigned long>(adapterLuid.HighPart), static_cast<unsigned long>(adapterLuid.LowPart));
  std::fflush(stdout);
}

inline HRESULT create(D3D_FEATURE_LEVEL level, ID3D11Device** device, ID3D11DeviceContext** context) {
  const HRESULT hr = D3D11CreateDevice(hardware ? adapter.Get() : nullptr,
    hardware ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_WARP, nullptr, 0,
    &level, 1, D3D11_SDK_VERSION, device, nullptr, context);
  if (hardware) {
    std::printf("SYSTEM_SEMANTICS_DEVICE requested=%04x hr=%08lx\n", unsigned(level), static_cast<unsigned long>(hr));
    std::fflush(stdout);
    if (hr == S_OK) {
      require(*device && *context && (*device)->GetFeatureLevel() == level, "exact-created-feature-level");
      Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
      Microsoft::WRL::ComPtr<IDXGIAdapter> actualAdapter;
      DXGI_ADAPTER_DESC desc{};
      require((*device)->QueryInterface(IID_PPV_ARGS(&dxgiDevice)) == S_OK
        && dxgiDevice->GetAdapter(&actualAdapter) == S_OK && actualAdapter->GetDesc(&desc) == S_OK,
        "created-device-adapter");
      require(desc.AdapterLuid.HighPart == adapterLuid.HighPart && desc.AdapterLuid.LowPart == adapterLuid.LowPart
        && desc.VendorId == 0x1af4 && desc.DeviceId == 0x1050, "created-device-exact-hardware-luid");
      checkModules();
    }
  }
  return hr;
}
}
