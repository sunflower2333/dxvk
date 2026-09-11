# ARM64 vertex-input and dynamic-map follow-up probe

Source: 272a067d2b71a22f65847c274485684ea5b89be6.
CI: https://github.com/sunflower2333/dxvk/actions/runs/34599288562 — all jobs passed.
Directory: `.planning/dxvk-umd/probe-272a067-arm64/` in this checkout.

| File | SHA256 |
| --- | --- |
| viogpudxvk.dll | 69c1531c569f64364becf0ae1198b45694942d55d06ecd7243b3eaeb2bf49d84 |
| dxvk-umd-backend-probe.exe | 933e050415f9e9525bc3624e3420c419bc840e75a2a8c7c3c8aad610674009c8 |
| dxvk-umd-ddi-probe.exe | d1f6b086322de6b7f052664cb30f7eb2e2dc2d10e3a753301159db893fdf1fa9 |

Use the same three-file isolated-folder procedure and LUID argument described
in hardware-probe-b47f955.md. Never mix a probe executable and development DLL
from different builds. This follow-up also requires a real vertex buffer,
register-based input layout, correctly typed vertex shader and dynamic
MapDiscard/Unmap upload; depth/stencil/pixels/native-copy success criteria are
unchanged.

Parent's hardware sweep `seven-dxvk272-backend-01` and
`seven-dxvk272-ddi-01` passed on58386 with process-local56bd30c ICD9AA5.
Backend exit0/224ms verified4096magenta pixels; native DDI exit0/195ms
verified64-byte copy, occlusion0/4096/0,4096red pixels/event1/error0 and
actual KMT allocation publication4096pixels. All mismatches0 and exact
binary hashes matched. DWM2028/Explorer5780 retained, no new appfault or
timeout. Parent's host trace seven-api-round-01 reports84873lines/zero loss,
GPUfault0, all observed submissions retired/pending0, dmesgGPUfault/timeout
before0/after0. DXVK timed context lifetimes39:14=1/1 and39:16=3/3 retired;
these are host observations, not GPU utilization measurements. Post-API
Explorer sweep3/3 retained2028/5780 and Application108540 unchanged.
These are standalone/backend/DDI/backing results, not Microsoft runtime
activation, native application acceptance or display Present evidence.

Parent's current hardware sweep uses freshly observed LUID
`2A58000000000000`; both probes must use that same eight-byte memory-order
identity and a matching Turnip ICD. LUIDs can change after restarting Windows,
so refresh it on the parent side if the VM restarts before this test.

This source is newer than paired package60005, which intentionally preserves
the validated b47f955 depth checkpoint. Neither standalone set changes the
installed Mesa UMD, driver registration or display path. Parent owns all
device execution and package pairing decisions.
