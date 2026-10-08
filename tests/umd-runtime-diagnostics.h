#pragma once

#include "../src/umd/umd_runtime_diagnostics.h"
#include <cstdlib>
#include <cstring>

// Exercise actual output and per-owner suppression without Windows callbacks,
// candidate loading, or GPU work. Included in the existing runtime GPU fixture.
inline unsigned runtimeDiagnosticsControls() {
  using dxvk::umd::RuntimeGpuDiagnostics;
  using dxvk::umd::RuntimeGpuDiagnosticInfo;
  using Event = RuntimeGpuDiagnostics::Event;
  unsigned checks = 0;
  auto check = [&](bool condition) {
    ++checks;
    if (!condition) {
      std::fprintf(stderr, "runtime diagnostic control failed check=%u\n", checks);
      std::abort();
    }
  };
  const char* flags[] = {nullptr, "", "0", "true", " 1", "10", "1\n"};
  for (const char* flag : flags)
    check(!RuntimeGpuDiagnostics::enabledFlag(flag));
  check(RuntimeGpuDiagnostics::enabledFlag("1"));
  FILE* file = nullptr;
#ifdef _WIN32
  check(::tmpfile_s(&file) == 0 && file != nullptr);
#else
  file = std::tmpfile(); check(file != nullptr);
#endif
  RuntimeGpuDiagnosticInfo info;
  info.generation = 0x123456789ull; info.context = 17; info.queue = 19;
  info.references = 6; info.streamBytes = 228; info.lockedReferences = 3; info.index = 4;
  RuntimeGpuDiagnostics disabled;
  check(!disabled.record(file, Event::Failure, "disabled", int32_t(0x80004005u), info));
  check(std::ftell(file) == 0);
  RuntimeGpuDiagnostics first(true), second(true);
  check(first.record(file, Event::ContextReady, "context", 0, info));
  check(!first.record(file, Event::ContextReady, "second-context", 0, info));
  check(first.record(file, Event::SubmitEntered, "submit", 0, info));
  check(!first.record(file, Event::SubmitEntered, "second-submit", 0, info));
  check(first.record(file, Event::Failure, "render-callback", int32_t(0x80004005u), info, true, 1));
  check(!first.record(file, Event::Failure, "cleanup", int32_t(0x887a0005u), info, true, -1));
  check(first.record(file, Event::SubmitSucceeded, "submit", 0, info, true, 0));
  check(!first.record(file, Event::SubmitSucceeded, "second-success", 0, info, true, 0));
  // Suppression belongs to the owner, not the process or its callback table.
  info.context = 23;
  check(second.record(file, Event::Failure, "submit-reference", int32_t(0x80070057u), info));
  check(std::fflush(file) == 0);
  std::rewind(file);
  char output[4096] = {};
  const size_t bytes = std::fread(output, 1, sizeof(output) - 1, file);
  check(bytes > 0 && !std::ferror(file) && std::feof(file));
  check(std::strstr(output, "event=failure stage=render-callback hr=80004005 callback=1 callback_hr=00000001") != nullptr);
  check(std::strstr(output, "generation=4886718345 context=17 queue=19 references=6 stream_bytes=228 locked_references=3 index=4") != nullptr);
  check(std::strstr(output, "event=submit-entered") != nullptr && std::strstr(output, "event=submit-succeeded") != nullptr);
  check(std::strstr(output, "stage=submit-reference hr=80070057 callback=0 callback_hr=00000000") != nullptr);
  check(std::strstr(output, "context=23") != nullptr);
  check(!std::strstr(output, "disabled") && !std::strstr(output, "second-") && !std::strstr(output, "cleanup"));
  unsigned lines = 0;
  for (size_t i = 0; i < bytes; ++i) lines += output[i] == '\n';
  check(lines == 5);
  check(std::fclose(file) == 0);
  return checks;
}
