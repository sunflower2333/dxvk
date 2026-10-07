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
producer. The original header marks bits23:11 of DCL_INPUT_PS_SGV as ignored0,
but the actual pinned System32 FXC engine emits CONSTANT1 for scalar sample
index. The validator retains the zero encoding and admits only that observed
CONSTANT form for a bounded four-word4.1 pixel declaration with semantic10.
Other flags, extended declarations, interpolation modes, semantics and stages
stay rejected. Original code tokens are preserved. Existing constant/linear/
centroid/noperspective interfaces keep their established4.0 behavior.

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

The original `tests/umd-sm41-container.cpp` passed568 checks with GCC and Clang ASan/UBSan:
exact stage/model selection, reserved bits/opcodes, malformed/truncated lengths,
new-opcode stage limits, original token/hash preservation, sample-index scalar/
built-in linkage and4.1-only interpolation. Existing portable shader-container
147 and compute-container35 controls also pass. The original first attempt
reached the old interpolation control expecting sample modes on a4.0 token;
that failure is retained, and the existing control now uses4.1 for modes6/7.
Final controls have no sanitizer findings; malformed parser cases retain their
expected upstream diagnostic text.

The native ps_index correction extends the controls to688. It replays the
exact33-word native FXC program30 and its original scalar uint signature,
preserves its original code/hash, and tests generated-value linkage even
when the sample-index register0 overlaps the upstream position register.
Mutations cover all16 interpolation modes, reserved/extended flags, every
truncated length, declaration lengths/masks, mismatched/absent signatures,
wrong semantics/stages and4.0 rejection. The exact old compiler replay rejects
the original program before rebuilding a container; the corrected replay
must accept the unchanged original words. Frozen sanitizer/native verification
of this follow-on remains separate from the original568 results.

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
program/profile/entry/file join. Typed resource/view callbacks create the
target, and typed target/viewport/topology callbacks establish native draw
prerequisites. Public COM inspection retrieves the actual bound target for
readback and the independent original-DXBC reference pipeline. Public input
resources supply textures, samplers and rasterizer state; shader creation,
binding and the draw go through the actual typed UMD callbacks. This does not
activate a Microsoft hardware runtime adapter.

Complete native MSVC ARM64 compilation/FXC-engine execution/WARP comparisons
of the current corrected source are pending root. The local original MSVC/SDK/WDK-header x64/x86 Clang COFF checks
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

- `^SM4.0/4.1 containers verified checks=763 typed_models=2 new_opcodes=4 hardware_admission=0$`
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

The frozen ad138ce causal attempt identifies the rejected operation as the
first typed Draw, with original HRESULT80070057 on the caller thread after
allthree shader creations and bindings succeeded. Its167-member original
archive, SHA66a8aa7e1a1738e7a06d24700a8fa7803b3e70e002b3ddfd5919869eaf1bcad4,
retains the original three FXC programs/token payloads and HLSL, byte-identical
to c9. Native container568 and strict18COFF/2PE compilation passed; seven
owned children exited/drained and all12 retention checks passed. No draw or
pixel oracle passed.

Local source inspection finds a fixture setup omission: public context
RSSetViewports, IASetPrimitiveTopology and OMSetRenderTargets do not update
the UMD's viewportBound/topologyBound/targetBound prerequisites. The typed
Draw therefore rejects before shader preparation or backend drawing. The
fixture now uses real typed resource/view creation and typed target/viewport/
topology/clear operations, with public COM inspection of the actual bound
target. Cleanup unbinds the target, destroys its view and then destroys its
resource while the device and live callback table remain alive. Production
code, original HLSL/FXC compilation, three negative callbacks,57 programs,
18draws and4608 exact-pixel comparisons remain unchanged. The corrected
setup's subsequent native result is retained below.

The frozen e1bc corrected-state native attempt compiled18 original ARM64
bigobj COFFs and two PE executables with first-party warnings0 and pinned
dependency warnings65. Native container568 passed, and eight draws completed
2048 exact-pixel comparisons:4.0 on both interfaces with/without GS, then
4.1 Gather4, LOD, SamplePos and SampleInfo without GS. Three logical10.0
4.1 negative shader creates used the replaced live callback. The next
CreatePixelShader for ps_index returned80070057 before its draw. Allseven
owned children exited/drained, collection/transfer completed, all12 retention
checks passed and target ownership was explicitly released. Original221-file
archive SHA7b80fc375e4e873e229a98c41ea94e18ab980518df8e27a742fcfb57e0048418
retains30 original DXBC/token pairs and HLSL; the full18/4608 oracle failed.

The original FXC sample-index declaration is04000863, with CONSTANT bit11,
v0.x and semantic10. Both the original code-chunk guard and pixel-input
resolver rejected that form. Local exact-original replay reproduces that
rejection before any backend API call. The narrow correction admits only
the bounded observed declaration and keeps the reconstructed input scalar
uint, mask1 and generated linkage. The original shader/HLSL bytes,57 program
count,18draw/4608pixel comparisons and three negative callbacks are unchanged.
The Microsoft [system-value input declaration](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-input-sv)
describes constant interpolation; acceptance of this particular SGV encoding
is based on the retained original native FXC bytes, not an inference from
the header's ignored0 comment. Its subsequent native result is retained below.

## Retained SampleIndex pass and interpolation fixture correction

Frozen a00 compiled18 ARM64 bigobj COFFs and two PE executables with
first-party warnings0 and unchanged pinned parser warnings65. Native
container688 passed. Nine completed draws matched2304 original-DXBC reference
pixels, including the four-sample ps_index scene and its independent red
average127–128. Three logical10.0 negative callbacks remained expected. The
next ps_interpolation4.1/noGS Draw returned80070057; all three shader creates
and typed target/state/bind calls had returned without errors. Allseven owned
children exited/drained, collection/transfer completed, all12 retention checks
passed and target ownership was released. Original227-file archive
SHAd1b4e7f161619d89d5a28b1fb11abafebaa7cbd5047c937a29b78b471624bd93
retains33 original DXBC/token pairs and HLSL. Complete18/4608 acceptance remains
failed for that exact source.

The original FXC31 VS output signature has SV_Position at register0,
TEXCOORD0 at register1 and TEXCOORD1 at register2. FXC33 ps_interpolation
has only TEXCOORD0 at input register0, and sample-mode6 declaration03003062
reads v0.x. The HLSL fixture omitted position before TEXCOORD0. The production
linker rejects that system-value/register mismatch correctly. Microsoft's
[D3D10 linkage FAQ](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/direct3d10-frequently-asked-questions#shader-linkage)
documents this position-first/omitted-position linkage error. The
[signature contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures)
requires corresponding locations and argument ordering; the
[DDI signature](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_stage_io_signatures)
supplies register unions and system values, without arbitrary HLSL semantic
names. The controlled D3D11 reference's semantic matching did not establish
that the original pair was valid for D3D10.

The fixture correction adds an unused float4 SV_Position argument before
sample float2 TEXCOORD0 in ps_interpolation. Its color expression, sample
modifier and all pixel/readback/reference bodies stay unchanged. New native
controls require position0/mask15 and TEXCOORD0 register1/mask3 in both
producer signatures and the PS signature, plus the actual sample-mode6,
v1.x declaration before either draw. The57 programs,18 draws,4608 pixel
comparisons, independent MSAA oracle and three negative callback gates stay
unchanged. A fresh packet must retain its own new HLSL/FXC bytes; none of the
original a00 files are edited or reused as corrected outputs.

The portable test extends to763 controls. It replays the exact46-word FXC33
payload, accepts and preserves its sample declaration, and rejects its
original incompatible producer linkage. A separately identified synthetic
control changes input declaration word5 and input-use word15 to1 with a compatible signature;
its container/resolver/linkage succeed, unused position drops from consumed
inputs, and wrong register/system/mask/signature/4.0 cases fail. Explicit
guards verify the original declaration/use operand tokens and their register
immediates; literal operand word16 stays00004001. All46 control words are
compared against the unchanged original, with only words5/15 differing. These
synthetic words are never substituted for original native FXC output.
Production compiler/linker sources, interface negotiation, live callback
ownership and admission requirements are unchanged by this fixture correction.
Native compilation and the complete corrected WARP oracle remain pending a
new explicit root handoff.
