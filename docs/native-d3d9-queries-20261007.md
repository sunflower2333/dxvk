# Native D3D9 GPU queries, 2026-10-07

Source `0a605643a78306a101350ec88fca51cd0974cbc2` implements device-owned typed queries and passes
both actual target GPU runs. Each verifies nine query completions, 192 new
query pixels, all4864 prior pixels and1056 transfer bytes. Ordinary Microsoft
system-runtime DX8-DX11 hardware rendering and presentation remain the gate.

## Native query contracts

The native DDI maps VCACHE4, EVENT8, OCCLUSION9, TIMESTAMP10, DISJOINT11 and
TIMESTAMPFREQ12 explicitly to the embedded core. Native Begin1/End2 are the
opposite of the core COM flags. Unsupported types return NOTAVAILABLE.
VCACHE is unavailable on matched Adreno830 and its output handle stays unchanged.

Each query has a private renderer owner and a monotonically unique device
handle. Arguments and output addresses are saved before callbacks. Unissued
and begun queries return S_FALSE with untouched caller storage. Ended GetData
uses owned aligned storage; only completed S_OK data is published after a fresh
runtime identity/reset check. Pending S_FALSE remains pending and preserves
all output bytes. EVENT publishes a complete four-byte BOOL even when the
core's cached result writes a one-byte bool. Other DDIs retain exact-S_OK rules.

Creation, issue, destruction and device teardown run on the serialized backend
worker. Failed destruction can be retried; stale and foreign handles fail.
The controlled native fixture covers callback reentry, caller metadata mutation,
unaligned/guarded output, pending/failure/exception paths, cached EVENT, device
retirement and post-result reset checks. It passes272881 checks.

Six separately compiled one-expression controls all compile without warnings
then fail their intended assertions: reversed flags3651/2618, pending status
3153/2596, pending output3982/2627, caller address reread5477/2639, short EVENT
BOOL11433/2639 and missing post-result epoch36384/2691. Every original source,
COFF, executable and output is retained. No acceptance assertion is weakened.

## Target GPU acceptance

An independent exact-rational CPU oracle is frozen before execution. Three
compact vertex draws use a full8x8 viewport, scissor{1,2,5,6}, and an all-reject
homogeneous plane respectively. Expected occlusion counts are64/16/0 and all
192 raw pixels match checksum `57872e55`.

| Run | Diagnostics | Pixels / transfer bytes | Query completions | Duration |
| --- | --- | --- | --- | --- |
| query-39 | Enabled | 5056 / 1056 | 9 | 35.2096953 s |
| query-40 | Disabled | 5056 / 1056 | 9 | 34.251827899999995 s |

Both runs verify six supported handles, guarded unissued/begun pending results,
three occlusion reuse results, full EVENT/cached/reuse, an ordered timestamp
pair, DISJOINT false and TIMESTAMPFREQ19,200,000. Every typed result has exact
little-endian width, retained guards, cached equality and completed null polling.
Ended GPU queries were ready on the first poll after explicit flush/readback;
ended pending-output behavior is covered by compiled native fixtures/controls.

Every earlier clear/draw/shader/texture/buffer/depth/fixed/transfer/clip gate
remains enforced, including independently recomputed640 raw clip pixels,
transfer512pixels/`49865e45` and1056bytes/`5bd18a08`. Each has96 nonempty render
callbacks, context1/1, allocation19/19, lock18/18, residency19/19 and zero
remaining references or wrong-thread callbacks. Eight exact payloads, three
native-parsed scripts and native ARM64 Limited USER/session1 match; both owned
tasks finish successfully and are removed.

Fresh before/after checks retain signed58624/oem17, binding0002/PnP0, the exact
installed SYS, DWM1552/Explorer6464 and their start times. All58 selected driver
readiness/reset/failure/admission fields have zero delta. Seven frozen query
helpers and all eight prior clip helpers remain unchanged.

Evidence archive SHA256:

- query-39: `3b92eac9c112b0a0c7a656be04a775ad6aa0140906f72d793fe2e9af72b1aae5`
- query-40: `8daa1e25c2fd499778e0523217c56c8117aa7b6858dc9a12a677b3af3fc74cc4`

## Builds and immutable evidence

The single automatic [CI37544531323](https://github.com/sunflower2333/dxvk/actions/runs/37544531323)
passes all six jobs. The build verifier checks actual query.cpp/private D3D9
core/backend compile/link logs, all three architectures,272881 device and342
comparator checks per architecture,15 actual native ARM64 executions and five
raw API size/digest/CRC/source-matched ZIPs. Root verifies all original ZIP
members and native binary/stdout/stderr links. Four unused workflows are disabled;
the offline/runtime controls remain manual-only.

A fresh committed native CPU build freezes72 primary plus15 pinned Vulkan
headers and verifies all87 Git/archive/build/after-run hashes. The controlled
fixture compiles its required source subset and the probe strictly; real embedded
renderer compilation is verified separately in CI. Root reviews/reruns native
positive, six controls and PowerShell proofs, preserving the originals.

ARM64 ZIP11450142911: `1c552b8f9a4f5999f0705f51b856a2c618fdfd404109b63359bce08b108f0507`.
DLL6017024 bytes: `f8c66b3138206aa6caa180f3dab3857bed48cb2899a2e58b8e68024873c88167`.
Probe217088 bytes: `39feb0c0e0f723711c4dba7315332185d8d07e9f3c3ebf9730495f927e4c6bb0`.
Installed SYS: `d48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a`.

Workspace evidence is under `artifacts/dxvk-native-d3d9-queries-20261007/`.
`root-query-checkpoint-verified-02.json` joins both GPU runs, rational pixels and
native/CI proofs. `root-query-ci-verified-03.json` joins all five raw ZIPs and
15 native executions; `full-ci/_independent-final-verification.json` retains
build-agent closure. The original failed local review attempts are retained:
UTF-8 BOM/CRLF parsing and an overly broad all-active-workflows assumption were
corrected in separate review helpers without changing any original result.

This checkpoint remains typed offscreen GPU acceptance. Production caps/exports,
registration and paired package pins stay closed. Continue remaining resource/
state coverage, typed presentation/reset and ordinary DX8-DX11 acceptance. The
separate zero-copy display and long-term installed Explorer stability gates remain.
