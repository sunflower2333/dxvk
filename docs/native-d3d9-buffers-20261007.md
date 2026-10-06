# Native D3D9 buffers, 2026-10-07

Source `053e39f` adds native vertex/index resources, range locks, resource-bound
streams and indexed drawing. `6ca877040690ff9510fa999ef0370e0fd9ff4536` repairs
strict probe compiler warnings. CI-only `cd4f5e22fddc3c30d910ec4fe798c8dc34bd3168`
retains identical production and fixture inputs and consolidates automatic CI.
The native ARM64 controlled fixture, four compiled semantic controls, all six
consolidated CI jobs and both actual target GPU runs are independently verified.
Ordinary system-runtime DX8-DX11 admission remains open.

## Implemented behavior

Worker-owned default-pool vertex and 16/32-bit index buffers support whole and
byte-range locks, Dynamic/WriteOnly/NotLockable usage, Discard/NoOverwrite and
DoNotWait. Linear height/depth/pitch/mip fields are reserved; index-buffer FVF is
ignored. SYSTEMMEM buffers are outside this slice. Output publication requires
exact S_OK and a usable mapping/resource; null mappings trigger cleanup, with
retryable unlock failures retaining ownership.

The DDI implements `SetStreamSource`, `SetIndices` and `DrawIndexedPrimitive`.
Every declaration stream checks its owned, unlocked buffer and final element
extent, including binding/declaration byte offsets. The last vertex need not
include unused stride padding. Indexed draws validate the index byte range and
the runtime's MinIndex/NumVertices range; a negative base is valid when the
effective vertex range stays nonnegative. Caller arguments and metadata are
snapshotted before runtime callbacks. Creation rechecks identity before publishing.

Bindings change only after backend success. Destroy unbinds every referring
stream/index slot and joins submission before worker retirement; retryable
unbind/flush failure retains the resource. Close drains mappings and bindings.
Production exports/caps and paired package pins remain closed.

## Controlled native verification

Exact committed ARM64 positive: 164072 checks, compile6.8425305s, run0.1118603s,
EXE SHA256 `622fab39331e6ca31ecab921ad68fcffa48a15ea0aab3edc6faefe85dbe66ce2`.
The verifier links56Git/archive/after-run inputs, native PE/static-runtime imports,
and unchanged signed58624/oem17/desktop metadata. An earlier uncommitted preflight
is separately retained; the initial committed probe's shadow warnings and failed
compile receipt are also retained.

Each control compiles the actual DDI with one deliberate mutation:

| Mutation | Observed native assertion |
| --- | --- |
| Omit vertex extent check | Check16153, line1665 rejects an out-of-range draw. |
| Keep a retiring stream bound | Check20142, line222 rejects a bound resource destructor. |
| Omit post-create identity check | Check21059, line1743 rejects stale generation publication. |
| Reread indexed arguments after a callback | Check16978, line1680 detects corrupted base/min/start/count. |

All controls compile successfully and exit1 through their intended assertion.
They establish CPU contract enforcement; GPU acceptance uses the separate oracle.

## Verified target GPU execution

Six stages verify384 buffer pixels with checksum621cd685. Each stage
clears the target first so a no-op cannot reuse preceding pixels. The workload
uses streams3/7, nonzero binding/declaration offsets, padded strides without
final padding, both index formats, nonzero index starts, negative/positive
bases, a NoOverwrite byte range and whole-buffer Discard. Verified total1472
pixels includes the existing1088 clear/draw/shader/texture cases.

Fresh runs `buffer-27` and `buffer-28` use the exact ARM64 sourcecd4f5e2 CI
payload and matched Mesa8443c71 under Limited USER/session1. Diagnostics1/0
both pass; elapsed8.316949s/10.8154619s. Each submits24 nonempty callbacks,
with ctx1/1, alloc15/15, lock14/14, residency15/15 remaining0 and wrong-thread0.
All eight payloads and three scripts match retained hashes/native ARM64 PE.
Owned tasks are removed with exit0. Fresh before/after readiness preserves
signed58624/oem17/binding0002/PnP0, exact SYS hash, DWM1552/Explorer6464 and all
58 selected failure/reset/epoch/timeout/admission fields. These bounded runs
do not establish long-term Explorer stability or ordinary runtime activation.

## CI and retained evidence

Offline run37508904695 completed all four jobs before consolidation. The single
automatic native run37509470022 passes all six jobs, builds ARM64/x64/x86, executes native ARM64
fixtures and shader tests, and retains GCC/Clang sanitizer and negative-control
logs previously unique to the offline workflow. Legacy app-local packaging/API
workflows are disabled; targeted contract/runtime workflows remain manual.
The independent build verifier matches all five artifact ZIP digests to the
GitHub API, three backend/probe architectures,164072 device checks per
architecture and14 actual native ARM64 execution hashes. ARM64 artifact
11435732444 SHA256 is
`b503455632832d435181fd4ea629a7717436c654747044927154cea77ec5f42d`.

Workspace evidence: `artifacts/dxvk-native-d3d9-buffers-20261007/`. Positive/control
receipts are under `guest-worktree-02/` and `guest-negative-buffer-*-02/`.
`buffer-source-ci-link-cd4f5e2.json` verifies the56 identical inputs and six
workflow-only changes. `ci-logs/full-verification.txt` records independent CI
verification. `buffer-27/verified.json` and `buffer-28/verified.json` record
GPU acceptance; their `readiness-verified.json` and `archive-verified.json`
retain readiness and transfer verification. Root plans remain active for remaining resource/state,
depth/stencil, queries, presentation/reset and ordinary DX8-DX11 acceptance.

Local Microsoft references: workspace
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/`, especially
CreateResource, SetStreamSource, SetIndices, Lock/Unlock and DrawIndexedPrimitive.
