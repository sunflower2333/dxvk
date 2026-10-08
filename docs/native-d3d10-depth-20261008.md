# D3D10/10.1 depth-only and query draws

The historical `drawReady` path rejected a null pixel shader, absent color
targets, or cleared viewports. These are legal pipeline states: a null PS can
write interpolated depth, and output-free draws can still affect queries.
The input prerequisites now match the existing native11 path. Shared shader
preparation, final raster producer SV_Position, topology/index validation,
shader model limits, and ordinary-runtime admission gates remain in force.

Microsoft contracts checked against the local WDK documentation include
`ne-d3d10umddi-d3d10ddi_query.md` lines 64 and 272 (occlusion without RTVs and
zero NULL-PS invocations), `nc-d3d10umddi-pfnd3d10ddi_setviewports.md` (atomic
binding and clearing), and `nc-d3d10umddi-pfnd3d10ddi_setrendertargets.md` (NULL
bindings). Public [PSSetShader](https://learn.microsoft.com/en-us/windows/win32/api/d3d10/nf-d3d10-id3d10device-pssetshader)
accepts NULL. Microsoft’s [functional specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm),
sections 4.1.3–4.1.6, also permits NULL PS and absent output targets while
retaining observable upstream/query work.

`dxvk-umd-d3d10-depth-test` uses actual typed D3D10 and D3D10.1 DDIs with a
private WARP factory. Its independent reference creates a public
[D3D10CreateDevice1](https://learn.microsoft.com/en-us/windows/win32/api/d3d10_1/nf-d3d10_1-d3d10createdevice1)
WARP device at the corresponding exact feature level. Both paths use the
retained original FXC containers. The new `d3d10_1` import library is required.

Each SM4/4.1 profile runs six 16x16 cases:

| Case | Pipeline | Literal depth | Occlusion |
| --- | --- | --- | --- |
| 0 | NULL PS, DSV only | `0.25 + (2*x+1)/128` | 256 |
| 1 | SV_Depth PS, DSV only | 0.625 | 256 |
| 2 | Empty PS, DSV only | Interpolated plane | 256 |
| 3 | Discard-all PS, DSV only | Clear value 1 | 0 |
| 4 | NULL PS, cleared viewports | Clear value 1 | 0 |
| 5 | NULL PS, all outputs unbound | Unbound image stays 1 | 256 |

All cases must retain IA vertices=3, IA primitives=1 and positive VS
invocations; NULL PS must have zero PS invocations. Clipper/helper counts
are retained but do not require equality between independent runtime paths.
Depth-writing and empty PS must have positive PS invocations. An unconditional
discard contributes no surviving OM samples, so its PS count is retained
without imposing an unsupported positive lower bound.
No-output/viewport draws are forwarded to the backend rather than skipped.

The fixture creates exactly 65 originals in a fresh working directory: one
259-byte HLSL source, eight FXC DXBC/token pairs, 24 depth planes, and 24
ten-word query records. The independent reader validates original token
extent/stage/profile, literal depth at every pixel, query invariants, and full
directory closure. Put process receipts and JSON outside that raw directory:

```
python3 tests/verify-d3d10-depth-originals.py --directory <raw-directory> --output <outside-json>
```

Expected fixture stdout:

```
D3D10 depth PASS checks=<number> scenes=12 pixels=3072 words=3072 original_frames=24 queries=24 fxc_programs=8 hardware_admission=0
```

Local strict optimized x64/x86 COFF compilation passed for the final
production source and fixture. The reader’s 37 synthetic mutation controls
passed; these controls do not execute FXC or a Windows renderer. Existing
57-program shader, MRT and data-only SO fixtures were preserved byte-for-byte.
Native Windows execution, ordinary system-runtime routing, target hardware
pixels/queries, and admission of a replacement UMD remain pending. No
discovery, version, feature-level, registration or hardware gate is relaxed.
