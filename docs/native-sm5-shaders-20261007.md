# Native SM5 graphics and interface reconstruction

`umd_shader11.cpp` decodes native raw SM4.0/4.1/5.0 tokens with the pinned
dxbc-spirv parser, bounds instructions before parsing, and retains original
tokens unchanged. The new descriptor includes graphics inputs/outputs,
geometry stream IDs, hull/domain patch constants and interface table lists.
Names are stable by register because the native ABI omits application names.
Pixel color scalar types and vertex layout types can be specialized later.

Container construction writes real DXBC hashes and stage-specific ISGN,
OSGN/OSG5, PCSG and SHDR/SHEX chunks. Individual native tessellation-factor
system values are translated to the corresponding signature enum plus
semantic index. Dedicated depth/coverage outputs retain register -1. Outputs
use the correct never-written component-mask convention.

Dynamic linkage uses native DCL_INTERFACE table lists to emit IFCE metadata
with deterministic type names per table ID. No class methods, shader tokens
or function table IDs are invented or rewritten. The existing DXVK D3D11
class-linkage implementation can map these names to the exact declared table
IDs. The native stage callbacks now create actual vertex, pixel, geometry,
hull, domain and compute shaders through the embedded DXVK D3D11 device.
The native shader and callback objects share the existing Device owner.

Native VS input types come from the bound input layout; interpolated or flat
downstream declarations determine output scalar types; bound RTV formats
determine uint/sint/float PS output types. Signature changes recreate the real
backend shader and rebind its existing class instances. A hull shader that
omits its identity control-point phase obtains implicit control-point I/O
from the runtime union signature. Clip/cull array ordinals also use the full
union so unused earlier registers do not change a later array element.

The GS stream translator preserves each native stream/register/component,
splits noncontiguous masks, represents explicit gaps, and validates stream
ownership per output buffer, 512-byte write windows, 2048-byte strides and
128 components per stream. It calls the actual typed GS/SO API with all four
strides and the requested rasterized stream. VS/DS code supplied for a public
GS/SO passthrough also reaches DXVK's real passthrough implementation.
Signature-only NULL-code passthrough is still rejected; no fabricated success
is returned. Command lists, full higher-interface query semantics, and the
existing native input-layout/format coverage still need separate admission
work. Production D3D11 stays unadvertised and hardware/runtime/presentation
acceptance stays closed.

SetShaderWithIfaces builds actual class instances using the declared table ID
and aligned native byte offsets, validates every instance before committing
stage state, and retains the class owners while bound. The WDK states that
any error invalidates the shader handle without a later DestroyShader call;
the UMD therefore unbinds and retires its owned child on that error. An error
on an unbound shader does not clear the currently bound shader's classes.

The portable `tests/umd-sm5-container.cpp` fixture passed 93 checks for exact
tokens/hash, two GS streams, hull patch factors, signed color/depth/coverage
signatures, sparse native table IDs, IFCE slot/type parse, aligned byte to
vector offsets, malformed instruction bounds, versions and interface ranges,
multistream gaps/strides, write-window limits and mixed downstream scalar types.
The production DDI, decoder and both extended fixtures pass strict
official-header x64 and x86 Windows clang syntax checks.

`tests/umd-d3d11-device.cpp` now executes typed uint/sint graphics, three GS
streams and explicit stride/gap canaries, stream0 rasterization, tessellation
with HS/DS invocation counts, and native class table selection with distinct
constant-buffer byte offsets. It also covers class-bound handle retirement
and unchanged bindings after an invalid unbound shader. ARM64 MSVC and WARP
execution remain coordinated by the root/DX10 agent and are not yet claimed.
These source-linked CPU controls do not register the system UMD or prove the
Turnip/KMD hardware path.

Meson target: `src/umd/dxvk-umd-sm5-container-test.exe`. An explicit native
source-linked build needs `tests/umd-sm5-container.cpp`,
`src/umd/umd_shader11.cpp`, pinned dxbc-spirv
`dxbc/{dxbc_container,dxbc_parser,dxbc_signature,dxbc_types,dxbc_interface}.cpp`,
`ir/ir.cpp`, and `util/{util_swizzle,util_log,util_md5}.cpp`.
Use the pinned include root and C++17; it needs no Windows graphics library.

The SDK native signature comments explicitly permit ignoring unused union
signature entries when registers are not reordered. Actual shader declarations
determine the entries the reconstructed container consumes.

- [Native signatures](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11ddiarg_tessellation_io_signatures)
- [Interface table IDs](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d11ddi_setshader_with_ifaces)
- [Native class data fields](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11ddiarg_pointerdata)
