# Native D3D11 stream-output declaration order

The original ARM64 WARP graphics fixture failed while creating its four-stream
geometry shader. Its native declarations selected streams 0, 1, 2, 3, followed
by a gap in stream 0, with independent output slots and strides 32, 16, 24, 16.
Compute, packed IA, and preceding graphics controls had already advanced.

Fresh diagnostic fixtures retained the original assertion and captured both
FXC and reconstructed bytecode. Both plain geometry shader creations succeeded.
The exact public `CreateGeometryShaderWithStreamOutput` calls rejected the
interleaved declarations for both bytecode versions with `E_INVALIDARG` and no
object. Stable grouping to 0, 0-gap, 1, 2, 3 succeeded for both versions with
the same slots, strides, masks, and non-rasterized selection. All four
single-stream original/rebuilt raw/grouped controls also succeeded.

`shader11StreamOutput` now stably groups its converted public entries by stream.
The order of values, split component masks, and gaps within each stream remains
unchanged. Validation and stride calculation still complete before publication.
No interface, feature-level, hardware-admission, or unsupported-operation gate
changes.

The portable fixture exercises all ten interleavings of two stream-local
declaration sequences, including signed x/z masks, split entries and gaps. It
checks every converted field and unchanged strides, and proves a malformed mask
does not publish partially converted entries or strides. GCC and Clang
ASan/UBSan both pass 945 checks. All six changed production/fixture x64/x86
official-header COFF checks pass; the final extra signed poison-buffer comparison
has its own two-fixture COFF recheck.

The native fixture retains all prior four-stream bytes, explicit stride poison,
per-stream statistics/overflow, aggregate predicate and rasterized pixel checks.
It adds an independent draw with original FXC shaders and grouped public
declarations, comparing all 1,024 captured bytes. Thirteen malformed creations
must report the live runtime error, preserve every private byte and its canary,
and preserve the exact previously bound geometry shader pointer. A signed
two-stream x/z capture uses discontiguous native groups and compares all 512
bytes against explicit negative values and untouched poison.

Evidence is retained under
`artifacts/dxvk-native-dx10-dx11-20261007/guest-warp-81fa39a-diagnostic-03`.
The original archive SHA256 is
`a5e660b8ccb67ece1ad33e24d14d8bf0a400e51e1c5283b8b58782b3ec0107aa`.
The independent join covers 88 source files, 21 headers, two copied API libraries,
19 ARM64 outputs, six owned process stages and all eleven retention controls.
Earlier failures and diagnostics remain separately retained, including the
host-only diagnostic02 parser-Flags conversion error.

Actual complete strict ARM64 WARP15 execution of the new frozen Git source is
pending at this source freeze. This slice descends from frozen f4bf37f plus the
three compute repairs; it does not claim native acceptance of unrelated changes
in the integration trunk. WARP validates this fixture backend only. Normal
production D3D11 admission remains closed pending required completeness and
actual hardware/runtime acceptance.

Primary API references:

- [CreateGeometryShaderWithStreamOutput](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-creategeometryshaderwithstreamoutput)
- [D3D11_SO_DECLARATION_ENTRY](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_so_declaration_entry)
- [Multiple output streams](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/overviews-direct3d-11-hlsl-gs-index)

The grouping requirement above is established by retained actual WARP calls;
the structure documentation alone did not expose the rejected declaration order.
