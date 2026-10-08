# BC physical resource manipulation

An explicit lower-mip BC update or copy box addresses complete physical 4x4
blocks. A logical 6x4 mip therefore uses the right-edge box `{4,0,0,8,4,1}`;
a logical 3x2 or 1x1 mip uses `{0,0,0,4,4,1}`. Texel/shader dimensions remain
logical. The [D3D11 functional specification, section 19.5.1](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm)
distinguishes padded physical lower mips from their logical size and requires
resource-manipulation regions aligned to complete blocks. The local official
DDI MIPINFO documentation likewise distinguishes TexelWidth/Height from
PhysicalWidth/Height.

The UMD now validates all four explicit BC box edges against wide rounded
physical bounds. Destination placement fits in encoded block units, avoiding
logical-size subtraction and UINT rounding overflow. NULL boxes still mean a
complete subresource. Existing owner, family, usage, sample, depth/stencil,
empty-box, upload pitch and source-address gates remain in force. BC1–5 remain
the supported transfer formats; no capability or hardware admission changes.

The two existing typed fixtures now provide physical MIPINFO and aligned BC
boxes to both native DDIs and their public reference. Their source patterns,
encoded expected bytes, destination untouched bytes, pitch padding, invalid
controls, snapshot counts and literal readers are unchanged: copy has 540 raw
files, update has 2520. This corrects the call geometry without replacing any
byte oracle.

Embedded DXVK UpdateTexture now validates the padded physical bounds and clips
only unused edge texels before constructing a Vulkan upload. Ceil block packing
still reads the same complete encoded blocks from the original padded rows.
[Vulkan buffer-to-image copies](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBufferToImage.html)
use logical image bounds; non-block-multiple extents must end at the image edge.

ROOT's actual Windows WARP diagnostic has 39 isolated cases across feature
levels 10.0, 10.1 and 11.0. The independent reader reopened all 273 originals
and 41 native child records. In every profile the four explicit logical-box
cases left the requested block unchanged; their four aligned physical-box
counterparts passed every literal byte. Two whole-NULL and three conservative
composition cases also passed. Each case checks complete 544-byte source and
destination planes and the owned padded input. This evidence is WARP-only.

Local GCC O2 and Clang O1 ASan/UBSan policy runs retain exact counts 164777
(copy) and 5780432 (upload). Six strict official SDK x86/x64 compilations cover
UMD DDI and both typed fixtures. The original incomplete frontend recipes and
their failed compiler receipts are retained; the later merged frontend and
DDI compile successfully for x86/x64 with the established project flags.

ROOT's native ARM64 EWDK BC03 run builds 19 COFF objects and three executables.
All twelve build/fixture children exit successfully. The unchanged independent
copy reader verifies 540 originals and 474624 byte observations; the update
reader verifies 2520 originals and 130944 compressed bytes. Source, selected
inputs, raw streams and owned process closure also pass the
[actual BC03 review](../../artifacts/bc-native-originals-reviewed-20261008-03/native-BC03-literal-originals-and-owned-closure-verified-01.json).
This verifies the controlled WARP fixtures; actual embedded DXVK GPU execution
remains pending.

A separate backend audit found that a shared image-copy extent can exceed one
logical BC edge while clipping it would violate the other image's block
alignment. The merged [GPU transfer-buffer route](vulkan-bc-asymmetric-edge-copy-20261008.md)
preserves complete encoded blocks using independently legal source and
destination extents and the existing tracked transfer helpers. Sixteen
standalone public Vulkan llvmpipe cases pass their literal source/destination
checks. Those results do not establish embedded target GPU correctness.
See [vkCmdCopyImage](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyImage.html),
source-image VUID 01728 and destination-offset VUID 00150. No CPU partial-copy
workaround is introduced.
