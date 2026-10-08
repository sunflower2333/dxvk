# D3D10/10.1 shared decoded shader integration

The typed historical VS/GS/PS callbacks now use the decoded shader representation
and container/linkage preparation already used by the native D3D11 tables. The
previous D3D10 reconstruction rejected InstanceID, FrontFace, PrimitiveID,
clip/cull distances, typed integer color outputs and dedicated depth outputs.
The new path retains the actual instruction stream and derives real register
usage from its declarations. Runtime signature unions supply clip/cull
ordinals, including earlier unused rows, in both ordinary and GS-with-SO
creation. Bound input-layout and render-target formats determine generic
input/output scalar types before draw compilation.

Historical tables still accept exactly SM4.0, or SM4.1 on logical10.1 devices.
The private renderer's broader feature level does not grant SM5, interfaces,
tessellation, compute, newer register types or generated system values. The
profile retains the original reserved-opcode/custom-data/SampleIndex rules,
pixel-only SamplePos/LOD, 4.1-only coverage output/sample interpolation, eight
color targets, and eight clip/cull components in two registers. Ordinary GS
creation and the final active raster producer still require full SV_Position;
VS feeding GS and no-raster SO remain position optional.

The legacy SO declaration and stride resolver is retained, and converted
declarations use the shared preparation with no rasterized stream. Direct
clip/cull capture and null-code SO with such union rows remain outside that
resolver's accepted scope. The existing data-only SO, cached-VS raster
rejection and positioned-GS controls remain unchanged.

Production discovery/version/feature/threading gates and all requirement
masks remain unchanged: D3D10 is0x17f and D3D10.1 is0x37f. Ordinary D3D10 draw
prerequisites still require a PS, color target and viewport outside SO, so
depth-only/null-PS pipeline integration remains another production gap. This
change cannot by itself replace the installed hardware runtime or Mesa.

## Controls and local validation

`dxvk-umd-shader10-profile-test` checks223 portable controls: retained original
tokens/hash, legal stage/system-value profiles, models/registers/masks,
clip/cull limits, typed/depth/coverage signatures, reserved flags/opcodes,
generated declarations, custom data and SM4.1 stage limits. The legal-profile
matrix intentionally isolates metadata policy; it does not substitute for a
renderer bytecode validator. Final GCC O2 and Clang ASan/UBSan executions pass
with no diagnostics.

`dxvk-umd-d3d10-system-shader-test` invokes actual typed10.0/10.1 creation,
binding and draws with a WARP factory substitution. Four scenes cover both
models with/without GS, four instances, clip half-plane and quarter bands,
cull-distance passage, integer MRTs, primitive IDs, front-face winding toggle
and dedicated depth. Every native readback is compared against the original
FXC container through the public shader API and fixed integer/depth values.
Malformed shader creates retain the prior binding and private-storage guards.
This fixture has been strictly compiled locally; native execution is pending.

The fixture retains49 `system-*` originals: one exact HLSL source, twelve
FXC DXBC/token pairs and24 native/public planes. Run the independent reader
against that fixture's fresh working directory:

```
python tests/verify-d3d10-system-shader-originals.py --directory RAW_DIRECTORY --output FRESH_RESULT.json
```

It checks all18432 observed words (9216 per role), literal clipping/instance/
primitive/integer/depth values, uniform/inverted front-face output, exact
container/token joins, source identity and file closure. It records file
hashes but requires the caller to join actual compiler/process originals.
Its15 synthetic mutation controls pass and explicitly claim no compiler or
native execution. Synthetic originals are removed after the selftest.

Final production/new fixture/profile optimized x64/x86 compilations pass with
original SDK/WDK headers, strict first-party warnings and LLVM machine-header
reopens. The receipts under artifacts/shader10-final-local-04 and -05 retain
actual starts, exit/reap/closed raw-output observations, selected source/API
header/tool pins and before/after comparisons. Portable dependencies are
reused from pinned prior objects. This is a selected-input local check, not a
complete toolchain attestation. The first final recipe attempt is retained
with its erroneous two-input compiler command; corrected commands pass.

CI must execute both new fixtures alongside the existing57-FXC and all SO
oracles. Real embedded-backend, ordinary system-runtime and target hardware
pixel/presentation/state evidence remain separate requirements. This worker
performed no MAIN, target, registration or deployment operations.

## Microsoft contracts

Local wdk-ddi-src/content/d3d10umddi/
ns-d3d10umddi-d3d10ddiarg_stage_io_signatures.md permits actual declaration
filtering from a full runtime union; its signature-entry document defines the
historical register/mask/system-name fields. Original26100 WDK token headers
provide the SM4/4.1 operand/opcode and reserved-global-flag encodings.

Microsoft's [system-value semantics](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)
describe stage usage, clip/cull limits and 4.1 coverage output. The
[SamplePos](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/samplepos--sm4-1---asm-)
and [LOD](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/lod--sm4-1---asm-)
instructions remain pixel-only; [SampleInfo](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sampleinfo--sm4-1---asm-)
and [Gather4](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/gather4--sm4-1---asm-)
support the three historical stages. Derivative-based
[Sample](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sample--sm4---asm-),
[SampleC](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sample-c--sm4---asm-)
and [SampleB](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sample-b--sm4---asm-)
remain pixel-only; [SampleD](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sample-d--sm4---asm-)
uses supplied gradients and supports all three.
