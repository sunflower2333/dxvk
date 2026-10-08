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
  check(!disabled.record(file, Event::CloseEntered, "disabled-close", 0, info));
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
  // Cleanup records remain visible after the first submission failure. A raw
  // callback status must not replace the close operation's accumulated error.
  info.allocations = 6; info.lockedAllocations = 5; info.activeCalls = 0;
  const Event closeEvents[] = {Event::CloseEntered, Event::AllocationCleanupFinished,
    Event::DestroyContextEntered, Event::DestroyContextFinished, Event::CloseFinished};
  for (Event event : closeEvents) {
    const bool callback = event == Event::DestroyContextFinished;
    check(first.record(file, event, "close-control", int32_t(0x8007000eu), info, callback, callback ? 1 : 0));
    check(!first.record(file, event, "second-close", 0, info));
  }
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
  check(std::strstr(output, "event=close-entered") != nullptr && std::strstr(output, "event=close-finished") != nullptr);
  check(std::strstr(output, "event=allocation-cleanup-finished") != nullptr && std::strstr(output, "event=destroy-context-entered") != nullptr);
  check(std::strstr(output, "event=destroy-context-finished stage=close-control hr=8007000e callback=1 callback_hr=00000001") != nullptr);
  check(std::strstr(output, "allocations=6 locked_allocations=5 active_calls=0") != nullptr);
  check(!std::strstr(output, "disabled") && !std::strstr(output, "second-") && !std::strstr(output, "stage=cleanup"));
  unsigned lines = 0;
  for (size_t i = 0; i < bytes; ++i) lines += output[i] == '\n';
  check(lines == 10);
  check(std::fclose(file) == 0);
  // The metadata pair is opt-in and bounded separately from close/failure
  // records. Its after record consumes scalar status, never old buffer memory.
  file = nullptr;
#ifdef _WIN32
  check(::tmpfile_s(&file) == 0 && file != nullptr);
#else
  file = std::tmpfile(); check(file != nullptr);
#endif
  dxvk::umd::RuntimeGpuRenderReferenceDiagnostic references[RuntimeGpuDiagnostics::RenderReferenceLimit] = {};
  for (uint32_t i = 0; i < RuntimeGpuDiagnostics::RenderReferenceLimit; ++i) {
    auto& reference = references[i];
    reference.allocationHandle = 0x12340000 + i; reference.writeOperation = i & 1;
    reference.allocationIndex = i; reference.allocationOffset = i * 4096;
    reference.relativePatchOffset = 44 + i * 16; reference.patchOffset = 256 + reference.relativePatchOffset;
    reference.slot = 0x123456789abcdef0ull + i;
    reference.boPresumedAvailable = i != 2; reference.boPresumedOffset = reference.relativePatchOffset;
    reference.boPresumed = 0xfedcba9876543210ull + i;
  }
  RuntimeGpuDiagnostics metadata(true), bounded(true);
  check(!disabled.wantsRenderSnapshot());
  check(!disabled.recordRenderBefore(file, info, references, 6));
  check(!disabled.record(file, Event::RenderAfter, "render-callback", 0, info, true, 0));
  check(std::ftell(file) == 0);
  check(metadata.wantsRenderSnapshot());
  check(!metadata.recordRenderBefore(file, info, nullptr, 6));
  check(metadata.wantsRenderSnapshot());
  check(metadata.recordRenderBefore(file, info, references, 6));
  check(!metadata.wantsRenderSnapshot());
  check(metadata.record(file, Event::RenderAfter, "render-callback", int32_t(0x80004005u), info, true, int32_t(0x80004005u)));
  check(!metadata.recordRenderBefore(file, info, references, 1024));
  check(!metadata.record(file, Event::RenderAfter, "second-render", 0, info));
  check(bounded.recordRenderBefore(file, info, references, 1024));
  check(std::fflush(file) == 0);
  std::rewind(file);
  char metadataOutput[8192] = {};
  const size_t metadataBytes = std::fread(metadataOutput, 1, sizeof(metadataOutput) - 1, file);
  check(metadataBytes > 0 && !std::ferror(file) && std::feof(file));
  check(std::strstr(metadataOutput, "captured=6 omitted=0") != nullptr);
  check(std::strstr(metadataOutput, "captured=8 omitted=1016") != nullptr);
  check(std::strstr(metadataOutput, "index=0 allocation_handle=12340000 write_operation=0 allocation_index=0 allocation_offset=0 patch_offset=300 relative_patch_offset=44 slot_u64=123456789abcdef0 bo_presumed_available=1 bo_presumed_offset=44 bo_presumed=fedcba9876543210") != nullptr);
  check(std::strstr(metadataOutput, "index=1 allocation_handle=12340001 write_operation=1") != nullptr);
  check(std::strstr(metadataOutput, "bo_presumed_available=0") != nullptr);
  check(std::strstr(metadataOutput, "event=render-after stage=render-callback hr=80004005 callback=1 callback_hr=80004005") != nullptr);
  check(!std::strstr(metadataOutput, "index=8 ") && !std::strstr(metadataOutput, "second-render"));
  lines = 0;
  for (size_t i = 0; i < metadataBytes; ++i) lines += metadataOutput[i] == '\n';
  check(lines == 19); // Two before summaries, two counts, fourteen rows, one after.
  check(std::fclose(file) == 0);
  return checks;
}
