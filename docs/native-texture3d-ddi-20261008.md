# Bounded native Texture3D resources and transfers

The common native D3D10.0/10.1 device callbacks now create real embedded
renderer Texture3D resources, typed SRVs and W-slice RTVs. Native D3D11 uses
the same volume descriptor and initial-pitch validation, and routes its typed
Texture3D SRV through the common range conversion. This slice changes resource,
view and transfer behavior; the production admission masks remain unchanged.

## Implemented behavior

- One volume per resource, with dimensions from 1 through 2048, an explicitly
  validated mip chain, one sample and no array, primary, presentation or shared
  allocation. The supported uncompressed format sizes come from the existing
  transfer-format helper. Backend creation and view-format validation remain
  authoritative for usage, CPU access and format combinations.
- Initial data and updates validate the last byte actually read in each padded
  volume. Independent row and depth pitches are retained. The checked arithmetic
  accounts for the caller's remaining address space on both 32-bit and 64-bit
  callers before the backend can read runtime-owned source memory.
- SRVs select a bounded mip range. RTVs select a bounded mip and depth-slice
  range. Both accept the SDK's all-remaining sentinel after resolving it against
  the actual resource descriptor. Output binding validates the effective mip
  dimensions and W-slice overlap before changing backend OM state.
- Staging and dynamic maps retain backend RowPitch and DepthPitch. An invalid
  volume mip reports an error and a map output remains zero. Subresource updates
  and copies support nonzero X/Y/Z coordinates and bounded depth boxes. Full
  volume copies reject self-copy, differing dimensions/mip counts/formats and an
  immutable destination. Empty update/copy boxes remain no-ops.
- GenerateMips validates the actual resource flags and selected Texture3D SRV
  before delegating the selected mip range to the backend.

The existing owners, retirement paths and live D3D10/11 core callback pointers
are reused. There are no DDI table casts and no changes to shader translation,
null-GS stream output, public capabilities or runtime interface admission.

## Official semantics used

The local WDK documentation under
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/` and the
original 26100 `d3d10umddi.h` supply the resource, SRV/RTV, update and map ABI.
Texture3D is a complete mip subresource; its RTV selects depth slices within
that mip. The all-remaining WSize behavior is documented in
[D3D11_TEX3D_RTV](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_tex3d_rtv).
Effective output dimensions and nonoverlapping view restrictions follow
[OMSetRenderTargets](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets).
The fixture records actual backend metadata and readbacks to test those rules.

## Meaningful controls and retained originals

`tests/umd-volume-policy.cpp` runs 385,547 comparisons against independent
integer calculations for padded byte spans, final-byte boundaries, zero and
hostile pitches, mip shrinkage and XYZ copy bounds. GCC and Clang ASan/UBSan
runs passed. The existing transfer-policy controls also passed unchanged.

`tests/umd-texture3d.cpp` is a source-linked production-DDI control using the
Microsoft WARP reference renderer explicitly. It selects separate typed
D3D10.0, D3D10.1 and D3D11 tables, with member-owned live core callback tables
through DestroyDevice. Per profile it checks padded immutable initialization,
nonzero XYZ partial update/copy, three dynamic discard rounds, mip-relative
W-slice outputs, three selected GenerateMips ranges, SRV sampling and atomic
resource/view rejection. Independent expected arrays gate 27 cases, 9,138
mapped volume voxels and 945 sampled pixels across the three profiles.

The sampling shader uses public reference VS/PS creation to isolate resource
and SRV behavior. It does not establish production shader decoder support.
The fixture preserves HLSL and DXBC, and every successful readback preserves
the actual words, independently calculated expected words and original mapped
dimensions/pitches. A complete pass produces 309 such files: 234 files from
78 mapped volume-mip records, 63 from 21 sampled slices, and 12 shader inputs.

Local optimized Clang COFF checks with original Microsoft SDK/WDK/MSVC headers
passed for x64 and x86: production `umd_ddi.cpp`, the portable policy fixture
and the native volume fixture. The first two local attempts and diagnostics
are preserved. The final fixture-only output-retention compile is separate;
production and portable bytes join the earlier successful compile. These are
local original-header checks, not a native MSVC or WARP result.

## Exact native build inclusion

Add `tests/umd-volume-policy.cpp` as a standalone portable executable and
`tests/umd-texture3d.cpp` as a source-linked executable. The volume fixture
links these first-party units:

```
src/umd/umd_ddi.cpp
src/umd/umd_shader.cpp
src/umd/umd_shader11.cpp
src/umd/umd_runtime_query.cpp
src/umd/umd_allocation.cpp
src/umd/umd_runtime_gpu.cpp
```

It also links the unchanged pinned `dxbc-spirv` units:

```
dxbc/dxbc_container.cpp
dxbc/dxbc_parser.cpp
dxbc/dxbc_signature.cpp
dxbc/dxbc_interface.cpp
dxbc/dxbc_types.cpp
ir/ir.cpp
util/util_swizzle.cpp
util/util_log.cpp
util/util_md5.cpp
```

Use the official ARM64 toolchain and original `d3d11.lib`, `d3dcompiler.lib`
and static CRT/system import libraries. First-party UMD and fixture policy is
`/W4 /WX /MT /Zc:preprocessor`; the unchanged pinned parser follows repository
policy `/W3 /MT /Zc:preprocessor`, with its diagnostics retained separately.
Run the portable fixture expecting `PASS volume policy: 385547 checks`, then
the volume fixture in an empty owned output directory. Require actual exit 0,
the three profile markers, `PASS Texture3D`, `profiles=3`, `cases=27`,
`voxels=9138`, `sampled=945`, and all original files. Verify word comparisons
independently after collection. The observed runtime check count is recorded
from stdout; it is not guessed from local compilation.

## Remaining gates

Native ARM64 MSVC build and the complete WARP volume run are pending a root
CPU handoff. Real-KMT embedded-renderer execution and ordinary Microsoft
runtime-loaded DX10/10.1/11 acceptance remain pending. This bounded slice does
not complete compressed/video/depth volume formats, shared volume resources,
all resource/view aliasing rules or the complete resource-family admission
contract. No target, GPU, installation, registration or CI action is part of
the local verification packet.
