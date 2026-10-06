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

Exact lifecycle source `af9ecd52ee2fac7ceacc1cdceeb2ad2a9712ffd5` passes
full CI `37419569213` (six jobs) and offline CI `37419571615` (four jobs).
The fixture passes 66,403 checks on each executed architecture, including
native ARM64. Independent verification covers 184 retained files, all 14
ARM64 execution-to-download EXE hashes, and three target probe PE/import
policies. Normal public API build `37419538950` and package `37419539028`
also pass. The earlier guest compiler preflight accepts the actual
device/adapter/GPU callback sources with `/W4 /WX`; its controlled fixture
passes 66,403 checks in 0.062 seconds after a 2.871-second compile/link.

The fixture substitutes only the renderer. Actual typed adapter/device,
runtime context/allocation/map ownership and caller callback dispatch run.
It checks malformed callbacks, construction failure, unchanged outputs,
input-table mutation, duplicate creation, callback reentry, overlapping
operations, reset/close cancellation, terminal cleanup order and stale tokens.
It does not prove GPU construction or rendering.

`dxvk-umd-d3d9-device-probe.exe` supplies development callbacks backed by
actual KMT calls and selects the exact physical LUID. Probe-only source
`bfd7e771cbfb0aabb6d9762456c4a288edf8a7b6` passes offline CI `37421820113`.
Its guest build, 51 Git/local input hashes, native ARM64 PE/import policy and
six malformed CLI controls are independently verified. The probe is intended
to construct/flush the offscreen device and verify teardown and caller-thread
callbacks. It presents nothing and does not use Microsoft's runtime for
activation.

## Current target construction gate

The guest currently runs diagnostic driver `100.6.101.58624 / oem17.inf`.
The exact signed SYS is
`d48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a`.
Replacement KMD CI `37428593406`, source `fdfd8f99`, passes both jobs. Its
production inputs match diagnostic `c1b9ea69`; the follow-up changes fixtures.

After reboot and interactive desktop recovery, physical LUID
`6e6c000000000000` opens the KMT adapter/device and typed D3D9 adapter.
Hardware03 fails CreateDevice `8876086a` under the elevated SSH token;
its Vulkan loader selects the installed DriverStore ICD despite local variables.
Hardware04 uses a measured limited `DROIDVM\USER` token in session1 and
selects the exact process-local Mesa ICD, but returns the same refusal in
0.5892604 seconds. Both receipts preserve the installed driver and desktop;
all nine input hashes verify. The owned interactive task is removed.

A property-only native ARM64 probe then measures the required private
runtime-support reply without creating a Vulkan device. The exact pinned
Mesa ICD SHA256
`9cbe528fa2139fe8f3ee0b2b435271bf168da7c48627e6ac127d48def4a80eac`
returns magic/version/size/flags `0/0/0/0`; DXVK requires
`3152574d/1/88/1`. This is the explicit refusal before Vulkan device creation
in `GpuBackend::initialize`. The diagnostic's 35 input hashes, archive,
ARM64 executable, limited token and driver/desktop continuity verify.
The diagnostic is local source with exact Git headers; its source/EXE hashes
are recorded separately from the D3D9 device probe's Git source.

Matched Mesa source `8443c71a5ab32b9d58b904fa51f4bf2f9089db8d` ports the
existing `ada48c1 + d6883df` runtime bridge onto exact pinned baseline
`3e50dd4ba4f941fcb4ddeb91d0cbd688cabfdb4a`. It retains direct WDDM residency,
copies runtime callbacks, imports and retains runtime allocations, and cleans
up retained tokens after failed device construction. The private protocol
header is identical to DXVK's. Local sanitizer tests pass 2,795 allocation
and 72 residency checks; three allocation and six residency semantic fault
controls also pass. Seven existing transport/compiler regressions, the full
source policy, workflow YAML and diff checks pass.

The exact production transport and controlled-dispatch fixture compile and
execute in the existing native ARM64 guest: 1.0699098-second compile/link,
0.8524681-second execution. Independent verification covers seven Git input
hashes, executable/AA64 PE, and driver/desktop continuity. This test substitutes
external KMT/runtime services and does not prove GPU construction.

Three-architecture candidate CI `37453381660` passes all four jobs and full
Turnip ARM64 CI `37453384744` passes all three. Its exact downloaded ICD is
`1b0cb01ab92c0f64168c9eac1848b05b9cd4f6492f7631a3de6aae36c6f5d0f6`;
13 bundle hashes, AA64 PE, source revision, exports and dependencies verify.
The process-local stage contains the unchanged loader, matched ICD and native
zlib dependency. Unused x64 CRT files from the earlier package are excluded.

Property support07 returns the expected `3152574d/1/88/1` under the measured
limited USER token. Device08 succeeds at create/flush/destroy and balances
context1/1, allocations3/3 and locks3/3, with no wrong-thread callbacks. Its
old oracle fails because it requires a render callback despite recording no
resource/clear/draw operation. Probe `271a83c` corrects that empty-lifecycle
expectation while retaining all ownership checks; offline `37455724595` passes
all four jobs. Independent guest compilation verifies its 51 exact Git inputs,
archive/EXE hashes, AA64 and six malformed CLI controls.

Follow-up09 creates successfully but loses the device during flush. A diagnostic
follow-up10 passes in 1.8902364 seconds, with the same balanced callbacks and
render0. All three receipts verify their eight input hashes, three wrapper
hashes, limited token and unchanged 58624 driver/desktop. This proves a bounded
empty lifecycle; it does not establish stability or the cause of09's loss.

Source `f648a1ab67bdf86794542d4e6bb5622290467e1f` closes a separate construction
timing gap: the owned D3D9 callback service now permits deferred requests before
the first callback pump starts. Previously a completion worker between pump
return and enabling deferral could receive `DXGI_ERROR_UNSUPPORTED`. D3D10
already enables deferral before backend construction. New behavioral checks
exercise workers before the first pump and between pumps, caller affinity,
closure and the unchanged synchronous non-owning rejection.

The native ARM64 guest fixture passes 66,410 checks after a 3.3824182-second
compile and 0.0547275-second run. A compiled control disabling startup deferral
fails at the first new behavioral assertion. Independent verification covers
52 source hashes, both archives, both native executables and desktop/driver
continuity. Offline `37456996101` passes all four jobs and full `37457000846`
passes all six. All three architecture builds pass 66,410 lifecycle checks;
independent artifact verification covers PE/import/export policies, all 14
native ARM64 execution-to-download EXE hashes and the unchanged production
admission gate. Normal public API build `37456982139` and package
`37456982302` also pass.

The exact CI ARM64 candidate is staged process-locally with the matched Mesa
ICD. UMD SHA256 is
`6fdc07b0db947b975632b45105b5b4bdeecc4c0de744ba2231466413f4a10350`;
the probe SHA256 is
`7260356b39b107a882a21d053fb0c6c84548fcae9dffb8b1311952653487c175`.
Both come from source `f648a1a`, run `37457000846`, artifact `11410126793`.
Three fresh bounded target runs use the same limited USER/session1 token and
physical LUID. Every KMT/open/create/flush/destroy stage returns S_OK.

| Target run | Mesa diagnostics | Probe elapsed seconds | Result |
| --- | --- | --- | --- |
| device11 | enabled | 0.9432165 | PASS |
| device12 | disabled | 5.2373924 | PASS |
| device13 | disabled | 2.3940590 | PASS |

Each run balances context1/1, allocations3/3 and locks3/3, with 44 identity
queries, six escapes, no render callbacks and no wrong-thread callbacks.
Independent verification covers each archive, eight payload hashes, three
wrapper hashes, native PE, selected local Adreno830 ICD, measured token and
driver/desktop continuity. All owned interactive tasks are removed.
An additional read-only snapshot resolves active PnP binding `0002`, PnP
error0, running service and the exact signed58624 SYS hash above. DWM1552 and
Explorer4184 remain unchanged. ADB5555 and both SSH services respond; root ADB
finds only the existing crosvm PID10130. No installation, registration, reboot,
VM configuration or image operation occurs during this candidate validation.

These runs establish bounded empty-device lifecycle acceptance for the exact
candidate. They do not prove GPU pixel work, long-term stability or that startup
deferral caused device09's earlier loss. Typed resource/state/clear/draw/readback,
presentation/reset and ordinary system DX8-DX11 runtime admission remain open.
The next resource slice needs its own nonempty-submission and pixel oracle.

## Earlier readiness evidence

On driver58623, exact physical LUID `d36b000000000000` opened the KMT
adapter/device but D3D9 OpenAdapter failed `d00000a3` because private identity
queries returned `DEVICE_NOT_READY`. Diagnostic KMD `c1b9ea69` added immutable
queue admission, standard-resource destruction and first reset records on the
same clock as submitted timeouts. Native guest compilation and signed58624
installation have been verified separately.

The fresh boot's first reset resolves to `VioGpuWddmResetEngine`; later
standard2Ddestroy refusal and native AHB paging timeout records have later
same-clock timestamps. Those records do not establish that either caused the
earlier reset. Boot02 recovered active hardware/queue state and the desktop.
Readiness recovery permits the current construction diagnosis; reset causation
and long-term stability remain separate open acceptance work.

Evidence: workspace `artifacts/dxvk-native-d3d9-device-20261006/`,
`artifacts/dxvk-kmd-readiness-20261006/mesa-matched-runtime-01/` and
`artifacts/dxvk-native-d3d9-startup-20261006/`. The last directory retains
all source/CI receipts, startup fault control, exact CI candidate, target11-13
archives and independently verified active-binding snapshot14. Binary source
pins remain `f648a1a` and Mesa `8443c71` after documentation-only commits.
Preceding embedded-core evidence: [native-d3d9-embedded-20261006.md](native-d3d9-embedded-20261006.md).
