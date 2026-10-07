# DX8 native system-runtime compatibility slice

Base c0ea2966f432f52752462bd5657b61a6b70677ec; branch work/dx8-native-port-20261007.

Goal: commit a concrete genuine System32 D3D8 diagnostic/compatibility implementation that can be integrated and independently hardware-tested. No app-local DXVK API replacement, WARP fallback or blind Interface8 admission.

1. Audit real DX8 runtime, local Microsoft docs, SDK and existing native9 contracts — complete.
2. Implement isolated DX8 system-runtime probe and justified compatibility contracts — complete.
3. Compile/check locally, document exact runtime/architecture limits and reserved-file proposals — complete.
4. Commit reviewable branch with session trailers and prepare frozen x86 CPU packet for root — complete.

Ownership: root reserves native9 adapter/device and runtime-front/probe; DX10/11 agents have separate worktrees. This task uses only its own worktree. Target VM is reserved for root; no SSH/ADB, builds, selectors, lifecycle, GPU, registry or daemon operations.

Error: read-only docs lookup used missing reference/codes/windows-driver-docs; corrected by workspace filename search. No code/build failure.

Local SDK header include search initially mixed UM headers with MinGW Windows
headers and failed on DriverSpecs.h; corrected by verbatim shared D3D9 header
copies. Local verification01 then rejected nested comments in original SDK
d3d9.h under Werror. Evidence retained; unmodified SDK copies are system includes
in verification02. Source warnings remain fatal. No target build has run.

Root currently reserves target for DX10/11 verification and genuine DX9
lifecycle. Frozen x86 native EWDK execution, script parse, x86 payloads and all
genuine D3D8 adapter/device/GPU/presentation acceptance remain pending. These
are explicit later integration gates, not acceptance claims in this slice.
