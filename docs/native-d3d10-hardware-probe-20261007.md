# Typed D3D10.0/10.1 probe over real KMT

This separate probe enters the explicitly supplied core DLL through
`VioGpuDxvkOpenAdapter10_2ForTest`. It supplies real KMT callbacks and exercises
the exact `D3D10DDI_DEVICEFUNCS` or `D3D10_1DDI_DEVICEFUNCS` table. Production
admission, public exports, installed driver selection, CI, and the completed
57-program SM4.1 fixture are unchanged. Native compilation and hardware
execution are pending; local controls establish the oracle and ABI only.

The shared `tests/umd-kmt-compute-transport.h` is an unchanged dependency from
DX11 commit `eb3a9d824ac55a702e7b789de49ed162d661d9a6`, SHA-256
`33bec601eb5148385cb515fb002b7d9df50a9b58832140cbcb97c4968fde7bb7`.
It must be integrated from its owner; this source slice does not duplicate or
modify it. The frozen packet records the separate Git origin of that header.

The invocation requires an absolute core DLL path, its exact SHA-256, 16 hex
digits representing the selected LUID's eight bytes, profile `10_0` or `10_1`,
and a fresh absolute output directory. There is no default adapter. Malformed
arguments return 2 before DLL loading or KMT access. The core path, loaded
module path and before/after hashes must agree. The probe loads only the exact
named development export; it never invokes a Microsoft graphics device
factory, WARP, or a source-linked test factory.

The transport opens the supplied nonzero LUID, rejects software devices and
adapters without rendering support, and joins the real 160-byte private KMD
identity to that LUID. WDDM 2.0 or later and a real paging queue are required.
Stable opaque runtime adapter/device/core/context cookies remain distinct
from private raw KMT handles. Adapter, kernel and D3D10 core callback tables
are device-session members and survive every child and device teardown.
The transport checks cookie identity and the DDI caller thread before
forwarding actual QueryAdapterInfo, context, allocation, lock, escape and
render operations. Pending residency requires its real fence and bounded
five-second wait. The transport's original replacement-render outputs and
failed ownership state remain available for failure cleanup.

The modern adapter returns `D3D10_2DDI_ADAPTERFUNCS`. The probe requires the
selected exact 64-bit supported-version value before calculating device
storage or creating the device. It also requires zero production pipeline
caps, preserving the closed admission gate. Microsoft documents modern
version discovery separately from the later device's negotiated interface;
the two adapter exports share the original OpenAdapter ABI.
[Version discovery support](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/version-discovery-support).

| Profile | Interface | Version | Device table | DXGI table |
| --- | --- | --- | --- | --- |
| 10.0 | `D3D10_0_DDI_INTERFACE_VERSION` (`0x000A0001`) | `0x00040000` | `D3D10DDI_DEVICEFUNCS` | `DXGI_DDI_BASE_FUNCTIONS` |
| 10.1 | `D3D10_1_DDI_INTERFACE_VERSION` (`0x000A0002`) | `0x00010000` | `D3D10_1DDI_DEVICEFUNCS` | `DXGI_DDI_BASE_FUNCTIONS` |

These values come from the original WDK supported-version macros. Each has
low revision zero, so the WDK `IS_DXGI1_1_BASE_FUNCTIONS` macro selects DXGI
1.0. Static assertions cover both selected values and both sides of the
legacy DXGI 1.1 threshold (`VISTA_GOLD_PRODUCT_VER |
DXGI_RESOLVE_SHARED_RESOURCE`, revision 6009). Device, DXGI, adapter and mapped
outputs have adjacent prefix/suffix guards with compile-time offset checks.
The probe never casts between interface tables. Microsoft's version rules
distinguish interface minor versions, upper-word build numbers and low-word
runtime revisions.
[D3D10.1 version numbers](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/version-numbers-for-direct3d-version-10-1).

The shader compiler is the exact System32 `d3dcompiler_47.dll` with original
SHA-256 `4b68cd1fc3d482d0965910f1995513549989ae17a53d9be122de08437102a1d1`.
Only its typed `D3DCompile` and `D3DReflect` functions are obtained dynamically.
Each actual compile uses strictness and warnings as errors. The fresh directory
retains the original HLSL, diagnostics if any, DXBC, bounded original SHDR
tokens and raw readback images. Reflection supplies owned writable native
signature entries; token version and container bounds are checked before
the typed shader creation calls.

All images are 16 by 16 `R8G8B8A8_UNORM`. The independently specified oracle
checks every pixel and every channel. A typed copy to staging, Flush, blocking
Map, bounded row-pitch copy and Unmap precede saving and checking each image.

| Profile | Workload | Expected image | Draws | Checked pixels |
| --- | --- | --- | --- | --- |
| Both | Clear and fullscreen `SV_VertexID` VS/PS draw | Green then red, exact RGBA bytes | 1 | 512 |
| 10.1 extra | Gather the four `R32_FLOAT` texels `{0,1,1,0}` | Blue, exact RGBA bytes | 1 | 256 |
| 10.1 extra | Four-sample clear and resolve | Green, exact RGBA bytes | 0 | 256 |
| 10.1 extra | Four-sample `SV_SampleIndex` draw and resolve | Red 127 or 128; green/alpha 255, blue 0 | 1 | 256 |

The final hardware markers must be exactly:

```
D3D10_KMT_PASS profile=10_0 draws=1 pixels=512 hr=00000000 ordinary_runtime_admission=0
D3D10_KMT_PASS profile=10_1 draws=3 pixels=1280 hr=00000000 ordinary_runtime_admission=0
```

Bound state is cleared before reverse typed child destruction. Views are
destroyed before their resources. Device destruction and CloseAdapter precede
the transport balance check and raw paging/device/adapter cleanup. Success
requires intact guards, one paired real context, positive paired allocation,
lock and residency counts, real render/completion escapes, no wrong thread,
bad cookie, malformed output or core error, and no remaining owners before
raw cleanup. Fallback cleanup cannot satisfy the ownership oracle. Failed
draw and image counts are reported as actual partial progress.

`umd-d3d10-hardware-oracle.cpp` checks narrow profile/digest parsing and flips
each channel in every pixel of all four independently specified scenes.
Its CPU-only marker is:

```
DX10 hardware oracle verified checks=4141 images=4 pixels=256 hardware_execution=0
```

The standalone native probe compiles only
`tests/umd-d3d10-hardware-probe.cpp` and links original ARM64 static CRT,
`kernel32.lib`, `gdi32.lib`, and `bcrypt.lib`. It requires no UMD import library
or Direct3D graphics library. The oracle compiles only its own translation
unit and links static CRT plus `kernel32.lib`. First-party compile policy is
`/W4 /WX /MT /O2 /std:c++17 /EHsc /Zc:preprocessor`, with original SDK headers
marked external. There is no parser dependency compilation in this probe
packet. Local optimized x64/x86 COFF checks use original Microsoft headers;
GCC and Clang sanitizer controls exercise the portable oracle. Neither is
native ARM64 execution or hardware evidence. Future owned native children
must use the existing bounded raw-process runner and a 60-second workload
deadline, retaining actual exit and pipe-drain results even on failure.

| Future hardware input | Current value |
| --- | --- |
| Successful consolidated core source | `null` |
| Successful consolidated CI run | `null` |
| Original core SHA-256 | `null` |
| Target ownership receipt | `null` |

Ordinary Microsoft runtime activation remains a separate next gate. Windows
loads the installed UMD and supplies runtime-owned adapter/device/core cookies,
kernel callbacks and DXGI arguments.
[D3D10 DDI initialization](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/initializing-communication-with-the-direct3d-version-10-ddi).
The installed package must name the D3D10 UMD in the documented driver slot
for the required process architecture.
[Enabling the D3D10 DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/enabling-support-for-the-direct3d-version-10-ddi).

After honest mandatory interface/callback/format/resource/shader requirements
are complete and reviewed, that gate needs a normal system `d3d10.dll` and
`d3d10_1.dll` process selecting the same hardware LUID, exact loaded frontend
and core hashes, native 10.0/10.1 creation, shader/readback acceptance, and
runtime-owned primary/backbuffer/DXGI presentation and teardown evidence.
The `D3D10CreateDevice1` hardware entry accepts an explicit DXGI adapter and
10.0 or 10.1 hardware level.
[D3D10CreateDevice1](https://learn.microsoft.com/en-us/windows/win32/api/d3d10_1/nf-d3d10_1-d3d10createdevice1).
This development KMT probe leaves those production gates closed.
