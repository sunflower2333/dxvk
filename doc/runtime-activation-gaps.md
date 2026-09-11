# Native Microsoft runtime activation gaps

Source audit: DXVK7c0b9e3 and VKD3D61edc56,2026-09-12. DXVK5b0983d and
VKD3D61edc56 pass standalone architecture CI; cf492c9 CI34613950771 and
ef4b174 CI34615056755 also pass all five jobs. GenMips source c9e389d compiles,
but the runner's WARP ignores GenerateMips for views starting at array slice1.
Independent API-only controls in CI34617903693 reproduce it while default
views and slice0 work. The strict slice0 oracle at6dfb092 passes all488WARP
checks onx64/x86; CI34619152294 passes all5jobs including ARM64. GS7c0b9e3
CI34620365858 also passes all5jobs, including original/rebuilt GS WARP pixels,
2257shader checks and all3production architectures. Target GS proof is pending.
Earlier bounded hardware readbacks remain valid
for their exact tested sources; neither candidate is a registered native
Direct3D runtime driver. Parent owns a new independent ordinary application
acceptance tool and all device actions.

## Minimum dependency chain

| Gate | DXVK D3D11 application path | VKD3D D3D12 application path |
| --- | --- | --- |
| Runtime chooses candidate | Export real OpenAdapter10 and OpenAdapter10_2; stage a separately selectable, signed INF/package. Current .def exports only development helpers; existing paired INF selects Mesa. | Export OpenAdapter12 and stage the real D3D12 UMD registration. Current .def only exposes VioGpuD3D12Bridge helpers; paired INF selects Mesa. |
| Adapter negotiation | Reuse exact-LUID runtime QueryAdapterInfo callback and retained AdapterIdentity. Add GetSupportedVersions/GetCaps for the precise implemented DDI and pipeline level. OpenAdapter10_2 must not reject the runtime's initial Interface before negotiation. | Add adapter lifetime, original runtime callback/handle, identity and KMD/WDDM capability query, version/capability negotiation and interface table selection. Existing create API receives loader/identity from a harness. |
| Actual CreateDevice | Construct in runtime-supplied hDrvDevice memory, preserve original callbacks, initialize the exact selected table and DXGI base. Existing code only accepts D3D10.0 and explicitly rejects all flags; no D3D11 table exists. | Implement native private-device size/CreateDevice and runtime callback initialization. Existing BridgeCreate allocates its own Context and accepts an arbitrary report callback, not the runtime CreateDevice contract. |
| Runtime core correctness | Fill all mandatory entries for the chosen interface, including runtime initialization/null state, default constant updates, table relocation, resource/shader capabilities and failure cleanup. Do not claim a feature level while its required shaders/resources are rejected. | Wire device/command/queue tables through the native runtime interface; implement resource/heap creation, placement, map/unmap, allocation information, GPUVA, residency and monitored fences. Harness BindObject is explicitly not CreateHeapAndResource. |
| Draw and output | Preserve the tested embedded Vulkan device and DDIs, finish shared/opened and primary/backbuffer ownership, residency and DXGI base operations. Current Present is a synchronous windowed CPU publication path using real callbacks, with no actual runtime/display proof. | Add graphics shaders and graphics pipeline/state/draw, texture and RTV/DSV/sampler descriptors, copy/resolve and actual runtime DXGI Present. Current pipeline creation rejects any graphics shader; only compute exists. |
| Acceptance | Parent tool must use system d3d11/dxgi, exact VIOGPU LUID and actual loaded candidate UMD, then shader/readback and visible Present. | Parent tool must use system d3d12/dxgi, exact VIOGPU LUID and loaded candidate UMD; device creation precedes ordinary queue/draw/Present acceptance. |

## DXVK exact present-day omissions

Comparing the current D3D10 table assignments with Microsoft's local
D3D10DDI_DEVICEFUNCS field list leaves10 fields unset after the current GS
continuation. Two are version-dependent
vertex pipeline hooks; this is an inventory, not an assertion that every field
is required for every negotiated version:

```
pfnCalcPrivateGeometryShaderWithStreamOutput pfnCreateGeometryShaderWithStreamOutput
pfnSoSetTargets pfnDrawAuto
pfnSetPredication
pfnCalcPrivateOpenedResourceSize pfnOpenResource
pfnSetTextFilterSize
pfnResetPrimitiveID pfnSetVertexPipelineOutput
```

Several rendering restrictions remain even for non-null callbacks: only
buffer/Texture2D resources, primary/shared descriptors rejected,
single color target, bounded VS/PS semantics and typed
resolves. Feature level negotiation must describe this honestly; adding an
entrypoint symbol or copying D3D10 pointers into a D3D11 table is insufficient.
GS shader creation/binding and resource slots now reuse DXVK's internal device;
VS->GS generic inputs preserve raw32 bits and GS->PS types follow the active
consumer. The runtime union input signature can include undeclared registers;
these must not require an upstream producer. WARP/SPIR-V verification passes
at7c0b9e3 CI34620365858. Stream output and further system values remain absent.
Most translation work can reuse DXVK's internal device, but SetPredication is
an upstream stub and cannot simply be forwarded as implemented functionality.

The runtime sharing path must use native ownership and synchronization.
DXVK's Wine metadata escapes do not meet the Windows KMD contract. The
Microsoft OpenResource contract says DestroyResource is not called after any
OpenResource error, so failure cleanup belongs inside OpenResource itself.

## Windows on Arm architecture and registration contract

Microsoft's WDDM64-bit guidance explicitly requires a separate32-bit UMD;
WOW64 does not translate the opaque driver-private allocation structures.
UserModeDriverNameWoW selects that32-bit driver. It is not an x64-on-Arm64
selection key. The same KMD serves all processes, so shared private structures
must use fixed-width fields and verified alignment across all three builds.

Microsoft's D3D11 INF guidance defines the third UserModeDriverName entry as
the D3D11 DDI. These list positions select D3D interfaces, not CPU architectures.
WDDM2.1 run-from-DriverStore guidance uses absolute %13% paths and explicitly
says filesystem WOW64 redirection does not apply there. Therefore placing two
same-name native/x64 DLLs in folders does not itself select the correct one.

Microsoft's Arm64X documentation says native ARM64 and x64/ARM64EC processes
can load the same physical DLL through its respective ABI view. Windows11 on
Arm has no separate System32 folder for pure x64 system binaries. An Arm64X
front DLL is thus the documented single-path solution for this project; it can
dispatch to separate architecture-specific implementation DLLs. This is an
architecture design, not proof that a D3D UMD has been activated.

| Process on Windows11 ARM64 | Planned registered module and implementation |
| --- | --- |
| Native ARM64 | UserModeDriverName D3D slot -> Arm64X native view -> ARM64 engine, ARM64 Vulkan loader/ICD and actual imported CRT dependencies |
| Emulated x64 / ARM64EC | Same registered path -> Arm64X EC view -> x64 engine and x64 Vulkan loader/ICD/dependencies |
| Emulated x86 | UserModeDriverNameWoW D3D slot -> separate x86 UMD and x86 Vulkan loader/ICD/dependencies |

Do not invent an emulated-x64 registration value. No reviewed Microsoft D3D
source here establishes one. OpenCL's shared Registry64 vendor list is a
different loader contract and cannot establish D3D selection behavior.

The future front DLL must preserve the actual WDK signatures/calling convention,
original runtime handles/callbacks, per-process engine lifetime and exact adapter
identity. Resolve implementation and Vulkan dependencies by validated absolute
package paths. A plain export forwarder does not automatically establish that
dependency search path; the OpenGL agent has already encountered that boundary.
Do not replace engine-side OpenAdapter with a harness factory.

Activation acceptance requires separate native ARM64/x64/x86 ordinary runtime
processes, system d3d11/d3d12/dxgi, exact hardware LUID and the expected loaded
front/implementation modules with hashes. A successful Arm64X loader fixture or
three architecture builds alone is insufficient. Parent's independent f6da604
runtime probe (CI34613737738 PASS) is the ordinary D3D acceptance path; explicit
minimum10_0 is a bring-up milestone and default11_0 is the formal D3D11 gate.

Official sources read2026-09-11:

- https://learn.microsoft.com/windows-hardware/drivers/display/microsoft-windows-vista-display-driver-64-bit-issues
- https://learn.microsoft.com/windows-hardware/drivers/display/enabling-support-for-the-direct3d-version-11-ddi
- https://learn.microsoft.com/windows-hardware/drivers/display/wddm-2-1-features
- https://learn.microsoft.com/windows/arm/arm64x-pe
- https://learn.microsoft.com/windows/arm/arm64x-build

## Implementation order

Source cf492c9 closes four runtime initialization slots: default constant
buffer updates reuse the real update path, relocation acknowledges the new table
without caching stale pointers, and counter queries report DXVK's actual absence
of performance counters even after device loss. It is still insufficient to
advertise the incomplete feature-level/table contract. No additional backend
workload is introduced; subsequent acceptance must use the ordinary runtime tool.

The following source continuation accepts the native UNDEFINED topology reset
and all D3D10 primitive topology setters, and atomically replaces up to16
viewports. A native all-NaN viewport remains an unbound zero-area slot without
compacting later indices; zero count clears the whole viewport/scissor state
even if the runtime clear hint is zero. This does not add geometry shaders or
claim their draw coverage. Existing Microsoft WARP CI verifies actual state
getters after replacement, shrink, invalid-input refusal and zero-count reset.

The next mandatory callback, GenMips, now maps the native auto-mip flag to the
embedded API resource and uses the backend's GPU mip blits. It validates owned
views, creation flags, MIP type and filterable format support. Independent
API-only controls localize the runner's WARP no-op to nonzero first array
slices, with an active but empty debug queue. The CPU oracle therefore tests
slice0/mips0-1 and strictly checks all eight subresources for generated pixels
and isolation; all488checks pass onx86/x64 in6dfb092 CI34619152294. Production is unchanged, and
nonzero-slice DXVK mip generation remains a target validation gap. Other native
MiscFlags remain rejected; this does not implement shared-resource ownership.

1. Build actual adapter/version/CreateDevice wiring and complete the chosen
   runtime table as one coherent candidate. Preserve exact-LUID filtering and
   use typed WDK assignments. Keep incomplete interfaces out of registration.
2. Implement native resource/shared/heap/fence contracts with real original
   runtime handles and callbacks, including failed creation cleanup and reset.
3. Integrate normal application draw/Present using the parent's strict runtime
   tool, then expand feature levels only alongside complete required behavior.

Reference sources reviewed: current src/umd/umd_adapter.cpp, umd_ddi.cpp,
umd_allocation.cpp and viogpudxvk.def; VKD3D libs/vkd3d-umd/ddi.h/ddi.cpp and
viogpud3d12.def; workspace Mesa Adapter.cpp/Device.cpp for the existing DDI
negotiation pattern; local Microsoft OpenAdapter10_2/GetSupportedVersions,
D3D10DDI_DEVICEFUNCS, OpenResource and D3D12 OpenAdapter/CreateDevice docs.
Actual WDK types take precedence when markdown parameter names disagree.
