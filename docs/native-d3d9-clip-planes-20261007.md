# Native D3D9 clip planes and vertex-input comparison, 2026-10-07

Source `ae61dded3d2ba9b93327abf1c9ea46bb981d7717` fixes stale vertex-input
layout reuse and passes homogeneous clip-plane rendering with the original
compact FLOAT3 POSITION at offset0, D3DCOLOR at offset12, stride16 layout.
Both target GPU runs pass all640 clip pixels and all4224 earlier pixels,
plus1056 buffer-transfer bytes. Ordinary Microsoft system-runtime DX8-DX11
hardware rendering and presentation remain the active acceptance gate.

## Native contracts and the rendering defect

Typed SetClipPlane snapshots all four coefficients before runtime callbacks,
accepts indices0-5, validates device status and preserves worker ownership.
The earlier exact-source native fixture and four compiled semantic controls
verify coefficient snapshots, bounded indices and reset/reentry behavior.
The full ae61dde CI fixture passes220595 checks on ARM64, x64 and x86.

The compact GPU probe originally failed even with clipping disabled: every
stage67 pixel retained the background. Read-only diagnostics narrowed the
failure to the color offset. Offset24 passed with and without a NORMAL input;
offset12 failed at the same stride28; offset16 drew the float1 padding bits
from offset24. Normalization and clip-enable refresh did not change the failure.
Every original failed build/run and full pixel log is retained.

DxvkGraphicsPipelineVertexInputState::eq incorrectly replaced an earlier
inequality with the final divisor comparison. Missing fixed-function shader
inputs produce a zero-divisor INSTANCE binding. A matching divisor could
therefore erase differences in the layout header, bindings or attributes.
A later matching divisor could also erase an earlier divisor mismatch.
MSVC's same-bucket unordered_map lookup consults equality without requiring
matching full hashes, so the defect can reuse a different cached input layout.

The production comparison delegates to a shared portable helper. Its algorithm
retains every original field comparison and adds the missing `&& eq` to the
divisor loop. The342-check fixture covers metadata, each binding/attribute/
divisor field, both comparison directions, unused storage, pointer independence
and distinct layouts in a forced collision cache. GCC/Clang sanitizer positives
pass. Removing exactly that guard produces the expected assertion failure.
Native MSVC ARM64 positive342 and the compiled guard-removal negative also
verify, including original executables, COFFs, strict flags, static KERNEL32-only
imports and70 primary plus15 Vulkan dependency input hashes.

The accepted probe is byte identical to committed25a9edd: the compact layout,
geometry, clip coefficients and rational pixel oracle are unchanged. The two
ae61dde target passes establish that the comparison fix resolves this workload.
No particular historical cache-key collision pair was captured.

## GPU acceptance

The independent CPU oracle computes `A*x+B*y+C*z+D*w >= 0` with exact rational
sample coordinates. No tested sample lies on a clip edge. Stages67-76 cover
disabled planes, updates, opposite half-spaces, sparse indices0/3/5,
intersections, z coefficients and constant accept/reject planes. Visible counts
are64/32/32/32/16/64/0/0/64/64, checksum`ac340bd5`.

| Target run | Diagnostics | Total pixels / transfer bytes | Duration |
| --- | --- | --- | --- |
| clip-37 | Enabled | 4864 / 1056 | 33.4559408 s |
| clip-38 | Disabled | 4864 / 1056 | 28.9399387 s |

Each independently verifies every640 raw clip pixel, all prior gates,
transfer512pixels/`49865e45` and1056bytes/`5bd18a08`, eight exact payload hashes,
three native-parsed wrapper hashes and native ARM64 Limited USER/session1.
Each has88 nonempty render callbacks, context1/1, allocation15/15, lock14/14
and residency15/15, with zero remaining references and wrong-thread callbacks.
The owned tasks complete successfully and are removed.

Fresh before/after checks retain signed58624/oem17, binding0002/PnP0, exact
installed SYS and DWM1552/Explorer6464 with their start times. All58 selected
readiness/reset/failure/admission fields retain zero delta. Eight local acceptance
helpers were frozen before execution and remain unchanged after both runs.
Root separately recomputes all raw clip pixels from the rational half-spaces
and joins both runs to exact-source CI, payload, archive and continuity receipts.

Evidence archive SHA256:

- clip-37: `d8edab340ad709e80dacc04ad31c34fb35f77e605dc0ac84ca798dd7db170eed`
- clip-38: `724071945210a5d4fd40fd3600b373a59bf0850c63c5c1a6a5e77bdf502f4387`

## Exact CI and artifacts

The single automatic [CI run37541475016](https://github.com/sunflower2333/dxvk/actions/runs/37541475016)
passes all six jobs. The build verifier independently checks all five raw ZIP
digests, all three architecture builds,15 actual native ARM64 execution hashes,
the comparator positive/negative controls and private renderer compilation/linkage.
Root reviews and reruns the native verifiers, checks every ZIP member against its
extracted bytes and refreshes live CI after GPU acceptance. Redundant push
workflows remain disabled.

ARM64 artifact11448817367 ZIP SHA256:
`d3374098b62022f017f648225743f7ee0abef55e16cc45173cd7f718b44f48b3`.
Staged DLL SHA256:
`41c0c9503dad513b967a434130eaed81527233fd0dc4bf1f91be43b320e6db18`.
Staged probe SHA256:
`b9c2dc84f31f0eab19881451009eb573d688450e773515a3cefaecbb1e156d20`.
Installed SYS remains:
`d48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a`.

Workspace evidence is under
`artifacts/dxvk-native-d3d9-clip-planes-20261007/`. The top-level
`root-clip-checkpoint-verified-01.json` joins acceptance;
`vertex-input-equality-01/root-final-ci-verification.json` joins five ZIPs;
`vertex-input-equality-01/guest-native-01/root-native-equality-verified.json`
retains the native positive/negative proof. Frozen helper set
`local-clip-verifiers-03/`, rational oracle and clip-37/38 directories retain the
complete independent evidence. Earlier diagnostics and superseded candidates
remain explicitly separate.

Production caps/exports, registration and paired package pins remain closed.
Typed query completion/occlusion/timestamp development continues next, followed
by remaining state/resources, presentation/reset and ordinary DX8-DX11 acceptance.
The earlier installed Mesa D3D10 Explorer faults and long-term desktop stability
remain open beyond these bounded checks.
