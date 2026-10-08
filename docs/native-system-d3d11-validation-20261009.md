# Ordinary SYSTEM D3D11 validation

The generic modern development adapter publishes DDI versions but returns zero
pipeline support while the production requirements remain incomplete. This
prevents an ordinary SYSTEM D3D11 device from reaching the typed device factory.
The separate core entry `VioGpuDxvkOpenAdapter11Fl10_0ForValidation` resolves this
validation prerequisite. It was implemented in a103c1f and integrated as
6b40a0f. It exposes only DDI11 with logical FL10_0 and retains the strict runtime
callback and output guards. The public entries and production requirement masks
remain closed. The private DXVK backend still requires its existing FL11 device.

Microsoft's local DDI originals describe modern OpenAdapter negotiation through
GetSupportedVersions and GetCaps, rather than the initial Interface/Version:

- `windows-driver-docs/windows-driver-docs-pr/display/initializing-communication-with-the-direct3d-version-11-ddi.md`
- `windows-driver-docs/windows-driver-docs-pr/display/pipelines-for-direct3d-version-11.md`
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10_2ddi_getsupportedversions.md`
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d11ddi_3dpipelinesupport_caps.md`

## Forwarding frontend

`umd-d3d11-system-front.cpp` exports only `OpenAdapter10_2` and the readonly
`VioGpuDxvkD11ValidationInfo` diagnostic. It loads the exact sibling core and
resolves only the dedicated validation entry. Its core basenames are
`viogpudxvk.dll` in native ARM64, `viogpudxvk_x64.dll` in ARM64EC/x64 and
`viogpudxvk_x86.dll` in x86. A foreign module with the same basename is rejected.
The exact core and diagnostic frontend are retained through process termination;
this is a bounded test process lifetime, not a public DLL unloading policy.

Five typed adapter thunks record the original pointers, opaque adapter handles,
interface/build/flags, callback addresses, results and bounded version/capability
outputs. They call the core without holding the trace lock and return its exact
HRESULT or private size. They neither replace runtime callbacks nor construct
capability responses. In particular, a zero cap result remains zero. A reserved
adapter record exists before the original OpenAdapter call, and the original
output table address remains authoritative through reentrant descriptor changes.

The CPU fixture `umd-d3d11-system-forwarding.cpp` injects a named stub, not an
installed core. It checks all five typed calls, original argument/handle identity,
the live core callback update, exact success/failure forwarding, unchanged failed
outputs, a reentrant diagnostic read and adapter retirement. It is CPU forwarding
evidence only. The actual frontend's malformed entry mode additionally checks
nine bounded null/table/info controls without a core load.

## Genuine runtime probe

`umd-d3d11-system-probe.cpp` accepts nine operands after its executable:

```text
<high:low-LUID> <absolute-frontend> <absolute-core>
<absolute-private-loader> <absolute-ICD-DLL> <absolute-owned-ICD-JSON>
<fresh-output-directory> <Local-hold-event> <hold-ms>
```

The hold bound is 1 through 60000 ms. Both `VK_DRIVER_FILES` and
`VK_ICD_FILENAMES` must already identify the exact owned JSON. That JSON must be
derived deterministically from the genuine ICD original with a manifest-relative
or absolute library path. The probe does not modify process configuration,
registration or default selection. The one existing binding controller owns
Api11/native slot 2, its global lease, restoration and rescue; this source adds no
second registration writer.

The probe obtains genuine `dxgi.dll`, `d3d11.dll`, `gdi32.dll` and
`d3dcompiler_47.dll` from the process's System32 view. It requires the exact
selected LUID, vendor/device 1af4/1050, render support, a nonsoftware adapter,
the actual private160 identity and the effective KMT DX11 frontend name. It calls
SYSTEM `D3D11CreateDevice` with that adapter, driver type UNKNOWN, no software
module, SINGLETHREADED and exactly one requested feature level, FL10_0. It checks
the returned level, creation flags, device's actual adapter LUID, typed11
negotiation, live runtime callback addresses and the exact loaded core/private
loader/ICD paths. It rejects WARP and winevulkan modules.

The offscreen oracle creates a 16 by 16 RGBA8 target and staging resource. It
compiles the original SM4 vertex/pixel HLSL with the system compiler, explicitly
binds viewport/raster/shader/topology/target state, clears black and draws a red
fullscreen triangle. Two blocking Map readbacks compare all 512 pixels and
retain all 2048 original bytes, pitches and actual device-removal HRESULTs.

Before waiting, the probe writes and flushes the closed sibling
`<fresh-output-directory>.held.json`. Its schema is 1, api is 11, and its fields
are `pid`, `event`, `timeout_ms`, `output`, `factoryCalled`, `factoryResult`,
`pixelsPassed`, `stage` and `result`. This checkpoint is written on failure as
well as success. The device/context remain owned while the single binding owner
restores registration and signals the fresh Local event. After release, the
probe drops the context/device, closes its KMT adapter and records final adapter
retirement. A missing event release remains a failed attempt.

Exact checkpoints are:

```text
SYSTEM_D3D11_HELD pid=<decimal> timeout_ms=<decimal> pixels_passed=<0|1> stage=<stage-or-empty> hr=<8hex>
SYSTEM_D3D11_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=0 software_fallback=0 production_admission=0 registry_changes=0
```

The entry-only mode is `probe --entry-negative <absolute-frontend>`. It expects:

```text
SYSTEM_D3D11_VALIDATION_NEGATIVE_PASS checks=9 core_loaded=0 registry_changes=0 gpu_calls=0
```

The raw output directory contains exactly `identity.raw`, `clear.raw`,
`draw.raw`, `vs.dxbc`, `ps.dxbc`, `negotiation.json`, `closed-negotiation.json`
and `manifest.json`. The positive independent reader joins the separate closed
held checkpoint, original retained-handle process receipt and stdout, real
identity, complete ordered typed negotiation, final retirement, exact loaded
module paths and every literal readback byte. Run it with `--directory`,
`--stdout`, `--process`, `--held`, `--frontend`, `--core`, `--loader`, `--icd`,
`--icd-json`, `--system-directory`, `--luid-high`, `--luid-low` and fresh `--output`.
Its acceptance covers ordinary offscreen originals; token authorization,
registration restoration, production admission and Present are separate gates.

## Native build and next gates

For the frontend, use the existing reviewed modern ARM64X nine-stage producer
route from source857a5fe: native ARM64 compile and full-path link with its four
explicit matching static CRT libraries; ARM64EC compile; original softintrin
archive symbol inspection; flat ARM64X merge; genuine hybrid headers/load-config,
exports and imports reopening. Adapt its frozen source manifest to these actual
three frontend sources and the derived producer; do not reuse the old five-file
857 manifest. The frontend has two expected exports, rather than that producer's
old three. All actual Microsoft header dependencies, selected compiler/backends
and corresponding native/EC libraries must be pinned. Actual ARM64 and x64 view
controls must prove their own exact sibling paths and independent view state.

For each probe or CPU fixture profile, compile its single translation unit with
matching official SDK/WDK headers and `/std:c++17 /EHsc /W4 /WX /MT /O2
/Zc:preprocessor /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0A00 /c`.
Link in a separate retained-handle stage, using the corresponding machine and
matching release CRT plus actual default-library closure. The probe uses dynamic
system entry resolution, so no D3D11, DXGI or D3DCompiler import library is needed.
No stage may execute the frontend, core or probe during CPU-only build admission.

Local x64/x86 optimized official-header COFF verification and 58 independent
owned synthetic format rejection controls passed. The controls exercise reader
validation only and are not native, shader-execution or GPU evidence. Native
MSVC, ARM64X views, fresh current-core binding, actual limited USER runtime and
all hardware results remain pending. The positive marker includes `presents=0`:
swap-chain creation, two Presents and independent desktop pixels are the next
concrete extension after this ordinary-device gate, and this slice does not
certify them or enable normal production/default registration.
