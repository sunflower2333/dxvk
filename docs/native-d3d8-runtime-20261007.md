# Genuine Windows D3D8 compatibility preparation

This slice adds a bounded DX8 caps projection and a genuine system-runtime
probe. It does not open ordinary production `OpenAdapter`, accept Interface8,
install an app-local D3D8 DLL, or claim hardware acceptance. The existing
private DXVK D3D8 COM wrapper delegates to D3D9; it is not used by this probe.

## Original runtime and ABI evidence

The retained target inventory in
`artifacts/dxvk-native-d3d8-runtime-20261007/root-system-d3d8-inventory-verified-01.json`
finds no native `System32\d3d8.dll`. The original `SysWOW64\d3d8.dll` is
I386, 737280 bytes, version 10.0.26100.1, SHA256
`65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8`.
It imports `d3d8thk.dll` and GDI32 KMT directly; it does not import D3D9.

Offline inspection of those exact original bytes confirms the UMD call:

| Runtime RVA | Original operation |
| --- | --- |
| `0x12c41` | Push literal `OpenAdapter` name, then resolve its entry |
| `0x12e42` | `c78544f5ffff08000000`: write literal `Interface=8` at argument offset4 |
| `0x12e73` | `ffd7`: call resolved entry with that argument |

The runtime iterates opaque runtime build versions; those are distinct from
Interface and returned DriverVersion. Microsoft documents the literal API
version in [D3DDDIARG_OPENADAPTER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_openadapter).
The local official DDI docs and
[D3DDDICAPS_TYPE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ne-d3dumddi-_d3dddicaps_type)
define `GETD3D8CAPS=12`, returning `D3DCAPS8`, separately from
`GETD3D9CAPS=13`. Actual D3D8 caps are 53 DWORDs / 212 bytes. The Microsoft
SDK's D3D9 `DevCaps2` starts exactly at offset212; both structures and the
shader-field offsets are checked by compiled assertions.

Official Microsoft public symbols were also retained at
`artifacts/dxvk-native-d3d8-port-20261007/microsoft-system-d3d8.pdb`, SHA256
`41886f83699262f7982b80f51b842ca39aac1555fc977fe875c35f5847ab75ac`, from
the [Microsoft symbol server](https://msdl.microsoft.com/download/symbols/d3d8.pdb/9FBCBAA02599976AB94436E5E85E4A081/d3d8.pdb).
The stripped public PDB has the same GUID and agreeing export/section RVAs,
but internal age3 versus the original DLL CodeView age1. No internal-age
identity is claimed. The Interface8 conclusion comes from original machine
code and does not depend on that age difference.

## Implemented contracts

`umd_d3d8_compat.{h,cpp}` snapshots the proven shared prefix, requires an exact
212-byte output, preserves native resource limits, removes D3D9-only caps,
and bounds vertex shaders to1.1 / 96 constants and pixel shaders to1.4.
Malformed shader tags and sizes fail before output publication; aliased
output is supported. Unadvertised cube/volume/gamma/autogen/MSAA/managed
resources remain unadvertised. The projection does not add capability bits.
The four existing native format rows remain X8R8G8B8/A8R8G8B8 2D textures
and render targets, plus D16/D24S8 depth; no format widening is proposed.
Microsoft's [D3DCAPS8 ABI](https://microsoft.github.io/windows-docs-rs/doc/windows/Wdk/Graphics/Direct3D/struct.D3DCAPS8.html)
is the reference for that bounded output shape.

The probe supports these explicit modes:

* `--enumerate`: load the absolute Windows system D3D8 module, call
  `Direct3DCreate8(220)`, adapter identifiers and HAL caps only. Zero adapters
  is reported without a device/admission PASS claim.
* `--front-enumerate <owned-frontend> <exact-installed-DX9-name>`: temporarily
  select an owned read-only frontend only for the genuine D3D8 module's exact
  GDI32 `D3DKMTQueryAdapterInfo` import. The PE32/PE32+ parser bounds every
  descriptor, name and pointer; it never reinterprets x86 slots as64-bit.
  Every unmatched query and original status passes through. There are no
  registry writes. The frontend must be an independently built x86 DLL under
  `C:\Users\Public\DxvkD3D8Runtime-*`, with permission
  `read-only-legacy-fog-478eca2`; existing ARM64 frontend/core paths do not
  satisfy that x86 gate. The selector is restored before normal exit.
* `--offscreen`: require the actual VirtIO1af4/1050 HAL and hardware VP/raster
  caps, with no WARP/REF fallback. Check only the four advertised format
  semantics; create an owned hidden window/device, render6 stages, log every
  pixel, then release/reset and destroy owned resources. Tests use native
  D3D8 FVF, declaration+VS1.1/PS1.1 handles, dynamic2D locking and D3D8
  `CopyRects` to an image surface. There is no Present or display claim.

The independent host oracle requires all384 original pixels, exact API
counts and x86 system-path/VirtIO identity; checksum `a9dc9545`. Its synthetic
selfcheck is verifier testing, not runtime evidence. SM1.1 execution does
not establish every SM1.4 instruction or a broad DX8 game compatibility claim.

## Reserved integration proposal and remaining gate

The separate
`.planning/dx8-native-port-20261007/dx8-caps-contract-proposal.patch`
is based on c0ea296 and proposes only GETD3D8CAPS sizing/projection plus
meaningful adapter canary/size/snapshot tests and link inputs. Reserved files
were not edited. It leaves Interface8 and ordinary admission closed. Root
should rebase that proposal onto its current adapter after the actual DX9
lifecycle fix, then freeze a new typed fixture packet.

A later Interface8 step needs an immutable API-version field in adapter
identity, exact adapter/core CreateDevice version agreement, reserved-bit
checks and both99-pointer Vista table guard tests. Snapshot caller Interface
before identity callbacks can mutate it. Thread that API into backend
construction and set the private `D3D9InterfaceEx` D3D8 compatibility flag
before device construction, following the existing private DXVK wrapper's
compatibility semantics. Test stale handles, mismatched8↔9 adapter/device,
failure nonpublication and8-specific declarations/texture-stage state.
Do not merely replace `Interface != 9` with an unconditional8-or9 check.

Real system8 execution on this ARM64 Windows requires the x86 ABI throughout:
frontend, embedded UMD/core, Vulkan loader and Mesa ICD, plus the actual WoW
driver-name query/binding. An ARM64 payload cannot fulfill an I386 runtime
request. Trace genuine Interface8/caps12 negotiation first, then establish
HAL CreateDevice, resource callbacks, exact pixels/reset and finally native
presentation. The ordinary production export stays closed through these
development checks. No second VM, API-DLL substitution or registry binding
change is part of this slice.

## Verification and frozen native build

GCC and Clang ASan/UBSan positives pass543 caps checks and4237 import checks
when the exact original Microsoft image is included. Both exact restored
SM2-cap negative controls compile then exit1 at the intended caps assertion.
The parser finds the original I386 KMT IAT slot at RVA`b11d4`. Strict local
MinGW Clang builds produce15 COFF objects and9 PEs across x86/x64/ARM64;
original SDK D3D9 and KMT layout assertions compile. No Windows PE was run.
Original local failure evidence from the unmodified SDK's nested comments
is retained; vendor headers are system includes while source warnings remain
fatal. Proof: `artifacts/dxvk-native-d3d8-port-20261007/local-verification-02/verified.json`.

`prepare-native-d3d8-packet.py` freezes10 exact Git inputs plus3 verbatim
Wine/MinGW legacy D3D8 headers, retaining their original license/provenance.
Modern Microsoft SDKs do not include those legacy API headers. The prepared
`build-native-d3d8-cpu.ps1` requires an explicit native EWDK compiler root,
checks HostARM64/x86 component readability (including clui), builds5 original
COFFs/3 x86 PEs under `/W4 /WX /MT`, and executes only CPU fixtures and7
malformed CLI guards. It records SDK/CRT/library hashes and original
SYS/service/registration/System32/SysWOW64/candidate/desktop continuity.
It does not execute enumeration, selector or offscreen mode. Its native
PowerShell parse and MSVC execution are pending target ownership release;
no native EWDK success is claimed by the local cross builds.
