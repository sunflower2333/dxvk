# Unregistered ordinary SYSTEM D10.0 validation preparation

This source packet prepares a concrete validation entry and probe. It does
not register a UMD, change the signed KMD/image, change a production admission
mask, replace an API DLL or provide ordinary-runtime hardware acceptance.
Default Mesa remains the installed driver. ROOT owns any future native build,
execution, adapter selection experiment and restoration.

`tests/umd-system-validation-front.cpp` has one typed D3D entry,
`OpenAdapter10(D3D10DDIARG_OPENADAPTER*)`. It accepts only the D10.0 interface
and forwards the original argument pointer and HRESULT to the existing core's
`VioGpuDxvkOpenAdapterForTest`. The core still checks runtime build revision,
actual runtime adapter identity, real callbacks and backend creation. The
frontend modifies no adapter/device tables, callbacks, versions or caps.
Its additional readonly telemetry export records real forwarding results and
the actual retained core path; it cannot open an adapter or load a core.

Native ARM64 loads the exact sibling `viogpudxvk.dll`; ARM64EC/emulated x64
loads `viogpudxvk_x64.dll`; x86 loads `viogpudxvk_x86.dll`. A same-basename
core already loaded from another directory is rejected. The successfully
resolved core is held for process lifetime, because published DDI tables and
children outlive OpenAdapter. No loader work occurs in DllMain. A null or
non-D10.0 control never loads a core.

The ordinary probe loads DXGI, D3D10 or D3D11, GDI and D3DCompiler by exact
`GetSystemDirectoryW` paths and verifies their actual module paths. No
SOFTWARE, WARP, default-adapter fallback or manual candidate preload exists.
The caller must supply the exact nonzero LUID and absolute frontend/core
paths. The selected DXGI adapter must be `1af4:1050`, non-software and have
that LUID. A separate actual KMT open checks render support and the private
VIOGPU identity/LUID. Its effective D10 or D11 UMD filename must match the
requested frontend before any public CreateDevice is attempted. A cached
Mesa name stops at `effective-kmt-frontend-selection-required` and prints both
paths; it cannot accidentally validate Mesa.

After the real SYSTEM factory succeeds, the probe requires the exact frontend
and core modules and positive successful private-entry forwarding telemetry.
It also checks the created device's adapter LUID. It creates an ordinary DXGI
windowed discard swapchain, compiles SM4 shaders through SYSTEM D3DCompiler,
clears black, draws a fullscreen red triangle, saves both original 16x16 RGBA
readbacks before comparing every byte, and calls ordinary `Present(1,0)` for
both frames. Map, device-removal and Present HRESULTs and actual row pitches
are retained in `manifest.json`. Successful Present and literal readback are
a bounded driver slice, not proof of DWM composition or onscreen correctness.
Independent KMD submission/completion originals and actual desktop evidence
must join any future hardware claim.

The exact successful raw closure is `clear.raw`, `draw.raw`, `manifest.json`.
Stdout, stderr, retained-process receipts and reader output stay outside it.
`verify-system-validation-originals.py` independently checks 512 literal pixels,
2048 bytes, metadata/module/LUID equality, both successful frame HRESULTs and
the actual owned runner's zero exit/drained process. It does not certify
registration restoration or hardware/production admission.

## Finite driver-selection blocker

Local Microsoft originals establish the adapter registration tuple and DDI:

- `windows-driver-docs/windows-driver-docs-pr/display/enabling-support-for-the-direct3d-version-10-ddi.md`
  places D10 in the second `UserModeDriverName` entry.
- `.../enabling-support-for-the-direct3d-version-11-ddi.md` places D11 in the
  third entry, including a shared DLL implementation.
- `.../loading-a-user-mode-display-driver.md` says the runtime obtains the
  name from registration and loads that DLL in its process.
- `.../initializing-communication-with-the-direct3d-version-10-ddi.md` supplies
  real runtime handles/callbacks and invokes OpenAdapter10/CreateDevice.
- `.../initializing-communication-with-the-direct3d-version-11-ddi.md` uses
  OpenAdapter10_2 and explicit supported-version/caps discovery.

These documents specify adapter-wide installation settings. They do not
specify a process-local UMD registry tuple or a supported live cache-refresh
operation. Existing project findings2238+ retain a real earlier temporary
first-slot edit: KMT still returned the original Mesa filename, no frontend
entry was called, and the exact original tuple was restored. Registry-byte
replacement therefore cannot be assumed to select this candidate.

An already loaded Mesa DWM module need not unload merely because registry
bytes change, but this is not an isolation guarantee: any new device/process
on the adapter can observe the changed selection, and DWM/Explorer can create
new devices. A tuple cannot be claimed to affect only this probe. An adapter
reset/restart might also unload/recreate existing devices; none is staged or
authorized by this packet. A target selection strategy still needs a
separately established ABI/cache contract or a ROOT-reviewed bounded experiment.
There is deliberately no registry writer or automatic reset/restart here.

## Separate D3D11 blocker

This frontend has no `OpenAdapter10_2` export. Mode11 exists to retain the
actual SYSTEM D3D11 selection/factory diagnosis; no undocumented fallback to
a legacy D10 entry is assumed. The current core's private modern entry does
return development versions, but `getCaps` retains zero
`D3D11DDICAPS_3DPIPELINESUPPORT` while production requirements are missing.
Forwarding that entry alone cannot establish D3D11 feature-level admission.
This packet changes neither that cap nor any requirement/mask. A later
independently reviewed bounded modern validation route is required.

## Reviewed native build and execution prerequisites

Do not duplicate or modify the frozen production modern frontend packet.
Its EWDK build recipe is at
`artifacts/modern-runtime-deploy-20261008/native-arm64x-front-857a5fe-build-module-negative-01/`:
`source/scripts/build-native-modern-front.ps1` and its toolchain manifests
preserve the actual native/EC merge, CSR provider and exact library inputs.
This candidate needs its own explicitly selected CPP/header/DEF and additional
original WDK header dependencies; those are not covered by that frozen packet.
An ARM64X build and actual native/x64 typed negative controls are prerequisites
before considering a native shared-slot binding. A plain AA64 candidate is
not suitable for the adapter's shared native/x64 name slot.

In an already reviewed actual MSVC/SDK/WDK environment, separate compile/link
commands for the standalone native probe are:

```powershell
cl /nologo /std:c++17 /O2 /W4 /WX /EHsc /MD /c tests\umd-system-validation-probe.cpp /Fo:probe.obj
link /nologo probe.obj kernel32.lib user32.lib /out:umd-system-validation-probe.exe
```

No D3D or GDI import library is used: the probe resolves genuine SYSTEM entry
functions dynamically. The actual matching kernel32/user32 and CRT library
inputs must be selected and pinned from the developer environment. Capture each actual compiler/linker separately
with the unchanged retained-process runner; do not infer a native build from
the local cross-compiled COFFs.

The CPU-only typed module control, after a reviewed frontend build, is:

```powershell
umd-system-validation-probe.exe --entry-negative C:\ReviewedValidation\viogpudxvk_validate10.dll
```

The ordinary mode has exactly seven operands after the executable:

```text
probe 10 00000000:0000abcd C:\ReviewedValidation\viogpudxvk_validate10.dll C:\ReviewedValidation\viogpudxvk.dll C:\FreshOriginals Local\UniqueReviewedHoldEvent 60000
```

The LUID and paths are examples, never target defaults. A readonly effective
KMT filename check must first show the independently selected candidate;
otherwise the probe deliberately stops before device creation.

Use the existing interactive user session for visible Present. The hold event
is fresh and session-local; its releasing controller must be in that same
session and have access. Even on a partial failure, the process holds before
exit for up to60seconds, with any created device/core retained. External ROOT
control can restore and independently re-read the original tuple/state before
signalling this event. The event itself does not prove restoration. A timeout
fails the process; it never performs restoration or forcefully closes DWM.

Any future binding experiment must independently preserve exact original
value kinds/bytes for native/WoW tuples and InstalledDisplayDrivers, source
and module identities, signed SYS/package/PnP state and desktop owners. ROOT
must check their restoration while the probe is held, then release/reap it.
This is a conditional evidence requirement, not a staged mutation command or
an authorization to change registration.
