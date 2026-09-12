# Native resource ownership checkpoint

The native D3D10 creation path now stages backend and runtime allocation owners
before publishing a resource into runtime private storage. Microsoft's
CreateResource contract says a failed handle receives no DestroyResource call.
Every failure therefore releases staged owners before reporting SetError.
An independent storage reservation prevents duplicate creation and detects
recursive cancellation or storage reuse before publication.

DestroyResource removes its live record and moves all owners out of runtime
storage before DeallocateCb. Recursive destruction can immediately reclaim that
storage without double deallocation or an outer access to retired bytes.
SetError receives its saved original callback/core handle only after local
destruction completes. Allocation release similarly clears all ownership before
the runtime callback, and context destruction clears its handle before reentry.

Each runtime publication allocation retains the distinct opaque runtime resource
handle, KMT resource handle and KMT allocation handle. The native entry supplies
its original adapter identity: exact LUID, nonzero reset generation, capabilities,
QueryAdapterInfo callback and opaque adapter handle. Allocation, synchronized
upload and Present revalidate it across runtime callbacks. Reset or identity
drift is sticky and prevents further publication; a reset after successful
AllocateCb balances the allocation before returning failure. A changed identity
after LockCb prevents CPU writes and still balances the successful lock.

The existing KMD v0 non-native CPU publication allocation requires zero wire
generation, IOVA and ContextId. Those fields remain unchanged; reset ownership is
retained in UMD metadata. The coordinated vkd3d/Mesa private v1 native import
protocol is unchanged. This checkpoint does not introduce native shared backing,
primary allocations, cross-device sharing, OpenResource, flip or zero-copy.

Controlled tests link actual adapter/device/resource/allocation sources and use
WARP plus explicit runtime callbacks. They cover malformed-success allocation
cleanup, reset during allocate/lock/unlock/context/Present, original callback
identity, recursive allocation/context/resource release, immediate resource
storage poisoning after recursive destroy, failed creation without a destroy,
cancelled creation and subsequent storage reuse. They do not establish ordinary
Microsoft runtime activation or target GPU behavior. Arbitrary device destruction
during active resource/backend operations and the runtime's backing-teardown
serialization remain separate acceptance gates. Production supported versions
and pipeline levels remain zero until every mandatory contract is complete.

Contract references:

- Microsoft PFND3D10DDI_CREATERESOURCE: errors invalidate the handle and runtime
  will not call DestroyResource.
- Microsoft PFND3D10DDI_DESTROYRESOURCE: runtime frees the private memory region;
  the driver may no longer access it.
- Microsoft D3DDDICB_ALLOCATE: hResource is the original opaque runtime handle;
  hKMResource is the distinct returned KMT handle.
- VIOGPU shared/viogpu_wddm_abi.h and viogpuwddm/wddmddi.cpp non-native v0 checks.
