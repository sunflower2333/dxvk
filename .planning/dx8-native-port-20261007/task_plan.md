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

## Second slice: immutable typed8/9 bridge (base b61f8a3)

Root authorized adapter/device edits in this isolated branch. Shared integration,
target CPU and genuine system9 runtime remain root-owned. No target commands
or workflow pushes are permitted here.

1. Thread const Interface8/9 identity through adapter, core device and private
   DXVK9 compatibility flags — complete; native/hardware execution pending.
2. Integrate genuine212-byte CAPS12; keep CAPS13 for9 and production gates and
   exports byte-identical — complete.
3. Add callback/mutation/lifetime, mismatched API and bounded shader/FVF tests
   while preserving every old9 test — implemented; native execution pending.
4. Strict three-architecture compile/link, portable sanitizer and actual SM1.x
   converter checks — complete: cross05 33 COFF/6 PE; GCC/Clang converter47
   and APIbounds2800; four exact semantic negatives compile0/exit1.
5. Commit a separate source packet/conflict handoff for root review — freezing.

Cross-build01 preserved a pre-existing sign-compare warning in the enabled-light
count onx86. Use an explicit nonnegative size_t count; source warnings stay
fatal. The new device fixture also includes algorithm explicitly rather than
depending on native STL transitive headers. Neither is a target compiler result.

Final cross05 enables Wshadow/Werror on both typed fixtures. The one new
adapter fixture shadow found during review was renamed before that run; its
original warning is retained in shadow-audit-01.stderr.txt. Final originals
are in artifacts/dxvk-native-d3d8-port-20261007/bridge-local-verification-05/.
No Windows PE, target, GPU, production admission or CI action occurred.
