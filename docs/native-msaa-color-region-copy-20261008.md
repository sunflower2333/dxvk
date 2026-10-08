# Complete color MSAA regional copies

The ordinary D3D10.1/D3D11 region handler previously rejected every
multisampled Texture2D during subresource lookup. It now admits complete
color subresources supported by the existing scalar color-format table.
Both real backend descriptors must have identical sample counts and quality,
equal complete mip extents, valid array indices, a null source box and zero
destination XYZ. Distinct layers of one resource are valid; copying a
subresource onto itself remains rejected. The ConvertRegion slot uses this
same copy handler.

MSAA metadata is opt-in only for the region-copy caller. Scalar and block
Update behavior, Map behavior, depth/stencil rejection, compressed/planar
region gates, immutable-destination validation, ownership checks and shared
state ordering retain their existing behavior. Single-sample quality stays
ignored. No feature, format, interface, registration or hardware gate changes.

The local Microsoft DDI document
`windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_resourcecopyregion.md`
defines the null-box/zero-offset and Count/Quality conditions. Existing DXVK
`CopySubresourceRegion1` selects actual mip/layer metadata and `CopyImage`
supports copies between images with the same sample count. This port checks
complete matching extents before invoking that backend; partial MSAA regions
and mismatched layouts remain outside its scope.

`dxvk-umd-msaa-copy-test` links production DDI to a fixture-only WARP factory.
Both native tables exercise 2/4 samples, three array layers, Copy/Convert,
different resources and distinct layers of one resource, and UNORM/sRGB
format-group copies. Native RTV creation/clear records actual backend
descriptors through a read-only bound-view inspection. Separately created
public resources receive their own clear/copy calls. Both paths resolve each
array slice into independent single-sample resources, copy to independent
staging resources, and map actual rows. Colors use exact 0/1 channels; the
literal 0/255 output checks all copied and untouched layers plus source data.
This proves resolved pixels when executed; it does not distinguish arbitrary
per-sample source patterns or replace hardware/backend acceptance.

Thirty-two positive scenes and four rejection scenes produce 204 snapshots,
78,336 bytes per native/public role and 612 original files. Eighty-four
rejections check one caller-thread E_INVALIDARG and unchanged full-array
sentinels: boxes, offsets, indices, self-subresource, sample mismatches, both
larger and smaller extents, unrelated formats, foreign/null handles, signed
boxes, MSAA Update, and depth source. Twenty-four native equal/reversed-axis
no-ops retain the same sentinels. These invalid/no-op DDI controls compare
against untouched independent public resources; no invalid public MSAA box
is submitted.

`verify-msaa-copy-originals.py` reconstructs every literal plane and exact
descriptor/control counter recipe independently. It requires all 612 regular
originals, records actual pitches and rejects missing/extra/renamed/symlinked
files, corrupted data and unsupported admission claims. Its synthetic
selftest exercises reader logic only. `dxvk-umd-msaa-copy-policy-test`
perturbs required sample-layout fields and preserves single-sample routing;
quality equality controls do not claim hardware support for those qualities.

The source packet keeps CI/shipping harness integration separate. Correct
Windows native/public execution and real Turnip/runtime acceptance remain
required before hardware admission or VM Mesa replacement.
