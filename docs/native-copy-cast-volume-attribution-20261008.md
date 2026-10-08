# 3D special-format regional copy attribution

The genuine CI63 x86 and x64 WARP-backed children both exited with access
violation status `-1073741819`. Their original 281-byte stderr records native
callback HRESULT `88760870` while creating a staging resource for attempted
observation 120: profile 0, 3D, R32_UINT (42) to R9G9B9E5_SHAREDEXP (67),
regional Copy operation 2. Both original archives retain 120 flushed manifest
rows and 120 native/public raw pairs. The independent original reader confirms
all saved byte planes against the unchanged literal recipes. The failed
observation itself has no saved planes.

The source mip1 is 4x2x2; its box is `[1,3) x [0,2) x [0,2)`. The destination
mip0 is 8x4x4 with offset `(3,1,1)`. These coordinates fit. Local Microsoft
ResourceCopyRegion and ResourceCopy DDI documentation, plus the official
[Direct3D10.1 format conversion table](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#format-conversion-using-direct3d-101),
permit UINT/SINT and R9 bit reinterpretation; no 3D restriction was found.

The native bridge and independent public resources share the fixture's WARP
device. Previously both void regional copies ran before staging creation, so
the original failure does not attribute device removal to either copy. The
diagnostic fixture now computes the same literal expected bytes and completes
an additional native staging Copy/Map readback before submitting the public
copy, only for 3D R9 exception pairs. A flushed native-before-public checkpoint
is emitted only after every mip passes its existing literal row checks.
Sticky `submission` diagnostics survive nested readback phases; the reported
`device_removed_checkpoint` is the last direct GetDeviceRemovedReason sample,
not an invented timestamp for a later asynchronous callback. If native staging
fails before that checkpoint, the public regional copy has not been submitted.
If the checkpoint passes, later failure occurs after a completed native readback.

All actual native Copy/Convert operations, source/destination descriptors,
boxes and offsets, six native empty vectors, 92 rejection controls, 320 final
observations, 640 raw byte planes, manifest recipes and independent reader
remain unchanged. Production, Meson, CI, format and feature admission are
unchanged. The added checks change the non-authoritative CHECK counter.

A separate standalone source artifact exercises exactly this public volume
region on one fresh WARP device per process, with independent same-format
controls at feature levels 10.1 and 11.0. Each successful process saves complete
584-byte destination and unchanged source planes for all three mips, with
explicit creation, copy, Flush, Map and device-removal HRESULT checkpoints.
It loads no physical driver and registers nothing. Strict local x86/x64 COFF
compilation is source evidence; actual Windows execution is required to
resolve the cause. No WARP workaround or production format rejection is inferred.

## Scoped graphics implementation after the standalone execution

ROOT built the standalone programs with the target's native ARM64 EWDK CL/LINK.
Separate FL10_1 and FL11_0 devices pass same-format 3D region copies, while
direct R32_UINT-to-R9 and R9-to-R32_SINT regions immediately sample device
removal `887a0020` after CopySubresourceRegion. The documented conversion and
valid geometry remain unchanged; the isolated public calls reproduce this
backend failure without loading the physical driver.

The fresh bridge producer, source SHA256
`1878959af5808750c44c7288ff5a876dfa418bd525ca363175cf1f349dc22575`,
passes all four direction/feature-level processes. Each checks the complete
source-shaped destination-format temporary after CopyResource, then performs
same-format CopySubresourceRegion with the original mip, box and destination
offset. Every process passes 88 checks and saves three 584-byte originals:
temporary, final destination and unchanged source. Independent fixed row
fragments verify all 12 original files (7008 bytes). Original process, compiler
and checkpoint records are retained under
`artifacts/copy-cast-volume-native-20261008/repro-03/originals/DxvkWarpVolumeRepro-20261008-03`.
Each process owns a separate `run-profile-case` raw directory.

The production CopyRegion path uses that composition only after its existing
ownership, dimension, documented format compatibility, usage, bounds, depth
and shared-resource validation, for unequal formats involving R9 on a 3D
texture. The preceding compatibility check permits precisely R32_UINT/SINT
as the other format. A temporary Texture3D preserves the complete source
width, height, depth and mip count, uses the destination format, DEFAULT
usage and no binds, CPU access or misc flags. CopyResource reinterprets all
source bits; same-format CopySubresourceRegion preserves the original selected
subresource, box and XYZ. The temporary is released through the normal COM
resource lifetime after submission. Allocation/creation failures reach the
existing native error path before either bridge copy is issued.

The typed fixture independently composes the two public calls from its actual
public source descriptor. Native calls still receive actual R9/integer
resources and the original literal Copy/ConvertRegion arguments. Native
staging readback still completes before public submission. All 320 observations,
640 raw planes, source checks, manifest bytes, 92 rejections and six literal
native empty boxes retain their earlier definitions and independent reader.
This costs an additional full-source temporary allocation and full-source
copy for this narrow exceptional region path. No CPU transfer fallback,
new format or hardware admission is introduced. Standalone WARP success
proves the composed backend calls and fixed byte region; the complete typed
native fixture still requires its current Windows/CI execution.
