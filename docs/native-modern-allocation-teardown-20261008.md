# Terminal modern allocation cleanup

The original typed residency fixture exposed an owned allocation that survived
`DestroyDevice`. A residency operation pins the surface's allocation while its
runtime callback can destroy the resource and device. Resource retirement drops
one surface reference, but the operation's alias keeps it alive. Closing the
callback service before the last alias disappears prevented the deferred
`DeallocateCb`. Actual CI57 x86/x64 failed the unchanged `owned.empty()` check;
the original failure outputs remain retained.

`RuntimeMemory` now keeps an intrusive ledger of modern allocation objects.
Successful creation/adoption links an existing object without allocating.
Moving the object rebinds its node, and ordinary release unlinks it before
callback reentry. Terminal `DestroyDevice` cleanup drains this ledger on the
original caller before closing the callback service. It clears the owner and
all handles before calling runtime code, so a callback can retire or move
another node safely; the loop reloads its head after every callback.

Owned allocations release their original runtime `hResource`; adopted views
only drop local ownership. Present and shared allocations share this ledger.
Backend/cache references can remain pinned until the interrupted operation
unwinds, with zero allocation handles and no late callbacks. A failed cleanup
callback remains a failure; the driver does not retry against a retired owner
or assert that the runtime released memory. Existing typed9 retry semantics and
ordinary context-only close are unchanged; typed9 objects do not join the new
ledger.

An allocation records a lock after the UMD accepts callback success. Terminal
cleanup balances a known acquired lock before releasing the resource group.
Transfers check terminal retirement after every runtime callback, before
accessing returned pixel memory and before further callback use. If destruction
occurs while `LockCb` itself is still pending, its later address/result cannot
revive the retired resource; the UMD rejects that return and performs no stale
`UnlockCb`. This negative control verifies memory/callback safety, not hardware
lock behavior or a keyed-mutex/GDI profile. Production admission remains closed.

The actual-WDK fixture tests ten owned allocations and an adopted view, move
rebinding, reentrant list mutation, four transfer-retirement points with
guarded returned addresses, two known-lock unlocks and a rejected late lock
return. Its failed-deallocation case deliberately retains the runtime's
residual object until external fixture runtime teardown, distinguishing nine
successful releases from ten attempts and one failure. It never relabels the
failed callback as cleanup success. The existing typed residency fixture and
its expected 36 release count stay unchanged.

The official [DestroyDevice contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_destroydevice)
requires child destruction first. The [Deallocate callback](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_deallocatecb)
supports group cleanup using `hResource`, and the [Unlock callback](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_unlockcb)
permits balancing device-owned locks during `DestroyDevice`. Exact local copies
of these Microsoft documents were used for the implementation audit.

Local validation compiles optimized x64/x86 COFF against original Microsoft
SDK/WDK/CRT headers and retains actual compiler processes and bytes. Native
MSVC execution, real KMT work and ordinary-runtime acceptance remain separate.
