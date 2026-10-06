# Native D3D9 fixed-function transforms and lights, 2026-10-07

Source `e86e0c50a55d05e6ef78e06fcc106e1e7e4adf49` implements typed native
transforms, material and owned lights using the embedded DXVK renderer.
The committed native ARM64 fixture and five compiled semantic controls
independently verify. All six consolidated CI jobs pass, and the exact ARM64
candidate passes two target GPU runs with diagnostics enabled and disabled.
Ordinary system-runtime DX8-DX11 acceptance remains open.

## Native contracts

SetTransform/MultiplyTransform accept VIEW/PROJECTION, texture0..7 and
world0..255. Inline matrices and material fields are copied before callbacks.
The renderer reuses its existing fixed-function transformation and lighting
implementation. Matrix multiplication preserves the supplied-matrix times
current-matrix order, including noncommuting translation and scale.

CreateLight maps arbitrary runtime indices to reusable compact renderer slots.
New lights have the standard disabled directional-white default. SetLight
follows the actual SDK enum ENABLE=0/DISABLE=1/DATA=2; it does not treat these
values as independent bits. Point/spot/directional DATA copies all fields after
device serialization and before callback pumping, preserving enabled state.
ENABLE/DISABLE do not inspect the unused light-data pointer. Unsupported enums,
foreign/stale light indices and a ninth active light reject.

Private enable and retirement state changes only after exact backend success.
Creation failure or exception rolls back the owned index; retryable destruction
retains it. DestroyLight disables before retirement, and DestroyDevice drains
enabled lights on the worker before releasing backend storage.

## Verified committed native contracts

Positive fixture:202248checks, compile7.1385361s, run0.1321767s; executable SHA256
`aa8336c351f5e98bcbe592f0121721b02c6509e36ba4f43b94a265587e48c267`.
The independent verifier matches56Git/archive/after-run inputs, native ARM64
PE/static runtime imports and unchanged signed58624/oem17/desktop metadata.

| Production mutation | Native assertion caught |
| --- | --- |
| Dispatch MultiplyTransform as SetTransform | Check1827,line2039 detects the lost multiply operation. |
| Reread pointed light DATA after callbacks | Check6714,line2096 detects the changed light snapshot. |
| Use a sparse runtime index as the renderer slot | Check8219,line472 rejects the out-of-bounds slot. |
| Keep a retiring light enabled | Check12409,line2133 detects the enabled retired slot. |
| Publish private enabled state after S_FALSE | Check19086,line2174 detects incorrect active-light capacity. |

All five accepted controls compile and fail their intended assertion with exit1.
The last two preserve every assertion and change only the CHECK failure exit
from std::exit to std::_Exit in their separately archived control harnesses.
This avoids static device teardown after an already recorded failure. The
independent verifier checks that exact additional harness mutation; the
committed positive and CI fixture remain unchanged. Earlier retirement-control01
fails strict compilation, and publish-control01 reaches the intended assertion
but times out during failure teardown. Both attempts are retained separately
and excluded from accepted controls. Uncommitted preflights01/02 also retain
their distinct source provenance; all56final preflight02 inputs match the commit.

## Verified target GPU pixels

Twenty-one stages38-58 verify1344new pixels with independently calculated
checksum `b3416e11`, total3712including the earlier2368clear/draw/shader/
texture/buffer/depth pixels. An independent CPU oracle calculates the row-vector
matrix/viewport geometry and vertex lighting before GPU execution.

Transform cases cover identity, world translation, view/projection composition,
and noncommutative world/view/projection multiplication. Lighting cases cover
emissive material, disabled defaults, directional normal response, DATA while
enabled, enable/disable, simultaneous lights, sparse destroy/reuse, point range
and spot cone changes. One enabled owned light remains for actual worker
DestroyDevice cleanup. Readbacks preserve CPU row padding and outer guards.

| Target run | Diagnostics | Verified pixels | Duration |
| --- | --- | --- | --- |
| fixed-31 | Enabled | 3712, including1344fixed-function pixels | 13.1856983 s |
| fixed-32 | Disabled | 3712, including1344fixed-function pixels | 20.6108307 s |

Both independently verify eight payload hashes, three frozen wrapper hashes,
native ARM64 architecture and the Limited USER/session1 token. Each run has
59nonempty render callbacks, context1/1, allocation14/14, lock13/13,
residency14/14 with zero remaining references and zero wrong-thread callbacks.
Both owned tasks are removed with exit0. Adreno830 uses the exact matched
Mesa8443c71 runtime; no device-loss or command-submission failure is recorded.

Fresh before/after inspection retains signed58624/oem17, binding0002/PnP0 and
the exact installed SYS hash. DWM1552/Explorer6464 and their start times remain
unchanged; all58selected failure/reset/epoch/timeout/admission fields have zero
delta. This bounded result leaves earlier installed Mesa D3D10 Explorer faults
and long-term desktop stability open.

Retained evidence archive SHA256:

- fixed-31: `bcf64841d5b1d3581d8034f149f54321637480bcc06e54704799d9963d7f4a08`
- fixed-32: `57a474880e3e42dd9f9bd5499e9398f6cdbbb30aa01553e74cf2447e3b941100`

## CI and retained evidence

The single automatic consolidated native [CI run37520111683](https://github.com/sunflower2333/dxvk/actions/runs/37520111683)
passes all six jobs. All five raw ZIP digests and exact source links verify.
ARM64/x64/x86 embedded renderer builds each pass202248device checks;
14actual native ARM64 execution hashes match. Linux sanitizer and semantic
controls pass. Root refreshes the live CI result after both GPU runs.

ARM64 artifact11440226149 ZIP SHA256 is
`33b7b06ed6d98cd46374e5833a008d9f08663e453b5abf6a269e35c6f6a1c91c`.
The staged DLL SHA256 is
`3b3863a505095dcb80f191647c0a777f5a4ca68f6eebc80ac006c7f5a778ddf0`;
probe SHA256 is
`cfbb7c79bb251356e9af2143f2948598a4ffc60816acc23d9762b23821cfad4a`.

Workspace evidence: `artifacts/dxvk-native-d3d9-fixed-function-20261007/`.
`native-fixtures-verification-01.txt` links the committed positive and controls;
`fixed-pixel-oracle-01.json` retains independent pixel expectations;
`fixed-runner-scripts-01/` retains three native-parsed, hash-matched wrappers.
`ci-logs/full-verification.txt` records the user-requested build verifier's
complete handoff; `fixed-31/` and `fixed-32/` retain payload, pixel, archive and
readiness proofs. Production caps/exports and paired package pins stay closed.
SYSTEMMEM/managed buffer transfer, remaining state/resources/queries,
presentation/reset and ordinary DX8-DX11 runtime gates remain active.

Local Microsoft references are retained under workspace
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/`,
especially SetTransform/MultiplyTransform/SetMaterial/CreateLight/SetLight/
DestroyLight and their native structures. The target SDK enum is authoritative
where the SetLight prose incorrectly describes the values as bitwise OR flags.
