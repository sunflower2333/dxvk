# Findings

- Isolated worktree at reference/codes/dxvk-umd-dx11-port-20261007, branch work/dx11-native-port-20261007, base c7e89537.
- No relevant memory registry entry exists.
- Root owns runtime-front and build-native-umd.ps1, DX10 agent owns adapter files and tests/umd-adapter.cpp.
- Current Device is registry-owned with typed DeviceEntry wrappers, callback service dispatch, intrusive resource retirement, and DXGI Present/Rotate. Reuse its owners rather than duplicating DX11 lifetime implementation.
- Native createAdapterDevice hardcodes D3D_FEATURE_LEVEL_10_0 and D3D10DDI_DEVICEFUNCS/pUMCallbacks. Add exact selected typed tables; D3D11 callback documentation requires a pointer to the runtime-owned mutable table, not cached callback slots.
- Native production policy currently retains gaps for resources/shaders/MRT/primary/threading; a typed D3D11 implementation must not itself clear these acceptance gates.
- Existing installed header search found no d3d10umddi.h. SDK ARM64 package contains libraries only. Locate core SDK package or obtain official package for ABI verification.
- Exact DXGI table layout is revision-sensitive via SDK IS_DXGI1_1_BASE_FUNCTIONS(interface,version), even for 10.1/11.0. Adapter agent changed shared helper to two arguments after audit.
- D3D11 typed table + core code compile cleanly with real SDK/WDK types under clang18 x64 Windows target. Local harness requires official SDK/WDK/MSVC include dirs, winrt WRL, NTSTATUS Windows/winternl prelude, _CRT_USE_BUILTIN_OFFSETOF and _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH.
- Implemented newer resource/SRV/read-onlyDSV/independent blend/UAV/SM5 compute/direct-indirect dispatch/indirect draw paths. Tessellation, class interfaces, general GS multi-stream/rasterization and format conversion remain explicit errors and admission gaps. Command lists stay optional NULL slots under no advertised command-list capability.
- Registry resource construction now catches exceptions and normalizes noncanonical success before publishing, so newer resource allocation failures cannot strand Creating reservations.
