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
