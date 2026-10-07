# Native D3D9 dynamic textures, 2026-10-07

Exact source `8d7e0686e406bc915c18bde5f260b945667b37d7` passes typed hardware
dynamic texture mip updates and discard on the existing target VM. Both accepted
runs verify 5,824 pixels and 1,056 transfer bytes. Ordinary Microsoft runtime
DX8–DX11 rendering and visible presentation remain the program gate.

Dynamic usage reaches the embedded renderer. Default-pool mip surfaces become
lockable; whole-chain discard is accepted only on an unmapped top-level whole
surface. Invalid flags, conflicting mappings and backend failures retain caller
outputs. SYSTEMMEM textures remain transfer resources and reject direct sampling
without a backend bind. A8/X8, all four native pools, metadata snapshots,
discard/update failures, retries and resource lifetime have controlled coverage.

The native target fixtures actually compiled source `0dd6f6f` and pass 42,309
adapter / 521,942 device checks with zero warnings under MSVC `/W4 /WX`.
The `8d7e068` change only renames two probe-local sampler variables; all 87
fixture inputs are independently byte-identical. A fresh strict native build
also verifies the corrected probe. The evidence preserves both source identities.

The single automatic [CI 37567440170](https://github.com/sunflower2333/dxvk/actions/runs/37567440170)
passes all six jobs. Root independently verifies five original ZIPs, three PE
architectures and 15 actual native ARM64 execution payloads. Four unused
workflows remain disabled; offline and system-runtime controls are manual-only.

An independent oracle was frozen before execution. Twelve 8×8 frames cover
two updates of all three mip levels for A8 and X8 textures. Every raw pixel
matches checksum `bfeeeac5`; A8 preserves alpha and X8 samples opaque alpha.
All earlier clear/draw/shader/texture/buffer/depth/fixed/transfer/clip/query
checks remain required.

| Accepted run | Diagnostics | Pixels / bytes | Duration | Probe deadline |
| --- | --- | --- | --- | --- |
| dynamic45 | Enabled | 5,824 / 1,056 | 46.3889 s | 65 s |
| dynamic47 | Disabled | 5,824 / 1,056 | 72.2869 s | 105 s |

Each accepted run has 108 successful render callbacks, context 1/1,
allocation 19/19, lock 18/18, residency 19/19 and zero remaining residency or
wrong-thread callbacks. Eight exact ARM64 payloads and three native-parsed
scripts match. Both Limited USER/session1 tasks are removed with exit 0.
Signed driver 58624/oem17, the exact installed SYS, DWM1552/Explorer6464 and
all 58 selected readiness/reset/failure/admission fields retain their values.

Original failed attempts remain separate. The first host review of dynamic45
omitted the unchanged earlier buffer-transfer oracle; adding that preexisting
file allowed the byte-identical verifier to accept the original evidence.
Dynamic46 hit the 65-second process deadline while still progressing through
clip readbacks and remains failed. Dynamic47 changes only the bounded deadlines
and isolated script paths; every binary, pixel oracle and cleanup check is unchanged.

Evidence is under `artifacts/dxvk-native-d3d9-dynamic-20261007/`.
`root-native-dynamic-checkpoint-verified-01.json` joins native source equivalence,
original CI/candidate/script proofs, both accepted hardware runs and failed46.
Original archives: dynamic45 `4c0a869774ccec49a00c5e42c20d333d095052ae6951d65d5f0ac1ec569eaabf`;
dynamic47 `4bbcd51e31e325d4d8a6d5db7748e6a08c69432f76cfe9315ae88e968dab7aee`.

This is typed offscreen GPU acceptance. No registration, installation, ordinary
production runtime admission or screen presentation is accepted here.
