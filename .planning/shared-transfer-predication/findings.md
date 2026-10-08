# Findings

transferSharedSurface currently issues immediate CopyResource before publish
Map and after refresh Map. A native application predicate can suppress these
copies while sharedRefreshed/sharedPublished update ownership state as though
the transfer happened. Central identity Blt's private command list cannot
protect these separate transfers. DXVK SetPredication remains a hardware stub.

Inspect application Update/copy/map separately; app operations must retain
their proper predication contract, while private ownership transfers must
preserve bindings and execute independently of that predicate.

Application Copy/Update callbacks are correctly predicated through DeviceEntry;
Map/Unmap are not. The private deferred helper leaves those tags unchanged.
Map must remain immediate: deferred Map only admits DISCARD/NO_OVERWRITE and
STAGING cannot use those. DXVK immediate MapWRITE can rename staging storage
instead of waiting for prior readers, while preserving the required ordering.
Do not claim guaranteed GPU idle for that direction.

Other distinct internal immediate-copy issues remain in nonshared identity
rotation and Present-only readback/resolve; this change stays bounded to the
two shared-surface staging hops and shared-cache refresh/publication.

Read original local Microsoft callback contract:
reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/
nc-d3d10umddi-pfnd3d10ddi_setpredication.md. Equal query/value suppresses
render/resource operations; non-hint issued/signaled native test is required.
The typed DDI wrapper's application suppression tags and nonpredicated Maps
remain byte-identical because umd_ddi.cpp is untouched.
