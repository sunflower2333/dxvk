# DXGI1.1 shared-resource handoff

`ResolveSharedResourceDXGI` previously had no implementation. The exact
DXGI1.1 table now publishes a typed entry that copies a dirty GPU cache into
its linear KMD allocation, synchronously submits remaining backend commands,
then invalidates that surface's cached ownership epoch. A clean surface still
owes the submission barrier. The original DXGI1.0 table remains bounded to its
documented size, selected by the official interface/revision macro.

The handoff captures the resource's registry reservation, independently owned
shared surface and backend references before any callback. It checks the
reservation and device after publication and submission. Foreign, retired,
guarded-memory and backend-only handles cannot reach an allocation callback.
Nested Resolve/Present/Rotate returns busy. Nested destruction of the specific
surface already being published retires private storage while the pinned
outer operation completes; it does not recursively map that staging image.

Publication failure retains dirty cache state. A submission failure can occur
after the linear allocation already received its pixels; it returns the
original failure and does not report a completed handoff or invalidate the
cache. Unexpected positive callback statuses become `E_FAIL`, and every
acquired KMD lock is balanced. Transfer copies only active row bytes, preserving
padding in an allocation opened from another owner.

Microsoft documents this notification for keyed-mutex ownership release and
GDI `GetDC`, including the requirement to flush partially built command
buffers. Those resource-creation flags remain reserved and rejected by this
driver. This implementation exercises the existing plain shared/opened surface
bridge and does not advertise keyed-mutex, GDI or whole-profile admission.
See the original [DXGI1.1 function-table contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dxgiddi/ns-dxgiddi-dxgi1_1_ddi_base_functions)
and its local copy under `reference/codes/windows-driver-docs-ddi`.

`umd-dxgi-shared-resolve.cpp` uses actual typed 10.0, 10.1 and 11.0 resource,
open, update, copy and staging-map entries. Only the embedded renderer is
replaced with fixture WARP. It covers dirty/clean handoffs, later external
writes, failed Lock/Unlock/submission, unexpected positive statuses, foreign
owners, nested busy calls, original-caller callback thread identity and
private-storage retirement. A real owned surface remains pinned by Resolve
while its status callback destroys the resource and device; the terminal
allocation ledger must deallocate it on that caller before DestroyDevice
returns. Each profile also creates a DXGI1.0 table with
trailing canaries to detect a DXGI1.1 tail write.

The native fixture saves 15 raw readbacks/525 pixels plus three padded KMD
allocation frames/105 uploaded pixels, in 36 original files. The independent
reader derives literal RGBA values from five fixed ownership seeds and XY
coordinates, checks allocation guard/padding bytes, and rejects missing,
extra, duplicate-marker or malformed original files. Native execution and
ordinary-runtime/hardware admission are separate gates. Local optimized
official-header COFF compilation is recorded as such, not as native MSVC or
runtime acceptance.

Ordinary Microsoft runtime replacement still requires residency coverage for
the renderer's cached images, primary/display ownership and the remaining
DXGI Blt/display-mode/gamma callbacks, complete requested DDI profiles and
genuine hardware/runtime acceptance. The existing admission masks stay closed.
