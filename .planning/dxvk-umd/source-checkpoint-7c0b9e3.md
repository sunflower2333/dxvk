# DXVK native GS checkpoint

Production source: `7c0b9e3007c7b8beab626511c6aad720cdf54c09`, personal branch
`viogpu-umd-20260911` at `sunflower2333/dxvk`.

CI: https://github.com/sunflower2333/dxvk/actions/runs/34620365858 — all five
jobs pass. Three production architectures compile/link and pass PE/import/
export checks. x86/x64 execute 2,257 shader checks, including original/rebuilt
VS->GS->PS WARP payloads, GS per-vertex SPIR-V input arrays, and 488 view/mip
checks. Linux ASan/UBSan covers 69 identity, 176 runtime identity and 147
shader cases.

This closes native CreateGeometryShader, GsSetShader, GsSetConstantBuffers,
GsSetShaderResources and GsSetSamplers. VS->GS preserves generic raw32 and F32
position; GS outputs follow the active PS types. Native union signatures may
contain unused input registers. GS execution in the independent WARP oracle
requires a b1 XOR mask and a sampled texture; all 16 expected payload words
match, including NaN, negative zero and infinity.

ARM64 GitHub artifact `10272751005` is in `probe-7c0b9e3-arm64/` beside this
file. Its STATUS identifies this exact source and ARM64 architecture.

| File | SHA-256 |
| --- | --- |
| viogpudxvk.dll | 283173e21d9dbf6e30f80370076239eb6d271529dea3e1a1736ae6170efd204c |
| dxvk-umd-ddi-probe.exe | 1ce4f77038718d37feda32545b2d33b8f84dc08287de8d544f79f902a549b3eb |

x64 artifact: `10271659827`; x86 artifact: `10272656621`.

The native GS DDIs have not run on the target. This is not a signed system
driver or ordinary Microsoft runtime activation. No new backend GPU workload
is requested. Existing frozen hardware artifacts and the active driver remain
untouched. Parent owns joint KMD/UMD packaging and device operations.

Remaining work includes stream-output declarations/passthrough, actual output
counters/DrawAuto, native sharing/opening, further shader/resource support and
OpenAdapter/version/capability negotiation. The D3D10 table inventory retains
10 unset fields (including version-dependent hooks). Do not register or
advertise a complete feature level until its contract is implemented. Ordinary
system-runtime acceptance uses the parent's f6da604 probe documented in
`docs/native-direct3d-runtime-validation-20260911.md` at workspace root.

The mip-generation checkpoint `6dfb092` passed CI34619152294. Independent
API-only WARP controls localized its prior no-op to FirstArraySlice1; the
strict CPU oracle now tests slice0/mips0-1 and every untouched subresource.
Nonzero-slice DXVK mip generation still requires target proof.
