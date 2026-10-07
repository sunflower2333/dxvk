# Typed DX11 compute probe with real KMT transport

`dxvk-umd-d3d11-compute-probe` links to the development UMD import library
and enters through `VioGpuDxvkOpenAdapter10_2ForTest`. It uses the exact
`D3D10_2DDI_ADAPTERFUNCS` and `D3D11DDI_DEVICEFUNCS` structures. It never calls
the source-linked `VioGpuDxvkCreateDdiTestDevice11` factory or a Microsoft
Direct3D device factory. Public `OpenAdapter10_2`, capability admission and
production device creation are unchanged.

The caller must provide exactly 16 hex LUID bytes and a new evidence directory.
There is no default adapter or automatic fallback. The transport opens that
LUID through `D3DKMTOpenAdapterFromLuid`, rejects `SoftwareDevice` and adapters
without `RenderSupported`, and checks the real 160-byte KMD identity reply
against that LUID. The existing UMD GPU backend independently requires valid
matching Vulkan device LUID and `VK_DRIVER_ID_MESA_TURNIP`; its selector is not
modified by this slice.

The callback bridge retains separate opaque adapter, device, core and context
cookies. It translates them to owned KMT handles only after checking the cookie
and DDI caller thread. Query, context creation/destruction, allocation,
lock/unlock, escape and render callbacks invoke their real KMT functions.
Only actual `STATUS_SUCCESS` becomes `S_OK`. MakeResident's actual
`STATUS_PENDING` requires a nonzero returned paging fence and a successful,
bounded five-second CPU wait before submission. Outstanding fence values remain
owned across callback failures. No callback fabricates a context, allocation,
mapping, completion fence, or successful render result.

The original D3D11 core callback table is a `DeviceSession` member and remains
alive through `DestroyDevice`, including failure cleanup. Its only used callback
is the live `pfnSetErrorCb`. An independently typed D3D10 core overload allows the separate graphics probe
to reuse this transport without any table cast. The kernel callback table supplies exactly the eight
callbacks the existing RuntimeGpu bridge needs. No presentation callback is
invented for this offscreen workload. Render replacement command/allocation/
patch buffers are copied from the real KMT outputs even when render fails.
Owned command/global BOs may remain CPU-mapped during residency and submission,
as the matched Turnip backend requires. Deallocation still requires Unlock.

Before device creation, eight malformed interface/build/flags/callback/device/
table cases must return their exact HRESULT, preserve both typed table outputs
and private storage, and invoke no additional identity query or kernel create.
Every table, mapped output and private allocation has byte-adjacent prefix and
suffix guards. Compile-time offset assertions cover the typed table/map guards
on x64 and x86 without relying on structure padding.

The workload creates a poison-initialized 1536-byte structured buffer with a
96-element `uint4` UAV. The original compiler receives this fixed HLSL as
`cs_5_0` with strictness and warnings as errors. The UMD receives its original
SHEX tokens and dispatches `(2,2,2)` groups of `(2,3,2)` threads. A typed staging
copy, Flush, blocking Map and Unmap precede an exact 384-word comparison.

For readback element `i`, set `x=i%4`, `y=(i/4)%6`, `z=i/24`:

| Word | Expected value |
| --- | --- |
| 0, SV_DispatchThreadID | `x + (y << 8) + (z << 16)` |
| 1, SV_GroupID | `x/2 + ((y/3) << 8) + ((z/2) << 16)` |
| 2, SV_GroupThreadID | `x%2 + ((y%3) << 8) + ((z%2) << 16)` |
| 3, SV_GroupIndex | `(z%2)*6 + (y%3)*2 + x%2` |

The fresh directory retains the original HLSL, compiler diagnostics if any,
DXBC, SHEX and raw 1536-byte readback. Stdout records each actual/expected word,
the selected LUID, loaded UMD path, individual KMT results and final owner counts.
Success additionally requires one balanced real context, positive balanced
allocation/lock/residency counts, actual render and completion escapes, intact
guards, no unexpected core error/wrong thread/bad cookie/malformed output, and
empty owner sets before raw cleanup. Raw fallback cleanup cannot satisfy that
ownership oracle. Bound resources are cleared before their typed Destroy DDIs;
device and adapter teardown precede raw paging/device/adapter destruction.

`umd-compute-oracle.cpp` independently simulates groups and local threads, checks
all 96 unique element indices, and flips every bit of all 384 words. Its exact
CPU marker is:

```
typed DX11 compute oracle PASS checks=12482 elements=96 words=384; CPU control only
```

Meson builds the hardware probe against the unchanged development UMD plus
`d3dcompiler` and `gdi32`; the CPU oracle has no UMD or graphics library.
`build-native-umd.ps1` explicitly builds both, retains both architectures and
checks the probe for forbidden Microsoft device-factory imports. Only the CPU
oracle is invoked through the original bounded fixture runner on non-ARM64 CI.
The hardware probe is never invoked by CI. The existing 27 bounded invocations,
runner function, C# raw process component, deadlines and build configuration
policy are preserved.

Local strict optimized official-SDK x64/x86 COFF checks cover the probe and CPU
oracle. GCC and Clang ASan/UBSan controls pass 12,482 checks each. Native ARM64
MSVC compile/link and actual hardware execution are separate pending evidence;
compilation or CPU controls do not demonstrate GPU execution.

| Future original CI hardware pin | Current value |
| --- | --- |
| Core source commit | `null` |
| Core SHA-256 | `null` |
| CI run | `null` |

An eventual hardware PASS is evidence for this development compute/UAV path.
Microsoft runtime activation, tessellation/GS/SO/class hardware acceptance,
presentation and complete D3D11 admission remain separate required gates.

The callback audit used the local primary WDK source under
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content`: the
`D3D11DDI_CORELAYER_DEVICECALLBACKS` live-function rule, `PFND3DDDI_RENDERCB` and
`D3DDDICB_RENDER` replacement-output contract, `D3DDDI_MAKERESIDENT` output count
and paging fence, and `D3DDDICB_LOCKFLAGS` renaming rules. No-Discard locks in
this workload reject unexpected renaming while retaining the returned owner
for failure cleanup.
