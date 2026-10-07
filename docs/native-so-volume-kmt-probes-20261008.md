# Typed D3D11 SO and Texture3D hardware probes

The two development probes use the existing
`VioGpuDxvkOpenAdapter10_2ForTest` export and an exact typed D3D11 table.
They open the supplied real adapter LUID through KMT, require render support
and a nonsoftware adapter, and pass the original live core callback table.
The runtime version is `0x00020009`, so the original SDK macro selects the
DXGI1.1 table. No public Microsoft device factory is linked or called.
Production capabilities and ordinary runtime/presentation admission are
unchanged.

`dxvk-umd-d3d11-so-probe <LUID bytes> <fresh originals directory>` creates
original SM5 VS/GS code with the system compiler and reflects its signatures.
It captures three input points through four GS streams. A discontiguous
stream0 gap, four strides and poisoned tails are checked across two draws
with different first-vertex values. Each stream's statistics and overflow
predicate, plus the aggregate predicate, are read before and after a tiny
stream2 target overflows. Eight malformed declarations must report exactly
one `E_INVALIDARG` callback, preserve private output bytes and guards, and
leave the bound shader usable. A signature-only NULL GS then captures VS
A/B/A, including NaN encodings, signed zero and integer payloads. All
704 buffer words are checked as raw bits and retained with the query bytes.

`dxvk-umd-d3d11-volume-probe <LUID bytes> <fresh originals directory>` uses
the accepted volume reference's logical dimensions, separate physical
padding, independent row/slice padding and coordinate colors. Nine full
readbacks check 3,046 voxels after immutable initialization, boxed
three-slice upload/copy, three dynamic discard mappings, selected/remaining
W-slice clears and three scoped mip-generation views. Four invalid copy,
upload and map operations must produce exactly one named `E_INVALIDARG`
callback; later entire-resource reads prove failed transfers are atomic.
The invalid map's zero output and guards are retained. An original SM5
compute shader samples every selected SRV-relative mip through `Texture3D.Load`
and compares another 551 words. Mip generation uses a constant selected
source mip, while independent base and tail colors detect out-of-view writes.

Every resource, query, shader and view owns guarded private storage. The
session destroys them before the device and adapter and requires balanced
kernel allocations, mappings, residency, context and paging ownership before
raw KMT cleanup. The old compute transport's `balanced()` gate retains its
zero-error default. The new optional expected-error count is supplied only
after the graphics session has separately admitted each exact named negative
or pending-query callback. Positive calls and cleanup require the raw total
to equal that admitted total. Raw callback events, counts and HRESULTs are
never reset or suppressed.

`umd-so-oracle.cpp` checks the independent flat-record capture oracle,
20,480 single-bit mutations and 55 callback accounting controls (missing,
multiple, overwritten, successful and overflowing callbacks).
`umd-volume-probe-oracle.cpp` checks an independent flat-coordinate oracle,
all 97,472 single-bit mutations and truncated/extended images. Expected
markers are:

```
SO capture oracle verified checks=21083 bit_flips=20480 streams=4 raw_bits=1 callback_controls=55
volume probe oracle verified checks=100632 voxels=3046 bit_flips=97472 sampled=551
```

`verify-d3d11-kmt-graphics-originals.py` independently reconstructs the
expected bytes, joins SHEX and reflected signature records to the original
DXBC containers, checks all raw query bytes, and joins the per-operation
callback trace to the unchanged kernel counts. For example:

```
python tests/verify-d3d11-kmt-graphics-originals.py so --originals SO-originals --stdout SO.stdout.raw --source-root frozen-source --luid ec6b000000000000 --output SO-verified.json
python tests/verify-d3d11-kmt-graphics-originals.py volume --originals volume-originals --stdout volume.stdout.raw --source-root frozen-source --luid ec6b000000000000 --output volume-verified.json
```

That reader proves raw observations only. A hardware gate must separately
join the original successful CI core/probe/PDB, source and import policy,
matched Mesa/loader/System32 payloads, actual KMT private identity, limited
USER session/token, bounded native child handle/exit/raw pipes, all input
hashes, static registration configuration and final task/process cleanup.
Previous d7 compute or CPU module-load evidence cannot be relabeled as these
new probes or a newer core.

Meson and the native build script build/package both probes and both CPU
oracles. Probe images use the existing architecture and forbidden Microsoft
runtime import checks. CI runs only the CPU oracles on x86/x64 through the
unchanged bounded runner; it never invokes either hardware probe.

Local optimized original-SDK x64/x86 COFF and GCC/Clang ASan/UBSan controls
are required before source freeze. Native ARM64 compilation/linking, fresh
core CPU module feasibility, successful original CI artifacts and limited
USER hardware execution remain distinct pending gates.

The SDK ABI follows local `d3d10umddi.h` from the original Windows26100
WDK. Resource, stream-output declaration, map, query and mip semantics are
documented in the local first-party Windows driver documentation checkout,
including `D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT`,
`D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY`,
`D3D10DDIARG_TEX3D_SHADERRESOURCEVIEW` and
`D3D10DDIARG_TEX3D_RENDERTARGETVIEW`.
