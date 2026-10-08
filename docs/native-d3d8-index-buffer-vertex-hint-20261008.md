# System D3D8 index creation with a vertex-only hint

The immutable frontend-lifetime offscreen01 attempt reached the second system
runtime `OpenAdapter` and the internal DXVK device constructor successfully.
Its first failed resource call was `format=101`, `flags=02100044`, `pool=1`,
`surfaces=1`, `mips=0`; `CreateResource` returned `80070057`. Five preceding
format100 buffer calls succeeded. The later failed Render callback and Vulkan
device-loss messages occurred during cleanup; they are not established as the
cause of the earlier resource rejection.

The resource flags are INDEXBUFFER (0x00100000), DYNAMIC (0x40), WRITEONLY
(0x4), and MIGHTDRAWFROMLOCKED (0x02000000). The actual de72 core source
rejects MIGHTDRAWFROMLOCKED on every non-vertex resource before inspecting its
surface list. This guard deterministically rejects the observed INDEX16 call.
The production source file in the new branch is byte-identical to de72 before
this narrow change.

Microsoft describes MIGHTDRAWFROMLOCKED as a vertex-buffer hint in
[D3DDDI_RESOURCEFLAGS](https://learn.microsoft.com/windows-hardware/drivers/ddi/d3dukmdt/ns-d3dukmdt-_d3dddi_resourceflags).
The independent
[D3DDDI_LOCKFLAGS](https://learn.microsoft.com/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddi_lockflags)
description says the lock-time flag applies only to vertex buffers created
with that flag. The reviewed local originals are under
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dukmdt/`
and `content/d3dumddi/`, respectively.

The fix applies the borrowed locked-vertex-buffer path only to vertex buffers.
An index buffer with the unrelated create-time hint follows ordinary buffer
creation. Its index format, alignment, resource ownership, and bounds checks
remain active. Its descriptor does not enable locked drawing. Lock-time
MIGHTDRAWFROMLOCKED remains rejected, as do binding or drawing an ordinary
locked index buffer.

The typed production-device fixture covers the exact observed flags with both
INDEX16 and INDEX32, checks ordinary dynamic WriteOnly storage, rejects invalid
metadata without publishing a resource, and exercises normal draw/lock/unlock
and locked-index rejection. The existing borrowed vertex-buffer controls stay
active. These controls use a controlled backend; native Windows execution and
fresh system-runtime pixel acceptance are separate evidence. The original
offscreen failure and its release remain immutable. Present is still gated on
successful fresh offscreen pixels.
