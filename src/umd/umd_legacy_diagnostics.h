#pragma once

#include <windows.h>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <utility>

namespace dxvk::umd {
inline bool legacyDiagnosticsEnabled() noexcept {
  static const bool enabled = [] {
    const DWORD error = GetLastError();
    char flag[2] = {};
    const bool value = GetEnvironmentVariableA("TU_WDDM_DIAGNOSTICS", flag, sizeof(flag)) == 1
      && flag[0] == '1';
    SetLastError(error);
    return value;
  }();
  return enabled;
}

// Bounded scalar-only diagnostics for the original runtime table. Preserve
// the callee's result and thread error state; no runtime callbacks or borrowed
// argument dereferences occur after the original DDI has returned.
inline HRESULT traceLegacyDdi(const char* name, HRESULT hr) noexcept {
  if (!legacyDiagnosticsEnabled()) return hr;
  const DWORD error = GetLastError();
  const int savedErrno = errno;
  static std::atomic<unsigned> calls{0}, failures{0};
  const auto sequence = calls.fetch_add(1, std::memory_order_relaxed);
  if (sequence < 1024 || (FAILED(hr) && failures.fetch_add(1, std::memory_order_relaxed) < 32)) {
    std::fprintf(stderr, "VIOGPU_LEGACY_DDI sequence=%u hr=%08x call=%s\n",
      sequence + 1, unsigned(uint32_t(hr)), name);
    std::fflush(stderr);
  }
  SetLastError(error); errno = savedErrno;
  return hr;
}

inline void traceLegacyState(const char* name, uint32_t state, uint32_t value) noexcept {
  if (!legacyDiagnosticsEnabled()) return;
  const DWORD error = GetLastError();
  const int savedErrno = errno;
  static std::atomic<unsigned> calls{0};
  if (calls.fetch_add(1, std::memory_order_relaxed) < 256) {
    std::fprintf(stderr, "VIOGPU_LEGACY_STATE call=%s state=%u value=%08x\n", name, state, value);
    std::fflush(stderr);
  }
  SetLastError(error); errno = savedErrno;
}

template<auto Function, typename... Args>
HRESULT APIENTRY tracedLegacyDdi(Args... args) {
  const HRESULT hr = Function(std::forward<Args>(args)...);
#ifdef _MSC_VER
  return traceLegacyDdi(__FUNCSIG__, hr);
#else
  return traceLegacyDdi(__PRETTY_FUNCTION__, hr);
#endif
}
}
