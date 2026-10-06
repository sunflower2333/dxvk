# Native D3D9 device lifecycle

The development D3D9 adapter now creates DXVK's embedded offscreen renderer
through the runtime-owned context and allocation callbacks. The returned
device table contains only `Flush` and `DestroyDevice`. Rendering caps are
zero and production `OpenAdapter` is absent. Resource, state, clear, draw,
readback, presentation and ordinary DX8/DX9 runtime admission remain open.

## Ownership and dispatch

- Snapshot the incoming runtime device handle, supported callback fields and
  output table address before the first reentrant identity query.
- Reserve runtime handles during construction and destruction; publish a
  monotonically issued driver token only after the backend succeeds. Reject
  duplicate device creation and never reuse stale tokens.
- Verify the adapter identity again before exposing the device to the caller.
  Reset or close unwinds the unpublished backend and preserves runtime outputs.
- Obtain command/allocation/patch buffers through `CreateContextCb`. Obsolete
  `CREATEDEVICE` buffer fields remain untouched, including on success.
- Pump runtime callbacks on the original DDI caller during construction,
  flush and destruction. Drain the backend before closing runtime allocations
  and the context. Unlock mapped backing before deallocation.
- A shared adapter lifetime gate prevents query and escape callbacks after
  `CloseAdapter`; device cleanup remains available. D3D10 uses the default
  absent gate and retains its existing behavior.
- Reject nested or concurrent flush/destruction while a device operation is
  active with `D3DERR_WASSTILLDRAWING`, preserving the device for a retry.
  Unsupported multithreading and flip batching flags remain rejected.

The D3D9 backend header now exposes its owned renderer through an opaque
implementation. This allows lifecycle compilation and controlled backend
substitution without linking the DXVK translation core into CPU fixtures.
The production DLL always links the real embedded implementation.

## Validation

Guest native ARM64 compiler preflight accepts the actual device/adapter/GPU
callback sources and the real-KMT target probe with `/W4 /WX`. The controlled
backend fixture passes 66,403 checks in 0.062 seconds after a 2.871-second
compile/link. These worktree results are preliminary; exact source CI and
receipt verification remain pending.

The fixture substitutes only the renderer. Actual typed adapter/device,
runtime context/allocation/map ownership and caller callback dispatch run.
It checks malformed callbacks, construction failure, unchanged outputs,
input-table mutation, duplicate creation, callback reentry, overlapping
operations, reset/close cancellation, terminal cleanup order and stale tokens.
It does not prove GPU construction or rendering.

`dxvk-umd-d3d9-device-probe.exe` supplies development callbacks backed by
actual KMT calls, selects the chosen display's exact LUID, constructs and
flushes the embedded offscreen device, then verifies balanced teardown and
caller-thread callbacks. It presents nothing and does not use Microsoft's
runtime for activation. Hardware execution is still pending.

Evidence: workspace `artifacts/dxvk-native-d3d9-device-20261006/`.
Preceding embedded-core evidence: [native-d3d9-embedded-20261006.md](native-d3d9-embedded-20261006.md).
