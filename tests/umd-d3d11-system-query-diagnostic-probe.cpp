// SPDX-License-Identifier: MIT
// Separate diagnostic child; the frozen ordinary probe remains unchanged.
#if !defined(VIOGPU_D11_ADAPTER_QUERY_DIAGNOSTIC) || VIOGPU_D11_ADAPTER_QUERY_DIAGNOSTIC != 1
#error Build this source only in the explicit adapter-query diagnostic role
#endif
#define wmain vioGpuD11OrdinaryProbeMain
#include "umd-d3d11-system-probe.cpp"
#undef wmain

namespace {
class AdapterQueryDiagnosticScope {
public:
  bool enable() {
    SetLastError(ERROR_SUCCESS);
    const DWORD required = GetEnvironmentVariableW(Name, nullptr, 0);
    const DWORD error = GetLastError();
    if (!required && error != ERROR_ENVVAR_NOT_FOUND && error != ERROR_SUCCESS) return false;
    if (required > 32768) return false;
    m_present = required != 0 || error == ERROR_SUCCESS;
    if (required) {
      m_original.resize(required);
      SetLastError(ERROR_SUCCESS);
      const DWORD copied = GetEnvironmentVariableW(Name, m_original.data(), required);
      if (copied + 1 != required || (!copied && GetLastError() != ERROR_SUCCESS)) return false;
      m_original.resize(copied);
    }
    if (!SetEnvironmentVariableW(Name, L"1")) return false;
    m_active = true;
    WCHAR value[2] = {};
    return GetEnvironmentVariableW(Name, value, 2) == 1 && value[0] == L'1' && value[1] == 0;
  }
  bool restore() noexcept {
    if (!m_active) return true;
    if (!SetEnvironmentVariableW(Name, m_present ? m_original.c_str() : nullptr)) return false;
    m_active = false;
    return true;
  }
  ~AdapterQueryDiagnosticScope() { restore(); }
private:
  static constexpr const WCHAR* Name = L"DXVK_UMD_ADAPTER_QUERY_TRACE";
  std::wstring m_original;
  bool m_present = false, m_active = false;
};
}

int wmain(int argc, WCHAR** argv) {
  AdapterQueryDiagnosticScope scope;
  try {
    if (!scope.enable()) {
      std::fprintf(stderr, "SYSTEM_D3D11_QUERY_DIAGNOSTIC_FAIL stage=child-environment-enable ordinary_admission=0\n");
      return 1;
    }
    std::puts("SYSTEM_D3D11_QUERY_DIAGNOSTIC_BEGIN child_trace=1 ordinary_admission=0");
    std::fflush(stdout);
    // Exactly the ordinary probe's nine operands and factory/pixel/module/
    // private160 checks. No second process, factory or registration writer.
    const int result = vioGpuD11OrdinaryProbeMain(argc, argv);
    if (!scope.restore()) {
      std::fprintf(stderr, "SYSTEM_D3D11_QUERY_DIAGNOSTIC_FAIL stage=child-environment-restore ordinary_admission=0\n");
      return 1;
    }
    std::printf("SYSTEM_D3D11_QUERY_DIAGNOSTIC_END probe_exit=%d ordinary_admission=0\n", result);
    return result;
  } catch (...) {
    const bool restored = scope.restore();
    std::fprintf(stderr, "SYSTEM_D3D11_QUERY_DIAGNOSTIC_FAIL stage=child-exception environment_restored=%u ordinary_admission=0\n", restored ? 1u : 0u);
    return 1;
  }
}
