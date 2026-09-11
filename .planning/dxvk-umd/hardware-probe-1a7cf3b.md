# Native texture array and MSAA resource checkpoint

Engine: `1a7cf3bfbb6e1ad353ed6244375591395f7c61c2`.
CI34608704564 ALL5PASS, including ARM64/x64/x86. Independent Microsoft WARP
native view/range/MSAA fixture passes246checks; original/rebuilt shader
fixture retains887checks. ARM64 artifact10267702456,3603484bytes, archive
SHA256`492e25930aab56626791323de2dd76f9d4ee2e0d8b42f64bb075716e558d3c94`.

Only these matched files are needed from`.planning/dxvk-umd/probe-1a7cf3b-arm64/`:

| File | SHA256 |
| --- | --- |
| viogpudxvk.dll | 0052c9c275f44fa9a72db182173d17a09171d3531fd4f250d705944f0b0edb67 |
| dxvk-umd-ddi-probe.exe | e1d7d4f434d800c2eea26e152855c8545187951140140107835f2d29d2bff079 |

Parent runs the usual`<fresh 16-hex memory-order LUID> --native-copy` with
the current process-local ICD. No installed UMD replacement or registration.
Require all precedingb8shader/depth/copy/KMT-publication checks and the new
`DDI_MSAA_RESOLVE samples=4 src_slice=1 dst_slice=1 dst_mip=1 pixels=640 mismatches=0`.
Exit17 means the new native MSAA section failed; original final-image failures
retain exit7 and linkage diagnostics.

The source extends 2D SRV mip/array ranges and typed views of typeless resources,
MSAA SRV/RTV dimensions, explicit format-cap mapping, native quality-level
queries and typed MSAA resolve validation. The probe creates a4xMSAA array,
clears its two slices with different colors, resolves only slice1 into
destination slice1/mip1, rejects an invalid late resolve, then checks both the
selected pixels and untouched neighboring subresources. WARP independently
samples relative coordinates from a view selecting absolute slice1/mip1.
Typeless resolves, other resource dimensions, full native runtime and visible
Present remain unfinished.

Parent targetseven-dxvk1a-resources-04 PASS, exit0/259ms. MSAA resolve640pixels
and draw4096pixels have zero mismatches; depth/stencil/copy/KMTpublication
also PASS. OS LUID5805, process-local ICD9AA5, KMD58386 after58422 Code43
rollback. Original DWM1036/Explorer5640 retained and task removed. Parent
host trace started before execution; correlated trace closure remains pending.
