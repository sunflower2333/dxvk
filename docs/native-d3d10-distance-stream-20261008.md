# Clip/cull-distance stream-output capture

The historical SO helpers rejected clip/cull system values and rejected
separate signature rows sharing a register. A later clip row also needs its
full-union ordinal, even when the earlier row is not captured. SO declarations
now resolve each requested register lane to its unique signature owner,
split runs at semantic boundaries, and preserve the clip/cull ordinal.
Null-GS metadata allows disjoint packed rows and retains the established
eight-component/two-register distance limits.

Microsoft’s local `D3D10DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY` contract
describes capture by register and xyzw mask. The local `Stream Output Stage`
document describes capture before rasterization and permits a null GS.
[Public D3D10 creation](https://learn.microsoft.com/en-us/windows/win32/api/d3d10/nf-d3d10-id3d10device-creategeometryshaderwithstreamoutput)
accepts prior VS bytecode for signature-only capture. Microsoft’s
[functional specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm),
sections 14 and 15.4.3, permits selecting an output subset before clipping,
requires unaltered 32-bit capture, and defines combined clip/cull limits.

The native register lane differs from a public SO semantic component.
DXVK’s `LowerIoPass::emitXfbForOutput` adds the semantic’s first register
component to the public component mask. Translation therefore subtracts
that first lane. Both historical and shared shader11 SO conversion now do
this, avoiding a second offset for cull components packed into zw.
Both null-GS creation paths enrich the metadata with full runtime-union
clip/cull ordinals before resolving the declarations.

The new native fixture uses genuine D3D10/10.1 DDIs with a private WARP
factory and a separate public D3D10CreateDevice1 WARP reference at the exact
logical feature level. Six retained original FXC programs produce Clip0
float4, Clip1 float2, Cull0 float2 and a raw uint payload. Clip1 and Cull0
share one register; only the later Clip1 row is captured. Negative Clip0
values also confirm capture occurs before clipping. It exercises real GS
and null-GS capture, full and sparse masks, and a prior-VS rebind through
the same SO object. Gap, stride padding and untouched buffer tails must
retain `0xcccccccc`. Negative zero, negative cull values and NaN-like uint
bits must survive exactly.

The fixture retains 77 originals in a fresh raw working directory: one
767-byte HLSL source, six DXBC/token pairs, 32 full 256-byte buffers and
32 six-word query records. Query words are IA vertices, IA primitives,
PS invocations, device-removed HRESULT, SO primitives written and needed.
Their literal result is `(3,1,0,0,1,1)`. The reader independently checks all
2048 captured/sentinel words, 192 query words, token stage/profile/extent,
the fixed HLSL hash and exact full-directory closure. Keep process receipts
and reader JSON outside that raw directory:

```
python3 tests/verify-d3d10-distance-stream-originals.py --directory <raw-directory> --output <outside-json>
```

Expected stdout markers:

```
distance stream policy PASS checks=108 packed_semantics=1 full_union_ordinal=1 hardware_admission=0
D3D10 distance SO PASS checks=<number> scenes=16 words=1024 original_buffers=32 query_frames=32 fxc_programs=6 hardware_admission=0
```

Local final evidence comprises eight strict optimized official-header
x64/x86 COFFs, GCC O2 and Clang ASan/UBSan 108-policy controls, unchanged
1118-SM5 controls, and 33 synthetic reader controls. The SM5 malformed-input
cases retain their expected 82-byte diagnostics. An initial native fixture
compile exposed an existing `StreamOutput` enum/type include collision;
the unused contract include was removed and fresh compilation passed.
These are source/compiler/reader controls, with no Windows renderer run.
The frozen depth packet and existing shader/MRT/SO fixtures remain unchanged.
Hardware/system-runtime registration, transport, remaining GS system-value
capture beyond this scoped distance fix, and complete feature-level semantics
remain separate acceptance work. No admission or advertising gate changes.
