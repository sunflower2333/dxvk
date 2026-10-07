# Native DX10 and DX11 controls, 2026-10-07

Exact source `affe7d43161a6a270e8779e9957de150e156617c` passed nine controls on
native ARM64 Windows using MSVC 19.44.35229.0. Source inputs came from 47 exact
repository Git objects and 24 files at dxbc-spirv commit
`213d2b859e83d91670ada15cc773e2c90c7c8b61`.

| Control | Checks |
|---|---:|
| Typed D3D10.1/D3D11 resources, views, UAVs and compute | 308 |
| Compute shader container | 35 |
| Production entry with incomplete backend contract | 2637 |
| Production entry and complete-contract lifetime | 10834 |
| DXGI resource rotation | 1428 |
| MRT drawing, sparse bindings and blend state | 6404 |
| Texture1D | 1629 |
| Texture transfer formats and bounds | 1144 |
| Runtime dispatch and retirement | 852 |

Root review independently joins the original 202-member evidence archive,
source/archive/build/after-run hashes, official 21 SDK headers/seven link
libraries, compiler/linker, 25 COFF objects, nine ARM64 PEs, all 22 bounded native
stages and the retained System32 DLL, installed SYS and desktop identities.
The production/fixture `/W4 /WX /MT /Zc:preprocessor` build has zero warnings.
The pinned dxbc dependency uses its separate `/W3` policy and retains 61
warnings without source mutation.

Original archive SHA256:
`3881b1d6b9a1d52bf3c86f81a7c1ca4e94097b8ef8aa2ffa0d83f749e56dd78b`.
Root proof is
`artifacts/dxvk-native-dx10-dx11-20261007/guest-warp-affe7d4-01/root-native-warp-affe7d4-verified-03.json`
under the workspace, SHA256
`94c37c2ff9f6f6b8b3764c40944a5b4a0eeef0bcfc08f732e09840aa3034220a`.
Earlier root review stdout parser failures are retained separately; the native
execution archive was unchanged.

These controls use WARP or controlled backends. Native Turnip rendering,
genuine DX10/DX11 system runtime activation and visible presentation remain
required. Production admission stays closed. Functional SM5 graphics,
stream-output, tessellation and class linkage are a newer separate slice.

After this WARP run, a desktop identity transition was recorded. Fresh snapshots
agree on DWM 1864 and Explorer 4464 with unchanged SYS, package and binding.
Windows System events record an unexpected restart; cause is unresolved.
Subsequent native9/frontend controls use a new recorded baseline and retain the
historical WARP before/after evidence unchanged.
