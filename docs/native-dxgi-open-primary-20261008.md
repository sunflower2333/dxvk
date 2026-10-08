# Opened standard primary and device-owned staging

The production OpenResource path now handles the existing single-allocation,
80-byte flags1 standard shared primary separately from a flags2 CPU allocation.
It borrows the supplied primary and kernel-resource handles, validates the
original dimensions, pitch, format and refresh, and acquires one independent
device-owned staging allocation. The original primary is never CPU-mapped or
passed to DeallocateCb. The creating process keeps its ownership.

AllocateCb receives hResource=NULL, hKMResource=0 and one non-primary flags2
allocation with the borrowed pitch and size. This staging allocation is an
internal device allocation, not a new shared resource. Its refresh fields are
zero. DeallocateCb receives hResource=NULL and a one-element HandleList with
the actual staging handle. The exact typed Microsoft callbacks remain on the
original DDI caller. This implements the documented internal-allocation
contract rather than inventing a new resource cookie for the opened primary.

Refresh and publication use the existing KMD 64-byte opcode2 command with the
two distinct real allocation-list handles, retained RenderCb replacement
buffers and synchronized staging locks. Only width*4 bytes are transferred
per row; the primary pitch remains authoritative and padding stays unchanged.
The embedded renderer cache refreshes before reads and publishes through the
existing ResolveSharedResource and Present-flagged DXGI Blt paths. The standard
primary metadata and copy protocol come from the named KMD Git source
ef8a493f03aec2d5f5fae90578f01e69e317e031. This source reference does not prove
that the installed KMD accepts this path.

The staging owner stays in the device allocation ledger until a successful
DeallocateCb. A failed nonterminal callback retains the exact handle for retry;
a move carries the in-flight release transaction. Recursive release and
transfer cannot issue a second callback. Storage reserved before acquisition
keeps failed resource cleanup owned after resource private bytes are retired.
A live direct cancellation before AllocateCb returns real outputs balances
that late staging allocation through an independently reserved owner, including
a failed cleanup followed by terminal retry.

Retirement before actual acquisition makes no deallocation. Retirement after
real returned handles or a successful synchronized mapping balances only the
owned staging. Failed UnlockCb retains a retryable mapping; a new transfer must
finish it before another LockCb or RenderCb, and mapped rotation is rejected.
DestroyDevice nested inside unresolved UnlockCb or DeallocateCb cannot observe
the suspended callback's result. It reports incomplete cleanup, detaches its
owner and issues no second or late callback. Actual runtime terminal cleanup
is recorded separately by the fixture and is never counted as a successful
UMD unlock or deallocation. These terminal requirements remain part of the
closed public profile.

The policy fixture covers 6760 independent valid/invalid pitch, size, wire,
format, refresh and addressability cases. The typed production fixture uses
10.0, 10.1 and 11.0 device tables with the SDK-selected DXGI1.1 revision and live
core callback members. WARP substitutes only the embedded renderer. Three
formats and four snapshots per profile retain 36 images / 1260 pixels, plus nine
borrowed images with prefix, row padding and suffix. The 90 raw files have an
independent arithmetic reader with no fixture expected arrays. Seventy-eight
malformed OpenResource frames require exact callback errors, no allocation or
private output mutation, and a successful retry at the same private address.
Reentrant allocation/map/release, failed cleanup, moves and terminal cleanup
are separately controlled. Native fixture execution is pending.

The two fixtures are built, run, exported and checked by the existing native
CI scripts with the unchanged owned runner and 30-second deadline. ROOT must
preserve its current 50 execution cases / 37 shipping fixtures and add these two,
including arm64-dxgi-open-primary-originals and the existing primary, Blt,
Resolve and SRV raw-output upload paths. The ordinary admission mask and
reported capabilities stay unchanged.

This path does not enable opened-primary SetDisplayMode or flip Present. Those
still require actual primary/display association and ownership, private-format
conversion and runtime acceptance. Gamma caps need a genuine adapter query;
the embedded renderer's Vulkan BOs still need real VidMm residency association.
The staging handle's residency cannot stand in for the renderer allocations.
Stereo/HDR, complete resource semantics and the other mandatory profile gates
also remain explicit. Current hardware proof for the earlier dual10.0/10.1
draw probe is a separate source/core tuple, not acceptance of this new path.

The original local Microsoft documents specify
[OpenResource](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openresource.md),
[its allocation arguments](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_openresource.md),
[AllocateCb's internal-device option](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_allocatecb.md),
[the shared-resource allocation distinction](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/ns-d3dumddi-_d3dddicb_allocate.md),
[HandleList deallocation](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/ns-d3dumddi-_d3dddicb_deallocate.md),
[successful and failed UnlockCb ownership](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_unlockcb.md)
and [terminal device cleanup](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_destroydevice.md).
