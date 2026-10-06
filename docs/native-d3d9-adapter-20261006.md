# Typed native D3D9 adapter checkpoint

The development UMD now compiles a separate typed D3D9 adapter handshake,
`VioGpuDxvkOpenAdapter9ForTest`, using the actual WDK
`D3DDDIARG_OPENADAPTER` and `D3DDDI_ADAPTERFUNCS` ABI. Production `OpenAdapter`
is absent, rendering caps/counts are zero, and `CreateDevice` is unavailable.
This is adapter contract coverage; DX8/DX9 runtime rendering remains open.
The D3D10 admission mask and paired driver source pin are unchanged.

## Contracts

- Preserve the incoming runtime `hAdapter` and its original query callback
  before publishing a unique driver token. Tokens are never reused.
- Accept literal `Interface == 9`. `Version` is an opaque runtime build ID,
  without a packed D3D10 build requirement. Return the compiled
  `D3D_UMD_INTERFACE_VERSION` as `DriverVersion`.
- Reuse the exact 160-byte paired KMD identity decoder through a raw `HANDLE`
  query overload. Require exact `S_OK` and retain LUID, reset generation and
  capabilities. The existing D3D10 overload delegates to the same query.
- Pin in-flight adapter ownership, release registry locks before callbacks,
  reject recursive queries and opens, and retire changed/reset identities.
  Callback close/reopen and concurrent close cannot reclaim an active query.
- Validate exact `UINT` count and `D3DCAPS9` sizes. Empty format/query lists
  have zero bytes. Do not cast away const or rewrite `GetCaps.DataSize`.
  Every failure preserves caller outputs, including in/out device handles,
  command buffers, allocation/patch lists and device function tables.

## Validation

`tests/umd-d3d9-adapter.cpp` passed **42,429 checks** inside the existing Windows
ARM64 VM. Tests cover malformed identity fields, old replies, exact `S_OK`,
exceptions, unsupported interfaces, opaque build IDs, caps canaries, retained
query ownership, nested calls, close/reopen, sticky reset/identity changes,
concurrent retirement and stale driver tokens.

Native portable MSVC 19.44.35229, SDK/WDK 26100.1, `/W4 /WX` for our sources,
external-header warning isolation, and static CRT linking were used.
Compilation/link process wall time was **18.006 seconds**, fixture process
wall time **3.503 seconds**, and the timed runner **28.599 seconds**. Setup,
transfers and initial library provisioning attempts are separate.
The fixture excludes the unused `OLDNAMES.lib` default library; unresolved
symbols remain fatal. Production CI retains its normal complete toolchain.

Guest fixture SHA256:
`2fc771f057c2994e614c24129f76a416bdbe968eb11c253cc437fe52dfb1d85b`.
All nine guest source hashes match the implementation; LLVM independently
confirms ARM64 and only a KERNEL32 import. Driver `100.6.101.58623` /
`oem10.inf`, DWM 1644 and Explorer 5828 were retained. No installation,
registration or VM configuration changed.

Local existing sanitizer checks passed: identity 69, runtime identity 180,
shader container 147. Exact implementation source is
`aac5172b1c0983dbe58b4f6fe4c8a1a3cd7e7e03`.
[Offline CI 37408748554](https://github.com/sunflower2333/dxvk/actions/runs/37408748554)
passed all four jobs, including MSVC x86/x64 execution and ARM64 compilation.
[Full CI 37408749906](https://github.com/sunflower2333/dxvk/actions/runs/37408749906)
passed all six jobs, including three embedded production builds and native
ARM64 execution. Each architecture executed exactly 42,429 D3D9 adapter checks.
Previous D3D10 ownership, rotation and resource regressions also passed.
Full builds retain the production export/import gates; rendering admission
and all eight native admission gaps remain open.

Workspace evidence: `artifacts/dxvk-native-d3d9-20261006/`. Reusable guest
recipes: `.planning/dxvk-umd-remote-20261005/{assemble-guest-umd-support.sh,
build-guest-d3d9-adapter.ps1,verify-guest-d3d9-adapter.sh}`. The portable KMD
toolchain remains reusable and unchanged.

## Next Implementation

Embed the existing D3D9 translation core with exact runtime LUID/Turnip
selection, runtime callbacks before Vulkan device creation, and an offscreen
construction path without an implicit swapchain. Implement typed D3D9 device
and resource/state/render/readback DDIs before exposing rendering caps or
production `OpenAdapter`. Runtime-owned presentation/reset and independent
DX8 system-runtime acceptance follow. See the
[DX8/DX9 roadmap](native-dx8-dx9-roadmap-20261005.md).
