# DXVK native Windows UMD

## Goal
Embed DXVK translation in a real VIOGPU D3D9/10/11 UMD, loaded by the Windows
runtime, with correct hardware rendering and full Display+Render integration.
ARM64, x64 and x86 are required; an app-local runtime is not the target.

## Next Step
e4a3654 fastCI34605452108 originalMicrosoftWARP failed exactlyone of16payload
words: rawFloats.y expected80000000(-0),actual00000000(+0). This establishes
an invalid constant-payload expectation independently ofDXVK. Generate that
signed zero from dynamicICB bits.x, retain strict checks and failure diagnostics,
then require both original/rebuilt WARP and next bounded target run.
URGENT: Main'sseven-dxvk9c55-ddi-02failed final4096/4096pixels atLUID582A,
exit7/238ms;creation/copy/depth-stencilPASS,event1/error0. Shell2028/5780
retained. Diagnose added linkage before further runtime DDIs. New candidate
adds separate UV/raw-float/immediate-array/fixed-integer failure bits and one
bounded payload-word draw. Independent WARP original/rebuilt-container checks
will distinguish invalid probe expectations from compiler/driver failure.
Do not request an identical9c55rerun. VKD3Dranged-copy edits are isolated WIP.
Paired60004/1ab0151/9eb4361 CI34596574202 ALL PASS artifact10263060497.
Depth b47f955 CI34597997951 ALL PASS and includes --native-copy/blend/scissor.
Standalone ARM64 files/hashes/usage delivered to main in hardware-probe-b47f955.md.
Paired60005/a27eb3e/b47f955 CI34598876655 ALL PASS, signed artifact10264495517.
Keep its older Mesa4ace separate from newer active desktop GPU fixes.
Typed native input layouts/vertex buffers7b30c07 CI34598775633 ALL PASS;
dynamic maps/error translation272a067 CI34599288562 ALL PASS.
Main ran standalone272a067 with LUID2A58000000000000: backend/DDI/native-copy
ALL PASS. Real KMT backing and GPU pixels verified; Microsoft runtime and
display Present remain unproven. No unchanged probe rerun needed.
Continue general SM4 VS/PS varying linkage with independently checked types.
Linkage8f7bce4 CI34601912517 compiledARM64 but x64/x86 SPIR-V checks failed.
Fixed ISGN read-use masks and added bounded immediate arrays in87697ea;
replacementCI34602941896 running. Local130shader checksPASS. Do not deliver
8f7bce4 merely because its ARM64 cross-build passed.
61bc32b fixes the fast-CI static-library filename through Meson introspection;
full replacementCI34603295333 found a fast-job /MD mismatch.9c55e92 fixes only
that CI runtime setting. LatestCI34603595256 ALL5PASS; the matched ARM64 set
is downloaded and hashed inhardware-probe-9c55e92.md. Production87697ea already passed all
three architecture jobs, including750real-HLSL/final-SPIR-V checks onx86.
Parent additionally delegated independent VKD3D engine/paired continuation:
engineb2c510d CI34603845851 CPU Vulkan/ARM64/x64PASS, x86 still running.
Paired743acf9 pins/documents SRV/null-range; CI34604382308 is running. Build/sign/package and
deliver target hashes after the DXVK three-file candidate. No remote operations.
Continue remaining mandatory resource/shader/primary/shared DDIs;
main explicitly asks not to stop at CPU OpenAdapter harness.

## Phases
- [complete] Implement internal exact-LUID DXVK backend factory (runtime proof pending).
- [in_progress] Implement native DDI resource/shader/draw/readback coverage.
- [in_progress] Build and validate with personal-fork GitHub CI on supported architectures.
- [complete] Coordinate real-device standalone backend/DDI/KMT-backing proof with main.
- [pending] Complete sharing, synchronization, presentation and full native acceptance.

## Constraints
Own this independent DXVK checkout and the explicitly delegated independent
vkd3d-native-umd-20260911/viogpu-vkd3d-umd-20260911 checkouts. No remote access until main grants a test
window. Preserve existing driver and desktop. Parent owns all shared worktree
and main-plan edits. This thread pins PWF_PLAN_ROOT to this directory.

## Errors
- Hardware9c55shader path failed4096finalpixels despiteCI final-SPIR-V interface
  checks passing. Interface type agreement is insufficient runtime evidence;
  expose actual values and compare original/rebuilt bytecode onMicrosoftWARP.
- Fast61bc32b CI34603295333 found the actual library but mixed Meson's /MD
  runtime with plain cl's default static runtime (LNK2038). Pin both sides to
  /MD. Production Meson targets already use a matched runtime; no UMD changes.
- New fast87697ea shaderCI34602941896 compiled its compiler library but
  failed LNK1181 on a guessed dxbc_spv.lib filename. Resolve its exact static
  library through meson introspect --targets instead. Architecture jobs link
  the library through Meson already and are independent of this script error.
- Full8f7bce4 x64 SPIR-V check558 failed after148HLSL/reflection checks passed.
  Source review found rebuilt ISGN component masks omitted the high-byte
  read-use bits. dxbc IoMap masked typed inputs tozero then used its implicit
  float fallback. Set input read-use masks explicitly; output mask semantics
  remain unchanged. New CPU checks and earlier full-SPIR-V CI gate added.
- General linkage intentionally permits SV_Position at nonzero registers;
  old test35 still asserted that it must fail. Updated that obsolete case to
  reject register32 and added positive nonzero-position linkage coverage.
  Current local identity69/trailer176/shader102 sanitizer checksPASS.
- Resume inspection guessed native-umd.yml; actual workflow will be located
  with rg before reading. Paired60005 success was independently reverified.
- Native input-register test caught swapped registerIndex/streamIndex at
  shader check43; fixed exact constructor order. Local shader53PASS and
  Windows real-HLSL/reflection74PASS on x86; full target builds pending.
- b40ccb1 CI34595019708 passed217native allocation/present callback tests and
  compiled the production UMD, then caught const input arrays incompatible
  with WDK's mutable pSysMem. Made probe arrays mutable and moved production
  DDI/probe object compilation before the expensive backend build.
- New paired allocation ABI checker initially resolved shared/ at repository
  root instead of viogpu/. Corrected the parent path; temporary test outputs
  were automatically removed by TemporaryDirectory.
- Native allocation CI34594671352 compiled implementation but its fixture
  used DoNotWait, absent from D3DDDICB_LOCKFLAGS. Confirm exact SDK spelling
  before correcting the test. Upload/copy fix1f4595d CI34594094209 allPASS.
- Constant-buffer patch had a documentation-context typo; the atomic patch
  applied no files. Corrected the exact documentation line before reapplying.
- Upload/copy828b535 CI34593487334 found WDK D3D10_DDI_BOX uses signed LONG,
  while D3D11_BOX uses UINT. Reject negative coordinates before explicit
  conversion; no potentially wrapped signed box can reach the backend.
- Initial patch context for src/meson.build mismatched; no partial edits were
  applied. Corrected the context and applied successfully.
- Two guessed filenames were absent; used rg results to locate actual files.
- CI34585824018 failed: x64 MSVC target used invalid x64_x64 alias; x86/ARM64
  backend library compiled, but probe omitted dxbc-spirv include dependency.
  Fixed both before the next run. Identity sanitizer test passed69 cases.
- Submodule add with --depth=1 did not fetch its nondefault branch. Explicit
  depth1 branch fetch/checkout recovered it; completed .gitmodules separately.
- Shader container integrity regression found pinned dxbc-spirv buildContainer
  emits regular MD5 while validateHash requires the DXBC variant. The bridge
  now recomputes with the existing DXBC hash function without submodule edits.
- Unknown opcodes are accepted with empty layouts by the upstream parser;
  the SM4 bridge now validates opcodes, lengths and exact reconstructed tokens.
- Parent general clang-format workflow34586690304 failed on inherited source
  style; this integration changes no parent C/C++ implementation files.
- Parent34586690632: KMD compiled, but initial MSBuild Inf2Cat failed because
  three DXVK files were staged only later in the workflow. Added them to
  FilesToPackage in the independent vcxproj; no KMD source changes.
- a52df2b CI34587997416 compiled all3architectures; Windows CPU checks rejected
  re-encoded real shader tokens at check43. b76de1d preserves tokens verbatim;
  fast Windows D3DCompile/container/D3DReflect CI34588836455 shader-cpuPASS.
- Superseded child7a373289 CI34588590093 and parentc025e63 CI34588644705 were
  cancelled after replacement runs started, because they retain the already
  diagnosed real-shader reconstruction failure. Not an observation timeout.
- Producer header was initially resolved relative to checkout root; rg showed
  its actual location isviogpu/shared/viogpu_adapter_identity.h. Read-only
  review completed at the correct path; no producer files modified.
- Parent total contract checker retained baseline epoch-version and two-file
  package rules, incompatible with this branch's existing explicit60003 and
  five-file package. Narrow checker integration validates exact reserved
  source version and exact five files; full contract nowPASS. Check-only
  follow-up does not change7051011 binaries and avoids duplicate full CI.
