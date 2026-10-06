# Native D3D9 texture development, 2026-10-07

The closed-admission development device adds coherent static2D A8R8G8B8 and
X8R8G8B8 mip chains. One worker-owned DXVK texture supplies the indexed mip
surfaces; resource metadata, runtime handle reservation and fresh identity
checks precede publication. Partial creation releases surfaces before their
parent. Cube/volume, compressed formats, dynamic/autogen and sharing remain
outside this slice.

Pixel sampler slots0–15 and vertex slots257–260 use device/type-safe resource
tokens. Deletion unbinds every slot and any render target, flushes, then retires
the mip surfaces and parent on the worker. Retryable failures retain ownership;
device destruction drains remaining bindings and locks. Static video textures
cannot be locked through the public renderer path; system levels preserve
caller-owned backing, pitches and NotifyOnly locks.

Native texture-stage state explicitly maps fixed-function D3DTSS and D3DSAMP
values. Fixed-function stages remain0–7. Native colorkey states and TEXTUREMAP
are rejected rather than interpreted as public enum values. The displacement
sampler remains outside this slice.

TexBlt matches the destination's top dimensions to a corresponding source mip
and copies every common level. It validates the complete copy before starting
callbacks and snapshots cropped caller-owned system pixels with their original
pitch. Individual-level Blt remains available. Creation metadata is prepared
after device serialization, so reentrant callbacks cannot read an in-progress
surface list.

The native controlled fixture covers caller mutation, malformed chains,
partial creation, null/S_FALSE replies, common mip copying, padding, state
mapping, stale/foreign handles, reset/reentry and retryable unbind/flush.
Exact source `787419696fc828bdc0f1c9d7d5337135fce0c82f` passes 134,906
checks in the existing ARM64 Windows guest. Compilation took 5.8650081 s;
execution took 0.0783895 s. All 56 source inputs, the archive, after-run
source hashes, native ARM64 PE, static runtime imports and desktop/driver
continuity independently match the committed source.

Four separately compiled production mutations exit 1 at their intended
checks; compilation succeeds for every control:

| Mutation | Rejected check | Meaning |
| --- | --- | --- |
| Reread upload backing after a runtime callback | 6909, line 1303 | The original caller snapshot must reach both mip copies. |
| Copy only the first common mip | 6907, line 1302 | Differently sized chains still copy every common level. |
| Omit postcreation identity validation | 21136, line 1376 | A removed device cannot publish a resource handle. |
| Retain a bound texture during destruction | 4216, line 209 | All bindings and mip views must retire before their parent. |

Full CI [37498655517](https://github.com/sunflower2333/dxvk/actions/runs/37498655517)
passes all six jobs. Offline CI
[37498596397](https://github.com/sunflower2333/dxvk/actions/runs/37498596397)
passes all four jobs; API 37498554766 and package 37498554868 also pass.
Retained ARM64/x64/x86 artifacts independently verify PE/import/export
contracts, closed production admission, 134,906 device checks per architecture,
42,429 adapter/49 backend/45 rejection checks and 14 actual ARM64 execution
hashes. The earlier 4beab5c probe compile failure is retained: Windows defines
`small` as a macro. Source 7874196 renames that local to `smallDestination`
without changing production textures or the pixel oracle.

The `--texture` GPU oracle retains the previous576 clear/draw/SM1–3 pixels and
adds512 pixels over eight sampling cases. It checks three source levels, a
smaller two-level destination, point/linear filtering, border/clamp/wrap and
unchanged caller padding. Texture checksum is `d3afb9c5`; total 1,088 pixels.
The exact CI ARM64 DLL and probe pass both fresh target runs with matched
Mesa 8443c71 and the installed signed 58624 driver:

| Run | Mesa diagnostics | Duration | Pixels | Nonempty GPU submissions |
| --- | --- | --- | --- | --- |
| texture25 | 1 | 4.2857411 s | 1,088 | 17 |
| texture26 | 0 | 6.5034715 s | 1,088 | 17 |

Both independently verify 192 clear pixels (`ffefa655`), 192 nonindexed draw
pixels (`53a03d45`), 192 SM1–3 shader pixels (`1384c5a5`) and 512 texture
pixels (`d3afb9c5`). Contexts 1/1, allocations 13/13, locks 12/12 and residency
references/evictions 13/13 balance, with zero remaining residency or
wrong-thread callbacks. All eight payloads and three runner scripts match
retained hashes. Both tasks use the Limited USER token in session 1 and are
removed with exit 0. Fresh before/after records retain the exact
58624/oem17.inf/binding0002/PnP0/SYS hash, DWM1552/Explorer6464 and all 58
selected failure/reset/epoch/timeout/admission values through these runs.
Explorer6464 is a fresh baseline; earlier Explorer2488 crashed in the
installed Mesa D3D10 UMD at the same recorded c0000005/RVA10db80. That
separate stability defect remains open.

Evidence is under workspace `artifacts/dxvk-native-d3d9-textures-20261007/`.
The immutable target archives have SHA256:

- texture25: `0d6c57ad8bc2cb2f3adae0f3a9e883da5bc8b57bdda5c6d9b27f792e36f7db69`
- texture26: `0074886a0025fd89434b5668f0e14e547bc02c215b45f49418680ed3b9e9b3e3`

These runs establish typed offscreen texture/mip/sampler rendering. Vertex
and index buffers, remaining drawing/state/resource contracts,
presentation/reset and ordinary DX8–DX11 runtime acceptance remain open.
Production exports/caps and paired binary pins remain closed.

The user-provided EWDK ISO is visible at
`C:\Users\USER\EWDK_br_release_svc_im_28000_260714-1640.iso`. Its current
17,912,889,344 bytes are below the ISO descriptor's 19,785,189,376 bytes;
the transfer has stopped. It has not been mounted or used for a build.
The native fixture above uses the already verified portable MSVC14.44/26100
toolchain. A separate EWDK production KMD runner is prepared; complete-image
location and tool inventory are the next build prerequisites.

Use the local Microsoft DDI references under workspace
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/`:
`d3dumddi/nc-d3dumddi-pfnd3dddi_createresource.md`,
`d3dumddi/nc-d3dumddi-pfnd3dddi_texblt.md`,
`d3dumddi/ns-d3dumddi-_d3dddiarg_texturestagestate.md`, and
`d3dukmdt/ns-d3dukmdt-_d3dddi_surfaceinfo.md`.
The inspected documentation revision is7515063cea4c9e98db6a92986c5b4ddb0463fd16.
