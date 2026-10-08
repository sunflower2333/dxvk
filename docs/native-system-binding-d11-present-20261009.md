# Explicit D11 Present phase in the existing binding owner

`scripts/test-system-d3d10-binding.ps1` now accepts `-D11Phase offscreen|present`,
with `offscreen` as the default. The parameter selects the expected result of
the exact separately approved probe binary; it does not change its arguments
or turn an offscreen binary into a Present binary. `present` requires API11.
The controller records the phase in its protected config and the worker checks
it before native helper, entry or factory calls. Watchdog restoration does not
depend on validating a render phase.

For `-Api 11 -D11Phase present`, ROOT must select the separately built probe from
`8d0b9876f3b4803425f7f5b698c1783953647008`, with the exact nine operands and
closed held schema retained from the offscreen probe. Its source is
`tests/umd-d3d11-system-present-probe.cpp`, 36035 bytes, SHA256
`e45dcce571db1a6cf6fe5b39cb043d425562d94be99644641cc6cabb877cb768`.
Its exact positive marker is:

```
SYSTEM_D3D11_PRESENT_VALIDATION_PASS feature_level=10_0 typed_ddi=11 pixels=512 presents=2 software_fallback=0 production_admission=0 registry_changes=0
```

The default still requires the original offscreen marker with `presents=0`.
Neither marker can satisfy the other phase. Both require the same exact held
PID/60000ms/success checkpoint and the existing retained-runner exit, census,
selection, restoration and release gates. The typed negative marker still has
nine controls. D9/D9Ex/D10 arguments, markers and default phases are unchanged.

ROOT must run the independently selected Present reader
`tests/verify-d3d11-system-present-originals.py` from the same source commit,
13929 bytes, SHA256
`5d7a959ceb0ef080288dd53c1ba79cc4adf31a25e1e318ae2aded1fe6eb450ff`,
on this attempt's exact eight raw originals and separate stdout/held/process
originals. That reader verifies the window, swapchain and both exact successful
Present calls. The controller's marker gate alone does not certify these.
Current CI/core/payload admission and any hardware execution remain separate
ROOT gates; neither capability masks nor Mesa/default registration change here.

`tests/system-d3d11-binding-phase-controls.ps1` stages a finite Windows PS5.1
AST and actual-source pure-routing check. Run it with the controller path as
`-Source` and a fresh `-Output`; it invokes only the two extracted pure routing
functions, never the controller. Its 27 cases cover all four APIs, both legal
phases, malformed phases/API, and exact producer markers. Windows execution
remains pending ROOT. Local source inspection proves the nine operands,
closed-held protocol, raw backup/intent/restore, watchdog and complete Apply
tail remain byte-identical to frozen020f. The frozen020f CPU preflight packet
and previous controller sources remain unchanged.
