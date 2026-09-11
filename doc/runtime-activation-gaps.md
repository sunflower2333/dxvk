# Native Microsoft runtime activation gaps

Source audit: DXVK5b0983d and VKD3D61edc56,2026-09-11. These candidates pass
standalone architecture CI. Earlier bounded hardware readbacks remain valid
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
D3D10DDI_DEVICEFUNCS field list leaves20 fields unset. Two are version-dependent
vertex pipeline hooks; this is an inventory, not an assertion that every field
is required for every negotiated version:

```
pfnDefaultConstantBufferUpdateSubresourceUP
pfnGsSetConstantBuffers pfnGsSetShaderResources pfnGsSetSamplers pfnGsSetShader
pfnCreateGeometryShader
pfnCalcPrivateGeometryShaderWithStreamOutput pfnCreateGeometryShaderWithStreamOutput
pfnSoSetTargets pfnDrawAuto
pfnGenMips pfnSetPredication
pfnRelocateDeviceFuncs
pfnCalcPrivateOpenedResourceSize pfnOpenResource
pfnCheckCounterInfo pfnCheckCounter pfnSetTextFilterSize
pfnResetPrimitiveID pfnSetVertexPipelineOutput
```

Several rendering restrictions remain even for non-null callbacks: only
buffer/Texture2D resources, all MiscFlags and primary descriptors rejected,
single color target, triangle-list pipeline, bounded VS/PS semantics and typed
resolves. Feature level negotiation must describe this honestly; adding an
entrypoint symbol or copying D3D10 pointers into a D3D11 table is insufficient.
Most translation work can reuse DXVK's internal device, but SetPredication is
an upstream stub and cannot simply be forwarded as implemented functionality.

The runtime sharing path must use native ownership and synchronization.
DXVK's Wine metadata escapes do not meet the Windows KMD contract. The
Microsoft OpenResource contract says DestroyResource is not called after any
OpenResource error, so failure cleanup belongs inside OpenResource itself.

## Implementation order

The next source step closes four runtime initialization slots: default constant
buffer updates reuse the real update path, relocation acknowledges the new table
without caching stale pointers, and counter queries report DXVK's actual absence
of performance counters even after device loss. It is still insufficient to
advertise the incomplete feature-level/table contract. No additional backend
workload is introduced; subsequent acceptance must use the ordinary runtime tool.

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
