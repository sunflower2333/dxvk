# Native predicate pending and device failure

The outer UMD previously treated a guaranteed predicate that remained pending
for two seconds as `DXGI_ERROR_DEVICE_REMOVED`. This was a deadline fabricated
by the CPU fallback, not an observation that the backend device failed. An
otherwise healthy query could therefore terminate ordinary D3D10 predication.

The Microsoft [SetPredication DDI contract](https://learn.microsoft.com/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_setpredication)
permits predication while QueryGetData reports `S_FALSE`. The predicate result
equal to PredicateValue suppresses subsequent rendering and resource commands;
hint predicates may allow those commands. Device removal is an error only when
it actually interferes with the operation. The corresponding local primary
source is `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_setpredication.md`.

The existing synchronous outer-UMD fallback still flushes the accepted query,
then waits for a non-hint predicate. It no longer manufactures failure from
elapsed time. Each pending iteration first asks the held backend device for
`GetDeviceRemovedReason`, then reads the held query with
`D3D11_ASYNC_GETDATA_DONOTFLUSH`. A real backend or query error retains its
HRESULT and is reported through the existing DDI error conversion. In this
embedded backend, GetDeviceRemovedReason reads the submission queue's actual
last error through `DxvkDevice::getDeviceStatus`; it maps a non-success Vulkan
status to `DXGI_ERROR_DEVICE_RESET`.

Independent renderer, context and query owners stay alive during these calls.
The caller checks device/query retirement immediately after submission, before
reporting a failed submission. The shared wait state machine checks retirement
before and after each backend operation, and after each pending yield. A
callback that retires an owner therefore cannot publish a new predicate or
cause a late error callback. Only a successfully resolved live predicate can
replace the old suppression state. NULL clearing, both predicate values,
hint behavior and the registered commands' suppression remain unchanged.

This remains a CPU/GPU synchronization fallback. A query that is pending on a
healthy live backend can wait indefinitely; genuine backend removal ends the
wait. Efficient GPU conditional rendering is separate work. No capability or
ordinary device-creation gate is cleared by this change, and the embedded
public SetPredication stub is not altered.

## Local controls

The existing SDK-typed `umd-query` fixture now calls the exact production wait
state machine using named SDK HRESULT constants. Its earlier query/map body
and output marker remain unchanged. A separate portable runner exercises the
same control body for both Boolean results and predicate values, hint bypass,
actual device/query failures, unexpected result codes, retirement during each
operation, and preservation of an older binding on failure. Four live-query
cases each return pending for 5,001 modeled one-millisecond yields before
becoming available. This checks state flow beyond the removed deadline without
claiming that a real GPU or runtime performed those yields.

`artifacts/predicate-wait-local-01` records four strict optimized x64/x86 SDK
COFF compilations (production DDI and query fixture), empty diagnostics, and
367 concrete selected compiler inputs unchanged before/after. GCC O2 and
Clang O1 with AddressSanitizer/UndefinedBehaviorSanitizer each passed 60,076
controls. Four dependency processes, four COFF compilers, four portable host
compile/run processes and two original Git reads have separate retained start,
raw and reaped/closed process receipts, plus the owning observer's closure.
The whole DDI outside its include and setPredication block, and the earlier
query fixture outside the added control include/call, rejoin the original base
`2fbb79131da999d52caa92f7341ac14aca12c212` bytes.

These are current-source compilation and CPU-model evidence. They do not
establish historical full-toolchain identity, Windows native query execution,
GPU availability, ordinary hardware device creation, or default desktop
replacement. Those remain explicit fresh execution gates.
