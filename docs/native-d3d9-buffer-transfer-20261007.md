# Native D3D9 SYSTEMMEM buffers and BufBlt, 2026-10-07

Source `0d585e2318266f0f910fc758b997e7e6defcc897` adds borrowed and owned
SYSTEMMEM vertex/index buffers and typed byte-range BufBlt through the embedded
DXVK renderer. The committed native ARM64 fixture and five compiled semantic
controls independently verify. All six consolidated CI jobs pass, and the exact
ARM64 candidate passes two target GPU runs with diagnostics enabled/disabled.
Ordinary Microsoft system-runtime DX8-DX11 acceptance remains open.

## Native contracts

Creation snapshots borrowed initial bytes before runtime callbacks. Borrowed
SYSTEMMEM buffers require NotifyOnly locks; owned SYSTEMMEM buffers use ordinary
locks. Writable borrowed unlock and BufBlt snapshot their input range before
callbacks can change the backing bytes. Read-only locks copy only their requested
output range and do not upload caller bytes. Retryable failures retain the lock.
DestroyDevice closes locked resources without rereading expired borrowed data.

BufBlt accepts device-owned, unlocked vertex or index buffers, with independent
64-bit source/destination bounds checks. It supports raw VB-to-IB transfer,
SYSTEMMEM/default pools, same-buffer overlap in either direction and bounded
readback. Zero-byte copies still validate resource ownership and device status.

The native renderer flushes staged source and destination writes before copying.
It reuses DXVK's overlap-safe temporary copy path, writes changed destination
bytes back to mapping storage and joins SYSTEMMEM writes before dynamic per-draw
CPU uploads. DEFAULT locks retain deferred synchronization. Native buffer usage
explicitly permits both transfer directions, including dedicated allocations.
The public COM renderer's buffer allocation policy is unchanged.

## Verified committed native fixtures

Positive worktree03 passes 216868 checks: compile 7.2410118 s, execution
0.1137467 s. Its executable SHA256 is
`0204e339b3356037ea50757bc5df3a5ac60bcb976dedfe20d7187212be2371a3`.
The independently verified evidence archive SHA256 is
`aab6f0b71e8aa33da87f9d1ccdda5c17854569d9f882f5ecc6c1399d159ac964`.

| Production mutation | Native assertion caught |
| --- | --- |
| Reread borrowed creation bytes after callbacks | Check 1993, line 1810 detects the changed mapping. |
| Reread borrowed BufBlt source after callbacks | Check 5005, line 1862 detects the changed copy snapshot. |
| Reread writable borrowed unlock data after callbacks | Check 2895, line 1810 detects the changed mapping. |
| Omit destination range validation | Check 5901, line 289 catches the invalid destination extent. |
| Omit the borrowed-buffer NotifyOnly requirement | Check 2222, line 1817 detects incorrect lock admission. |

All five controls03 compile and exit 1 through the intended assertion. Each
separately archived control changes one production expression plus the CHECK
failure exit from `std::exit` to `std::_Exit`, preserving all assertions and
avoiding static teardown after failure. The independent verifier checks both
exact mutations. The committed positive and CI fixture retain their original
harness.

All 57 Git/archive/after-run inputs, native ARM64 PE and original strict probe
COFFs verify. The positive fixture imports only KERNEL32 statically. Installed
signed 58624/oem17 and DWM1552/Explorer6464 metadata remain unchanged.
The user-requested build subagent independently verifies preflight05 and its
57-input link; its archive is byte identical to committed worktree03.

Earlier preflight02 retains its MSVC C2975 probe compilation failure. The probe
was corrected using an outer VertexPayload alias. The earlier unlock-control01
fails a read-only upload assertion before its intended snapshot assertion; it
is retained and excluded. Fresh controls02/03 use a writable-only reread mutation.

## Verified target GPU acceptance

The frozen CPU oracle expects 512 new pixels, checksum `49865e45`, and 1056
readback bytes, checksum `5bd18a08`. Total expected pixels are 4224, including
all earlier fixed-function, depth, buffer, texture, shader, draw and clear gates.

Stages59-66 cover borrowed SYSTEMMEM-to-default transfer, owned dynamic
SYSTEMMEM upload, forward/reverse overlap, default-to-borrowed SYSTEMMEM output,
NotifyOnly writes and 16/32-bit index draws. Full byte images retain untouched
padding; caller storage retains outer guards. The vertex declaration and stream
bindings preserve component alignment when DXVK repacks dynamic SYSTEMMEM data.
Raw copy offsets, lengths and payload/oracle bytes remain unchanged.

Three frozen PowerShell wrappers pass native parsing. Five local orchestration
and independent acceptance helpers are frozen before GPU execution; root checks
their unchanged hashes after both runs. Each independently verifies 4224 pixels
and 1056 bytes, all prior gates, eight payload hashes, three target wrapper hashes,
native ARM64 architecture and the Limited USER/session1 token.

| Target run | Diagnostics | Pixels / transfer bytes | Duration |
| --- | --- | --- | --- |
| buffer-transfer-33 | Enabled | 4224 / 1056 | 19.0761805 s |
| buffer-transfer-34 | Disabled | 4224 / 1056 | 18.0587911 s |

Both use exact matched Mesa8443c71 on Adreno830. Each has 78 nonempty render
callbacks, context1/1, allocation15/15, lock14/14 and residency15/15, with zero
remaining references and zero wrong-thread callbacks. Owned tasks are removed
with exit0. Fresh inspection before/after each run retains signed58624/oem17,
binding0002/PnP0, exact SYS hash, DWM1552/Explorer6464 and their start times.
All 58 selected readiness/reset/failure/admission fields have zero delta.
Earlier installed Mesa D3D10 Explorer faults and long-term desktop stability
remain open beyond this bounded result.

Retained evidence archive SHA256:

- buffer-transfer-33: `05cdfe764d573988a95399129afecee12e1ffcf469aa82380ec0154238448fcd`
- buffer-transfer-34: `6caf99e8841b94970e0b1bd00364251e04d61ae2fd0eb30af529611b6bc7d6dc`

## CI and evidence

The single automatic [CI run37530530983](https://github.com/sunflower2333/dxvk/actions/runs/37530530983)
passes all six jobs for exact source0d585e2. The user-requested build verifier
independently verifies all five raw ZIP digests, ARM64/x64/x86 PE builds with
216868 device checks per architecture, 14 actual native ARM64 execution hashes,
57 frozen source links and Linux sanitizer/semantic controls. Original build
logs prove private D3D9 common-buffer/device/backend compilation and DLL linkage
on all three architectures. Root refreshes live CI after both GPU runs.

ARM64 artifact11444158161 ZIP SHA256 is
`1a09e3a02c2108c86b9337b940f67a166d1858c881e749a6c7c462450738a5c4`.
The staged DLL SHA256 is
`18146f8a9cc71e18d47836a6a92237842a6d6d51036dde2548e2e5ca747fbe7b`;
probe SHA256 is
`ba66af56490453588bfaf5acbb32e53792eb802b403f1a4f22cb81d62439a11e`.

Superseded sourcec8e7ff5 CI37526611880 was canceled before GPU
execution after the probe alignment correction. Its three complete ZIPs and
canceled jobs remain explicitly separate from replacement acceptance.

Workspace evidence: `artifacts/dxvk-native-d3d9-buffer-transfer-20261007/`.
`native-fixtures-verification-03.txt` verifies the committed positive and controls;
`probe-objects-03.tar.gz` retains six original compiled COFFs;
`buffer-transfer-oracle-01.json` records independent byte/pixel expectations;
`local-transfer-verifiers-03/` freezes local GPU acceptance helpers;
`buffer-transfer-runner-scripts-01/` freezes the native-parsed target wrappers.
`ci-logs/full-verification.txt` and `full-ci/_independent-final-verification.json`
retain the build subagent's complete proof. Both GPU run directories retain
archive, payload, pixel/byte and fresh readiness proofs.

Production caps/exports, registration and paired package pins remain closed.
Managed resources, remaining state/resources/queries, presentation/reset and
ordinary system-runtime DX8-DX11 acceptance remain active. Local Microsoft
BufBlt, Lock/Unlock, buffer-range and NotifyOnly references are retained under
workspace `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/`.
