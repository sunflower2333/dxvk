# DX8/DX9 native UMD audit, 2026-10-05

Initial read-only source audit: `dxvk-umd-ci` at `4f59adf`, paired driver
baseline `gg-dxvk-umd` at `fac1bfe6`. The roadmap remains applicable after
validated source `34ff484` and paired source `b6bf4c4f`; those checkpoints
repair DXGI ownership without adding a D3D9 bridge. Paths naming these
checkouts are relative to the DroidVM workspace.
**Native D3D9 offscreen clear/draw/readback now works through the development
DDI. Ordinary system-runtime activation and full DX8/DX9 support remain open.**
The 2026-10-06 [typed adapter checkpoint](native-d3d9-adapter-20261006.md)
adds identity/caps/lifetime development contracts. It keeps rendering caps
zero, CreateDevice unavailable and production OpenAdapter absent. The table
below describes the earlier rendering/package baseline.
The [embedded renderer checkpoint](native-d3d9-embedded-20261006.md) adds
the private D3D9 core, common exact-LUID/runtime owner factory and offscreen
initializer. The [typed device checkpoint](native-d3d9-device-20261006.md)
implements CreateDevice/Flush/DestroyDevice with runtime ownership and passes
all architecture lifecycle CI, including native ARM64 execution. Actual
target readiness now succeeds on diagnostic58624. The measured Limited
interactive probe selects the pinned local ICD, but that Mesa build returns
zero private runtime support and CreateDevice refuses before Vulkan device
creation. Matched Mesa8443c71 ports the existing callback bridge onto that
baseline. Its full ARM64 transport/link CI37453384744 and three-architecture
candidate CI37453381660 pass. The matched property reply and a bounded empty
device lifecycle pass on the target with balanced callbacks. An intermittent
flush loss remains separately recorded; startup callback repair f648a1a passes
the native66410check fixture and a compiled disabling control. Full37457000846
all6PASS/offline37456996101 all4PASS and exact three-architecture artifacts
verify. The rebuilt exact ARM64 UMD/probe pass three fresh target lifecycles
(Mesa diagnostics enabled once and disabled twice), with balanced context,
allocation and lock ownership, zero wrong-thread callbacks, and unchanged
signed58624 driver/desktop. This is empty offscreen lifecycle acceptance;
earlier device-loss causation and long-term stability remain open.
The [surface checkpoint](native-d3d9-resources-20261006.md) adds owned surface
groups, clear and padded readback; target18/19 verify192 pixels with diagnostics
enabled/disabled. The [draw checkpoint](native-d3d9-draw-20261006.md) adds typed
declarations, states and user-memory nonindexed drawing. Exact c874d55 full
CI37477931517/offline37477937872 pass; target20/21 independently verify384
clear/draw pixels, six real GPU submissions and balanced callbacks/residency
with diagnostics enabled/disabled. Signed58624/desktop continuity is retained.
The [shader checkpoint](native-d3d9-shaders-20261006.md) adds typed SM1–3
VS/PS ownership and float/int/Boolean constants. Exact source8c4e097 full/
offline CI passes. Corrected probeb3e83c8 target23/24 independently verify576
clear/draw/shader pixels with diagnostics enabled/disabled, nine nonempty
submissions and balanced callbacks/residency. Signed58624 and the fresh
DWM1552/Explorer2488 baseline remain continuous through these runs. An earlier
Explorer fault in the installed Mesa D3D10 UMD remains separately recorded.
The [texture checkpoint](native-d3d9-textures-20261007.md) adds coherent
static2D mip chains, all-common-level TexBlt and explicit sampler/state
mapping. Exact source7874196 full/offline/API/package CI passes; the native
134906-check fixture and four compiled semantic controls independently verify.
Target25/26 verify1088 pixels, including512 texture samples with checksum
d3afb9c5, seventeen nonempty submissions and balanced callbacks/residency,
diagnostics1/0. Fresh signed58624/DWM1552/Explorer6464 continuity and58
selected readiness values remain unchanged through both runs. Earlier
Explorer2488 also faults in the installed Mesa D3D10 UMD at the same RVA;
long-term desktop stability remains open. The [buffer checkpoint](native-d3d9-buffers-20261007.md)
adds owned VB/IB/range locks, multi-stream drawing,16/32-bit indexed draws and
signed base vertices. Consolidated cd4f5e2 CI37509470022 passes all6jobs;
164072native device checks perarchitecture and four target semantic controls
verify. Actual targetbuffer27/28 verify1472pixels each, including384buffer
pixels621cd685,24nonempty submissions and balanced callbacks/residency,
diagnostics1/0 with unchanged signed58624/desktop/readiness.
The [depth/stencil checkpoint](native-d3d9-depth-20261007.md) adds owned
D16/D24S8 attachments and independent native color/depth/stencil clearing,
including preclipped and computed rectangles. Exact2d285542 consolidated
CI37515057679 passes all6jobs and182260device checks perarchitecture; four
compiled semantic controls verify. Targetdepth29/30 verify2368pixels each,
including896depth/stencil pixels eac96ea5,38nonempty submissions and balanced
callbacks/residency, diagnostics1/0. Signed58624/DWM1552/Explorer6464 and58
readiness fields remain unchanged through both runs.
The [fixed-function checkpoint](native-d3d9-fixed-function-20261007.md) adds typed
transforms/material and sparse owned lights. Exact e86e0c50 consolidated
CI37520111683 passes all6jobs and202248device checks perarchitecture; five
compiled semantic controls verify. Targetfixed31/32 verify3712pixels each,
including1344fixed-function pixels b3416e11,59nonempty submissions and balanced
callbacks/residency, diagnostics1/0. Signed58624/DWM1552/Explorer6464 and58
readiness fields remain unchanged. That checkpoint left SYSTEMMEM/managed
buffer transfer, remaining state/resources/queries, presentation/reset and
ordinary runtime acceptance open.
The [SYSTEMMEM/BufBlt checkpoint](native-d3d9-buffer-transfer-20261007.md)
adds borrowed/owned SYSTEMMEM VB/IB and guarded byte-range/overlap/readback
transfers. Exact0d585e2 single CI37530530983 passes all6jobs and216868device
checks perarchitecture; five compiled semantic controls verify57inputs.
Targetbuffer-transfer33/34 diagnostics1/0 verify4224pixels and1056bytes each,
78nonempty submits, balanced lifetime/residency and unchanged signedSYS/desktop/
58readiness fields. Typed clip planes are the next contract; managed resources,
remaining state/resources/queries, presentation/reset and ordinary runtime
acceptance remain open.
Production OpenAdapter and rendering caps remain closed; the historical table
is not an inventory of the new static core or device bridge.

| Path | Current evidence | Meaning |
| --- | --- | --- |
| Upstream DX8 | `src/d3d8/d3d8_main.cpp` exports `Direct3DCreate8`; `d3d8_interface.cpp:10` calls `Direct3DCreate9` and requires DXVK's private `IDxvkLegacyD3DInterfaceBridge`. | App-local `d3d8.dll` plus DXVK `d3d9.dll`; cannot simply substitute Microsoft's d3d9 runtime. |
| Upstream DX9 | `src/d3d9/d3d9_main.cpp:31` exports `Direct3DCreate9`; `d3d9_interface.cpp:17` enumerates Vulkan adapters/displays and includes first-adapter fallback. `CreateDeviceEx` creates a Vulkan device without the private runtime owner, then `InitialReset` creates an implicit swapchain. | COM API replacement, not a WDDM D3D9 DDI. |
| DXVK native UMD | `src/umd/viogpudxvk.def:2` exports only `OpenAdapter10`/`OpenAdapter10_2` plus harness functions. `scripts/build-native-umd.ps1:41` disables D3D8/D3D9 and `:105` rejects a bare `OpenAdapter` export. `src/umd/meson.build:1` embeds the D3D11 core only. | No D3D9 adapter/device DDI table, and no embedded D3D9 renderer in this build. |
| GG DXVK entry | `viogpu/dxvk/umd-front.cpp` reuses the D3D10 ARM64X frontend; `viogpu/d3d10/build-front.ps1:22` and `viogpu/dxvk/build-umd.ps1:169` require only the two D3D10 exports and target probe. | Both ARM64 and emulated-x64 entry views lack D3D9 `OpenAdapter`. |
| GG legacy shim | `viogpu/viogpud3d/viogpud3d.cpp:2361` exports `OpenAdapter(D3DDDIARG_OPENADAPTER*)` but returns `E_NOTIMPL`; `check-contract.py:1053` enforces that. | Export presence alone still does not give D3D9. This shim is separate from the package's normal Mesa D3D10 path. |
| Existing DX9 smoke | `.github/workflows/build-wddm-arm64.yml:111` packages public API DLLs; `:203` explicitly requires running its D3D9/D3D10/D3D11 smoke with the app-local bundle. | Useful Vulkan/DXVK backend evidence; not ordinary Microsoft runtime/installed UMD activation. |

The native D3D10 candidate independently remains unadvertised: `src/umd/umd_adapter.cpp:26` accepts only the D3D10.0 device ABI, and `src/umd/umd_contract.cpp` retains the admission mask. Embedded D3D11 rendering capability therefore does not establish complete native DX11 runtime support either.

## Concrete implementation sequence

The existing Windows ARM64 guest was inventoried read-only on 2026-10-06
(Windows build 26100). System32 contains ARM64 D3D9, D3D9On12, D3D10, D3D11
and DXGI DLLs, but no D3D8 DLL. SysWOW64 contains all six as x86 DLLs.
The current guest therefore provides an x86 system-runtime target for DX8
acceptance. DLL presence alone does not establish the DDI path or DXVK UMD
activation; those still require loaded-module and rendering evidence.
Workspace evidence is
`.planning/dxvk-umd-remote-20261005/system-d3d-inventory.json`.

1. Add a **separate typed D3D9 adapter/device bridge** using `D3DDDIARG_OPENADAPTER`, `D3DDDI_ADAPTERFUNCS` (`GetCaps`, `CreateDevice`, `CloseAdapter`), `D3DDDIARG_CREATEDEVICE`, `D3DDDI_DEVICEFUNCS`, and `D3DDDI_DEVICECALLBACKS`. Negotiate DDI/runtime versions explicitly; do not cast the existing D3D10 tables or infer D3D9 support from them. First acceptance slice should be offscreen create/clear/draw/readback with exact object destruction and conservative caps, in an unregistered development harness.
2. Embed a static D3D9 implementation core excluding public API exports, analogous to `src/d3d11/meson.build:95`. Factor a private construction path using the **exact runtime adapter identity** and Turnip driver ID, copied runtime callbacks before `vkCreateDevice`, and the existing private Mesa runtime ABI. `D3D9DeviceEx` already accepts a `D3D9Adapter*` and `Rc<DxvkDevice>`, but also dereferences its COM parent for instance/config/compatibility; those dependencies need a controlled private parent. Calling `Direct3DCreate9` unchanged reintroduces enumeration, direct Vulkan/KMT ownership and implicit WSI.
3. Map the D3D9 resource/lock/open/DDI ownership, render and texture-stage state, FVF/vertex declarations, shader-model 1-3 token handling, fixed-function transforms/lighting, queries and flush through that embedded core. Reuse DXVK's existing D3D9 fixed-function and shader translators; translate DDI-specific resource handles and creation semantics rather than exposing COM objects as runtime handles. Gate `D3DCAPS9`, formats and optional features on actual completed contracts.
4. Implement **runtime-owned D3D9 presentation** through D3D9 callbacks/KMD allocation handles. The upstream `D3D9SwapChainEx::CreatePresenter` (`d3d9_swapchain.cpp:958`) creates its own Vulkan window surface; this must be separated from native UMD Present, shared primary ownership and reset/lost-device behavior. Test windowed Present, resize, reset, multi-device sharing and device removal through Microsoft's d3d9 runtime, with no app-local d3d9/dxgi DLLs and no software adapter fallback.
5. Prove DX8 separately on the guest's supported application architectures. Establish whether the system DX8 runtime reaches the new D3D9 DDI, and test representative fixed-function/FVF/shader-1.x workloads and device reset. If an explicit DX8-to-DX9 compatibility layer is necessary, adapt the dependency on DXVK's private bridge or document that separately; the current upstream app-local bundle alone is not system-wide DX8 driver support.

## KMD and packaging requirements

The first typed adapter slice should be an unregistered development harness
using proposed `umd_d3d9_adapter.{h,cpp}` and `tests/umd-d3d9-adapter.cpp`.
The actual WDK `D3DDDIARG_OPENADAPTER.hAdapter` and
`D3DDDIARG_CREATEDEVICE.hDevice` are in/out runtime-to-driver handles. Preserve
the incoming runtime handle separately before publishing a registered driver
token; these layouts differ from D3D10's split handles. Reuse the existing
160-byte identity decoder behind a raw-HANDLE query entry, retaining the
original QueryAdapterInfo callback and exact LUID/generation/capabilities.
Adapter calls need pinned ownership across callbacks, unlocked registry queries,
sticky close/reset retirement and nested-query rejection.

Initially validate typed GetCaps size/count dispatch without positive rendering
caps, and reject CreateDevice before publishing a driver device/table. Keep
the production bare OpenAdapter export absent until the rendering contract is
complete. Test original callback handles, malformed identity, S_FALSE,
generation/LUID changes, callback close/reset, stale handles, canaries and
unsupported versions on all three architectures. Negotiate D3D9 interface and
runtime versions explicitly; D3D10's packed Version rule is not its contract.
The exact-adapter renderer factory must be shared with the embedded D3D9 core
before this adapter slice can become a rendering acceptance result.

- **Identity plumbing is available:** GG `wddmddi.cpp:4003` `QueryUmdPrivateInfo` already produces the versioned 128-byte adapter prefix plus 32-byte identity trailer with Windows LUID/reset generation. Adapt its consumer to the real D3D9 callback/opaque runtime adapter handle; no new identity ABI is inherently required.
- D3D9 must use the existing context/allocation/render/escape services under `D3DDDI_DEVICECALLBACKS`, including reset generation checks, BO residency/lock synchronization, fences and teardown. Its native primary/opened/shared-resource formats must satisfy the same KMD validation and Present allocation lifetimes. Existing KMD support is reusable infrastructure, not proof that all D3D9 semantics work.
- `gg-dxvk-umd/viogpu/package/flat_package.py:80` currently repeats one filename in **three ordered registry slots** for D3D9/D3D10/D3D11. Either a unified binary must really export the corresponding D3D9 and D3D10 entry points, or the first slot must name a dedicated D3D9 UMD while later slots keep their appropriate UMDs. Apply the same distinction to `UserModeDriverNameWow`, `InstalledDisplayDrivers`, ARM64X native/EC dispatch, x86 artifacts, exports/import checks, source pins, PDBs and catalog inventory. Merely repeating the current DXVK candidate filename does not enable D3D9.
- GG's candidate pipeline currently accepts only a closed-admission, unregistered DXVK candidate (`flat_package.py:195`) and verifies D3D10 exports (`build_candidate_umds.ps1:172`). Extend those contracts deliberately when D3D9 is implemented; keep successful compile/load/probe results distinct from native runtime rendering acceptance.
- Local Microsoft references: `windows-driver-docs/.../display/initializing-communication-with-the-direct3d-user-mode-display-driver.md` describes the D3DDDI handshake; `enabling-support-for-the-direct3d-version-11-ddi.md` defines ordered registry slots. The `required-direct3d-9-capabilities.md` 9_1/9_2/9_3 tables concern level-9 capability exposure; they are not a complete D3D9 API/SM3 conformance checklist.

The initial 2026-10-05 audit changed no source, registration or remote VM.
Subsequent development and validation are recorded in the checkpoints above.
