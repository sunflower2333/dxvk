#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
static void log(const char* text) {
  DWORD written;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, DWORD(std::strlen(text)), &written, nullptr);
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &written, nullptr);
}
int wmain(int argc, wchar_t** argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  if (argc != 5 || (std::wcscmp(argv[2], L"inline") && std::wcscmp(argv[2], L"worker"))
      || (std::wcscmp(argv[3], L"exit") && std::wcscmp(argv[3], L"unload")
          && std::wcscmp(argv[3], L"close-exit") && std::wcscmp(argv[3], L"close-unload") && std::wcscmp(argv[3], L"exit-locked"))
      || (std::wcscmp(argv[4], L"cold") && std::wcscmp(argv[4], L"warm"))) return 64;
  char identity[80]; std::snprintf(identity, sizeof(identity), "HOST pid=%lu tid=%lu", GetCurrentProcessId(), GetCurrentThreadId()); log(identity);
  auto module = LoadLibraryW(argv[1]); if (!module) return 65;
  auto setup = reinterpret_cast<BOOL (*)(BOOL, BOOL, BOOL)>(GetProcAddress(module, "ProbeSetup"));
  auto close = reinterpret_cast<void (*)()>(GetProcAddress(module, "ProbeClose"));
  if (!setup || !close) return 66;
  if (!setup(!std::wcscmp(argv[2], L"inline"), !std::wcscmp(argv[4], L"warm"), !std::wcscmp(argv[3], L"exit-locked"))) return 67;
  log("HOST_SETUP_RETURN");
  if (!std::wcscmp(argv[3], L"close-unload")) close();
  if (!std::wcscmp(argv[3], L"unload") || !std::wcscmp(argv[3], L"close-unload")) {
    log("HOST_FREE_LIBRARY"); if (!FreeLibrary(module)) return 68;
    log("HOST_FREE_LIBRARY_DONE"); return 0;
  }
  if (!std::wcscmp(argv[3], L"close-exit")) close();
  log("HOST_EXIT_PROCESS"); ExitProcess(0);
}
