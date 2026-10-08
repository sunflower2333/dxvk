# Vulkan BC copies with different logical edges

D3D BC resource regions cover complete physical blocks. A source interior block
may be copied into the padded edge block of a smaller logical destination mip.
For example, an interior 4x4 source region copied to x=4 in a logical 6x4 mip
needs source extent4x4 and destination extent2x4. A shared Vulkan image-copy
extent4x4 exceeds destination bounds; reducing it to2x4 violates source
interior block alignment. See [vkCmdCopyImage](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyImage.html),
VUID dstOffset-00150 and source-image-01728. These are independent validity
requirements, even though encoded block counts are equal.

DxvkContext::copyImage now selects a device-local transfer buffer when logical
source/destination extents differ. The route is limited to the same BC1–5
family, matching block extent and encoded byte size, sample1 2D color images,
and equal explicit array-layer counts. It requires the independently clipped
destination to retain exactly the source's encoded block count. Equal extents
continue through the existing clear/inline/image-copy path. Full copies,
depth/stencil, multisampling, R9 Texture3D transfer, upload policy, format masks
and admission gates are unchanged.

The source-image-to-buffer command uses the already snapped source extent;
the buffer-to-destination command uses its separately snapped destination
extent. Both have valid logical bounds, block-aligned offsets and the allowed
logical-edge exception. Each operation reads/writes the same packed encoded
bytes. The [image-to-buffer](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyImageToBuffer.html)
and [buffer-to-image](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBufferToImage.html)
commands each describe one image extent, so they can express this operation
without changing any compressed block.

The temporary buffer has only transfer source/destination usage and
DEVICE_LOCAL memory. No production CPU mapping or partial-image modification
is involved. Existing copyImageToBufferHw/copyBufferToImageHw call syncResources
for image layout and transfer access. The first helper runs on ExecBuffer and
tracks a transfer write to the new buffer. prepareOutOfOrderTransfer therefore
keeps the second buffer read on ExecBuffer; acquireResources observes the
write and emits the required dependency before reading it. acquireResources
also calls m_cmd->track for both buffer and images. DxvkResourceRef acquires
those resources until command-list execution completes, so releasing the
local Rc at function exit cannot free queued storage. Existing deferred clears
are flushed by the transfer helpers' normal acquisition path.

A standalone public Vulkan probe runs on an explicitly selected CPU ICD. Its
cases have these logical source/destination extents (all original D3D-style
regions represent aligned complete blocks):

| Case | Source | Destination | Path |
| --- | --- | --- | --- |
| 0 | 3x2 | 4x4 | buffer |
| 1 | 2x4 | 4x4 | buffer |
| 2 | 4x4 | 2x4 | buffer |
| 3 | 4x4 | 3x2 | buffer |
| 4 | 3x2 | 1x1 | buffer |
| 5 | 2x4 | 2x4 | direct control |
| 6 | 4x4 | 4x4 | direct control |
| 7 | 8x4 | 6x4 | buffer, two layers |

Actual local llvmpipe execution passed16 isolated processes: these eight cases
for BC1 and BC3,8/16-byte blocks, UNORM source and sRGB destination. Each checks
all five mips and both array layers, complete unchanged source, complete
destination including untouched blocks, and32 guard bytes. The independent
fixed-layout reader passes96 actual raw files and26112 source/destination byte
observations. Eleven mutations of copied originals are rejected. The initial
reader attempt had a pixel-versus-block coordinate error in case0 and failed
with IndexError; that receipt remains preserved. The corrected reader passes
the unchanged actual producer bytes.

Final selected-input snapshots before/after three actual SDK compiler children
are identical. Official SDK26100 Clang18 O2 COFF compilation passes the final
DXVK context for x86/x64 and the previously frozen BC-only UpdateTexture
frontend for x64. The recipe uses the repository's existing warning exclusions
plus explicit exceptions for pre-existing MS header macro redefinition,
unused-set variable, Microsoft include search, unused captures and concrete
COM-state nonvirtual destructor warnings. Every prior failed compiler receipt
is retained. These local recipe exceptions do not change shipping flags or the
strict UMD/fixture compilation recipe.

The probe executes public Vulkan primitives, not the embedded DXVK context.
No validation layer is installed in the local environment. Embedded runtime,
target GPU execution and hardware admission remain pending. No Meson, CI
selection, shipping manifest or existing fixture count is changed by this
standalone local verification source.
