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
IDs when typed native bindings are connected. This metadata layer itself
does not enable the higher native interface, clear runtime admission gaps or
claim GPU execution.

The portable `tests/umd-sm5-container.cpp` fixture passed 71 checks for exact
tokens/hash, two GS streams, hull patch factors, signed color/depth/coverage
signatures, sparse native table IDs, IFCE slot/type parse, aligned byte to
vector offsets, malformed instruction bounds, versions and interface ranges.
The source also passes strict official-header x64 Windows clang syntax checks.
Native executable shader and dynamic-linkage controls are the next slice;
ARM64 MSVC and WARP execution remain coordinated by the root/DX10 agent.

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
