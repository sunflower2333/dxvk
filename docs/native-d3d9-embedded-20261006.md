# Embedded native D3D9 renderer checkpoint

The development UMD embeds DXVK's D3D9 translation core and has an internal
offscreen construction path. It uses the runtime's exact adapter identity
and copied callbacks before Vulkan device creation. Typed D3D9 device and
rendering DDIs are still unavailable; production `OpenAdapter` is absent and
rendering caps remain zero. No successful target GPU construction or ordinary
DX8/DX9 system-runtime rendering is claimed by this checkpoint.

## Implementation

- `AdapterIdentity` keeps the original raw runtime adapter handle independently
  of driver tokens. D3D10 and the new D3D9 core share this neutral identity.
- `GpuBackend` selects exactly one matching valid LUID with the Mesa Turnip
  driver ID. Runtime-owned construction checks the private Mesa protocol and
  returned context identity; it does not fall back to a different adapter.
- `RuntimeBackendSnapshot` pins the callback owner and copies the descriptor
  and callback table before callbacks can reenter and mutate their originals.
- The static D3D9 core excludes `d3d9_main.cpp` and its public API factories.
  Its annotation singleton is defined in the shared annotation implementation,
  so both embedded and public API builds retain exactly one definition.
- A private `D3D9InterfaceEx` parent receives the selected instance and adapter.
  It skips global singleton acquisition, display enumeration and DPI changes,
  and rejects calls to its public device factory.
- `InitializeNativeOffscreen` initializes render state, viewport and scissor
  without creating the implicit swapchain. Recording and submission are joined
  before construction returns. FPU preservation is requested.
- Constructor helper guards release annotation, initializer and converter
  owners when later setup fails. The internal `D3D9Backend` owns the concrete
  device, private parent and Vulkan backend in teardown order.

`VioGpuDxvkProbeD3D9BackendForTest` is a development construction/destruction
export. It returns only the implicit swapchain count and publishes no renderer
object. The caller must keep a runtime callback dispatcher active throughout
construction and destruction. It is not a typed `CreateDevice` DDI.

Implementation source is `2a4095c9ef2802f2ce223a0db4e72d72795e1324`, including
`effb654` embedding, `e46f10f` constructor cleanup, `48dd2bf` explicit Windows
header prerequisites and `9b4168b` header regression coverage.

## Validation

Local ASan/UBSan fixtures passed identity 69, runtime identity 180, runtime
owner 49 and shader container 147 checks. Source `9b4168b` passed all four jobs
in [offline CI 37412714285](https://github.com/sunflower2333/dxvk/actions/runs/37412714285),
including Windows x64/x86 execution and ARM64 compilation.

Full builds at `9b4168b` compiled all three architecture cores but exposed the
annotation singleton dependency at link time. The shared-source repair passed
all six jobs in
[full CI 37415595130](https://github.com/sunflower2333/dxvk/actions/runs/37415595130)
at exact `2a4095c`: three embedded builds, identity/shader CPU checks and native
ARM64 execution. Each architecture executed 42,429 adapter checks, 49 runtime
owner checks and 45 actual-DLL backend rejection checks. Existing D3D10
entry/lifetime, rotation, resource and WARP controls also passed.

The shared annotation source also passed the normal public API
[Windows build 37415565405](https://github.com/sunflower2333/dxvk/actions/runs/37415565405)
and [package builds 37415565396](https://github.com/sunflower2333/dxvk/actions/runs/37415565396)
at the same source. Independent LLVM inspection passed the three DLL PE
architectures, required development exports, absent production `OpenAdapter`
and public D3D factories, and forbidden D3D factory import checks. All 13
native ARM64 execution hashes match the corresponding downloaded executables.
The 166 retained CI files, including independent PE reports, are hashed.

The existing ARM64 Windows guest independently compiled and executed the
Windows runtime header/owner fixture at `9b4168b`: **49 checks passed**.
Portable native MSVC 19.44.35229 and SDK/WDK 26100.1 used `/MT`, fatal source
warnings and external-header warning isolation. Compile/link took **0.807
seconds**, fixture execution **0.060 seconds**, and the timed runner **1.012
seconds**. These exclude transfer and toolchain provisioning.

All ten guest input hashes match exact Git blobs, and the downloaded fixture
hash matches the receipt:
`2f075b0975e1799b4e779700c19883c558a9a890e5215e38918cba1e28d47d9b`.
Independent LLVM inspection confirms native ARM64 with no D3D, DXGI, Vulkan
or dynamic CRT imports. Installed driver `100.6.101.58623` / `oem10.inf`,
DWM 1644 and Explorer 5828 were retained. No installation, registration,
GPU workload or VM configuration change occurred.

Workspace evidence is `artifacts/dxvk-native-d3d9-embedded-20261006/`.
Reusable scripts are under `.planning/dxvk-umd-remote-20261005/`:
`build-guest-runtime-backend.ps1`, `verify-guest-runtime-backend.sh` and
`verify-d3d9-embedded-ci.sh`. Failed compiler/link attempts are retained.

## Next Implementation

Connect a separate typed D3D9 device bridge to the embedded core. Preserve
the original `D3DDDIARG_CREATEDEVICE.hDevice`, copy the required callback
fields before construction, run backend work under `RuntimeService`, and
publish a driver token/table only after construction succeeds. Device and
resource owners must survive callback reentry and drain before their runtime
handles become invalid. Implement resource/state/clear/draw/readback DDIs
with exact bytes and cleanup checks before exposing rendering caps.

The obsolete command-buffer/allocation/patch-list fields in `CreateDevice`
must not be used as context backing; obtain that backing through
`CreateContextCb`. Preserve the incoming runtime `hResource` independently
of each resource's driver token, and map surface indices explicitly. Internal
Vulkan BO ownership does not implement the separate atomic shared-resource
allocation and destruction contract.

Then prove runtime-owned construction and rendering on the target, followed
by presentation/reset and ordinary Microsoft DX8/DX9 runtime acceptance.
All eight D3D10 admission gaps and the paired KMD binary pins remain unchanged.
See the [DX8/DX9 roadmap](native-dx8-dx9-roadmap-20261005.md).
