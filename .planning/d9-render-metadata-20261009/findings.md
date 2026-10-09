# Findings

The published RuntimeGpuDiagnosticInfo retains only generation/context/queue,
reference count, stream length, lock counts and cleanup counts. It lacks the
exact allocation handles, write flags, patch positions and input slot values.

Signed c1b9 native request36 + BO16 with Presumed offset8 requires relative
patch44+16*i. Six actual rows require44/60/76/92/108/124; a484-byte wrapper has
stream offset256 and full patch300/316/332/348/364/380. Predicate:
wddmddi.cpp7371-7378. KMD copies and patches its own snapshot/DMA; runtime may
replace command buffers on RenderCb return. Reading old buffers afterward
would be unsafe and could not prove the miniport's patched values.

Snapshot actual allocation/list scalars and generic supplied slot before the
callback. Read the independent expected BO.Presumed field only if command7,
length, BO count and native36+16*BO+32*command layout identify that field within
the stream. These diagnostic checks do not reject or alter a submission.
After callback, emit only normalized HRESULT and exact original callback HRESULT.

Existing TU_WDDM_DIAGNOSTICS=1 enables this; other values cause no added output
or snapshot reads. First callback only; eight references maximum, omitted count
explicit. Actual six references fit fully. No command dump or runtime handle
dereference; values are scalar opaque-handle numbers.

ROOT accepted native ARM64 CPU validation of source 0d02f17. The accepted release
is `reference/codes/dxvk-umd-d9-render-metadata-20261009/artifacts/render-metadata-native-0d02f17-01/ROOT-native-render-metadata-CPU-scoped-release-01.json`,
3954 bytes, SHA-256
`e1ec008cc08b75bedc7f09902788f646f9d8a005fa9c90172030a071e553c511`.
This binds strict native compilation, actual exact PASS 70 and the finite
retained/drained process and original-file review. Full compiler/process trees
and the Add-Type compiler tree remain unclaimed. No hardware admission or
RenderCb failure fix is established.

For the next ROOT-only API9 hardware diagnostic attempt, use a fresh genuinely
built core containing MAIN merge 59917b5; selected96 cannot produce the new
metadata rows. No additional logging implementation or probe flag is needed.
`scripts/test-system-d3d10-binding.ps1:540` already sets the worker's process
environment `TU_WDDM_DIAGNOSTICS='1'` before starting its owned API9 child at
line 554. Only API10/11 override it to 0. The ordinary SYSTEM D9 validation probe
does not replace that selector. Retain the exact private Vulkan tuple and the
worker's recorded `vulkan_environment` receipt.

`RuntimeGpu::create` reads that selector at `src/umd/umd_runtime_gpu.cpp:67`;
the two-byte buffer and `enabledFlag` accept exactly the single character 1.
Set it before RuntimeGpu creation, within the existing worker/owned child scope.
The first actual RenderCb emits `VIOGPU_RUNTIME_GPU event=render-before`, then
`VIOGPU_RUNTIME_GPU_RENDER captured=... omitted=...` and at most eight scalar
reference rows. `render-after` captures the exact callback HRESULT without
reading old buffers. Existing failure and finite close events remain enabled.
Collect the actual `probe.stderr.raw`; the six previously failing references
fit within the bound. Preserve all strict stderr, factory, typed device, held
module, pixel, Present and recovery guards: a diagnostic run with stderr cannot
serve as ordinary successful hardware admission.
