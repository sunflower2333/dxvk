# BC regional copies through the native UMD

`ResourceCopyRegion` and `ResourceConvertRegion` now accept matching BC1–BC5
format families on single-sample Texture2D resources, including array slices and
cube faces. The driver validates logical pixel coordinates before submitting the
existing GPU copy. Source left/top and destination X/Y must be multiples of four;
a source right/bottom may be unaligned only at the logical mip edge. Both logical
coordinates and complete encoded block ranges must fit the source and destination.
Rounding uses division and remainders to avoid overflowing UINT sizes.

The local primary DDI contract is
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_resourcecopyregion.md`:
NULL copies the whole subresource; an equal or reversed axis is a no-op; copying
requires compatible format families and valid pixel coordinates. The public
[CopySubresourceRegion contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion)
and [Direct3D functional specification, section 19.5](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm)
distinguish logical mip sizes from complete physical 4×4 blocks. The encoded
unused texels in an edge block are copied along with that block. Implementation
row-pitch padding has no prescribed contents and is not a readback byte oracle.

The portable policy target is `dxvk-umd-bc-copy-policy-test`. Its exact marker is
`BC regional copy policy PASS checks=164777 edge_blocks=1 hardware_admission=0`.
It covers aligned interior copies, partial mip edges, tiny mips, partial source
blocks copied into an aligned destination interior, invalid geometry, and UINT
bounds. GCC O2 and Clang O1 with ASan/UBSan each executed those 164777 checks.

`dxvk-umd-bc-copy-test` uses the real D3D10.1 and D3D11 typed DDI tables with a
fixture-only WARP backend and separate public resources. BC1/2/3 typeless and
sRGB/UNORM pairs plus BC4/5 signed, unsigned, and typeless variants are exercised on two-layer arrays
and six-face cubes. Twenty scenes contain 160 whole/region Copy/Convert calls,
400 native rejection controls, and 120 native equal/reversed-axis no-ops. Invalid
native parameters are never submitted to public WARP. The independent public
reference only submits legal copy operations. Source and destination readbacks
cover every mip and slice, untouched blocks, encoded edge padding, and an
immutable destination after its rejected write. CPU initialization row padding
and trailing guards are also checked for mutation.

The fixture produces exactly 540 regular originals: 180 native byte planes,
180 public byte planes, and 180 layout records with actual map pitches. Those
180 resource snapshots contain 3600 subresource readbacks and 237312 bytes per
role. The marker is:

```text
BC regional copy PASS checks=<decimal> profiles=2 scenes=20 copies=160 rejections=400 noops=120 snapshots=180 subresources=3600 bytes=237312 raw_files=540 hardware_admission=0
```

Run the fixture in a fresh raw directory. Then run
`python3 tests/verify-bc-copy-originals.py --directory <raw-directory> --output <outside-result.json>`.
The reader independently constructs fixed literal block fragments, requires exact
540-file closure, checks every native/public byte, checks typed layout records,
and retains hashes of all originals. Its result has `byte_observations=474624`,
`bytes_each_role=237312`, and both admission and registration false. The reader's
36-control self-test uses explicitly synthetic files; it does not execute the
UMD or WARP. Runner receipts and reader results belong outside the raw directory.

This source change preserves BC Update/full-copy, color MSAA, R9 volume-copy,
Map, depth, structured-buffer, BC6/7 and feature/version admission gates. Local
official-SDK x86/x64 COFF compilation and portable/synthetic checks passed.
Windows native execution and physical viogpu proof remain pending; this packet
does not advertise any new capability.
