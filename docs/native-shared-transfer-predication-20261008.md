# Shared transfer predication

The shared cache uses a staging image to move pixels between the renderable
GPU image and the separate linear runtime allocation. Both staging hops used
immediate CopyResource. A suppressing native application predicate could skip
the GPU copy even though the subsequent CPU transfer succeeded, allowing
refresh to memoize an unchanged cache or publication to upload stale staging
bytes and clear dirty state. This is an evidenced source-semantic issue;
actual native execution of the new controls is still pending.

Both internal copies now record CopyResource in a fresh deferred context and
execute its finished command list with RestoreContextState TRUE. The private
list starts with no application predicate or bindings. Resource descriptors
remain the existing matching cache/staging pair; no formats, shapes, ownership
rules or public capabilities are broadened. CreateDeferredContext and
FinishCommandList failures propagate, and device-removal status is returned
before the caller records a successful ownership transition. There is no
fallback to a predicated immediate copy.

Publish executes the cache-to-staging copy before synchronized immediate
MapREAD. Refresh performs immediate MapWRITE, runtime download and balanced
Unmap before recording staging-to-cache. Deferred Map is unsuitable here:
it supports DISCARD/NO_OVERWRITE, while these staging images require ordinary
READ/WRITE. DXVK's MapWRITE may rename backing storage to preserve prior GPU
readers instead of waiting for them to finish; the ordering guarantee remains,
and this change adds neither a redundant Flush nor an idle-wait claim. Runtime
allocation callbacks and their failure/terminal-retirement behavior remain
unchanged. The caller's existing resource/context pins and live checks still
surround the whole transfer.

Application Copy/Update callbacks remain predicated. Map/Unmap remains
non-predicated. Private ownership transfers execute independently so they do
not silently change those application contracts. DXVK's actual GPU predication
is still a stub; the native WARP predicate controls test the reference behavior
and preserve closed hardware/ordinary-runtime/display gates.

The existing typed shared-resolve phase and its fifteen raw images, 525
pixels, three padded backings, 36 originals, retry, reentrant and terminal
retirement checks are retained unchanged. The appended D3D10.0/10.1/11.0
phase prepares 21 additional originals with 735 exact pixels and 42 files.
An issued/signaled, non-hint occlusion predicate suppresses ordinary native
application operations. A failed shared download must permit retry; the
successful refresh caches seed17 while a later external seed31 change stays
unobserved within the same epoch. Ordinary Copy and Update leave their seed3
destination unchanged. Private publish uploads seed19, failed publish keeps
the old seed19 allocation, and retry uploads seed23. Borrowed allocation
padding and both boundary guards are exact. Immediate MapREAD still executes
while the predicate is bound. Predicate, viewport, scissor, rasterizer,
topology, shaders and render target are retained after every ownership hop.

The separate raw reader requires its exact phase marker and complete file
closure. The existing shared-resolve reader invokes it after the unchanged
legacy arithmetic/padding verification, so existing native harnesses require
both phases without changing fixture counts. Synthetic reader packets and
mutation controls verify this reader, not GPU execution. Optimized official
SDK x86/x64 compilation and COFF reopens are likewise local controls only.

Other distinct internal immediate copies in nonshared identity rotation and
Present-only readback/resolve remain outside this bounded change. End-to-end
Present predication is not admitted by shared refresh/publication tests alone.
