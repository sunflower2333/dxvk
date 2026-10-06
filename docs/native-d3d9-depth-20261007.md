# Native D3D9 depth/stencil, 2026-10-07

Source `2d28554271e2326c0baa7bfc578adb04d9e98e2c` implements typed D16/D24S8
surfaces, SetDepthStencil and native color/depth/stencil clearing. The committed
native ARM64 fixture and four compiled semantic controls independently verify.
Full renderer CI passes, and the exact ARM64 candidate passes two target GPU
runs with diagnostics enabled and disabled.
Ordinary system-runtime DX8-DX11 acceptance is still open.

## Resource and clear contracts

Default-pool depth surfaces use a single nonmultisampled subresource. Color,
texture and buffer aliases cannot become depth bindings; unsupported depth
formats, SYSTEMMEM, locking and depth blits are rejected. Native bindings change
only after exact backend success. Destruction unbinds before flush/retirement;
retryable failure retains ownership. Close drains bindings on the worker.
A depth attachment smaller than the render target rejects drawing.

Clear selects TARGET/ZBUFFER/STENCIL independently and validates only requested
aspects and values. It snapshots rectangles after device serialization and
before callbacks. The private ClearNative renderer path preserves runtime
preclipped rectangles and zero-count no-op. COMPUTERECTS uses the actual
viewport/scissor intersection; disjoint scissor rectangles clear no pixels.
Shared clear logic now clamps image extents relative to the rectangle offset.
The public Clear method retains its public zero-count/pointer behavior.

## Verified committed native contracts

Positive fixture:182260checks, compile6.8519516s, run0.1222363s; executable SHA256
`b63169c0c10cf00daa88a86e977e957e7e0462c4c5d2c5c4bb941c63ab702cb7`.
The independent verifier matches56Git/archive/after-run inputs, ARM64 PE/static
runtime imports and unchanged signed58624/oem17/desktop metadata.

| Single production mutation | Native assertion caught |
| --- | --- |
| Keep the retiring depth surface bound | Check5329,line326 rejects a bound surface destructor. |
| Omit depth resource typing | Check4159,line515 rejects a color surface depth binding. |
| Clear TARGET instead of the requested depth aspect | Check2823,line524 rejects a missing color target. |
| Reread caller rectangles after a callback | Check3167,line859 detects a changed rectangle snapshot. |

All four controls compile and fail their intended assertion with exit1.
Earlier uncommitted preflights are retained separately. Preflight01 has an
incorrect source identifier and is excluded from committed-source acceptance;
preflight02 precedes the depth-size draw case, and preflight03 matches the
final56source inputs before commit.

## Verified target GPU pixels

Fourteen stages24-37 verify896new pixels with independently calculated checksum
`eac96ea5`, total2368including earlier clear/draw/shader/texture/buffer cases.
D16 cases cover LESS/GREATEREQUAL, ZWRITEENABLE, preclipped full clears outside
the active viewport/scissor, zero-count preclipped no-op, computed whole and
explicit rectangles, and an empty viewport/scissor intersection. D24S8 cases
cover independent depth/stencil preservation, masked stencil replacement,
preclipped/empty/computed stencil clears and combined color/depth/stencil clear.
Each case initializes distinguishable state; final pixels must also be produced
by drawing. All readbacks retain CPU row padding and outer guards.

| Target run | Diagnostics | Verified pixels | Duration |
| --- | --- | --- | --- |
| depth-29 | Enabled | 2368, including 896 depth/stencil pixels | 14.5854506 s |
| depth-30 | Disabled | 2368, including 896 depth/stencil pixels | 14.7961268 s |

Both independently verify all eight payload hashes, three frozen wrapper
hashes, native ARM64 architecture and the Limited USER/session1 token. Each
run has 38 nonempty render callbacks, context1/1, allocation14/14, lock13/13,
residency14/14 with zero remaining references and zero wrong-thread callbacks.
Both owned tasks are removed with exit0. Adreno830 uses the exact matched
Mesa8443c71 runtime; no device-loss or command-submission failure is recorded.

Fresh before/after inspection retains signed58624/oem17, binding0002/PnP0 and
the exact installed SYS hash. DWM1552/Explorer6464 and their start times remain
unchanged; all58 selected failure/reset/epoch/timeout/admission fields have zero
delta. This bounded result does not resolve earlier installed Mesa D3D10
Explorer faults or prove long-term desktop stability.

Retained evidence archive SHA256:

- depth-29: `c2e1f6f00d07d28909885a57cb27c28cd680b423306153d69c9184b9a01d3177`
- depth-30: `ef88465062eaa84cd63502641880b4b4c17a16076e1210f0acb39c6283058220`

## CI and retained evidence

The single automatic consolidated native [CI run37515057679](https://github.com/sunflower2333/dxvk/actions/runs/37515057679)
passes all six jobs. All five raw artifact ZIP digests and exact source links
verify. ARM64/x64/x86 embedded renderer builds each pass182260 device checks;
14 actual native ARM64 execution hashes match. Linux sanitizer and semantic
controls also pass. Root refreshes the live CI result after both GPU runs.

ARM64 artifact11437500275 ZIP SHA256 is
`b2398010aa6446cef39015c881c76f4b7be8f13608989ea36f9f2ac84ff76a74`.
The staged DLL SHA256 is
`d55ab71430085d1d6393d475952baf50033708d251a5a58dbe90dda88ab5556f`;
probe SHA256 is
`2250a5f6049e4ee2c19d8f0791acc8e2cb411874a387081c2fae332ae4079d08`.

Workspace evidence: `artifacts/dxvk-native-d3d9-depth-20261007/`.
`native-fixtures-verification-01.txt` links committed positive and four controls;
`depth-pixel-oracle-01.json` retains independent pixel expectations;
`depth-runner-scripts-01/` retains three native-parsed, hash-matched wrappers.
`ci-logs/full-verification.txt` retains the independent build handoff;
`depth-29/` and `depth-30/` retain payload, pixel, archive and readiness proofs.
No production caps, exports, registration, installation or paired package pins
are widened. Remaining fixed-function/resource/query/presentation/reset and
ordinary DX8-DX11 runtime gates remain active.

Local Microsoft references are retained in workspace
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/`, especially
CreateResource/SetDepthStencil/Clear and the native resource/clear structures.
