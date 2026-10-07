# Genuine Windows D3D8 compatibility preparation

This branch adds a bounded DX8 caps projection, an immutable typed Interface8/9
development bridge and a genuine system-runtime probe. Ordinary production
`OpenAdapter` remains absent and all admission gates stay unchanged. It does
not install an app-local D3D8 DLL or claim hardware acceptance. The existing
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

## Immutable typed Interface8/9 bridge

The second slice, based on frozen b61f8a3, makes `AdapterIdentity::legacyApi`
const, defaults existing direct-core callers to9 and snapshots Open/Create
metadata before runtime callbacks. An8 adapter creates only an8 device; a9
adapter creates only a9 device. Reserved Create flags remain rejected and
selected callback functions/output destinations are copied before caller
storage can change. Both adapters publish only the99-pointer Vista table.

GETD3D8CAPS12 is available only on8, with the exact212-byte projection above;
GETD3D9CAPS13 remains9-only. Sizes, query result, LUID/generation and close
state are checked before publishing output. Unsupported/mismatched cap types
and malformed sizes do not enter identity callbacks. Existing9 capability
values and every pre-existing Interface9 fixture remain in place.

The API identity is also passed into real `D3D9Backend::create`. API8
constructs its private renderer parent with D3D9Ex disabled and applies
DXVK's D3D8 compatibility flag before `D3D9DeviceEx` copies that flag set.
The private constructor's default remains extended=true for9. This follows
the existing DXVK8 wrapper's non-Ex parent, without entering that public
wrapper. D3D8-specific defaults/resource behavior therefore reach the actual
renderer. Its SM1-3 compiler already handles SM1.1 TEX and SM1.4 TEXLD,
including their different operand layouts. No shader bytecode translation to
a capability-only placeholder is needed.

The typed8 bridge additionally bounds VS/PS models to1.1/1.4, VS/PS float
constants to96/8, legacy declaration types and8 texture stages. It rejects
integer/bool constants and9-only texture-state/sampler/declaration requests.
The real Microsoft runtime is responsible for converting its public FVF and
declaration tokens into the common typed DDI; the new controlled fixture
exercises the resulting POSITIONT/COLOR declaration and exact vertex bytes.
It cannot establish that the genuine runtime issued those calls. That stays
an actual x86 runtime gate.

New tests cover caller metadata and callback storage mutation,8↔9 mismatches,
invalid identity-query replies, callback failures, all reserved flag bits,
CAPS12 canaries/boundaries, reentry/close/nonpublication, SM1.x bytecode
forwarding and legacy state limits. Guard-page table tests now run for both
API identities through adapter and direct-core creation. They have locally
compiled and linked; native Windows execution is still pending.

`tests/umd-d3d8-sm1.cpp` actually converts five valid VS1.1/PS1.1/PS1.4
programs using the pinned DXVK compiler. It checks stage, output stores and
real sampling IR. The converter assumes a validated header; malformed
versions are rejected by ShaderInfo and the typed DDI before conversion.
An initial direct malformed-header converter experiment asserted during
finalization; its originals are retained rather than being counted as a
production failure. GCC and Clang ASan/UBSan pass47 converter checks and2800
API-bound checks; four exact reverted-bound/mismatch controls compile then
exit1 at the intended assertion. These are CPU compiler/contract checks,
not shader hardware execution.

Meson provides `dxvk-umd-legacy-api-test` and `dxvk-umd-d3d8-sm1-test`; future
CI integration must explicitly build/run/retain them. Ordinary OpenAdapter
and production gate/export sources have not changed. The exact implemented
caps projection does not add the legacy FOGANDSPECULARALPHA bit when it was
absent from the9 source profile; genuine8 runtime admission needs its own
trace, rather than assuming the Microsoft9 capability gate is identical.

## Earlier proposal and remaining runtime gate

The separate
`.planning/dx8-native-port-20261007/dx8-caps-contract-proposal.patch`
is the preserved first-slice c0ea296 caps-only proposal. At frozen b61f8a3,
reserved adapter/device files had not been edited and Interface8 was closed.
The second slice above supersedes that proposal with root-authorized isolated
implementation. Root owns merging it onto newer9 fixes, preserving those
fixes and freezing native verification inputs before any runtime action.

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
`build-native-d3d8-cpu.ps1` in the immutable b61 packet requires an explicit EWDK compiler root,
checks HostARM64/x86 component readability (including clui), builds5 original
COFFs/3 x86 PEs under `/W4 /WX /MT`, and executes only CPU fixtures and7
malformed CLI guards. It records SDK/CRT/library hashes and original
SYS/service/registration/System32/SysWOW64/candidate/desktop continuity.
It does not execute enumeration, selector or offscreen mode. Its native
PowerShell parse and MSVC execution are pending target ownership release;
no native EWDK success is claimed by the local cross builds.

The second-slice actual private parent/backend and SM1 regression also
cross-compile on all three architectures using the existing Meson warning
policy; typed adapter/device units retain unsuppressed source Werror.
Native typed fixture execution, private-core linking and real8 compatibility
flags on hardware remain pending. For a later x86 EWDK build, an official
Hostx64/x86 compiler running under ARM64 emulation is valid when native
Hostarm64/x86 is unavailable; record the actual host/toolchain identity.
The previously frozen b61 packet and its original helper are preserved.

Final second-slice proof is
`artifacts/dxvk-native-d3d8-port-20261007/bridge-local-verification-05/verified.json`:
33 original COFFs,6 fixture PEs,50 primary before/after source hashes and242
compiler dependency hashes. The typed fixtures use Werror/Wshadow; actual
private renderer units use the existing Meson warning policy. Original GCC
and Clang sanitizer converter runs retain15 objects each and47 checks; API
bounds run2800 checks each, with four exact compile-success/assertion-failure
negative controls. These are local CPU/cross proofs, not Windows execution.
