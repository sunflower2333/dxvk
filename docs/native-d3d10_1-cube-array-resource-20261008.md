# Typed D3D10.1 cube-array resources

The D3D10.1 device table previously selected the base10 CreateResource callback,
which rejected twelve- and eighteen-face cube resources. Its distinct callback
now preserves complete six-face cube arrays in the embedded DXVK Texture2D
resource. Base10 still requires exactly six faces. The runtime-owned callback
pointers and the existing atomic resource publication/retirement path are shared.

The official26100 WDK table declares D3D10.1 `pfnCreateResource` as the same
`PFND3D10DDI_CREATERESOURCE` four-argument function type as base10. This change
selects semantics from that actual typed table; it neither casts a table nor
derives support from the renderer's broader feature level. All other resource
dimensions use the existing path.

The D3D10 descriptor documentation requires six faces for a cube. The10.1
cube SRV documents `First2DArrayFace + 6*NumCubes <= Resource.ArraySize`.
Both primary sources are in `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/`:
`ns-d3d10umddi-d3d10ddiarg_createresource.md` and
`ns-d3d10umddi-d3d10_1ddiarg_texcube_shaderresourceview.md`.
The originalSDK `d3d10.h` defines the array axis512 and cube edge8192, anchored
by native-header static assertions. A10.1 array can therefore contain at most
85 complete cubes (510 faces), with square logical mip extents and finite mips.
Multisampling, primary/shared cube arrays, invalid binds/usage, incomplete faces,
and nonphysical mip chains continue to fail before resource publication.

`tests/umd-cube-array-resource.cpp` is an actual production DDI reference
fixture with a separately linked WARP factory. It uses exact base10 and10.1
tables, retains each live core callback owner through DestroyDevice, and checks
the original caller thread. It creates immutable6/12/18-face arrays, inspects
actual backend cube ranges/descriptors, reads every face and mip, and exercises
cross-cube update/copy boundaries. It retains observed/expected little-endian
words and original dimensions/pitches before reporting the first mismatch.
Malformed shapes, inaccessible mip/initial-data pointers behind invalid face
counts, and repeated failed-address reuse must leave private storage untouched.
The intended fixed reference totals are7 cases,3540 texels and56 deliberate
invalid operations. These are pending actual native execution.

`tests/umd-cube-array-policy.cpp` compares bounded shapes against an independent
set of complete face groups and explicit integer mip chains. Its expected
portable marker is97387 checks. Local sanitizer and original-header COFF
results are retained with the original source/compiler inputs under this
worktree's `artifacts/`; they do not substitute for native runtime execution.

The resource fixture links the same common native-entry objects as the original
cube fixture: `umd_ddi.cpp`, `umd_shader.cpp`, `umd_shader11.cpp`,
`umd_runtime_query.cpp`, `umd_allocation.cpp`, `umd_runtime_gpu.cpp`, and the
existing native-entry dependencies (including RuntimeService and the pinned
DXBC parser), plus `d3d11.lib`. The portable policy fixture has no graphics
dependencies. First-party native units require `/W4 /WX /MT /Zc:preprocessor`;
the unchanged pinned parser retains its separate `/W3` policy.

This slice does not change cube SRV translation, GenerateMips, cube-array
RTV/depth contracts, format capability bits, or CompleteResources admission.
The previously accepted d7 real-KMT1792-pixel probe is separate old source
evidence; this new production resource path still requires its own native and
hardware validation. Ordinary Microsoft runtime and production admission stay
closed.
