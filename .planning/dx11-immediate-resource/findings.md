# Findings

Base 488b484 was the actual HEAD selected by git worktree add.
Local Microsoft ResourceCopy and ResourceCopyRegion documentation permits
same-typeless-group format casts. Current conversionIdentity requires exact
format equality and copyRegion requires exact equality; legitimate UINT/FLOAT,
UNORM/SRGB/SNORM and TYPELESS same-group bit copies are rejected before the real
backend call. This is immediate-resource behavior, not a missing named callback.

The production transfer metadata currently supports ordinary uncompressed
texels; BC/depth/plane contracts remain separate work. The fix must preserve
pixel bytes and cannot implement numeric/color-space conversion. Full copies
also need authoritative real resource descriptor shape/usage/sample validation
before their backend's void CopyResource call can be treated as success.

Initial lookup errors: guessed compute fixture/shader include filenames and
ResourceConvert doc names did not exist; rg --files resolved actual names.
UAV artifact directory is workspace artifacts, not main-repo artifacts.
No memory registry matches were found; no relevant memory was used.

Independent Microsoft/DXVK audit confirms samples Quality matters only for
multisampling, reversed source boxes are empty no-ops, and R32_UINT/SINT ↔
R9G9B9E5_SHAREDEXP is an explicit conversion bit-copy exception. XR bias belongs
to the R10G10B10A2 group, but current backend XR creation is unsupported and no
format support is advertised here. Typed↔BC conversions with geometry scaling,
BC boxed transfers, depth/MSAA whole-subresource region copies, mapping state
validation and legacy D3D10.0 cube-vs-2D metadata remain distinct gaps.

Implemented source and fixture independently reviewed without a blocker.
Native recipe derives 320 observations, 92 rejected calls and 134624 bytes per
native/public side. Reader scope does not claim it independently proves each
intermediate rejected call; actual producer originals remain mandatory.
Structured-buffer stride/alignment validation is also a pre-existing separate
missing immediate compute-resource contract, now explicitly documented.
