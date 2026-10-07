# Native D3D10/10.1 SM4.1 shader slice

The legacy VS/GS/PS compiler previously required the exact4.0 version token and
rejected every opcode above DclGlobalFlags. A genuine4.1 program therefore
failed before the pinned decoder could process it. The compiler now accepts
exact4.0 and4.1 tokens for those three stages, preserves every original token,
and emits the existing checksummed SHDR container. The typed legacy creation
callback rejects4.1 on logical10.0 devices even when the embedded backend can
create broader shaders. Hull/domain/compute, newer models and reserved version
bits do not enter this legacy path. The native11 stage/signature path is unchanged.

Four new opcodes use the pinned decoder's existing operand layouts: LOD,
Gather4, SamplePos and SampleInfo. Reserved107/112 and SM5 opcodes remain
rejected. LOD and SamplePos require pixel stage; Gather4 and SampleInfo may
appear in VS/GS/PS. Instruction/custom-data bounds and exact token/hash checks
remain in force. The original WDK26100 d3d10TokenizedProgramFormat.hpp defines
108–111 as the10.1 opcodes, with107 reserved and112 as the end sentinel.

Pixel sample interpolation now requires4.1. SV_SampleIndex uses the original
4.1 generated-value declaration, a scalar uint input and SV_SampleIndex
signature. It is a rasterizer built-in and does not require a vertex/geometry
producer. The original header marks bits23:11 of DCL_INPUT_PS_SGV as ignored0;
the control and validator use that encoding rather than inventing interpolation
flags for a generated value. Existing constant/linear/centroid/noperspective
interfaces keep their established4.0 behavior.

These are bounded compiler changes. Legacy depth/integer/coverage outputs,
additional system values, all resource/MSAA features, runtime-loaded system
D3D10/D3D10.1 creation and hardware/runtime/presentation acceptance remain
separate requirements. Existing production admission and capability gates
stay closed. Query/input-layout ownership and live core callback pointers
remain intact. No installed runtime, registration or target action is part
of this slice.

The rules are supported by the original token header and Microsoft's
[SM4.1 instruction list](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-sm4-asm),
[LOD](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/lod--sm4---asm-),
[Gather4](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/gather4--sm4-1---asm-),
[SamplePos](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/samplepos--sm4-1---asm-),
[SampleInfo](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sampleinfo--sm4-1---asm-)
and [10.1 feature/profile requirements](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-10-1).

## Verification and native packet

`tests/umd-sm41-container.cpp` passes568 checks with GCC and Clang ASan/UBSan:
exact stage/model selection, reserved bits/opcodes, malformed/truncated lengths,
new-opcode stage limits, original token/hash preservation, sample-index scalar/
built-in linkage and4.1-only interpolation. Existing portable shader-container
147 and compute-container35 controls also pass. The original first attempt
reached the old interpolation control expecting sample modes on a4.0 token;
that failure is retained, and the existing control now uses4.1 for modes6/7.
Final controls have no sanitizer findings; malformed parser cases retain their
expected upstream diagnostic text.

`tests/umd-d3d10-shaders.cpp` is a source-linked CPU fixture with only the private
factory replaced by WARP. It obtains actual typed10.0/10.1 tables and keeps
runtime callback storage alive. The backend requests FL11_0 while the logical
native device stays10.0 or10.1, so the10.0 negative control cannot accidentally
pass because WARP refused the shader. A replaced live core callback reports
three rejected4.1 VS/GS/PS creates on10.0. Positive cases cover4.0 on both
tables and genuine4.1 native creation/binding/draws on10.1.

The planned18 native draws compare all4608 RGBA8 pixels against the original
D3DCompile DXBC programs on the same controlled WARP device: VS/GS Gather4 and
SampleInfo, four added PS opcodes, GS linkage, four-sample SV_SampleIndex and
sample-frequency interpolation. MSAA cases also require the independent
sample-frequency red average127–128. The fixture retains its exact HLSL,
57 original DXBC binaries and57 token payloads using CREATE_NEW, and logs each
program/profile/entry/file join. Public resources supply the CPU reference
pipeline; shader creation/binding and the draw go through the actual typed
UMD callbacks. This does not activate a Microsoft hardware runtime adapter.

Native MSVC ARM64 compilation/FXC-engine execution/WARP comparisons are
pending root. The local original MSVC/SDK/WDK-header x64/x86 Clang COFF checks
cover production DDI/compiler and both new fixtures; local code generation
cannot establish native execution. All first-party native units require
`/W4 /WX /MT /std:c++17 /EHsc /Zc:preprocessor`. The unchanged pinned213d2b8
dxbc dependency uses repository `/W3 /MT /Zc:preprocessor` policy; its original
diagnostics remain evidence and are counted separately from first-party units.

Meson adds `dxvk-umd-sm41-container-test` and `dxvk-umd-d3d10-shader-test`.
An explicit build of the latter needs `tests/umd-d3d10-shaders.cpp`, UMD
`umd_ddi.cpp`, `umd_shader.cpp`, `umd_shader11.cpp`, `umd_runtime_query.cpp`,
`umd_allocation.cpp`, `umd_runtime_gpu.cpp`, `umd_contract.cpp`, pinned
`dxbc/{dxbc_container,dxbc_parser,dxbc_signature,dxbc_interface,dxbc_types}.cpp`,
`ir/ir.cpp`, `util/{util_swizzle,util_log,util_md5}.cpp`, all corresponding
original headers and official ARM64 static CRT/d3d11/d3dcompiler libraries.
The portable container needs its test, umd_shader.cpp and the same pinned
parser units; its Windows static assertions use the original10 token header.

Expected native markers (actual check counts must come from execution):

- `^SM4.0/4.1 containers verified checks=568 typed_models=2 new_opcodes=4 hardware_admission=0$`
- `^native D3D10/10.1 shaders verified checks=[0-9]+ callbacks=3 draws=18 pixels=4608 hardware_admission=0$`

The native packet uses D3DCompile through the original system
`d3dcompiler_47.dll`, not an unverified FXC executable. Its compiler image,
SDK header and official ARM64 import-library inputs are pinned to their
original receipts and checked before/after by the owned build helper.
Frozen local preparation, PowerShell parsing and runner controls are distinct
from native compiler/shader execution; root owns all target/CI/publish work.

## Retained first native attempt and causal logging

The frozen c9 first native ARM64 attempt compiled18 original COFF objects and
two PE executables under the strict policies above. First-party compilation
emitted zero warnings; the unchanged pinned parser emitted65 retained warnings.
Native container568 passed. The typed shader fixture rejected an expected
success with E_INVALIDARG before its first draw. Seven owned native children
exited and drained, collection/transfer completed, and all12 input/system/
driver/desktop retention checks passed. Its167-member original archive,
SHA7301d4bd22d223c09113c6f1927ebea6826d76021d74758d58460ad6b69040a4,
retains three genuine4.0 FXC VS/GS/PS blobs/token payloads and the HLSL.
It establishes neither the18-draw oracle nor native4.1 acceptance.

Local replay accepts those three exact original token/signature programs
through the unchanged c9 compiler/resolvers. Buffered stdout did not retain
the callback/stage boundary on abort. The fixture therefore now disables
stdout buffering and records create/bind/draw checkpoints plus the live
callback's original HRESULT, thread and runtime handle. These diagnostics
change no shader bytes, HLSL, production code, callback ownership, expected
results, program counts, draw counts or pixel comparisons. A new frozen
native attempt is required to identify the rejection and verify the complete
shader suite; root controls the next target handoff.
