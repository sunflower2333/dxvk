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
UMD DDI and both typed fixtures. The full embedded frontend compile remains
unverified locally: an attempted project-flag recipe stops at a retained
OBJ_CASE_INSENSITIVE redefinition and absent SPIR-V header. The original
failed compiler receipts are preserved. Native full fixtures await ROOT's
fresh run.

A separate backend audit found that CopyImage computes individually snapped
source/destination extents but its image-to-image dispatch sends only the source
extent. An aligned BC source interior copied into a smaller logical destination
edge can exceed the destination Vulkan bounds. Merely clipping the shared
extent would violate the source block alignment rule. This patch does not
claim complete Vulkan asymmetric-edge copy validation; a separate GPU transfer
buffer path must preserve complete encoded blocks with independently legal
source and destination extents. See [vkCmdCopyImage](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyImage.html),
source-image VUID 01728 and destination-offset VUID 00150. No CPU partial-copy
workaround is introduced.
