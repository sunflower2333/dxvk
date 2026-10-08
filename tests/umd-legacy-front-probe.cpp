// CPU-only negative forwarding controls, run in separate native/x64 processes.
// No valid adapter query, device factory, GPU operation or registry change.
#include <windows.h>
#include <d3d9.h>
#include <d3dumddi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>

int wmain(int argc, WCHAR** argv) {
  if (argc != 3) return 2; // original ARM64X entry and expected view-specific core
  if (GetModuleHandleW(L"viogpudxvk.dll")) return 3;
  HMODULE front = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!front) return 4;
  const FARPROC address = GetProcAddress(front, "OpenAdapter");
  PFND3DDDI_OPENADAPTER open = nullptr;
  static_assert(sizeof(open) == sizeof(address));
  std::memcpy(&open, &address, sizeof(open));
  if (!open) { FreeLibrary(front); return 5; }
  const HRESULT nullResult = open(nullptr);
  if (nullResult != E_INVALIDARG || GetModuleHandleW(L"viogpudxvk.dll")) { FreeLibrary(front); return 6; }
  struct Guarded {
    BYTE prefix[64]; D3DDDI_ADAPTERFUNCS functions; BYTE suffix[64];
  } output;
  std::memset(&output, 0xa5, sizeof(output));
  const Guarded before = output;
  D3DDDI_ADAPTERCALLBACKS callbacks{};
  D3DDDIARG_OPENADAPTER args{};
  args.hAdapter = reinterpret_cast<HANDLE>(uintptr_t(0x4200));
  args.Interface = 0xffffffffu; args.Version = 0;
  args.pAdapterCallbacks = &callbacks; args.pAdapterFuncs = &output.functions;
  const auto original = args;
  const HRESULT unsupported = open(&args);
  const bool retained = !std::memcmp(&args, &original, sizeof(args)) && !std::memcmp(&output, &before, sizeof(output));
  HMODULE target = GetModuleHandleW(L"viogpudxvk.dll");
  WCHAR loaded[32768]{};
  const DWORD length = target ? GetModuleFileNameW(target, loaded, DWORD(_countof(loaded))) : 0;
  const bool matched = length && length < _countof(loaded) && !_wcsicmp(loaded, argv[2]);
  const bool closed = FreeLibrary(front) != FALSE;
  std::printf("LEGACY_FRONT_CPU null_hr=%08lx unsupported_hr=%08lx canaries=%u args=%u target=%ls frontend_released=%u\n",
    static_cast<unsigned long>(nullResult), static_cast<unsigned long>(unsupported), unsigned(retained), unsigned(retained), loaded, unsigned(closed));
  if (unsupported != E_INVALIDARG || !retained || !matched || !closed) return 7;
  std::printf("LEGACY_FRONT_CPU PASS typed_OpenAdapter=1 no_callbacks=1 explicit_device_calls=0 source_scope=1\n");
  return 0;
}
