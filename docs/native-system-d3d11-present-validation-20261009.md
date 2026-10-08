# Ordinary SYSTEM D3D11 windowed Present validation

This separate validation probe follows the frozen ordinary-device source
`786593629c373503f708552c541921186f94c9ec`. It creates a genuine public DXGI
swapchain on the already verified SYSTEM D3D11 FL10_0 device and requires two
`Present(1, 0)` calls to return exactly `S_OK`. The old offscreen probe,
independent reader, forwarding fixture and native CPU17 packet stay unchanged.
Production admission masks and default UMD registration stay unchanged.

The new files are `tests/umd-d3d11-system-present-probe.cpp`,
`tests/verify-d3d11-system-present-originals.py`, and
`tests/umd-d3d11-system-present-reader-controls.py`. No production source or
binding-controller behavior changes in this slice.

## Runtime sequence and ownership

The command takes the same nine operands as the frozen ordinary probe:
selected `high:low` LUID, absolute validation frontend, core, private Vulkan
loader, ICD DLL, owned ICD JSON, fresh output directory, fresh `Local` hold event,
and a hold duration from 1 through 60000 milliseconds. The worker must run as the
limited interactive user in a nonzero Windows session. Session 0 fails closed.

Before the SYSTEM device factory runs, DXGI enumeration and real KMT queries
must identify one nonsoftware `1af4:1050` adapter with the exact requested LUID,
valid 160-byte viogpu private identity, and effective DX11 UMD equal to the
validation frontend. Device creation uses the explicit adapter,
`D3D_DRIVER_TYPE_UNKNOWN`, `D3D11_CREATE_DEVICE_SINGLETHREADED`, and the single
requested `D3D_FEATURE_LEVEL_10_0`. The existing frontend telemetry must prove
that the typed D3D11 DDI actually created the device. All SYSTEM DLLs, including
`user32.dll`, must resolve to their exact System32 paths. The selected private
frontend, core, loader and ICD must already have been loaded by the real factory
and match their approved absolute paths. A foreign loader or WARP is rejected.

The created device's actual DXGI adapter supplies the parent factory used for
`CreateSwapChain`. The probe creates its own process-specific class and visible
window on the calling thread. It creates one windowed DISCARD buffer, with
explicit 16×16 `R8G8B8A8_UNORM` dimensions, sample count 1, quality 0, render-target
usage, and no swapchain flags. It reopens the actual descriptor and checks every
one of these values plus the owned HWND. The swapchain's public device must have
the same COM identity as the previously verified device.

The probe stages and independently checks two complete 16×16 RGBA planes:
opaque black from clear and opaque red from the fullscreen triangle shader.
Each plane is saved before its corresponding Present, with blocking Map,
bounded RowPitch, all 256 literal pixels, and `GetDeviceRemovedReason() == S_OK`.
It clears the full buffer and rebinds the RTV for each frame because DISCARD
permits prior contents to change. Before each Present, a bounded message pump
checks that the window is still visible, not minimized, and belongs to the same
process and thread. The actual parent factory disables Alt+Enter and window
changes for that owned HWND with exact successful MakeWindowAssociation flags
`NO_WINDOW_CHANGES | NO_ALT_ENTER`. Successful GetFullscreenState must report
non-fullscreen immediately before and after each Present; these observations
are saved per frame and checked independently. Both exact `S_OK` Present results and both subsequent
removal checks are required; positive occlusion status is not a pass. Private
module paths and effective KMT UMD name are checked again after presentation.

The device, immediate context, swapchain and owned window remain alive while
the external controller restores the registry and effective KMT name. The
closed, flushed held sibling JSON retains the original schema and API11
checkpoint format. A passing attempt requires external release of the owned event before teardown.
Timeout and other failures still perform cleanup and fail admission; the external
controller remains responsible for restoration on those paths. After release, the probe releases all swapchain/context/device owners, destroys
its own window, unregisters its class, closes its KMT handle, and saves the
closed negotiation trace. The reader requires all frontend adapters to retire. The held wait retains the
original event-only bounded wait, so it does not certify continuing UI response
or desktop animation while waiting.

## Original evidence and independent reader

The fresh output directory has exactly eight regular files: `identity.raw`,
`clear.raw`, `draw.raw`, `vs.dxbc`, `ps.dxbc`, `negotiation.json`,
`closed-negotiation.json`, and `manifest.json`. The separate stdout, process
receipt and `<output-directory>.held.json` belong to that same worker attempt.
The retained-handle runner must be the original SHA256
`d8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad`,
which the reader verifies along with PID, exit, timeout, reap and pipe closure.

The Present manifest records both Present results, both subsequent removal
results, the actual swapchain descriptor, owned HWND/PID/thread/session, and
successful window/class cleanup, exact window-association flags/result, and
non-fullscreen observations immediately before and after both Presents. The independent reader requires 512 literal
pixels, exact typed negotiation, exact approved paths, two exact `S_OK` calls
with interval 1 and flags 0, matching swapchain HWND and worker PID, and held
release and complete adapter retirement. It certifies its narrow public
swapchain/readback/API-Present evidence. It does not certify current CI/core
identity, private Vulkan dependency inventory, registration restoration,
independent desktop pixels, DWM hardware, or production admission. ROOT must
join those separate originals before any broader claim.

## Concrete native CPU follow-on

Keep the existing 786 source CPU17 packet and its completed originals frozen.
Prepare a new attempt rather than editing or rebuilding that packet. Reuse a
reviewed native frontend only if its three source inputs, official dependency
closure, toolchain, native and EC CRT inputs, original ARM64X PE metadata,
exports, and source commit still match the frozen 786 frontend. This Present
slice does not change that frontend.

For the new probe, compile one TU independently for ARM64 and x64 with the
matching Microsoft SDK/WDK headers and `/std:c++17 /EHsc /W4 /WX /MT /O2
/Zc:preprocessor /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0A00 /c`.
Use an actual discovery compile for the full header closure, hash it before the
actual compile, and confirm it remains stable afterward. Link each object in a
separate bounded retained-handle stage with the matching release CRT closure
and **`user32.lib`** for the new window APIs. D3D11, DXGI and D3DCompiler entry
points remain dynamically resolved from System32, so those import libraries
are unnecessary. Inspect both actual PE machines, exports/imports, and outputs.
The two views require eight stages (header discovery, actual CL, LINK and PE
inspection each). None may run the probe or load the core during CPU admission.

For a later ordinary-runtime attempt, freeze this source, probe PE/hash,
reader/hash, final successful CI source/config/core tuple, actual loader/ICD
inventory and target LUID into a fresh controller authorization. The shared
binding owner must derive a separate API11 Present attempt whose final marker
and manifest expect `presents=2`; the existing offscreen marker remains valid
only for the offscreen attempt. Preserve its existing lease, durable intent,
raw six-value rollback, rescue tasks, limited held worker and external KMT
restoration. Run the new reader exactly once on that attempt's originals and
collect/reap all held/controller/rescue owners before releasing target ownership.
Local source checks passed in `artifacts/d3d11-present-local-02`: two strict
optimized official-header x64/x86 COFFs, two LLVM inspections, and 93 meaningful
owned synthetic reader rejections. All five child receipts plus their supervisor
record exit 0, closed output files and reap, without timeout. The compiler
stderr files are empty. Attempt01 and its 87-control originals are preserved;
it was superseded after peer review identified a missing Alt+Enter/windowed
state guard. Attempt02 verifies that substantive correction. Local headers/toolchain overlays are not full native
MSVC attestation. No native hardware or Present execution occurred while
preparing this source.

## Microsoft documentation used

Local workspace documents were inspected at
`windows-driver-docs/windows-driver-docs-pr/display/dxgi-presentation-path.md`
and `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/dxgiddi/`
`ns-dxgiddi-dxgi_ddi_base_functions.md` and
`ns-dxgiddi-dxgi_ddi_arg_present.md`. They explain the windowed DWM shared-surface
Blt path and the distinction from fullscreen Present.

The official public [Present documentation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present)
confirms interval and return-status behavior, including possible occlusion.
The [CreateSwapChain documentation](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-createswapchain)
confirms the device argument, explicit windowed descriptor and Session 0
restriction. This slice uses the existing legacy bitblt-model path to exercise
that concrete UMD API route; optimized flip-model and DWM coverage are separate.
