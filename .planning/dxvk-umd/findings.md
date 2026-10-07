# Findings

2026-10-08 continuation: root independently rejoined all60 raw CPU05 archive
members (47 original payloads/13 helpers) and nine native03 helper source bytes;
proof2d691375 and CPU-only ownershipfd5acd14. Native staging exits0. Actual
Prepare guard exits0; controller exits1 in1.33s; separate failure cleanup exits0
and proves the fresh task absent. No Start/task/core/GPU action happened. The
original Prepare helper only writes its actual caught exception after creating
the task evidence root, so an early input failure may retain just the generic
outer error. One separate bounded input-only native diagnostic is authorized
to capture unchanged Get-ComputePins/Test-ComputeConfiguration exceptions.
This diagnostic is not a retry or a GPU admission. Preserve all05 originals.

## 2026-10-07 exact4e native verification and parallel port integration

Root independently verifies raw adapter archivefc45eb6b (52members/12Git,
4ARM64COFF/1PE) and raw discard archiveecfb03cf (141members/74Git+15Vulkan,
9ARM64COFF/2PE). Actual MSVC14.44.35207 /W4 /WX /MT runs pass adapter93976
and native9 adapter43067/device554085, including actual99-pointer protected
adapter/core table writes. OfficialSDK21/CRT5/toolchain/SYS58624/DWM1552/
Explorer6464 and original System32d3d9 retain. Proofs are
guest-adapter-4e91632-01/root-native-adapter-4e91632-verified-01.json and
dxvk-native-d3d9-discard-20261007/root-native-discard-target-fixtures-verified-01.json.
No hardware admission is inferred from controlled fixtures.

DX8b61f8a3 cherry-picks as5cff890. Root verifies frozen13inputs,10exactGit+
3verbatim Wine/MinGW legacy headers,8compiled-source hashes and all original
15COFF/9PE outputs. Fresh integrated Linux sanitizer controls pass543caps /
4235boundedimports; earlier agent4237 includes two original Microsoft mapped
image checks. Independent384-pixel oracle selfcheck rejects four corruptions;
all output is synthetic verifier testing. Actual matching x86 UMD/loader/Mesa
and immutable8/9 backend compatibility remain required.

SM5metadata134c061 cherry-picks as550d336, preserving firstslice4e native build
inputs. Native GS/SO/HS/DS/class execution and production admission stay pending.
WARP4e attempt01 fails before compilation on absent d3d11.lib in the narrow
support kit; agent stages only byte-original official SDK libraries under owned
paths for attempt02. Original failure is retained and not a source failure.

Standalone68d0d4c root native review02 passes original53-member archive43b2e7e5,
60Git inputs,2COFF/2PE,zero warnings,10malformedCLI and3nullguards. Review01
stops at a reused original d3d9 import-library receipt omitted from prepared
packet copies; original error is retained. Review02 references that original
receipt and rejoins the raw official SDK member. Fixed4e lifecycle candidate
is absent and not loaded during these CPU controls; SSH enum sees zero adapters.

## Native D3D9 resource contract audit

Microsoft CreateResource requires separate saved runtime and published driver
handles, atomic groups retaining SurfCount input order, and ignoring Fvf,
MipLevels, refresh/output and multisample fields when their usage flags are
absent. System-memory backing may use a synchronized CPU access path.
Clear zero rectangles without COMPUTERECTS is a no-op; with COMPUTERECTS it
uses viewport/scissor clipping. Explicit preclipped rectangles must not be
clipped again against current viewport/scissor. DestroyResource submits
dependent commands before retiring backing; system-to-system Blt synchronizes
only and leaves the actual copy to the runtime. These distinctions require
behavioral tests and a nonempty real target submission/pixel oracle.
Sources: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createresource
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_clear
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_blt
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_destroyresource

Discovery errors: no tests/meson.build (native fixture targets live in
src/umd/meson.build); guessed prepare-d3d9-startup-build.sh is absent. Use
rg --files on the known helper directory. One combined web output spread a
string into character keys; corrected to print the web result directly.

## 2026-10-06 independently verified diagnostic build

Diagnostic c1b9ea69 native guest build verifies all118 Git/archive input
hashes, source tree, SYS/PDB/MAP/compile-PDB hashes, ARM64 Native PE, CFG/NX/
ASLR and kernel-only imports(ntoskrnl.exe/HAL.dll). Runner5.429254s,
VirtIO0.754602s, KMD4.367460s; installed58623 and DWM1644/Explorer5828 retained.
Evidence: artifacts/dxvk-kmd-readiness-20261006/guest-c1b9ea69-02/build-verified.json.
Pushed c1b9ea69 to personal origin work/dxvk-readiness-diagnostic-20261006;
unsigned full-miniport CI37426954641 is running.
Native guest signtool exists; matching WDK26100 NuGet contains x86 Inf2Cat
and dependencies plus native ARM64 stampinf. Test-signing is already enabled.
Reserve distinguishable diagnostic58624 after local/remote branch/version
audit found no allocation. Preserve the active58623 full payload by copying
it for rollback and retaining every runtime DLL hash in the diagnostic package.
Android SSH runs as app uid10316; root app-dir inventory is available through
ADB su rather than this Termux identity. Live daemon reconfirms sole Windows
PID3980 with6656MiB, so preserve its actual configuration.
Independently verified distinguishable58624 guest build and signed overlay.
Unsigned SYS3c5c8506e42148a2aafdde98f5fda790387d78421bfb1de7f588b447412a80d1;
signed SYSd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a.
Signing modifies only PE checksum/certificate directory and appended signature;
all executable content matches the unsigned build. INF settings are identical
after normalizing DriverVer. All47 retained files,50 catalog members and51
rollback file identities match the exact original signed58623 package.
Package result SHA256f836257b318828f068627fd780a9af2d81cef80673c0c1f359e8e3d88766a989.
Evidence: artifacts/dxvk-kmd-readiness-20261006/package-c1b9ea69-58624/package-verified.json.
CI fixture correctionfdfd8f99 changes only two host fixture files; all118
production build inputs remain exactlyc1b9ea69. Replacement CI37428593406 active.

- The next diagnostic uses five direct-system-D3D11 controls inside the same
  CPU fixture: single/array resources, default versus selected-array SRVs,
  full automatic versus explicit mip count. It bypasses every UMD helper,
  runs before the state tests and prints first lower-mip values. Existing
  mandatory full-image mip checks remain strict, so observations cannot turn
  a failed native-range expectation into an apparent test pass.

- 4ba6a19 created the debug device and InfoQueue successfully, but neither
  prior creation nor GenerateMips produced a stored diagnostic. Lower mip
  remains unchanged. Official CreateTexture2D says MipLevels0 for automatic
  full-chain allocation. Added that control with queried actual4mips and8
  initialized subresources, still generating only slice1/mips0-1 and checking
  all out-of-view mips. No production workaround or support claim yet.

- Typed control e98896b also fails old-green mip1. Actual resource format28,
  MiscFlags1, support3fef3f3, viewfirstMip0/mips2/firstSlice1/slices1 all match
  the intended call. This disproves a typeless-only explanation. Add official
  D3D11 debug layer/InfoQueue diagnostics, with fallback only when the SDK
  debug component is absent; do not change production or relax pixels.

- c40dc0d WARP diagnostics prove the selected array's mip1 remains its old
  green valueff00ff00 after GenerateMips; the base is correctly red and other
  slice remains zero. Oracle resource was typeless. Microsoft GenerateMips
  documents typed supported formats and silent failure for unsupported ones.
  Split mip generation onto a separate typed RGBA8 base with an explicit
  MIP_AUTOGEN cap check, preserving every original typeless-view test and
  every exact mip pixel assertion. This control is pending; typeless base
  behavior is a WARP observation, not a newly claimed DXVK limitation.

- Native GenMips requires D3D10_DDI_RESOURCE_AUTO_GEN_MIP_MAP, not a guessed
  D3D10 API flag name. DXVK already emits GPU mip blits for the precise SRV
  range. Add native flag validation and callback, checking required bind
  flags and filterable MIP_AUTOGEN support. Missing creation flags produce
  E_FAIL; invalid MIP kind produces E_INVALIDARG per Microsoft contract.

- ARM64/x64 activation audit: Microsoft documents UserModeDriverNameWoW for
  32-bit UMDs, and UserModeDriverName list positions for D3D versions. Neither
  supplies an x64-on-ARM selector. DriverStore paths have no WOW filesystem
  redirection. Arm64X provides a documented shared path with native/EC views;
  both engine and Vulkan dependencies must match the calling ABI. OpenCL
  agent confirmed its Registry64 multi-vendor behavior must not be inferred
  for D3D. Full contract and official URLs saved in runtime-activation-gaps.md.
- Before ef4b174, the native table rejected UNDEFINED topology during runtime
  reset and accepted only one viewport. Microsoft specifies an atomic complete viewport
  replacement; zero count also unbinds even when the clear hint is zero.
  These are runtime initialization gaps, not additional benchmark coverage.
- The next resource coverage gap includes typed buffer SRVs/RTVs. Local
  Microsoft/Mesa source confirms native Buffer.FirstElement and NumElements;
  current CreateShaderResourceView rejects every non-Texture2D resource even
  though buffer creation accepts shader-resource binds. No source edits for
  that next task have been made, and no support is claimed.

- Parent redirects the next milestone to ordinary Microsoft D3D11/D3D12
  runtime activation and owns an independent system-DLL/exact-UMD application
  acceptance tool. Stop adding backend-only cases. Source audit: DXVK has no
  OpenAdapter10(_2) exports, no GetSupportedVersions/GetCaps, and20 documented
  D3D10 table slots remain unset (some version-dependent). D3D11 table absent.
  D3D12 has no OpenAdapter12/adapter runtime callbacks, native CreateDevice,
  runtime heap/resource/GPUVA/residency/fences or graphics/Present wiring.
  Backend Vulkan objects and harness tables cannot substitute for these.
- DXVK SetPredication only stores state and logs Stub. No predication source
  edits were made; do not advertise that feature from a simple DDI forwarder.

- VKD3D continuation found UAV creation still dereferenced arbitrary incoming
  data/counter handles, unlike the newer SRV path. Both now resolve the locked
  live buffer registry before backend access. New WDK fixture uses pointer1,
  wrong object kind and removed registry entries and verifies zero backend
  writes after every rejection; valid counter forwarding remains covered.

- Native ResourceIsStagingBusy queries the whole resource and is legal while
  subresources remain mapped. Implement through embedded DXVK CS sequence and
  GPU access tracking, never Map again or return a constant. Pending buffers
  count busy without requiring submission. Untracked sequence sentinel must
  not be compared as a real sequence. Existing Vulkan barriers own actual
  read/write transitions; hazard DDIs validate native ownership without GPU
  idle. IMPORTANT: Microsoft's local markdown names the SRV hazard parameters
  in the wrong order. Real WDK compilation proves the signature is device,
  view, resource. Corrected implementation and probe to the actual typed ABI.
- Target1a7cf3b andbc61de9 now PASS with original shells retained. These are
  bounded native callbacks and GPU readback, not Microsoft runtime activation
  or application/visible Present support. Parent owns final host trace closure.

- Independent original-bytecode WARP execution caught a probe defect that
  final-SPIR-V interface checks could not: FXC emitted rawFloats.y=+0 for the
  expected-0 source.37678fbdisassembly confirms literal0 inoutputo3, even
  when selected from a zero/denormal-only ICB. RuntimeVSconstant input fixes
  the source expectation without weakening any payload checks. b8c73ef
  fastCI34606380427 passes887checks; original and rebuilt WARP both match
  all16words. Target replacement remains pending;9c55is not a linkage pass.

- Parent delegated VKD3D after sourceb2c510d. Paired743acf9CI34604382308
  ALL7PASS and downloaded signed manifests match; retainsMesa4ace, so main
  keeps newer installedMesa. b2c510d target14workloads/14336readbacksPASS
  in1015ms, parenthost26343lines/zero loss-fault,context41:5retired18/18;
  Explorer3/3 and2028/5780retained. New0447a76 implements full prevalidated
  multirange descriptor copies; local15workloads/15360CPUVulkanwordsPASS,
  WindowsCI34606533835running. This child owns both independent engines,
  never remote operations or main scripts/plans.

- Critical shader-signature bug discovered by full SPIR-V CI: SignatureEntry
  constructor stores the supplied component mask verbatim. ISGN low byte is
  declared components; high byte is components read. Previously only low bits
  were written. IoMap::declareIoSignatureVars therefore removed all typed
  inputs and generated implicit float fallback variables. Set both bytes for
  every input, preserving output's inverse used-mask convention. This matters
  for typed vertex inputs as well as new flat PS linkage; plain reflection of
  ComponentType alone was insufficient evidence. Fix needs newCI/runtime proof.

- Native format-query audit found a significant boundary: D3D10's
  NOT_SUPPORTED bit is legal only for Microsoft's listed optional formats,
  not a generic marker for every unfinished DXVK format. SHADER_SAMPLE means
  any-filter sampling, and sampleCount1 always returns one quality level.
  Do not copy backend D3D11 capability bits or Mesa's broad unsupported-format
  behavior into a new native table; restrict responses to implemented paths.
- Windows fast8f7bce4 real-HLSL/reflection job103271026075 passed148checks.
  Full architecture/SPIR-V jobs remain active. Local extra syntax check
  initially lacked the pinned nested SPIRV-Headers; populated only that
  existing dependency recursively with --depth1 (no source pin changes).

- Parent's final seven-api-round-01 trace for272:84873lines, zero loss,
  GPUfault0 and dmesgGPUfault/timeout before0/after0; all observed submissions
  retired, pending0. DXVK timed lifetimes39:14=1/1 and39:16=3/3; these are
  host submission observations, not hardware-busy/utilization measurements.
  Post-API Explorer sweep3/3 retained2028/5780 and Application108540 unchanged.
  Shared evidence is chat/2026-09-11-seven-workstreams-acceptance.md.

- Linkage implementation will normalize generic VS outputs and PS inputs by
  register/mask. Pixel dcl_input_ps interpolation selects F32 for interpolated
  data or raw U32 for flat data; both stages must agree. Reject incomplete or
  conflicting declarations instead of silently assuming float. Current scope
  keeps one compatible type per register and a float target0; packed mixed
  interpolation, additional system values/MRT/stages remain explicit gates.
- Local inspection confirmed actual workflow build-native-umd.yml and test
  script scripts/test-native-umd.sh. Missing guessed header/script paths were
  resolved from rg results; no source or build mutation resulted.

- Paired60005/a27eb3e/b47f955 run34598876655 independently reverified ALL PASS.
  Signed artifact10264495517,11542329bytes, SHA256
  7557f872ba8c6b38c97eb52f138a2efc93c2e69f3427edc43be65d667f8614e7.
  Main plans to test newer standalone272a067 with LUID2A58000000000000 after
  GLES; full native D3D9/11 runtime/application/Present acceptance is mandatory
  beyond standalone/KMT-backing checks. No device operations by this agent.

- Confirmed dxbc IoMap::convertScalar emits ConsumeAs for non-bool values;
  LowerConsumePass::handleConsume lowers equal-width values to Cast (not
  numeric Convert). This is source support for a raw32-bit flat-varying
  linkage scheme; mixed payload and SPIR-V execution tests are still needed.
- Shared-open next-step audit: current Mesa Resource.cpp::OpenResource
  validates exactly one80-byte VIOGPU allocation record, reconstructs a
  GPU cache, and borrows hAllocation with null originating runtime resource.
  DestroyResource deallocates only creator-owned allocations. DXVK's current
  RuntimeAllocation has only owning state, so importing handles requires
  explicit borrowed lifetime plus synchronized refresh/publication; setting
  shared flags or just adopting the integer handle would be incomplete.

- Next shader-linkage research: dxbc IoMap::determineInterpolationMode
  reads dcl_input_ps opcode interpolation bits. IoMap declares variables
  by signature type and uses convertScalar at both I/O loads and stores.
  A possible native solution is F32 for interpolated values and a shared
  raw U32 interface for flat values, with producer variants matched to the
  bound pixel shader. Verify convertScalar is bit-preserving at32bits and
  test mixed float/int/NaN payloads before enabling arbitrary varyings.
- Previous requested checkpoint60005/b47f955 covers
  --native-copy/blend/scissor/depth. Typed vertex layouts7b30c07 independently
  passed all architectures in CI34598775633, including74real-HLSL checks.
  Dynamic maps272a067 CI34599288562 is separate and ALL PASS.

- Input-layout audit: D3D10 DDI supplies InputRegister, while DXVK public
  CreateInputLayout maps semantic names to that register. Both sides can
  safely share generated register-index semantics; original names are not
  needed. Scalar types must come from the bound format. Generic vertex
  shaders will retain raw tokens until a compatible layout supplies those
  types, then cache the compiled variant. No first-adapter or public factory.
- Paired60004 CI34596574202 ALL PASS. Signed artifact10263060497,
  11537610bytes, SHA256
  ae0e58217a05efd49530abab9198a8deaa76c15f8b1e80881f13e29c26c716b1.
  Exact parent1ab0151/child9eb; older Mesa4ace retained, main warned.

- Microsoft native SetRenderTargets requires all color slots and depth to
  update atomically, even NumViews=ClearSlots=0. ClearSlots is only an
  optimization aid. D3D10 depth/stencil descriptors expose FrontEnable and
  BackEnable absent in D3D11 API descriptors; disabled faces must translate
  to ALWAYS/KEEP operations. Native implementation now handles both.

- Blend/indexed draw implementation follows DXVK D3D10Device's existing
  D3D11 blend translation, preserving per-target enables/write masks.
  The development GPU pixel criterion now depends on actual additive
  blending (half-red clear plus sampled half-red output), alongside real
  indexing, constant buffers and texture sampling. No target result yet.

- Next device-proof extension can use harness-owned callbacks backed by real
  D3DKMT device/allocation/context/Lock operations. Its Present callback must
  verify the KMD allocation pixels without claiming actual screen Present or
  Microsoft runtime activation. This would exercise the production GPU
  readback-to-allocation publication on target while preserving the desktop;
  root alone coordinates execution. No such device result exists yet.

- Microsoft DXGI base-DDI contract requires real QueryResidencyCb results
  and backbuffer identity rotation including dependent views. Querying only
  the new CPU-visible presentation copy would not describe the separate
  Vulkan rendering cache's allocation, so do not advertise full residency
  support from that one handle. Shared resources and runtime-visible GPU
  allocation ownership still need unified integration.

- Added native runtime allocation ownership and synchronous windowed-blit
  Present plumbing under the development adapter entry point. The runtime's
  original device/resource/DXGI handles are forwarded only to their own
  callbacks. CPU-visible BGRA8/RGBA8 present sources are synchronized through
  backend Map, kernel Lock/Unlock, then PresentCb with a real callback-created
  context. Primary/flip/shared-open paths remain rejected. New callback tests
  check failure sequencing, stale-frame refusal, row padding and cleanup;
  compilation and runtime validation are pending. SDK DXGI handles are
  UINT_PTR, unlike the D3D10DDI pointer-wrapper structs; explicitly converted.

- Present/shared-resource audit: DXVK D3D11CommonTexture::ExportImageInfo
  uses D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE and a legacy Proton metadata
  fallback. Enabling its public shared MiscFlags is not a native VIOGPU
  ownership implementation. The current Mesa Resource.cpp uses runtime
  Allocate/Deallocate/Lock callbacks with VIOGPU private allocation data and
  explicit GPU-cache/shared-backing transfer; DxgiFns.cpp passes real kernel
  allocations to PresentCb. A native DXVK bridge must implement that owning
  allocation path (or validated external-memory import), not reuse Wine's
  metadata escape. Existing DXVK VkInterop can wrap VkImage on its own
  device, but that alone supplies neither runtime allocation nor fences.

- Microsoft QueryGetData DDI returns void and reports unfinished work via
  SetErrorCb(DXGI_DDI_ERR_WASSTILLDRAWING). Backend GetData returnsS_FALSE;
  treating that as ordinary success would expose incomplete query data.
  Added scratch-buffer publication only onS_OK, explicit pending/device-loss
  translation, DO_NOT_FLUSH handling, and real event/occlusion/timestamp/
  disjoint query objects. Predicate/statistics queries remain unsupported.

- Microsoft D3D10 OpenAdapter/CreateDevice structures carry independent
  adapter, runtime device and core-layer handles. D3D10.0 interface version
  is0x000a0001 and minimum runtime build4; native D3D11 tables must not be cast to
  D3D10. Reject unsupported flags, including threading restrictions that
  DXVK does not yet honor here. Adapter identity lifetime must outlive its
  handle when devices retain it. Added development-only OpenAdapter harness
  without registering incomplete tables or changing any KMD files.

- Initial checkout is clean at96a7190 onviogpu-umd-20260911. The existing
  WDDM-named workflow/tests package app-local DXVK API DLLs, not a native UMD.
- Existing exact-LUID Vulkan filtering is reusable, but native device creation
  must never fall back to the first adapter or recurse into system D3D/DXGI.
- Existing Mesa native frontend supports only lower feature levels; the
  intended new UMD reuses DXVK shader/pipeline semantics while implementing
  Windows DDI object ownership and submission/presentation contracts.
- Standing remote rules were reviewed; the user's4GiB authorization overrides
  older2048MiB guidance. This agent currently has no remote test window.
- The current adapter-private KMD response contains no Windows LUID. Native
  OpenAdapter receives only an opaque runtime adapter handle; it cannot be
  cast to a KMT handle. An explicit versioned identity query is needed unless
  a documented existing binding is found. Parent requests proposal/consumer
  only, no KMD edits for now.
- Added a separate static D3D11 implementation library excluding public API
  exports, an exact-LUID plus Turnip-driver-ID backend factory, null-DXGI
  safety for embedded device GetParent/GetAdapter, and a4096-pixel clear/
  copy/readback executable. This is internal-backend validation, not DDI
  runtime acceptance or a system-installed driver.
- Child eaf3f078 CI34586456839 passed identity69sanitizer cases and ARM64,
  x64 and x86 compile/link/PE/import validation for the embedded backend,
  viogpudxvk development DLL and real-WDK resource-DDI readback probe.
- Parent94a27498 pins eaf3f078 in external/dxvk alongside Mesa4ace9df.
  Paired CI34586690632 passed regressions plus Mesa and DXVK ARM64 build;
  KMD build/sign/catalog/package job remains in progress at this note.
- Current committed shader/draw work adds a bounded SM4.0 profile with VS
  SV_VertexID/SV_Position and PS float target0, no user varyings or custom-data
  instructions. Raw DDI code is rebuilt into a checked DXBC container and
  passed to the private DXVK D3D11 device. General shader I/O is unfinished.
- DDI probe clears green then draws a red full-screen triangle through real
  shader/state/draw DDIs and checks4096red pixels. It has NOT run on hardware.
- Local ASan/UBSan passes identity69 and shader41cases, including malformed
  bytecode/unsupported signatures and container integrity/token preservation.
- User is backing up/compressing VMDISK. Parent explicitly keeps this thread
  on local development and personal CI, with no device operations.
- Microsoft DXGK_START_INFO.AdapterLuid is available since Windows8 and is
  the required KMD identity source. The output-only QueryAdapterInfo callback
  does not accept a request discriminator in its output buffer.
- Optional32byte trailer proposal preserves the existing128byte v0 prefix,
  header version/size and reserved fields. Decoder and callback consumer are
  implemented, but no KMD producer or OpenAdapter activation exists. Decoder
  ASan/UBSan176casesPASS; Windows callback tests use only mocked replies.
- Current child a9c69c0 includes raw-token shader wrapping and explicit
  DO_NOT_WAIT bit translation. Parent801fde5 pins this child and advances the
  development package to60002. Final CI handles are in task_plan.md.
- Native D3D9 cannot call DXVK's existing API factory unchanged: that path
  enumerates displays with first-adapter fallback and owns an implicit
  swapchain. A separate exact-adapter D3D9 DDI bridge remains required.
2026-09-11 continuation: CI34617903693 x64 job103324299501 API-only WARP
controls produce red for one-slice/default, two-slice/default and explicit
slice0 views, but retain green for explicit slice1 views with both auto4mips
and explicit2mips. This bypasses native descriptors and localizes the runner
limitation. Strict oracle now selects slice0/mips0-1 while checking all8
subresources. Production c9e389d is unchanged; nonzero-slice DXVK GPU proof
remains open. Removed temporary observation-only controls after documenting
their exact source/run so the normal test PASS cannot imply those cases pass.
2026-09-11 GS audit: DXVK backend implements geometry shaders, but the native
bridge currently has only VS/PS and assumes VS outputs link directly to PS.
GS inputs have two-dimensional declaration indices and no interpolation;
generic raw32 signatures preserve typed instruction bits, while SV_Position
stays F32. Compile GS outputs using the active PS types and relink VS to GS
inputs. GS bindings need the same ownership/atomic-range rules as VS/PS.
2026-09-12: Microsoft local D3D10DDIARG_STAGE_IO_SIGNATURES explicitly says
the runtime signature is a union shared by shaders; actual declarations can
use a subset. Previous PS resolver wrongly rejected wholly undeclared union
entries. Both PS/GS now skip undeclared entries, but still reject any declared
input absent from the signature or with conflicting mask/system-value data.
CreateGeometryShader has the same typed5argument signature as VS/PS and uses
CalcPrivateShaderSize. Failed creation never gets DestroyShader; candidates
own and clean up all temporary vectors/COM objects before publication.
2026-09-12 remaining DDI audit: Native SO declarations give OutputSlot,
RegisterIndex and xyzw RegisterMask, with one StreamOutputStrideInBytes.
DXVK CreateGeometryShaderWithStreamOutput takes semantic/range declarations,
requires extTransformFeedback, and DrawAuto consumes the actual SO counter
attached to IA vertex buffer0. Preserve these contracts rather than substitute
CPU vertex counts. SetTextFilterSize is a real 1-bit monochrome convolution
sampler contract (default1x1,1..7), not an empty state acknowledgement.
No SO/text-filter code was changed during the GS CI wait.
2026-09-12 SO follow-up: Local Mesa's native GS+SO entry accepts null shader
code and resolves passthrough against bound VS later. Native RegisterIndex
UINT_MAX is a declaration gap, masks must be contiguous, and the explicit
stride applies when all outputs use slot0. Multi-buffer packing derives
per-slot strides. SOSetTargets must clear all higher slots regardless of its
clear hint and preserve offsets/append semantics. A future full SO change
must allow no-PS/no-color-target capture and runtime-owned counters; merely
forwarding DrawAuto behind current drawReady would still reject that path.

## 2026-10-05 resumed DX8-DX11 UMD gate
- User explicitly prioritizes DXVK as the native VIOGPU KMD/UMD driver gate.
- DXVK checkout dxvk-umd-ci is clean at a677ab0 on work/driver-ci-umd-20260917.
- Paired driver checkout gg-dxvk-umd is clean on work/viogpu-dxvk-umd-20260917.
- Shared-surface create/open/coherence was added after the plan's September 12
  checkpoint; OpenedResources is now an acceptance gap, not an absent stub.
- Eight admission gaps remain. DXGI base table currently fills only Present;
  runtime rotation and primary ownership are material source gaps.
- Main gunyah-guest-drivers-windows checkout contains unrelated dirty haptics,
  INF and WDDM edits; continue in the existing independent clean checkouts.
- No relevant memory-registry entries exist. Recovery uses project files only.
- Initial broad file search was excessively noisy; use bounded project paths.
- One guessed paired-workflow filename was absent; discover workflow names
  before reading. No code changes or remote actions have occurred yet.

- Rotation now advances the DXGI base table from 1/7 to 2/7; all eight
  admission gaps intentionally remain. Private/present textures rotate pixels
  through one cached scratch resource so existing views keep their identity.
  Shared buffers publish dirty pixels before changing backing, then invalidate
  each cache, avoiding a GPU copy for clean allocation-only rotation.
- RuntimeAllocation rotates kernel handle/metadata/publication state while
  keeping runtime hResource stable for DeallocateCb, matching Microsoft ROS
  sample and the existing Mesa implementation.
- Linux GCC and Clang sanitizer policy suites each pass 172822 upload-span
  and 1125 shared-state checks. Windows runtime tests remain pending CI.
- Local SDK WRL lives under winrt; its lower-case includes reference mixed-case
  WeakReference.h. Use disposable lowercase aliases for syntax checks rather
  than changing SDK or source. Earlier two attempts were header-path failures.

2026-10-05 exact rotation validation: offline CI37310474949 executed the
production callback on x64 and x86: 1281 checks / 43 synchronized locks each.
Its ARM64 artifact11345428856, SHA256
9c4f107af80d7a3c4a86614cc6d37e9c65705f6da34fc198d5105ca8df0bc1f7,
also passed those checks in the existing native ARM64 Windows guest.
All use explicit test-only WARP; admission remains closed. Evidence is retained
under workspace artifacts/dxvk-native-rotation-20261005/. Full embedded backend
CI37310476652 and paired candidate pin integration remain pending.

Full CI37310476652 subsequently passed all six jobs, including native ARM64
execution of rotation (1281), runtime GPU (852), runtime entry (2637), device
lifetime (9075) and allocation (5154) fixtures. The eight admission gaps remain.
Paired gg-dxvk-umd reserves58522 and older Mesa sources; preserve its baseline
and treat its new package solely as candidate integration evidence. Current
native export set lacks D3D9 OpenAdapter, and native build disables D3D8/D3D9
cores. Full DX8-DX11 support therefore remains unfinished. Detailed source audit:
workspace .planning/dxvk-umd-remote-20261005/dx8-dx9-audit.md.

2026-10-05 callback-table continuation: Microsoft supporting-the-dxgi-ddi.md
and WDK dxgiddi.h require retaining the runtime DXGI table pointer, as callback
addresses can move between UMD calls. af84b74 fixes fixture lifetime after
8dbcc93 implements that production contract. Exact x64/x86 offline DDI
execution has passed; remaining architecture/full CI is monitored separately.
The kernel/core callback slots remain copied, with a live DXGI exception.

Paired37318070394 final job failed before KMD compilation: VS18/MSBuild18.10
on windows-11-arm requests WDK tasks18.0, while pinned WDK26100 supplies17.0.
Existing gg-zero-copy proves windows-2022/x64 MSBuild ARM64 cross-build plus
separate native ARM64 fixture and signed-package load jobs. Adapt that CI
plumbing while preserving target/source/version pins and all load checks.

Present review found raw Resource/allocation reads across reentrant backend
and LockCb operations; DestroyResource can reclaim/poison that private storage.
A focused resource reservation/independent state repair is in progress before
final source promotion. The DXGI callback-pointer contract remains correct.

Current source implements all mandatory D3D10 hardware table slots, bounded
GS/SO/DrawAuto and synchronous predication, opened shared resources and identity
text filtering. Old runtime-entry notes and contract comments claiming missing
slots or unconditional OpenResource failure were stale; corrected those facts
without changing the eight admission gaps. Full semantics/Turnip acceptance,
primary/full DXGI, D3D11 typed ABI and native D3D9 remain open.

Present repair review also found same-surface nested Present while readback is
mapped, plus Present/rotation mutual reentry while allocation metadata changes.
Use a per-surface busy guard, reject rotation chains containing an active
Present, and reject Present while device rotation is active. Resource retirement
still needs independently held backing and reservation checks after callbacks.

Independent follow-up: per-surface Present exclusion alone does not protect
the device's shared publication sweep; a nested Present for a different source
can remap the same shared staging texture. Use device-wide Present exclusion
and mutual rotation exclusion. Also validate the independent resource
reservation after RuntimeMemory's identity/context callbacks, immediately
before PresentCb; a pinned allocation can outlive a logically retired Resource.

Next DXGI residency source work must connect each native resource to the
actual backend BO allocation identities before QueryResidencyCb can report
VidMm residency. RuntimeGpu currently owns token-to-kernel-handle records
privately, while D3D11VkInteropSurface::GetVulkanImageInfo exposes Vulkan image
metadata rather than kernel BO handles. The separate Present publication
allocation is not proof that every rendering allocation is resident. Do not
fill the missing QueryResourceResidency slot with unconditional resident values.

2026-10-06 read-only guest system inventory (Windows26100, native ARM64
PowerShell): System32 has ARM64 d3d9/d3d9on12/d3d10/d3d11/dxgi, no d3d8.dll.
SysWOW64 has all six as x86. DX8 system-runtime acceptance can target x86 on
this guest; existence/architecture does not prove its DDI route or DXVK loading.
Remote worker retains separate inventory JSON under workspace remote-test plan.

Bounded native D3D9 design: actual WDK OpenAdapter.hAdapter and
CreateDevice.hDevice are in/out opaque runtime-to-driver handles. Preserve
original runtime handles independently of driver registry tokens and negotiate
versions with D3D9 policy, not D3D10 packed-version rules. Next harness slice
can test exact160-byte identity/query ownership and typed GetCaps/close without
advertising rendering or bare production OpenAdapter. It must then connect the
embedded D3D9 core to renderer-neutral exact-LUID/Turnip/runtime owner selection;
unchanged public CreateDeviceEx would reintroduce direct ownership/implicit WSI.

2026-10-06 guest KMD build continuation: the existing Windows VM has no
MSVC, MSBuild, SDK, WDK, git, Python or winget. Audit completed in0.597s;
C: has13,158,424,576free bytes. Local SDK/WDK26100.1 payloads provide
headers/libs but no cl/link/rc/tracewpp executables. Remote worker identified
official VS17 native HostARM64-to-ARM64 MSVC14.44.35229 package(22.6MB)
and is preparing an isolated portable toolchain. Setup and actual compile
timings must be distinguished before comparing with CI.

Paired docs-only checkpoint09ec84ca56eca381c953a9a307852b8e05920625 is
pushed and clean; binary source remainsb6bf/34ff.166retained evidence files
verified, duplicate signed ZIP removed after recording its metadata digest.

Independent final receipt review matches all counts/source pins/scope. It
found guest process creation UTC labels later than fixture UTC labels. Before
and after identities still match; annotate evidence README and preserve raw
receipts. New guest build timing uses Stopwatch rather than wall-clock delta.
Retained CI KMD WPP-to-code-generation interval is16:25:10.181-16:25:14.428
(~4.25s), excluding VirtIO and packaging. Guest iteration saves full CI
turnaround; do not compare direct KMD compile with full signed package jobs.

Guest build source is exactb6bf Git archive(build/VirtIO/viogpu),294files.
Independent reviewer checked every file byte against Git blobs/receipt:
source tree04d3c8ce75ca2d5d2c9a310ea371862851bae178; archive SHA256
acccddd1f91a570e848ec83fb0a35547c91be3fce6d110dfe0fbd81d5115b765.
Native SDK.BuildTools26100.1 provides rc and tracewpp; portable MSVC catalog
package revision35229 uses internal path14.44.35207. Actual compiler banner
and package revision must be recorded separately. Source and installed driver
are unchanged while direct compiler build-only reproduction is prepared.

Refined CI37338579561baseline from retained logs/API: KMD WPP/CL/RC/link
4.391s; KMD project wall6.624s(reported6.55s). Plain VirtioLib CL+LIB6.222s;
full dependency project13.751s. All ARM64 driver step73s, product job293s,
full workflow1190s(19m50s). CI used HostX86-to-ARM64 compiler on x64 Windows;
guest uses native HostARM64. Direct compile is a faster potential feedback
path, not equivalent signing/UMD/hardware acceptance. Guest link currently
needs arm64rt.lib not found in SDK/WDK/CRT Desktop packages; parallel research
is active while remote prepares compilation.

Resolved arm64rt.lib: official Microsoft.Windows.SDK.CPP.arm6410.0.26100.1
contains c/um/arm64/arm64rt.lib, 1,365,588bytes. Library SHA256
83925e5f51c0599deca7871667e04b0a6dc046dcec77115cfb5d4af1a655b823;
archive SHA256e0d6e5e019e28d92f24c9807afb04ad9e95ec685a1f742c30c3c7fe5abd0a109.
Portable base setup is complete; compilation remains pending. No installation.

Final guest KMD build and independent output verificationPASS. Native
MSVC19.44.35229 tools need no system installation; SDK26100.1ARM64/WPP/RC
works once kernel CRT headers precede MSVC and RC uses direct arguments.
VirtIO0.648s/KMD4.113s/runner5.114s. SYS329216bytes,100.6.101.58522,
SHA2567c0a45f2bbf3424ff8a67c4c6c2369d13e329331c5e4adf46ab3b74d196e934b.
Source/input/output hashes and unchanged58623/DWM/Explorer are verified.
Unsigned/uninstalled build provides rapid compiler feedback; full package
signing/runtime admission/hardware acceptance still require independent gates.

2026-10-06 recovered clean DXVK docs HEAD f003ed6; binary source remains
34ff484. No D3D9 source edits existed at handoff. WDK D3D9 OpenAdapter and
CreateDevice use in/out runtime handles, GetCaps receives a const argument,
Interface is a literal API version, and DriverVersion uses the D3D9 UMD
interface constant. This continuation implements only typed identity/caps/
close development contracts and keeps production device admission closed.

Implemented a raw HANDLE query overload used by the typed D3D9 bridge while
retaining the D3D10 overload. D3D9 only accepts literal Interface9; runtime
Version has no documented packed minimum and is not used as one. Caps sizes
are strict: UINT counts and D3DCAPS9, zero-byte empty data lists. All counts/
rendering caps are zero. DriverVersion is the compiled header constant. Saved
runtime owner/query pointer, shared in-flight state and monotonically issued
tokens protect close/reentry and stale-handle reuse. Current identity changes
and reset/removal errors retire the adapter; CreateDevice publishes nothing.

Guest D3D9 adapter fixture PASS42429checks. Native MSVC/SDK-WDK source compile
with /W4 /WX succeeds; static release CRT fixture has only KERNEL32 imports.
Compile/link18.0064796s, fixture3.5028309s, timed runner28.5989371s; setup and
transfers separate. Guest sources match all nine input hashes; downloaded EXE
hash2fc771f057c2994e614c24129f76a416bdbe968eb11c253cc437fe52dfb1d85b.
Driver58623/oem10.inf, DWM1644/Explorer5828 retained. No installation or
configuration changed. Full CI still required for normal toolchain/architectures.

Next renderer audit: D3D9InterfaceEx constructor acquires the API singleton,
enumerates displays/adapters, and may change process DPI awareness. Its public
CreateDeviceEx calls DxvkAdapter::createDevice() without RuntimeBackend and
then InitialReset; InitialReset invokes ResetState and ResetSwapChain, which
constructs the implicit presenter. D3D9DeviceEx constructor already accepts
an owned DxvkDevice but depends on parent config/options/compatibility and
starts/synchronizes its command thread. A private parent and separate offscreen
state initialization are required. RuntimeGpu already accepts raw device
HANDLE/common D3DDDI_DEVICECALLBACKS; its identity still uses D3D10 wrappers.
Factor renderer-neutral adapter identity/selection/runtime validation with
focused shared-contract tests before reusing it for the D3D9 embedded core.

Offline37408748554 ALL4PASS at sourceaac5172: x64/x86 production DDI plus
D3D9 fixture execution, ARM64 compile and Linux sanitizers/negative controls.
Next constructor detail: ResetState's render-state defaults are independent
of the implicit swapchain, while InitialReset invokes both state and WSI
initialization. An explicit offscreen initializer can reuse defaults without
ResetSwapChain; swapchain-dependent COM methods must not become native DDIs.

Full37408749906 ALL6PASS at sourceaac5172. Logs confirm42429 D3D9 checks on
x64/x86 and native ARM64; previous D3D10 rotation/runtime/entry/lifetime and
resource fixtures also passed. Full artifact IDs: x64 11388846935,
x86 11387858864, ARM64 11388661772, native validation 11388318354. These are
development contract/build results; production OpenAdapter/rendering admission
and real DX8-DX9 system runtime acceptance remain open.

Private D3D9Adapter construction queries Vulkan capabilities and format tables
but does not enumerate displays. Its parent can receive exactly the selected
instance/adapter and skip singleton/DPI/display setup; the private parent's
public CreateDeviceEx must reject direct device creation. ResetState plus
explicit viewport/scissor and a recording/submission barrier form offscreen
initialization. FPU_PRESERVE prevents native construction changing caller FPU.
The concrete device remains internal; the development probe returns only
swapchain count and destroys it synchronously. An active caller dispatcher is
required across constructor worker joins and destruction. This source slice
does not wire typed CreateDevice or claim successful target GPU construction.

Failure-path review found existing D3D9 constructor raw helper ownership:
annotation/initializer/converter are allocated before constant-buffer and
fixed-function creation, but only deleted in the successful object's destructor.
A late constructor failure can keep the DxvkDevice/runtime owner alive.
Use local unique_ptr guards until construction commits; preserve existing
successful destruction and module-detachment behavior.

Source9b4168b offline37412714285 passed all four jobs. Full37412713033
compiled all three embedded architecture cores but failed DLL linking:
D3D9GlobalAnnotationList::s_instance was defined in excluded d3d9_main.cpp.
The definition belongs in d3d9_annotation.cpp, shared by public API and embedded
targets; moving it resolves ownership without embedding public API exports.
Retained failed logs under the embedded evidence directory.

Guest native ARM64 runtime header/owner fixture reports49PASS at exact9b4168b,
compile/link0.8069173s, execution0.0603778s and runner1.0122613s. Receipt lists
ten source inputs; all must be checked against Git plus executable/PE and
driver/desktop continuity before claiming independent evidence verification.

Guest receipt verification is complete: all10inputs match exact9b4168b Git
blobs; EXE SHA256 matches, LLVM confirms nativeARM64 and no D3D/DXGI/Vulkan/
dynamic CRT imports, compiler/run stderr empty and58623/desktop retained.
Full replacement37415595130 runs exact2a4095c; identity/shader CPU jobsPASS.

Next-device audit against local official WDK docs: CREATEDEVICE preserves the
incoming opaque runtime hDevice then returns a unique driver handle. Its
pCommandBuffer/allocation/patch-list fields are obsolete; obtain actual backing
through CreateContextCb. Devices must not have an arbitrary hardcoded count
limit. DestroyDevice must unlock allocations before DeallocateCb and release
all device tracking. AllowMultithreading and AllowFlipBatching need separately
supported semantics rather than passing raw Flags.Value as COM behavior flags.

Resource bridge audit: CREATERESOURCE hResource is also in/out; runtime
and driver handles must be distinct. SurfCount includes mip/face surfaces and
must map to explicit resource/subresource ownership. Shared allocations must
be created atomically in one AllocateCb and destroyed with runtime hResource,
NumAllocations0; RuntimeGpu's internal Vulkan BO allocations are not sufficient
proof of shared D3D9 resource semantics. Keep sharing/primary caps closed.
References: workspace reference/codes/windows-driver-docs-ddi/wdk-ddi-src/
content/{d3dumddi,d3dukmdt}; CreateDevice/DestroyDevice/CreateResource pages.

Final2a4095c evidence verificationPASS: full37415595130 ALL6SUCCESS, all
three native DLL architecture/import/export policies,42429adapter/49owner/
45DLL-rejection checks per architecture, and all13nativeARM64 executable
hashes tied to downloaded artifacts.166retained CI files hashed. Public API
Windows37415565405 and package37415565396 alsoPASS at exact2a.
The embedded checkpoint does not establish successful target construction,
typed device/resource/rendering semantics or ordinary DX8/DX9 activation.

Typed-device continuation recovered clean c1ec9e0. The D3D9 adapter stores
only raw adapter/query identity today and unconditionally rejects CreateDevice.
RuntimeGpu already copies DEVICECALLBACKS and accepts a neutral AdapterIdentity
plus RuntimeService. The embedded D3D9Backend requires synchronous caller
pumping during construction and teardown; its development probe does not pump.
Connect the typed lifecycle with controlled backend fixture coverage before
resource/rendering DDIs. Keep zero caps and production OpenAdapter absent.

Guest initial lifecycle fixture PASS63962 checks, nativeARM64 compile2.961s/
run0.061s with /W4 /WX. Safety review identified the raw adapter lifetime edge:
after callback CloseAdapter, RuntimeGpu identity and fence/context escapes
must stop before reading an expired adapter handle. Added an optional shared
identity lifetime gate used by D3D9; device-only unlock/deallocate/context
cleanup remains callable. D3D10's default absent gate preserves its contract.
Added adapter-close-before-Flush and close-during-CreateContext controls.

Recovery at af9ecd5: offline37419571615 completed all four jobs successfully;
full37419569213 still builds ARM64/x64/x86. The old embedded CI verifier covers
13 ARM64 execution hashes and42429/49/45 checks; the device continuation must
also verify66403 device checks, the new executed fixture hash and the real-KMT
probe's native PE/source provenance. The hardware runner stages unregistered
process-only ICD/loader copies and refuses existing evidence directories.

Next resource audit confirms reserved fields must be ignored when their usage
flag is absent: MipLevels without texture/cube/volume, Fvf without VB,
multisample fields without render-target/video usage, and RefreshRate/Output
without Primary. SurfCount determines surfaces; per-surface indices retain
the runtime's input ordering. Plain flags-zero resources are lock/blit only.
DestroyResource must submit dependent batched commands before allocation
cleanup, which can be deferred for nonshared resources. Lock must validate
mutually exclusive read/write, discard/no-overwrite and range/area/box flags.
These contracts constrain the next typed resource slice after hardware
construction; no resource DDIs are implemented in this checkpoint.

First hardware probe (af9ecd5 hardware01) fails HRESULTd0000001 before
KMT_ADAPTER output, so the failure is the GDI-display-name adapter lookup,
not D3D9 backend creation. Inspect actual adapter/session visibility and
prefer exact LUID selection if GDI display enumeration is unavailable to SSH.

Explicit native KMT enumeration succeeds in SSH and returns d36b/686b LUIDs,
both with zero presentation sources. Guest bfd7e77 probe compilation and six
CLI controls pass, but neither private160byte reply passes identity decoding
and LUID comparison. Inspect raw query status/reply before assuming adapter
ordering, modifying runtime admission or constructing the backend.

Recovered readiness inventory downloaded successfully. It contains class keys
0002 (58623/oem10.inf) and0004 (58609/oem5.inf); both have fail mask256,
but hardware reset/epoch/timeout values differ. These are not current-state
proof until the active PCI PnP Enum Driver binding is resolved. Extended the
read-only collector to bind the exact active device, signed driver, service
and binary hash. Windows22 and Android8022 independently online; DWM1644 and
Explorer5828 were continuous through hardware02. No installation changed.

Active PCI binding confirmed0002, serviceVioGpuWddm running with58623/oem10.
DriverStore SYS SHA25654f4d93769ad317b389fb887422208031e20726c1950123d53338adaf7cbc37a
matches retained signed CI package37242831438, parentd10f9ba6. Current receipt
has reset state1, native context/adapter epoch3 generation2, readiness256,
first reset141868 (0x22a2c), first synchronous timeout251108 (0x3d4e4),
STATUS_TIMEOUT258,32command bytes, lifecycle held50019ms. Exact SYS/MAP BLs
place first reset in CompletePagingBufferOperation and timeout in
CtrlQueue::SubmitNativeControl. Scalars lack first-event timestamps, so do not
infer which came first. Aperture failure stage19/statusc00000a3/detail0.
Live daemon lists sole running Windows VM PID3980 with configured6656MiB;
preserved live configuration takes precedence over stale8192MiB notes.
The primary KMD checkout has pre-existing source/haptics edits; preserve them.

Continuation confirms both SSH22 and SSH8022 online. Exact d10f9ba6
UnmapStandard detail0 can originate in Dod submission rundown, missing
adapter, invalid resource ledger, reset reconciliation, or queue admission/
allocation. Destroy2DResource may run UNMAP_BLOB then UNREF; the existing
scalar does not distinguish these operations. The older paging diagnostic
patch provides common-clock timeout/engine-reset observations, but the actual
first reset caller here is CompletePagingBufferOperation, so engine-reset-only
capture cannot identify this first refusal. Do not change reset admission or
declare event ordering from the current receipt.

The c1b9ea69 diagnostic records immutable first queue-admission refusal,
standard2Ddestroy failure and reset request using the timeout's common
KeQueryInterruptTime clock. It does not repair readiness. Fixture-only
fdfd8f99 retains identical118production build inputs. Verified signed58624
package preserves47installed runtime files and carries51verified rollback
files; active PnP installation reports oem17.inf after PnPUtil0. A fresh boot
and exact active service/binary hash are still required to attribute new
records. Nonexportable test signer DF9791D9362A8E2FA962C1683625D74D4DAE5D05
is trusted in machine Root/TrustedPublisher; no private key was exported.

Fresh58624boot01 binds device08/0002/oem17/serviceVioGpuWddm to signed
SYSd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a.
First reset time1025960378ticks (102.5960378s), hardware state0before request,
callerRVA595a8: exact MAP/PDB and ARM64 BL at595a4 resolve to
VioGpuWddmResetEngine. First admission/destroy/submitted-timeout records are
absent in this snapshot. This differs from old58623paging-reset provenance;
the old UnmapStandard hypothesis is not established for this boot. Desktop
snapshot has DWM2172 only; query user reports no logged-on session. Preserve
the evidence and inspect current boot/session rather than assume success.
Lifecycle holder4460 resolves to AcquireNativeContextSnapshot. Historical
present scalars are not automatically tied to this boot's immutable records.
Native transport=zero has no usable VNC listener5900 despite stored config;
one attempted capture closed at handshake. Removed the temporary ADB15900
forward and collected the Android console instead.

Boot02 after autologon has DWM1552/Explorer4184 and active hardware reset0,
queue epochs enabled/generation3. The immutable observations are reset request
102.5960378s, first2Ddestroy108.8039849s/stage2Dod submission rundown/refused,
and first native AHB paging timeout130.9818822s/48bytes/caller3ba1c. Exact PDB
resolves timeout to CtrlQueue::PageNativeAhbSynchronous. These captures do not
establish that destruction or the submitted timeout caused the earlier reset.
The historical NativeReadinessFailMask256 was stale until a new private query.
Correct --list-adapters now returns physical LUID6e6c000000000000 with decoded
VIOGPU identity1; software LUID046c000000000000 has identity0. Initial --list
was rejected with the documented usage and corrected without a backend run.
Host logs report native display surface unavailable while the management page
was open. Opened the existing VM's Native Display; screenshot confirms the
3040x1904Windows desktop. No memory, image, backend or registration changed.

Hardware04 uses unchanged af9ecd5UMD/bfd7e77probe and pinned Mesa/loader.
Measured SSH token: Administrator, session0, elevated/high integrity12288.
Limited task runs as Explorer ownerDROIDVM\USER, session1, elevationtype3,
not elevated/medium integrity8192. Loader now selects process-local
turnip-native.json/viogpu_gl_vk_arm64.dll; CreateDevice still returns8876086a
in0.5892604s. Owned scheduled task removed; desktop/58624oem17retained.
Thus installed ICD selection was a real measurement issue but is insufficient
to explain the remaining construction failure. DXVK log ends after physical
Adreno830/LUID6e6c enumeration, before device initialization logging.
Pinned Mesa source3e50dd4 has no MWD_STYPE_SUPPORT/mwd_support implementation;
runtime integration is separate ada48c1+d6883df under the existing
mesa-dx-runtime-integration-20260913checkout. Measure the private property
reply, then integrate the missing bridge into the pinned candidate source.

## 2026-10-06 measured missing matched Mesa runtime bridge

Property-only native ARM64 probe, compiled against35 hashed local/Git inputs,
exe00f402d1821ddaaba86d0647e0e81c290caafb1c691f4cee832897a7612e4d11,
ran under measured Limited USER/session1. Exact pinned ICD9cbe528f selected;
MWD_SUPPORT returns magic/version/size/flags all0 instead of3152574d/1/88/1.
No VkDevice is created. Desktop and installed58624oem17 continuityPASS;
owned task removed. Independent receipts/EXE/PE/input verificationPASS.
Hardware03 and04 all9 payload hashes also verify. The property diagnostic is
local source plus exact bfd7e77 Git headers, not a tracked probe at bfd7e77;
its actual cpp/archive hashes are retained in runtime-support-01/verified.json.

Created isolated mesa-dxvk-matched-runtime-20261006 at exact3e50dd4.
Porting ada48c1+d6883df requires resolving shared-runtime/residency adjacency
conflicts while retaining both paths. Second patch adds teardown fixture fields;
one premature fixture compile saw unresolved second-patch markers and is
invalidated. Complete conflict resolution before rerunning source validation.

Mesa integration8443c71 retains exact3e50dd4 ancestry and identical DXVK
protocol header. Runtime-owned mode bypasses direct KMT handle/residency
creation; its callbacks own those services. Direct mode preserves all existing
residency operations and six fault oracles. Guest real MSVC/SDK/WDK compile
and controlled dispatch fixture are independently verified (7source hashes,
exe8d77424eba1802fe4bc4b0244f9c7d64c05f03cc38ac1d44b49c6c1fbea9017d).
Retain exact CI metadata and source-hashed candidate before trying construction.

## Matched Mesa target bridge and lifecycle08

Mesa8443c71 exact ARM64 artifact11408122116 from successful CI37453384744
returns MWD support3152574d/1/88/1 in the real Limited USER session1. The
unchanged af9ecd5 UMD and bfd7e77 KMT probe now succeed at open, create,
flush and destroy, with balanced context1/1, allocations3/3 and locks3/3.
Overall probe still fails E_FAIL because it requires renders>0 despite issuing
no resource/clear/draw operation. Empty offscreen initialization only changes
state; no GPU submission is yet proven. Audit the oracle and preserve this
failed receipt before adding a rendering stage. Desktop/58624 remains retained.

## Matched runtime follow-up09 submission loss

Mesa8443 multiarchitecture CI37453381660 all4SUCCESS (ARM64/x64/x86 plus
source regressions). Corrected empty-lifecycle probe271a83c was compiled on
target in1.066s, all51Git/source hashes, native AA64 and6invalid CLI checks
verified. Follow-up09 creates successfully but Flush reports88760868; DXVK
logs VK_ERROR_DEVICE_LOST from its submission queue. Do not claim a stable
lifecycle PASS from08; preserve both failed receipts and collect Mesa/KMT
submission diagnostics before adding resources or re-running identical work.

## Construction callback pump boundary

Diagnostic device10 with TU_WDDM_DIAGNOSTICS1 passes the empty lifecycle
(context1/1, allocation3/3, lock3/3, render0, wrong_thread0); independent8input
and3script hashes/token/58624continuity verification passes. The prior09
submission loss is intermittent, and no root-cause equivalence is claimed.
D3D10 enables deferral before its construction pump; D3D9 did so only after
run() returned. RuntimeService rejects non-caller requests when m_pumps0 and
m_deferredfalse. A completion worker crossing that gap can see UNSUPPORTED.
Source f648a1a constructs the owned D3D9 service with deferral enabled from
the beginning; synchronous non-owning probes retain their rejection behavior.
Native ARM64 fixture now66410checksPASS; a compiled control disabling initial
deferral fails at the first new behavioral assertion (not compiler failure).

## Exact startup candidate target acceptance

Source f648a1ab67bdf86794542d4e6bb5622290467e1f passes offline37456996101
all4 and full37457000846 all6 jobs, including actual native ARM64 execution.
All three architecture artifacts and14ARM64 execution-to-download EXE hashes
independently verify; each device fixture now has66410checks. Normal public API
build37456982139 and package37456982302 are also successful.
Exact ARM64 artifact11410126793 yields UMD6fdc07b0db947b975632b45105b5b4bdeecc4c0de744ba2231466413f4a10350
and probe7260356b39b107a882a21d053fb0c6c84548fcae9dffb8b1311952653487c175.
Archive e97ea08c26c74154de482e6492305dbd3a18a54ed2ffce57d1582847d2e8690e
is verified by the guest staging script before process-local extraction.

Fresh device11/12/13 all PASS with diagnostics1/0/0 and stopwatch durations
0.9432165/5.2373924/2.3940590seconds. All KMT/open/create/flush/destroy stages
S_OK. Each context1/1, allocation3/3, lock3/3, query44, escape6, wrong_thread0,
render0. Limited DROIDVM\USER/session1 token independently verifies. All
three archive checksums, eight payload hashes and three wrapper hashes verify
against retained exact CI and shared Mesa8443 payloads. Wrappers are saved
per evidence run, and all owned scheduled tasks are removed.

Read-only snapshot14 independently verifies actual PnP binding0002, statusOK/
error0, running driver service, 58624/oem17 and exact signed SYS
d48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a.
DWM1552/Explorer4184 retain their original start times. ADB5555 responds and
root ADB finds only existing crosvm PID10130; Windows/Android SSH both respond.
Termux's app process namespace does not expose crosvm to its pgrep/pidof;
that command's exit1 is not an SSH service failure or evidence of a stopped VM.
No candidate installation, registration, reboot, image or configuration change.

Acceptance is bounded empty offscreen lifetime. No pixel work or nonempty
render submission is proven; long-term stability and prior09loss causation
remain open. Source deferral repair closes a proven CPU callback-service
contract gap but is not established as the cause of09loss. Next typed resource
work must add ownership/state/clear/draw/readback and a separate pixel/submission
oracle before widening caps or runtime activation.
Retained evidence: artifacts/dxvk-native-d3d9-startup-20261006/ with CI logs,
exact candidate, device11-13 verified receipts and active-binding snapshot14.

### Independently verified resource fixture controls

Latest worktree03 receipts are under workspace-root artifacts/
dxvk-native-d3d9-resources-20261006/guest-*/verified.json. Reusable verifier
.planning/dxvk-umd-remote-20261005/verify-native-resource-fixtures.py checks
source52/archive/EXE identity, architecture, desktop and driver continuity;
negative receipts retain their original failures and separate measured sources.
Actual src/d3d9 core resource implementation still requires the full CI build
and target GPU byte test; fixture renderer is an explicit controlled substitute.

### Next draw contract audit while full resource CI builds

Microsoft CreateVertexShaderDecl takes a counted D3DDDIVERTEXELEMENT array
and returns a nonzero ShaderHandle (output only, unlike resource runtime
cookies). The native26100 header matches D3DVERTEXELEMENT9 field widths, but
explicit element validation and a copied terminator are required for the COM
core; never expose COM pointers as driver tokens.
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_createvertexshaderdecl

SetStreamSourceUM permits one user memory stream, nonzero DWORD-aligned stride;
its typed argument has Stream and Stride only. Keep the source address in
per-device state and snapshot the exact DrawPrimitive vertex span before any
runtime query/callback, with checked VStart/primitive-count arithmetic.
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_setstreamsourceum

Native SetRenderState maps runtime BeginScene/EndScene through the special
D3DRENDERSTATE_SCENECAPTURE state. Blind casting every DDI state to COM
D3DRENDERSTATETYPE is incorrect; route scene capture explicitly and validate
actual standard state mappings. DrawPrimitive flag buffer encodes triangle
edge flags for line-fill fans and must not be silently ignored.
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_setrenderstate
https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_drawprimitive

Native viewport splits XY/width/height (SetViewport) from MinZ/MaxZ (SetZRange).
Core SetRenderTargetInternal resets viewport/scissor when binding RT0; audit
that native behavior together with explicit state before the next typed draw.
The current clear probe intentionally uses only full target viewport.

### Target17 KMT refusal and residency ownership

The diagnostic callback is reached: first stage1readback submits484bytes,
6allocation entries and6patch entries; D3DKMTRender returns c0000001 while
keeping capacities65536/1024/1024. KMD NativeRenderFailure fields remain0;
no new epoch/reset/timeouts. Empty record does not prove absence of DdiRender,
since initial validation branches precede the diagnostic publication.

KmtRuntime9::open only creates a KMT device. Its allocation/render/deallocation
callbacks omit CreatePagingQueue, MakeResident, pending paging fence wait and
Evict. Exact matched Mesa direct transport has these operations; its shared
transport delegates backing to the original runtime owner. The target harness
is that owner for these tests. First correct its ownership without modifying
or reinstalling production UMD/KMD; retain16/17 as failed evidence.

Primary specifications:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ns-d3dkmthk-_d3dkmt_render
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/driver-residency-in-wddm-2-0
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/gpu-virtual-memory-in-wddm-2-0
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dukmdt/ns-d3dukmdt-d3dddi_makeresident
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ns-d3dkmthk-_d3dkmt_waitforsynchronizationobjectfromcpu

Physical engines retain allocation/patch-list scheduling. Thus missing explicit
residency is an audited harness gap, not yet proven root cause of c0000001.

### Raw-KMT residency hypothesis tested

The corrected probe aaa70c9 passes the unchanged production dde00ed workload
with the same Mesa8443/KMD58624: six allocation handles each gain one residency
reference, pending fences7001..7006 are waited on, and all three raw renders
return0. Matching evictions and all runtime lifetimes balance. Targets18/19
verify192 actual pixels with diagnostics1/0. The missing residency ownership
was the measured harness gate for16/17. No installed KMD Render/reset/timeout
record changes. Do not generalize this proof to older09 loss, ordinary runtime
admission, drawing, presentation/reset, or long-term stability.

### D3D9 draw callback lifetime

The original operation helper checked identity before executing its body.
A UM draw therefore cannot first copy caller memory in that body: the query
callback can overwrite it. A serialized preparation step now snapshots on
the DDI caller before entering RuntimeService::run/pumping. The native fixture
mutates arguments and vertices in the first query callback; production consumes
the old snapshot, while a separately compiled reread mutation fails. Bounded
UP upload sizes and pointer ranges are checked before any memory access.

The actual SDK declaration deletion slot is pfnDeleteVertexShaderDecl.
SCENECAPTURE62 is a native scene command, not a public render state.
Non-null DrawPrimitive edge flags describe line-fill triangle edges and need
a separate path; this slice accepts the fast path only. Multi-stream/buffer
and indexed draw remain explicit gates. The clear/readback slice18/19 remains
verified; new quad/scissor/colour-mask checks have no target receipt yet.

### Verified hardware D3D9 draw20/21 and next shader work

- Sourcec874d55 full37477931517 all6SUCCESS/offline37477937872 all4SUCCESS;
  API37477846431/package37477846467 SUCCESS. Downloaded3architectures and
  14 actual native ARM64 execution hashes independently verified.
- Exact c874 UMD/probe with matchedMesa8443 and installed58624 pass target20/21,
  diagnostics1/0. Both independently verify384pixels (192clearffefa655,
  192draw53a03d45), intact padded readback, nonzero vertex-start and three
  quad/scissor/colour-mask stages. Six nonempty KMT submits succeed; context1/1,
  allocation10/10, lock9/9, residency10/10 and wrong-thread0. Elapsed3.7381376s
  and2.9324823s; eight payloads/three scripts/nativeARM64/Limited USER session1.
- Evidence archives22b0e6fcb1a61c60e8c294bf4a1364b1288ae6201f498faf5c64402f276e375f
  and92c86b386e833d37c120234037b9a875ed0559a7accbb48b749457d9832a913e
  independently verified. Both owned tasks removed. Fresh exact signed58624/
  oem17/SYSsha/PnP0/binding0002 and DWM1552/Explorer4184 retained;54 native
  render-failure/reset/epoch/timeout/admission values unchanged after both runs.
- Record offscreen draw acceptance only. Next typed programmable shader and
  constant ownership; textures/buffers, presentation/reset and ordinary DX8-DX11
  system-runtime acceptance remain open. Full goal active, no caps/pins widened.

### Typed D3D9 programmable shader worktree

- Added owned VS/PS create/bind/delete and six float/int/bool constant DDIs.
  Caller snapshots precede first callbacks. Bounded shader entry keeps byte
  count through the existing SM1-3 translator. Stage/device/type-safe handles,
  postcreate identity, retryable deletion/flush and worker release enforced.
- Native ARM64 MSVC fixture02 PASS107385, compile6.5373586s/run0.0744307s.
  Four compiled mutations caught intended contracts: shader reread2440/1044,
  constants reread3603/940, missing identity9303/1084, missing flush7451/1069.
  Independent56source/archive/after-run/nativePE/driver/desktop verification
  passed. Earlier01 PASS107297 retained; it lacks the optional build-base pin,
  but all56 byte hashes independently link.02 explicitly pins ba2d147 base.
- --shader target oracle includes previous384pixels plus192 actual SM1/2/3
  pixels. SM3 uses float/int/bool in both stages; independent checksum1384c5a5.
  Commit exact source, complete architecture gates and run in Limited USER.
  Ordinary admission, caps and paired pins remain closed; full goal active.

### Shader validation continuation and next texture contract

- Exact shader source8c4e097 and56 native positive fixture input bytes match.
  Offline37486999026 all4PASS; API37486881619PASS; full37486995183 x64/x86,
  identity and shader CPU PASS, ARM64 renderer/native execution pending.
  Strict576pixel shader harness is transferred and all3 hashes/parser checks
  pass. Fresh target baseline records DWM1552/Explorer2488 and exact signed
  58624/oem17/binding0002/PnP0/SYSsha;58 failure/reset/readiness values remain
  identical to historical21. Explorer changed before this new GPU test.
- Texture DDI is a coherent mip chain, not independent offscreen surfaces.
  WDK26100 flags distinguish Texture/CubeMap/Volume, with MipLevels and SurfCount
  determining the group. Existing surface slice ignores reserved MipLevels
  appropriately when no texture flags exist. Keep that behavior for surfaces.
- [Microsoft CreateResource](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_createresource)
  defines all implied attachments in atomic creation, retained raw runtime
  hResource versus returned private token, and permits deferred allocations.
  A2D mip chain has one surface per level; a cube has six times the levels.
- [Microsoft TexBlt](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_texblt)
  must copy all common corresponding mip levels, including differently sized
  top levels. Arbitrary individual-level copies use Blt. Avoid a top-level-only
  upload implementation that silently leaves lower mip data stale.
- Native texture-stage state combines fixed-function D3DTSS and D3DSAMP state
  enum values; explicit mapping is required. Retained SDK also includes native
  colorkey states33/34, which cannot be cast to public texture/sampler enums.
  [SetTextureStageState contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_settexturestagestate).
- [D3DDDI_SURFACEINFO](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dukmdt/ns-d3dukmdt-_d3dddi_surfaceinfo)
  restricts pSysMem to SYSTEMMEM and preserves caller pitch/slice pitch. Reuse
  external lifetime/padding checks; texture creation and teardown must retain
  worker-owned underlying DXVK texture plus typed subresource surfaces.

### First shader target refusal is a probe input declaration error

Source8c4e097 passes all full/offline/API/package CI and independent actual
artifacts. Target22 returnsS_OK for shader create/draw/readback and all7KMT
submits; old384pixelsPASS but first shader pixel remainsclearff17293b rather
thanff204060. VS1 probe omitsdcl_positionv0. Actual core BindInputLayout maps
elements only through shader semantics, and pinned sm3IoMap requires DCL for
VS inputs even beforeSM3. [Microsoft vs_1_1 input registers](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-1-1)
explicitly require DCL; [dcl_usage](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-usage-input-register---vs)
supportsvs1.1. Correct onlytheprobe and retain unchanged productionDLL8c4e097,
pixel expectations/checksums/Mesa/KMD. No programmable pixel acceptance yet.
Failed22 archive24ec8d89ae202daf8405b14c203bcad6c00422d6882e8bb4b9d03f4228dcb6d0
and same-run driver/desktop/58 sensitive-value continuity independently retain.
Earlier Explorer fault identifies installedviogpud3d.dllc0000005/RVA10db80,
timestamp6ac2defb. Retainedfdfd legacyDLL has another timestamp and smaller
image; exact installed bytes are required before attributing its source.

### Verified SM1-3 target checkpoint and texture continuation

- Probe-only b3e83c8 corrects the required VS1 input DCL. Offline37491825113
  all4/API37491604110/package37491604172SUCCESS. Native ARM64 MSVC compile
  0.7457505s, all51 source/archive/after-run hashes, native PE/imports, six CLI
  negatives and exact KMT identity verify. Production DLL remains8c4e097.
- Target23/24 independently PASS576pixels diagnostics1/0:192clearffefa655,
  192draw53a03d45,192shader1384c5a5; nine successful nonempty KMT submissions,
  ctx1/1alloc12/12lock11/11residency12/12remaining0wrong0. Elapsed4.0513128s /
  4.3172908s. All8payloads3scripts/nativeARM64/LimitedUSERsession1 verify.
  Owned tasks removedexit0. Evidence archives28c9f39bd0820b1903fb1dbe2a695a06f001dd4455a37fadcb45f7bae5a9a547
  anddbe2cc26de78c10f2f7d4ead6f1e0730d21ea308ed559ca2b6c72691700ce334.
  Fresh exact signed58624/oem17/binding0002/PnP0/SYSsha/DWM1552/Explorer2488
  and all58 sensitive failure/reset/epoch/timeout/admission fields unchanged.
- Installed Explorer fault binary9d72bfee matches signed58624 Mesa D3D10 Zink
  sourceeaad15fa/image11833344/timestamp6ac2defb/viogpud3d_zink.pdb identity.
  Keep separate from legacyshim and uninstalledDXVKcandidate; matchingPDB/dump
  needed for exact cause. Guest/host absolute clocks differ; use guest-relative
  ordering and Stopwatch timings. No VM/image/package/clock changes.
- Continue coherent owned2D mip chains, explicit native sampler/state mapping,
  all-common-level TexBlt and worker teardown with caller metadata snapshots.
  Then GPU texture pixel oracle. Full DX8-DX11 goal active, no Finish/caps/pins.
- Resume read used a nonexistent src/dxvk_native_runtime path and briefly read
  tests from workspace instead of pinned repo. Discover src/umd and use exact
  project cwd; neither error affected edits, binaries or acceptance evidence.

### Texture probe Windows compile contract

Offline37497502780 identifies `small` as a Windows macro collision on x64,
ARM64 and x86 after production device/controlled fixture compilation succeeds.
Rename the new probe local to `smallDestination`; preserve the `small` receipt
string and all 1088 pixel expectations. Complete replacement architecture,
native controlled/semantic and actual target GPU gates before acceptance.

### Verified texture checkpoint and target EWDK preparation

- Source7874196 full37498655517 all6/offline37498596397 all4/API37498554766/
  package37498554868SUCCESS. All retained3architecture artifacts and14actual
  ARM64 execution hashes independently verify134906device checks perarch,
  42429adapter/49backend/45reject checks and closed production admission.
- Native ARM64 positive worktree02PASS134906, compile5.8650081s/run0.0783895s;
  four compiled controls reject upload-reread6909/1303, first-mip-only6907/1302,
  no-identity21136/1376 and no-unbind4216/209. All56 source/archive/after-run/
  mutation/Git/nativePE/driver/desktop links independently verify.
- Target texture25/26 independently PASS1088pixels diagnostics1/0:clear192
  ffefa655/draw19253a03d45/shader1921384c5a5/texture512d3afb9c5;17nonempty
  KMTsubmits,ctx1/1alloc13/13lock12/12residency13/13remaining0wrong0. Durations
  4.2857411s/6.5034715s. All8payloads3scripts/nativeARM64/LimitedUSERsession1
  verify; owned tasks removedexit0. Archives0d6c57ad8bc2cb2f3adae0f3a9e883da5bc8b57bdda5c6d9b27f792e36f7db69
  and0074886a0025fd89434b5668f0e14e547bc02c215b45f49418680ed3b9e9b3e3.
  Fresh exactsigned58624/oem17/binding0002/PnP0/SYSsha/DWM1552/Explorer6464
  and58selected failure/reset/epoch/timeout/admission fields unchanged.
- Explorer6464 predates these runs; prior2488 also faulted in installedMesa
  D3D10UMD c0000005/RVA10db80/timestamp6ac2defb. Preserve separate stability
  diagnosis; do not claim historical desktop continuity or artifact-free UI.
- WindowsSSH22 recovers with curve25519-sha256/publickey; existing root socat
  forwards to guestLLA22. Packet capture/sshd accepted-key records confirmpath.
  No service restart/VM/image/memory/clock/install/registry changes performed.
  AndroidSSH8022 andADB5555 remainreachable. Initial Android file discovery
  needed rootADB because the Termux app UID lacks storage access; root search
  finds no full EWDK in scopedDownload/1DMP/data-local-tmp locations.
- EWDK guestISO17912889344bytes stable, descriptor19785189376bytes; transfer
  stopped and no scp/sftp/clientssh processes. Complete-source path requested
  asynchronously. Read-only completed-image mount helper and automatically
  inventoried EWDK production KMD runner are prepared, not yet executed.
  Native controlled checks use the separate portableMSVC14.44/26100toolchain.
- Continue typedvertex/index buffers and indexed/resource-stream drawing;
  ordinary DX8-DX11 admission/presentation/reset remainopen. Fullgoalactive.

### Native buffer development and actual EWDK compiler recovery

- User confirms EWDK mounts; verify actual required reads/build, not descriptor
  size alone. D: read-only/MSVC14.50.35717/WDK28000 are present. Actual native
  and x64 compiler builds failC1510 with unreadable clui.dll. Build subagent
  traces UDF extents18283870208/18287994880 beyond physical17912889344bytes,
  matches official ISO raw ranges, recovers19missing files beside67readable
  originals in an owned compiler copy. Fresh C1b9 KMD build is running; original
  ISO/mount/VM/installed driver are preserved. Independent verification pending.
- Root implements typed worker-owned vertex/index buffers, dynamic usage,
  byte-range locks, multiple streams and indexed drawing in053e39f. Linear
  reserved metadata and index FVF are ignored; SYSTEMMEM buffers stay outside
  this slice. Resource creation snapshots metadata before callback and rechecks
  identity before publication; retryable failure keeps output/ownership intact.
- Controlled preflight01 has164072PASS on native ARM64, separate uncommitted
  four-file provenance. Committed worktree01 probe preflight catches /W4 /WX
  local shadow warnings; frozen failed receipt retained. 6ca8770 renames only
  probe locals. Final56Git inputs and four one-contract mutations are frozen
  asworktree02; native execution and full/offline CI are running.
- GPU oracle adds6cases stages18-23: separate streams3/7, byte binding and
  declaration offsets, missing final stride padding, both index formats,
  negative/positive BaseVertexIndex, nonzero StartIndex and dynamic
  NoOverwrite-range/whole-Discard updates. Each draw follows a fresh clear
  so a no-op cannot inherit the preceding result. Expected384buffer pixels
  checksum621cd685, total1472including existing texture/shader/draw/clear.
  Actual GPU execution remains pending; ordinary DX8-DX11 admission staysopen.

### Verified EWDK build, buffer controls and consolidated CI

- Requested build subagent independently verifies EWDK C1b9 production KMD:
  all9steps9.1133401s,118Git/tree/archive/after-build sources,86compiler files
  (19exact recovered/67mounted originals),55matching official metadata ranges.
  ARM64 Native410624byteSYSsha b773c84e8df806293a87a6e27c019c4e0995f51cbc660c871b826ce60947ae9e;
  onlyntoskrnl/HAL imports. Root independently rechecks local SYSsha/PE/CFG/NX/
  ASLR. Signed58624/oem17/DWM/Explorer retained; unsigned and uninstalled.
  Reusableownedcompiler C:\Users\Public\EwdkNativeCompiler-01\bin; EwdkRootD:,
  CompilerHostarm64, originalkit28000headers/libs. Exact handoff/evidence under
  artifacts/dxvk-ewdk-target-build-20261007. No full-ISO hash claim.
- Exact6ca8770 native positive164072PASS compile6.8425305s/run0.1118603s;
  executable622fab39331e6ca31ecab921ad68fcffa48a15ea0aab3edc6faefe85dbe66ce2.
  Four compiled one-contract controls independently caught at expectedchecks
  16153/20142/21059/16978 forrange/unbind/identity/index-reread. All56Git/archive/
  after-run inputs, nativePE and installed-driver/desktop links independently
  verify. Offline37508904695 all4SUCCESS before consolidation.
- User requests disabling redundant CI. cd4f5e2 changes only6workflow files:
  one automatic nativeUMD workflow includes unique Linux encoder sanitizer/
  negative controls inidentity. LegacyAPI/package/WDDM/source-transfer workflows
  disabled inGitHub; targeted offline/runtime controlsmanual. Redundant active
  runs canceled; latestpush creates exactlyonerun37509470022. Root verifies
  YAML/triggers/threearchitectures/nativeARM64 job plus56source matches6ca.
  Subagent continues independent consolidated-build verification; root owns
  actualGPUbuffer execution. Production source/caps/exports remain unchanged.
- GPU runners3local/nativeparse/hash links verify; actualGPU buffer execution
  remains pending compiledARM64candidate. Fullgoal staysactive.

### Next depth/stencil contract audit while buffer CI completes

- LocalMicrosoft PFND3DDDI_SETDEPTHSTENCIL requires retaining an owned depth
  resource in the device context; native clear handles TARGET/ZBUFFER/STENCIL
  and COMPUTERECTS independently. Clear remarks distinguish zero preclipped
  rectangles (no-op) from zero computed rectangles (whole viewport), and
  require clipping only for computed rectangles.
- Current clear backend uses publicClear only for computed rectangles and
  ColorFill for preclipped color rectangles. Depth/stencil support must add a
  private renderer path for preclipped depth/stencil, preserving those native
  semantics instead of sending already-clipped rectangles through publicClear
  and applying viewport/scissor again.
- Next slice should type D16/D24S8/default-pool depth surfaces, their binding/
  clear/lifetime and render-state mapping; actual occlusion/stencil pixel
  oracles and semantic controls are required before any caps/admission change.
  No depth/stencil source change or acceptance is claimed here.

### Verified buffer target acceptance and next depth/stencil slice

- Independent build verifier passes consolidatedcd4f5e2 run37509470022 all6jobs,
  all5GitHub artifactZIP digests, three native PE architectures,164072device
  checks perarchitecture and14actual native ARM64 execution hashes.
- Actual buffer27/28 diagnostics1/0 each independently verify1472pixels:
  clear192ffefa655,draw19253a03d45,shader1921384c5a5,texture512d3afb9c5,
  buffer384621cd685. Durations8.316949s/10.8154619s;24nonempty submits,
  ctx1/1alloc15/15lock14/14residency15/15remaining0wrong-thread0.
  Eight payload hashes/nativeARM64 and three frozen scripts match; Limited
  USER/session1 and tasks removedexit0. Fresh before/after readiness verifies
  signed58624/oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464 and58selected
  fields unchanged through both bounded runs. Longer-term Explorer stability
  and ordinary runtime activation remain open. Evidence underbuffer27/28;
  archiveSHA41d0e786d1e83bd89ce59cc58e19b4d0c26c5ed3c192acec962473893d37d0ad /
  d7fa6e392568707c1bc59f95af36dbbee0a22229799bb837568381e4b30bd0ce.
- Next implement D16/D24S8 native resource ownership, SetDepthStencil, independent
  TARGET/ZBUFFER/STENCIL clearing and actual depth/stencil pixel acceptance.
  Preclipped clear must avoid a second viewport/scissor clip; computed clear
  must use the actual intersection, including empty/outside scissor rectangles.
  No production caps/exports or paired pins are widened at this checkpoint.

### Native depth/stencil implementation and preflight

- Add worker-owned D16/D24S8 default-pool depth surfaces and native
  SetDepthStencil binding. Reject color/texture/buffer aliases, unsupported
  pool/multisample/lock/copy contracts and undersized depth attachments at draw.
  Destroy/close unbind before retirement; retryable failure retains ownership.
- Native Clear snapshots caller rectangles after device serialization and
  independently dispatches TARGET/ZBUFFER/STENCIL. Validate requested aspects,
  finite depth0..1 and stencil0..255; ignored fields stay ignored. Private
  ClearNative preserves preclipped rectangles and zero-count no-op, while
  computed clears clip against actual viewport/scissor intersections. Fix
  empty/disjoint clipping and image-extent bounds in shared renderer logic.
- Frozen uncommitted preflight03 compiles native ARM64 probe and actual DDI
  fixture under/W4/WX,182260checksPASS,compile6.903271s,run0.1019123s.
  All56current source hashes match frozen inputs before commit. Preflight02
  had178063checks before adding independent depth-size draw coverage. Earlier01
  compiled178063checks but used an incorrectSourceCommit field; preserve it as
  preflight only and exclude it from committed-source acceptance. Actual full
  renderer CI, committed fixture/semantic controls and GPU depth remain pending.
- Fourteen new GPU stages24..37 are implemented:896depth pixels, independently
  calculated checksumeac96ea5,total2368including prior1472. CoverD16less/gequal/
  ZWRITE, D24S8 independent aspects/write masks, preclipped and computed clear,
  empty rectangles, disjoint scissor and combined color/depth/stencil. Every
  case initializes distinguishable color/depth/stencil before rendering; no
  production caps/exports/registration or package pins change.

### Independently verified committed depth contracts and runner preparation

- Exact source2d28554271e2326c0baa7bfc578adb04d9e98e2c native ARM64 positive
 182260checks compiles6.8519516s/runs0.1222363s. EXEsha
 b63169c0c10cf00daa88a86e977e957e7e0462c4c5d2c5c4bb941c63ab702cb7.
 All56Git/source-archive/after-run inputs, nativeAA64/static runtime, signed
 58624/oem17 and desktop metadata independently verify asworktree01.
- Four single-mutation controls compile and fail their intended assertions:
 no-unbind5329line326; no-type-check4159line515; wrong-clear-flags2823line524;
 clear-reread3167line859. Each fixture exit1 and probe/compiler exit0 verifies.
 Evidenceartifacts/dxvk-native-d3d9-depth-20261007/native-fixtures-verification-01.txt.
- Native PowerShell parser reports zero errors for three frozen depth GPU
 runners; guest SHA256 hashes match depth-runner-scripts-01. GPU execution
 remains pending the actual ARM64 renderer/backend artifact. Single automatic
 consolidatedCI37515057679 runs; Linux/CPU checks pass, backend builds pending.

### Next mandatory D3D9 gaps audited from local Microsoft docs

- SetTransform/MultiplyTransform contain inline TransformType/Matrix, while
 SetMaterial contains inline Diffuse/Ambient/Specular/Emissive/Power. Native
 SetLight uses DATA/ENABLE/DISABLE bit flags and a separate optional pointed
 D3DDDI_LIGHT payload, which must be snapshotted after device serialization.
 These remain absent from the typed table; general DX8/DX9 fixed-function
 acceptance cannot be inferred from transformed diffuse quad rendering.
- Native BufferBlt specifies destination byteOffset and source byteRange;
 SYSTEMMEM/managed buffer creation and notify/upload ownership remain absent.
 Audit this next resource gate before advertising buffer/copy capabilities.
- Production source remains pinned2d285542 during CI; no later source slice,
 ordinary runtime activation, registration or package pin update is claimed.


### Accepted depth/stencil checkpoint and next fixed-function work

- Exact2d285542 consolidatedCI37515057679 all6SUCCESS independently verifies
  all5rawZIPdigests, three PE architectures/182260device checks and14native
  ARM64execution hashes. Root refreshes live CI after the GPU acceptance.
- Exact targetdepth29/30 diagnostics1/0 independently pass2368pixels each,
  including896depth/stencil pixels eac96ea5. Durations14.5854506s/14.7961268s;
 38nonempty rendercallbacks,ctx1/1alloc14/14lock13/13residency14/14remaining0
 wrong-thread0. All8payloads3frozen scripts/nativeARM64/LimitedUSERsession1
 verify; both ownedtasks removedexit0. Fresh readiness retains signed58624/
 oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464/starttimes and58selected
 fields with zero delta. GPU archiveSHA c2e1f6f00d07d28909885a57cb27c28cd680b423306153d69c9184b9a01d3177 /
 ef88465062eaa84cd63502641880b4b4c17a16076e1210f0acb39c6283058220.
 Evidence artifacts/dxvk-native-d3d9-depth-20261007/depth-29 anddepth-30.
- Preparation initially caught a copied expected-hash mismatch before starting
 depth29; compare against the actual retained receipt and frozen hashes, then
 start only after every preparation check passes. No failed GPU run occurred.
- Correct the earlier SetLight documentation audit: local docs call DataType
 bitwise flags, but actual target26100d3dumddi.h defines ENABLE=0,DISABLE=1,
 DATA=2. Follow the exact typed enum and validate unsupported values; audit
 CreateLight/DestroyLight lifetime along with separate optional light snapshots.
- Next missing typed callbacks: SetTransform/MultiplyTransform, SetMaterial,
 CreateLight/SetLight/DestroyLight. Add untransformed geometry and actual GPU
 lighting acceptance. SYSTEMMEM/managedBufferBlt, other resources, queries,
 presentation/reset and ordinary system-runtime DX8-DX11 remain mandatory.
 No production caps/exports/registration/installation or paired pins change.


### Typed fixed-function transform/material/light implementation

- Add typed SetTransform/MultiplyTransform for VIEW/PROJECTION, texture0..7
 and world0..255, plus copied SetMaterial. The renderer reuses DXVK's existing
 fixed-function matrices and lighting instead of translating them a second time.
- CreateLight maps arbitrary sparse runtime indices to reusable compact private
 slots. Native ENABLE=0/DISABLE=1/DATA=2 follows the actual target SDK enum.
 Point/spot/directional DATA snapshots copy after device serialization and
 before callback pumping; DATA preserves enabled state. Unsupported enums,
 unowned/stale indices and more than8active lights reject. Exact-success-only
 enable/retire state, retry/exception creation rollback, recreation and enabled
 close cleanup are covered by native controlled fixtures.
- User-requested build verifier independently passes frozen preflight02:
 native ARM64 /W4/WX probe and202248device checks, compile7.1772515s/run0.1041418s.
 All56source/archive/after-run hashes match the exact5worktree inputs; KERNEL32
 only/static runtime, ARM64 PE/COFF, signed58624/oem17/SYSsha and desktop retain.
 This is uncommitted-source preflight; full renderer CI and committed controls
 remain pending. Earlierpreflight01 retained separately without final GPU probe.
- Implement21actual GPU stages38..58:1344new fixed-function pixels checksum
 b3416e11, total3712including prior2368. Independent CPU matrix/lighting math
 derives expectations before GPU execution. Cover world/view/projection and
 noncommutative multiplication, emissive material, directional normal response,
 enable/disable/DATA preservation, sparse destroy/reuse, point range and spot
 cones. Leave one enabled owned light for actual DestroyDevice worker cleanup.
 No caps/exports/registration/installation or paired pins change.

### Fixed-function committed-source and actual GPU acceptance

- Exact source e86e0c50a55d05e6ef78e06fcc106e1e7e4adf49 committed positive
 worktree01 passes202248checks; independent verifier matches56Git/archive/
 after-run/nativePE/static imports and signed58624/desktop continuity. Five
 compiled controls catch lost multiplication, light DATA reread, sparse slot
 misuse, enabled retirement and S_FALSE private publication. Last two control
 harnesses preserve all assertions and use exact CHECK failure _Exit instead of
 static teardown; production fixture unchanged. Earlier retirement01 compiler
 failure and publish01 assertion/teardown timeout are retained and excluded.
- User-requested build verifier passes complete single CI37520111683 all6jobs,
 five raw ZIP digests, ARM64/x64/x86202248checks,14actual native ARM64 execution
 hashes,56preflight02source links and Linux sanitizer/semantic controls.
 ARM64artifact11440226149 DLL3b3863a5/probecfbb7c79 retained; root live-refresh
 all6SUCCESS after target execution confirms the exact source link.
- Freshtargetfixed31/32 diagnostics1/0 independently verify3712pixels including
 1344fixed-function b3416e11, durations13.1856983s/20.6108307s. Each59nonempty
 submits,ctx1/1alloc14/14lock13/13residency14/14remaining0wrong-thread0;
 all8payloads3scripts/nativeARM64/LimitedUSER1 verify. Owned tasks removedexit0.
 Freshsigned58624/oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464 and58selected
 readiness fields retain. Full proof artifacts/dxvk-native-d3d9-fixed-function-20261007;
 checkpoint docs/native-d3d9-fixed-function-20261007.md. No production cap/export/
 registration/installation or paired pins widened; ordinary DX8-DX11 remains open.
- Next local Microsoft BufBlt audit: hDst/hSrc are vertex or index resources,
 Offset is destination bytes, SrcRange carries source offset/size. SYSTEMMEM
 buffers are currently rejected; native DXVK common buffers already implement
 SYSTEMMEM staging, dynamic draw uploads and mapping sequence synchronization.
 Reuse that renderer with typed byte ranges and borrowed-data snapshots, plus
 actual GPU copy/draw/readback acceptance. Existing copyBufferRegion handles
 same-buffer overlap through a temporary allocation.

## 2026-10-07 buffer transfer GPU-layout correction
- Before GPU execution, audited D3D9 UploadPerDrawData: dynamic SYSTEMMEM
  buffers are repacked with stride=min(source stride, declaration extent),
  dropping stream binding offset. An internal declaration offset3 would
  produce unaligned FLOAT4 addresses and stride23 after repacking. Changed
  the probe only to declaration0/16 and stream offsets+3, preserving every
  copied byte range and independent pixel/byte oracle. Both real-buffer
  and packed SYSTEMMEM fetches now use four-byte-aligned addresses.
- Current Vulkan vertex-input primary reference requires component-size
  alignment when legacyVertexAttributes is not enabled; this DXVK tree
  has no enabled legacyVertexAttributes feature. Source reference:
  https://docs.vulkan.org/spec/latest/chapters/fxvertex.html#fxvertex-input-extraction
- Old c8e7ff5 CI37526611880 retained as superseded; replacement exact-source
  preflight, native controls and full CI required before any GPU claim.
- Next local DDI audit: SetClipPlane inline coefficients obey homogeneous
  Ax+By+Cz+Dw >=0, six DXVK planes. Native DDI must reject Index>=6 before
  backend (COM SetClipPlane silently caps it). CLIPPLANEENABLE mapping
  already exists; typed plane dispatch and snapshot/GPU tests remain absent.
## 2026-10-07 accepted SYSTEMMEM and next clip-plane contract

Exact0d585e2 native216868checks/five compiled semantic controls and single
consolidated37530530983 all6jobs independently verify57inputs, three PE
architectures,14native ARM64 execution hashes and Linux gates. Target33/34
diagnostics1/0 each independently verify4224pixels/1056bytes,78nonempty submits,
balanced lifetime/residency and unchanged signedSYS/desktop/58readiness fields.
The alignment correction changes only probe declaration/bindings; byte payloads,
copy offsets and pixel/byte expectations retain. No ordinary DX8-DX11 claim.

SetClipPlane's native structure owns four inline float coefficients and uses
Ax+By+Cz+Dw>=0. NativeDDI must reject indices>=6 before the public core caps them
to its last valid plane. Local typed implementation now owns the entry snapshot
and reuses the existing renderer. Controlled native preflight01 passes220595;
final probe preflight02 is frozen. Pixeloracle uses exact rational half-space
equations at D3D9 integer sample centers with identity transforms and z=.5,w=1.
The x/y planes put boundaries between samples to avoid edge-rule ambiguity.
Ten stages67..76 expect640pixels/ac340bd5 and4864total; controls/CI/GPU pending.

## 2026-10-07 clip hardware failure and next query audit

Exact48efb7d CI/native contracts all pass, but targetclip35 fails disabled-plane
stage67 corner pixel (actual clear color). The complete failed archive independently
verifies eight prior slices, exact8payloads3scripts/LimitedUSER1, signedSYS/desktop
and58readiness fields unchanged. No second acceptance run was launched. Diagnostic
local25a9edd changes only retained clip pixel logging; original geometry/CPUoracle
and failure artifacts remain unchanged. Raw pixel evidence and a renderer/probe
fix must precede any clip acceptance claim.

Local Microsoft d3dumddi CreateQuery/IssueQuery/GetQueryData docs identify a later
mandatory contract: GetQueryData must preserve S_FALSE for pending work. Generic
native operation currently normalizes positive non-S_OK to E_FAIL, so query pending
must be handled specifically with owned result storage and publication only after
successful completion. Native Begin=1/End=2 differ from public D3DISSUE flags and
need explicit mapping. Native hQuery is driver-created, device-owned and retired;
caller GetQueryData pData has no byte-count field, requiring an exact per-type size.
Existing D3D9 core supports VCACHE/EVENT/OCCLUSION/TIMESTAMP/DISJOINT/FREQ. Its cached
EVENT path writes one bool byte, so a native BOOL result requires owned zeroed data
and TRUE normalization only after actual S_OK completion. No query code/caps changed.

Diagnostic02 d79b3bb proves NORMALIZENORMALS=0 does not change the failure: all64
raw pixels remain ff091725 at stage67/mask0. All prior slices, signedSYS/desktop
and58readiness fields remain unchanged; no device loss. Root code audit confirms
CLIPPLANEENABLE 1->0 marks ClipPlanes dirty, so diagnostic03 d230cb0 forces a fresh
zero-plane constant upload while retaining the identical geometry and640pixel oracle.
Its fresh strict native build and all57source/archive/build/after-run links pass;
this is a controlled experiment, not an accepted production fix.
Diagnostic03 records that exact enable1/disable0 refresh and reproduces all64clear
pixels. Thus neither that refresh nor the normalization reset changes the observed
failure. Diagnostic04 changes only vertex layout to the passing fixed-function
POSITION/NORMAL/COLOR layout, with normal0,0,1 and lighting0. This also changes
stride/color offset, so a changed outcome requires further isolation; it would not
prove a missing-normal shader defect by itself. Original pixel oracle remains strict.
Diagnostic04 passes all640 exact pixels with visible counts64/32/32/32/16/64/0/0/64/64,
ac340bd5, all4864pixels and1056transferbytes. Plane coefficient/index/enable behavior
works with the POSITION/NORMAL/COLOR layout; failure remains layout dependent.
Diagnostic05 removes only the NORMAL element while retaining the identical28byte
stride, coloroffset24 and hostnormal0,0,1bytes. Its result will separate missing-normal
declaration handling from the padding/color-offset changes in diagnostic04.

Diagnostic05 with NORMAL omitted still passes all640pixels and balanced GPU ownership.
Remaining layout factors are stride16vs28 and coloroffset12vs24; diagnostic06 holds
stride28 fixed and restorescoloroffset12. Original compact failure also occurred with
both the CI-built and nativeMSVC-built probes, limiting a single-compiler explanation.

Diagnostic06 reproduces all64clearpixels when color is restoredtooffset12 while
stride stays28. Failure tracks color placement relative to FLOAT3 POSITION; native
declaration conversion copies each Stream/Offset/Type/Method/Usage/Index explicitly
into D3DVERTEXELEMENT9 before core creation. Diagnostic07 tests a single DWORD gap
with coloroffset16, FLOAT3 POSITION and identical homogeneous geometry/oracles.

Diagnostic07 changes the failure to all64pixels3f800000, precisely the float1
bit pattern in unused padding atoffset24. Together with diagnostics04/05
passingcolor24 and06 failingcolor12 atconstantstride28, this points to stale
input-layout reuse rather than clip-coefficient math. Native MSVC14.44 xhash
_Find_last checks same-bucket equality without checking full hashes. D3D9
BindInputLayout emits a zero-divisor INSTANCE binding for missing shader inputs.
DxvkGraphicsPipelineVertexInputState::eq final loop lacks &&eq, allowing that
matching divisor to erase header/binding/attribute mismatches; a later divisor
can similarly erase an earlier divisor mismatch. This is a confirmed source
defect. Runtime causality still needs a new UMD and the unchanged compact probe.
The portable tested helper retains all old comparison fields/arrays and changes
only the short circuit; production eq delegates directly to it. No Mesa patch,
layout workaround, caps/export change or ordinary-runtime admission is included.

Exact ae61dde candidate and original compact25a9edd probe pass clip37/38 with
diagnostics1/0, all640raw rational-oracle pixels/4864total+1056bytes. This
resolves the compact-layout defect after correcting equality's divisor loop;
no padded-layout workaround is accepted. Both retain all prior slices, balanced
runtime/worker cleanup, installedSYS/desktop and58readiness fields. Full
source/native/CI/archive/helper/pixel joins are recorded in
artifacts/dxvk-native-d3d9-clip-planes-20261007/root-clip-checkpoint-verified-01.json.

Query native ABI has Begin1/End2, opposite COM flags. Completed EVENT requires
four-byte BOOL TRUE even when the core's cached EVENT writes only a bool byte.
Pending/failure output stays untouched through owned storage and post-result
device epoch checks. Unissued/begun queries must not invoke the core's implicit
first-GetData issue. VCACHE is NVIDIA-specific in this renderer and unavailable
on matched Adreno830; do not fabricate those hints or advertise target support.
Native query preflight01 passes272881; revised probe/source preflight02 remains
under build verification. GPU query acceptance and ordinary runtime remain open.

D3D9 presentation contract audit uses local Microsoft Present/PresentCb docs
(reference/codes/windows-driver-docs-ddi) and the exact accepted native SDK header.
D3DDDIARG_PRESENT carries source/destination resource/subresource handles and
Blt1/ColorFill2/Flip4 flags. Typed D3DDDICB_PRESENT must contain the real source
allocation and a previously created runtime context; its ABI differs from
DXGIDDICB_PRESENT and has no opaque pDXGIContext. Existing RuntimeMemory
presentation copies D3D10 readback into an owned kernel allocation then invokes
DXGI; available() currently requires live DXGI callbacks and ensureContext()
creates a separate context. It cannot be blindly reused or cast for typed9.
Current typed9 surfaces own private core resources only; pfnPresent remains
absent. Next implementation needs owned allocation metadata, typed PresentCb,
serialized callback/reset/retirement handling and pixel/display acceptance.
CPU staging correctness would not establish the separate zero-copy goal.
No presentation source changes or production admission are included in query0a.

Exactquery0a60564 hardware completion is accepted on Adreno830 with matched
Mesa8443c71 and unchanged58624KMD. Bothdiagnostics query39/40 produce64/16/0
actualocclusion and192 matchingrawpixels. Timestampfrequency is19200000; pair
ordered,DISJOINTfalse,fullEVENTTRUE twice. Everyendedquery wasalreadycomplete
on firstpoll after explicitflush/readback; actualendedpending was not observed.
That semanticpath remains separately proven by nativepositive/compiledcontrols.
Nine unissued/begun pendingresults retainguards; sixquerylifetimes closecleanly.
Full5056pixel/1056byte acceptance retainsall earliercompactclipgates. This is
typedoffscreen hardware acceptance; ordinarysystemruntime/Present remains open.

Presentation061ee8f uses separately owned GDI-context identity for linear80-byte
allocation metadata (A8 wireformat1, X8 wireformat2), preserving the private
renderer native context. The completed readback copies tightly packed rows into
the owned source, then calls typed PresentCb with the kernel allocation handle.
The target harness maps that callback to real D3DKMTPresent/windowed blit and
reads the actual64x64screen client area; fallback WM_PAINT fills only black.
Four distinct quadrant frames and two consecutive full matching captures per
frame prevent stale image or callback-success-only acceptance. CPU publication
is the first correctness path; it does not fulfill the separate zero-copy goal.
Native466831checks and six compiled semantic controls verify publication,
snapshots/kernel ownership/post-callback epochs/retryable failed deallocation.
Full ordinary runtime remains closed: current GetCaps still reports empty
format/query lists and zero D3DCAPS9, and production entry admission is unchanged.
The presentation helper review corrected an inherited55s scheduled-task cap to
present-only75s before any GPU run, preserving65s probe and77s polling bounds.

## 2026-10-07 shared source ownership diagnostic
Native probe8d9e196 is warning-free; root-present-diagnostic-probe-ready-01.json
independently joins57Git/archive/build/after-run sources, originalAA64COFF and
PE, exact source-derived import library and SDK libraries/headers. No native
fixture rerun claimed; unchanged466831 fixture evidence remains source linked.
Present42 actual HDC lookup returns nativeLUID6e6c/source0, closing its extra
adapter successfully. Shared acquisition returnsc01e0342 before allocation or
present. No release was required because acquisition failed. All prior GPU
slices pass, residency19/19remaining0, signedSYS/DWM/Explorer and58readiness
fields retained. Failure archiveb22c15177e2e226199e8ed7d203bb4c883d86f01c2e889b44f1e466a61013ae7
retains12originalmembers. Root failure review recomputes832query/clip pixels,
checks8inputs/3scripts/token and task removal; presentation_accepted=false.
Emulated owner probe-only diagnostic is prepared next: SDK defines type4 as
no real primary ownership. It may expose whether legacy raw KMT presentation
can coexist with DWM; this is a hypothesis, not successful redirection evidence.
UMD061ee8f, all frame colors and strict screen oracles remain byte-identical.
Full061ee8f CI37549764498 is independently complete, all six jobs, five ZIPs,
three actual architecture links and15actual native executions. No duplicate
automatic run was triggered by the shared probe-only [skip ci] commit.

## 2026-10-07 emulated ownership diagnostic
Frozen57input sourced22863e nativeARM64 probe compiles warning-free, six invalid
CLI and read-only adapter checks pass. Root independently rejoins original
COFF/PE/libraries/SDK headers and raw official User32 NuGet ZIP member; native
fixture466831 remains byte-linked and was not rerun. Root reviewer02/03/04 stale
helper/archive/reused-CI paths remain as failed evidence; fresh05 resolves actual
receipt paths and passes without modifying original agent evidence.
Targetpresent43 acquires EMULATED type4 source0 on actualLUID6e6c successfully,
but first raw legacy primary blit still returnsc01e0342. Emulated owner releases
status0; sourceallocation/GDIcontext deallocate/destroy0, residency20/20remaining0.
No screen capture or presentation PASS. SignedSYS/DWM/Explorer and58readiness
fields retain. Raw12memberarchiveacc123311b702f54bbfcc78ddedabc239ab9e73ce890fc326c7590b0fe1adeef.
Root failure proof checks8payloads/3scripts/token/task removal and832rational
earlier query/clip pixels. Source ownership alone does not fix this raw legacy
primary path. Next isolated probe-only diagnostic sets documented
PresentToBitmap (0x4000), with exact source/window/rectangles and unchanged
16384screen oracle; no exclusive owner, mode change or production admission.
Actual ordinary runtime still requires caps/export negotiation, primary/opened
resource ownership and runtime-managed window redirection; this raw harness
provides its own callbacks and must not be mistaken for that admission.

## 2026-10-07 bitmap presentation rejection and system runtime diagnostic
Bitmap probeaf1ae8a compiles warning-free on nativeARM64 in2.4950676seconds,
six invalid CLI/read-only enumeration checks pass. Root review06 joins all57
Git/archive/build/after-run sources, originalCOFF/PE/LIB, originalSDK headers
and raw official User32 NuGet member, reused originalCI inputs and continuity.
Native32memberevidencec56d0090acf299b74c4a106944331a61d93775f46886478376b9341ad5547206.
Targetpresent44 acquires/releases EMULATED source0/nativeLUID6e6c status0,
then exact PresentToBitmap flag40c1 returnsc000000d/invalid parameters on first
submission. No screen pixels or presentation acceptance.20residency refs and
evictions balance; sourceallocation/GDIcontext close0, signedSYS/desktop and
58readiness fields unchanged. Root failure proof verifies8payloads/3scripts,
LimitedUSER/token/task cleanup and832raw rational prior query/clip pixels.
Original12memberGPUevidence0889cdc973e291624450b9cafa3b121f919f7b4b6f55898d5d1161cf24b8e5b1.
Do not accumulate speculative raw KMT present flags or claim DWM redirection
from a private callback harness. Next implementation is a separate read-only
OpenAdapter frontend forwarding only adapter/caps queries to exactUMD061ee8f.
It always blocks CreateDevice; original caps/counts remain zero. A separate
probe uses genuine System32 d3d9.dll for Direct3DCreate9/Ex and enumeration,
plus two null-input frontend guard checks. Native build/guard/baseline come
before any temporary legacy-driver binding. No production export/caps, D3D10
registration or renderer changes; no ordinary rendering acceptance claimed.

## 2026-10-07 actual Microsoft runtime adapter gate
Readonly native source a84fef4 builds both standalone ARM64 binaries with zero
warnings. Root independently verifies60Git/archive/before/after inputs,42original
archive members,2original COFFs/2PEs, sole frontend OpenAdapter, original SDK
NuGet d3d9 import library/eight headers, guard checks and signedSYS/desktop.
Microsoft System32 d3d9.dll SHA0658854da684062f722bd96135f40109f7beb7d724866d55cd4f8194e3d7d248.
SSH session baseline sees0adapters; LimitedUSER session1 seesone1af4/1050 adapter,
HAL caps8876086a, ExS_OK but zeroLUID and HAL8876086a. No device creation/render.
Actual desktop KMT source0/LUID6e6c returns all three registered DX9/10/11 paths.
Names-only original18memberarchive58f1eef56db7d6cb6799b9f38da86c4cd3dce606571d9b588f828205c7ba0bfe.
Temporary first-slot-only binding retains DX10/11/Wow; actual KMT still returns
the oldDX9 path during binding. System runtime logs no frontend OpenAdapter or
GetCaps calls and repeats baseline failure. This target keeps cached driver
names; registry write alone did not select the prototype. Exact registration
restored;33memberarchive9b980659bea022f80930042a2e98e2115c39b03b2ca9f0d74daaae8caf063c60.
Root verifies34registration fields/58failure-reset-epoch-timeout-admission fields
unchanged, signedSYS/DWM1552/Explorer6464 retained. Initial root reviewer01
incorrectly treated all1215registry values as static; its failure is preserved.
Reviewer02 records ambient Native display counters separately and retains all
established58stability gates and exact registration, source, task/token checks.
Next diagnostic selects only this package's DX9 filename through the genuine
Microsoft d3d9.dll process-local import slot for D3DKMTQueryAdapterInfo. Actual
system PE imports confirm this documented GDI32 entry. Original query always
runs first; only successful DX9 exact-package pathname responses can change.
All other adapter/version/private/caps queries remain original. Import pointer
and page protection restore; system DLL files/registry remain original. The
frontend still always blocks CreateDevice. This is callback tracing, not
ordinary runtime rendering/admission or presentation acceptance.

## 2026-10-07 real Microsoft D3D9 adapter contract and caps continuation
Exact5214397 native build independently passes original42members/60source
links/twoCOFF/twoPE/SDK and null guards. Session1 selector01 original32member
archive736d3fc7 independently verifies seven process-local exact DX9 redirects,
two real System32 d3d9 OpenAdapter calls Interface9/Version69632, successful
private identity negotiation and 18GetCaps calls. Type13/304-byte caps, type3
format count4-byte and type6query count4-byte return success/zeros. Type34
DDIGAMMACAPS4-byte returns8876086a. Type4format data receives0-byte empty list.
Both adapter handles close0; Ex now returns actual6e6cLUID matching physical
KMT. HAL still returns8876086a. No CreateDevice call; selector restored and all
three LimitedUSER tasks removed. SYS/desktop/candidate,34registration and58
critical fields unchanged, physical KMT paths unchanged in all phases.
Root proof root-system-runtime-selector-01-verified-03.json; source/archives
retain and no ordinary rendering/presentation acceptance is inferred.

Typed9 caps now implement a bounded profile for the native DDI's implemented
four formats/six queries, SM2 and one RT. X8 is the sole display format; alpha
formats never report display mode. No cube/volume/MSAA/autogen/dynamic textures,
instancing, gamma, shared resources, or stretched/color-fill plain-surface
operations are advertised. Caps/list outputs require exact size and valid data;
all metadata is copied before the identity callback. New adapter fixture checks
canaries, list bounds, absent unsupported features, callback-free malformed
requests and a callback mutating the original output pointer/type/size. Only
original snapshotted destination changes. Actual native fixtures/CI and system
runtime caps are next gates; normal OpenAdapter export is still absent.

## 2026-10-07 exact caps CPU checkpoint
The frozen committed478eca2 88-input packet passes native adapter42308 and
unchanged full device466831 checks. Root original33member archive review02
independently validates exact source/build/after-run links,9AA64COFFs/2PEs,
strict/W4/WX/MT flags, no warnings, original SDK/static CRT libraries and
installedSYS/systemD3D9/desktop/oldcandidate continuity. This validates caps
contracts in the controlled fixtures; it does not establish runtime admission.
Only one automatic CI37561240247 starts for the production push. The next
diagnostic uses a separate fresh process-local candidate directory for478eca2
and preserves the blocked CreateDevice path and original installed registration.

Local Microsoft CreateDevice DDI documentation states that the runtime does
not create a default GPU context; the driver explicitly creates contexts via
CreateContextCb. The standalone frontend remains an unconditional rejection
boundary and invokes no core device creation/callbacks. The prepared separate
contract mode will use actual system HAL CreateDevice/Ex requests to capture
Interface/Version/Flags and callback availability before implementing their
missing native contracts. Two API requests are distinct from zero core device
calls; no rendering or presentation claim follows from that diagnostic.

Actual caps-selector01 fixes the old zero/list/gamma protocol responses but
does not yet admit HAL. Both9/Ex return8876086a after20successful callbacks.
All original32archive members and continuity checks independently pass.
Matching actual System32 d3d9.dll public-symbol GUID/section mapping resolves
GetDeviceCaps RVA166a00 and IsD3DHALSupported RVA1226f0. The latter returns
false when DevCaps2 is nonzero and Caps2 bit29 is absent. STREAMOFFSET is set
and DYNAMICTEXTURES is absent in478. Unlike the reference's broad WARP profile,
the actual SM2 validator does not require cubemaps; its Npatch minimum applies
only when DevCaps.NPATCHES is set. Do not add unimplemented features or guess
MaxNpatch limits. Public stripped PDB age3/image CodeView age1 are distinct;
exact GUID and section/address joins support this scoped disassembly inference.
Dynamic2D/default-pool locks and top-mip whole-resourceDiscard are documented
in local Microsoft LockFlags/ResourceFlags and official LockRect/performance
docs. NoOverwrite is a buffer contract, not a texture lock flag. Native feature
implementation and targeted CPU/GPU verification precede production Caps2 bit.

2026-10-07 03:14 UTC: Independent review02 of actual single-bit runtime
trace device-contract-dynamic-01 verifies original32member archive536bf095,
40successful GetCaps callbacks/eight exact Caps2 deltas/four genuine adapter
opens, but HAL9/Ex remain8876086a. VirtIO preflight accepted0; API/core device
calls0. Registry34/critical58/SYS/desktop/candidates/task cleanup retain.
This disproves sufficiency of the dynamic bit; full goal remains active.
Read-only matching-PDB disassembly identifies legacy PrimitiveMisc0x2000:
FetchDirectDrawData RVA19b7cc maps it into public fog/specular-alpha0x10000,
IsD3DHALSupported RVA122854 requires it forSM2, and FillInCaps RVA16571c clears
it from public output. New isolated frontend retains dynamic diagnostic and
adds only legacy0x2000 to exact478 profile (28ef0 ->2aef0). No production caps
change/device callback/GPU admission. Fixedb21 native fixtures compile warning
free; device test fails at379107 because SYSTEMMEM binding wrongly expectsS_OK.
Keep originals; correct fixture to verify rejected binding makes no backend call.

2026-10-07 ordinary runtime caps/device input checkpoint:
The dynamic-only diagnostic777 still fails HAL caps with8876086a. Adding only
legacy DDI PrimitiveMisc0x2000 to that diagnostic lets both genuine Microsoft
9/Ex HAL caps and VirtIO1af4/1050 preflights succeed. Nativee229 frontend's
separate static CRT initially split/reordered stdout records, so retain runtime
archive93d8d0bd as caps/restoration evidence and reject complete contract trace.
Synchronous one-WriteFile-per-record frontendeaa7 builds warning-free; fresh
runtime02 archivebbf7e32c/root review01 verifies8actualOpenAdapter,60successful
GetCaps,12exact dynamic deltas and12legacy fog deltas,19process-local name
redirects/restoration, exactly2API attempts and2blocked CreateDevice callbacks.
Actual Interface9/Version69632 has flags3 for9 and1 for9Ex; all11observed
callback pointers are present, legacy command/allocation/patch buffers are
null/zero. Device creation remains blocked; registry34/critical58, candidates,
signedSYS/dwm/explorer and owned-task cleanup independently retain.

Local and current Microsoft D3DDDI_CREATEDEVICEFLAGS documentation names bits
0/1 AllowMultithreading/AllowFlipBatching; both are permissions, with remaining
bits reserved. Native c8fbd55 accepts mask3 at both validation boundaries,
retains serialized backend/synchronous Present, and tests all four valid
combinations, every reserved bit with no query/backend side effects, and caller
flag mutation after snapshot. Original35member archived58d76b1/root native
review01 joins88 exact Git/Vulkan inputs,3+6strict CPP/9COFF/2ARM64 PE/raw official
SDK/CRT and unchanged candidates/System32/SYS/desktop. Actual43067adapter and
535562device checks pass warning-free. Exact architecture and hardware gates
remain pending for this source; normal production OpenAdapter remains closed.


## Genuine runtime lifecycle01 and DDI version mismatch

Exact889 frontend/c8f CI37569563644 enters actual System32 CreateDevice for
HAL9/9Ex, returns two private S_OK devices and immediately receives two
DestroyDevice calls before any resource/state callback. Both public API calls
return8876086a. Original42-member archive SHA2e40fa9f916456059a15d2c4f3b6a3ba5e09d3a69a4dfc7b03ac2ec99ffed485
is retained; registry34/critical58/signedSYS/DWM1552/Explorer6464 and selector
restoration pass. Frozen512-pixel lifecycle oracle has no positive GPU result.

Matching public PDB locates ValidateUMDeviceFuncs at RVA0668d8 and
IsDDIVersioningInvalid at RVA062378. The latter rejects a non-null old function
or a null new function. For driver version>=4002 the runtime requires this
old/new pairing for BufBlt/TexBlt/VolBlt/CreateResource, and for version>6000
requires AcquireResource and ReleaseResource. Native9 currently reports the SDK
default11000 while implementing the original entrypoints. Official local
d3dukmdt.h defines Vista000c and original d3dumddi.h DEVICEFUNCS has99 pointers
through Rename. New local fix negotiates000c and bounds both outer/inner table
publication to that prefix; native guard-page/canary fixtures remain required.
No production OpenAdapter export, registration change or GPU acceptance.


2026-10-07 continuation: Root originalc7 CPU review02 now PASS43067/538869,
35raw members/88 source inputs/9COFF/2PE/zero warnings and exact guard marker.
Review01 rejected a stale stdout assertion; original error and review01 are
preserved, unchanged native archive re-reviewed correctly in02. Supplemental
rawc8 candidate continuity proof01 passes all four original files.
Standalonec0ea native Vista runtime build rootPASS60 Git/53raw members/2COFF/
2PE/10CLI/3nullguards/zero warnings; candidate absent during guards, GPUfalse.
Six-script Vista lifecycle family01 stages and natively parses five executable
scripts, root proof SHA5b514339a3b4687de9d282549707714dde0f95c0fff5686baac321664d1ec0f1.
Prepared stage/run helpers retain independent512-pixel oracle and deadlines;
actual device-lifecycle-02 awaits full c7 CI37573265151 success and verification.
DX8 read-only original target inventory proof01 verifies System32 d3d8 absent,
SysWOW64 d3d8/d3d9 I386 and d3d8 imports d3d8thk. No runtime load or installation.
DX8 agent receives raw imports/exports/CodeView inventory; DX10 and DX11 remain
isolated, coordinate typed ABI and shared declarations, with no target overlap.


2026-10-07: Exactc7 consolidated37573265151 rootPASS6jobs/5rawZIPs/3PEarch/
15actualnativeARM64. CI proof SHA29352289cf1eece8060c73c760c0a13523330d96319a672f729d2166c821bdfc.
Exact CI ARM64 core SHA141ee74feb1d1b51e44bb0673769b848a695efe45136f5fc0ff1b1c05f66aae8
is process-local staged, no installation/registration. Actualc0ea/c7 Vista
lifecycle02 moves past ValidateUMDeviceFuncs and requests two16x16 X8 back buffers,
flags1081 (RT/NotLockable/DiscardRenderTarget), pool3,mips0; CreateResource mask
rejects1000/E_INVALIDARG. Both API calls80070057/object0. All58critical/34registry/
SYS/desktop/names/selector restored and both created devices destroyed. Original
42member5c8740b37566a32c5ff30c4533b8dd1a2b918cc37539aa72874bf65e88ac2ed2
reviewPASS20d7a2349b9eba40e2ee50a29711aabce9258745a5644b3d0c4127c6465a83e5,
evidence/restoration only, no rendering. Independent512-pixel oracle unchanged.
DX10b490ed1 and DX11bdc963c integrate as85ca8eb/0d4ff1d with typed10.0/10.1/11
selection, live runtime callbacks and resource/UAV/compute paths. Higher production
feature gates stay closed. Root fixes four ctor callback lifetimes, exact direct
factory interface/builds and native-entry six-failure live callback mutation;
MSVC native execution remains pending. GeneralSM5graphics/SO/tessellation/class
linkage and runtime activation are still incomplete. DX8 originalI386 runtime
calls OpenAdapter Interface8 and CAPS12; its isolated bounded projection is pending.


2026-10-07: Implement target-only DiscardRenderTarget1000 hint to accept observed
runtime flags1081 without changing preservation, primary/shared ownership or
feature advertisement. Add actual-shape four A8/X8/lockability controls with
1024 readback pixels, failure/retry, invalid-output/no-backend and caller-mutation
checks. Local strict official x64/x86 COFF01 first implementationx86 exposes
preexisting signed count_if comparison; original failure retained, bounded count
converted tosize_t. Fresh implementation02 passes both architectures; unchanged
fixture01 objects both pass. Native MSVC and actual runtime still pending.
Integrated callback fixes/ports source8d7a2ea is frozen for exclusive port_dx10
native CPU verification. New consolidated build/package/execution lists include
DX11 and compute fixtures, making17 future native ARM64 cases; no second workflow
or push dispatched. Root will send new exact discard source packet next.


2026-10-07: Actual native adapter8d7 attempt01 /W4 /WX /MT fails fixture C4127
at two constant offsetof CHECKs; three production adapter/query/contract AA64
objects compile cleanly. Agent original failed tar84fa1254e0610376494e0dc4c1d72d017381fc91e28837ed6782347383fc7284
retains source12/SDK21/libs5/RSP/logs and SYS/System32/desktop checks; root review
pending. Convert only these compile-time ABI bounds to static_assert, preserving
runtime guard-page semantics. Native rerun and discard-source packet pending.


2026-10-07: Freeze exact4e91632 discard/native9 source89inputs SHA
c27811a6f2cd25c3524064fa38757c30a07ced3ae4588c36418adadd8fe162c2.
Native verifier owns new static_assert adapter retry and then target9 fixtures.
Prepare separate frontend/probe permission device-lifecycle-4e91632 and fixed
DxvkD3D9DiscardCandidate-4e91632 path; table99/512-pixel oracle/deadlines and
blocked caps mode stay unchanged. No native build/run or CI push yet.


2026-10-07 continuation: affe7d4 native ARM64 nine-control original archive
3881b1d6b9a1d52bf3c86f81a7c1ca4e94097b8ef8aa2ffa0d83f749e56dd78b
collected; root independent review pending. Production/fixtures strictW4/WX
zero warnings; pinned dxbc W3 retains61 dependency warnings. WARP retains old
DWM1552/Explorer6464 before/after. Native9affe attempt01 stops before compilation
on strict Get-Process Explorer enumeration. Separate read-only snapshot02 shows
currentDWM1864(start1791378029900)/Explorer4464(start1791352985381), USERconsole1
Active, same SYSd48e/oem17/58624/PnPStarted/service4. Cause of post-WARP desktop
identity transition unresolved; no shell restoration justified or attempted.
Root owns target; capture fresh stability/boot snapshot then freeze new desktop
baseline for subsequent CPU/runtime controls, preserving all old proofs.
Standalone2851 and native9affe source archives frozen separately; no new push.
Local search shell unmatched wildcard is a read-only lookup error, no inputs changed.


Root snapshots02/03 join current desktop/SYS/PnP/session; System41/6008 records
unexpected restart, cause unresolved. New baseline01 frozen, no recovery. Port_dx10
owns native9affe02 and standalone2851 CPU. Root WARP review01/02 stdout-parser
errors retained;03 explicitly matches actual runtime GPU PASS852 and rotation1428
forms, original native packet unchanged. Front packet preparation assertion used
a nonexistent user32-copy anchor; unexecuted partial four receipts preserved,
completed via actual librarySource anchor. New2851 build copies originald3d9 SDK
receipt into native evidence; source60/coreaffe/oracle/deadlines unchanged.


2026-10-07: Root affe WARP original review03 PASS, proof94c37c2ff9f6f6b8b3764c40944a5b4a0eeef0bcfc08f732e09840aa3034220a.
Source71=47Git+24pinned dxbc,202original members,25COFF/9PE/22 native stages;
checks308/35/2637/10834/1428/6404/1629/1144/852. Strict UMD/fixturewarnings0;
unchanged pinned dependencywarnings61 retained. Historical SYS/desktop unchanged
during WARP. Fresh baseline03 independentlyjoins newDWM1864/Explorer4464; System
41/6008 unexpected restart, no current shell absence/recovery. Exactaffe production
object pushed once; livequery confirms sole consolidatedCI37580729947inprogress.
No frontend2851 or newer agent slices pushed. Root originalreview01/02 parse faults
and unexecuted frontend anchor-preparation error retained separately. Source and
execution archives unchanged; native9affe/frontend2851 CPU verifier owns target.


Root integrates functional DX11 1f asbf4f779 and DX8 498 as7c2c8ef locally; only
Meson shared_library conflict, retained both shader11 andd3d8_compat source units.
Integrated root SM5 ASan/UBSan93 and legacy2800/actual pinned SM1 converter47PASS.
No newer push. Prepared affe runtime family preserves512-pixel oracle/100000ms/
115s deadlines; replaced historical hardcoded desktop IDs with frozen current
PID+starttime checks, original c7 helper family retained byte-for-byte.


Root fresh affe native9 original142-member review01PASS43067/554085, proofSHA
1eab24277147ff0fb22f18de7571634aff755dcba45395f8eede1806c0526c86. Front2851
original54-member review01PASS60Git/2COFF/2PE/10CLI/3guards, proofSHAcd941b7341ed159cd2c08226ff27984c0245cb2dbc0c0609eaf7d2aff5dc5d3e.
Native-parsed fresh6scriptfamily staged with no API/registry calls; unchanged
512oracle/100000ms/115s deadline. Soleaffe CI37580729947 all6jobs success; raw
5ZIP/source/PE/17nativeexecution collection running. Target root-owned after
CPU verifier release. New root7c2 portableSM5/legacy/SM1 passed93/2800/47;
DX8 original302source/311validation root review passes202Git+92pinned+8SDK/
33COFF+6PE. Future build/package/native control lists now include allthree new
SM5/legacy/SM1 fixtures,20native cases; no additional CI push or dispatch.


Before actual runtime dispatch, root found helperfamily01 start-time guard used
100ns ticks reconstructed from JSON Unix milliseconds. That could reject a valid
unchanged process due to lost submillisecond precision. Family01 was parsed but
unexecuted; preserved. Freshfamily02 checks PID and DateTimeOffset Unix milliseconds
exactly matching original baseline precision. No fixture/source/oracle/deadline
changes; actual runtime remains pending candidate artifact verification.


Root soleaffe CI original collector PASS five ZIP/threearchitecture/17native,
proof31f9105d. Candidate3files staged core952118bc/probe4d0aa194/STATUS20cda9b2,
rawCI ARM64 ZIPd8e5ba44. Actual lifecycle03 helper02 stops in baseline/restored
KMT preflight before any API/probe call due historicalLUID6e6c vs currentec6b.
Original20member archivea48c940f preserved; collector removes bothownedtasks.
Native queries all S_OK, Source0, three identical original UMD names, close/release
balanced, exactLimited USERsession1. Result incorrectly retains historicalCIrun
37573265151 and restoration_failure from repeated same preflight; no restoration
success inferred from that result. Separate after-readinessfa901e50 joins exact
SYS/desktop/PnP and58critical fields; original34registration unchanged. Root
independent reviewdc47a7b1 scopes verification to failedpreflight andzeroAPIcalls.
Fresh helper03 archivea33d5c9e corrects LUID guard andactualCI37580729947 metadata
only, joins originalfailureproof before execution, keeps512oracle/deadlines.
Originalhelper01/02 andallfailedarchivebytes unchanged; nativeparse/lifecycle04next.
Root read-only lookup unmatchedglob and absentselector trace lookuperrors retained;
no production input changes. Fresh x86 packet7c2 frozen bybuildverifier, target
root-exclusive while actualgenuine lifecycle continues. No newCIpush.


2026-10-07 continuation: Genuine family03/lifecycle04 completed. Original42-member
archive03bd63362010a975402a8dc7578564c3919a168b63b3c5dfed635a41cfbb6704
and independent reviewb15d5cfc68bceae8cc06b2793ed51ca514ff6c9fbfa0622eac84c1e02c8c65ed
join Limited USER/currentec6b, two accepted1081 backbuffers, two rejected
SYSTEMMEM65536-byte VB02080004 creations, public9/9Ex80070057 and zero API pixels.
34registration/58critical fields, desktop/SYS/owned tasks restore. Secondary
first-flush88760868/Mesa-4 remains unresolved; no evidence of successful submit.

Root merges DX10formatbcbde4f asc2c4d14 and DX11query/IAffb418f asf4bf37f cleanly.
New root MightDrawFromLocked implementation snapshots borrowed bytes before
callbacks and forces owned draw-time SYSTEMMEM backing. Independent DX11 review
found two issues corrected: require Lock's own0x100 rather than creation-only
flag, and explicitly FlushBuffer after DirtyRange so mixed/unused streams queue
updated bytes before every draw. Locked state/count stays held through draws.
Reviewer also found upstream UploadPerDrawData subtracts vboOffset instead of
srcOffset in the partial-tail branch, permitting logical over-read for60-byte
VB/first1/count3/stride16/extent12; root owns bounded planner repair/control.
Agent owns isolated actual renderer repeated mixed/unused/16/32-index controls.

Local failed controls retained: integratedportable01 missing shared DXGI header
after private4029PASS; portable02 input41PASS then Clang dependency unused-private
reserved field under strict flags. Fresh03 uses original GCC SM5 command without
suppression and adds portableMSAA. LocalCOFF01 retained missing build-policy
flags/ARM64 Clang builtin-vs-official intrinsics mismatch;02 device/fixture4PASS
but baseline winternlOBJ_CASE_INSENSITIVE conflict in both backends. Fresh03 uses
Windows-only harness prelude and unchanged project warning policy, no source-wide
suppression. Actual ARM64 native execution remains mandatory.

Build agent exclusively owns target after root lifecycle04. Official EWDK14.50
Hostarm64/x86 owned tree575inputs/17recovered originals/61Microsoft ranges READY;
source ISO/mount unchanged. Exact7c2 CPU packet now running. User authorizes
passwordless Administrator. Root continues local development; no new CI push.


Root vertex-copy production helper and independent scalar-byte oracle pass
4050438 checks under GCC+Clang ASan/UBSan. LocalCOFF04 passes8UMD/backend/fixture
objects, but core warns on three preexisting unused captures. Removed those
captures; fresh05 strictly compiles both production cores with zero diagnostics.
Root local proof joins8+2 objects, exact input bytes and SPDX-only provenance.
Portablequery/IA/format03 completes SM5351/MSAA269, joining4029/41 prior originals.
Consolidated Linux script passes old controls plus planner/private/MSAA; original
malformed-token diagnostic retained. Future Meson/build/package/native lists
now cover25 ARM64 cases. Existing sole CI37580729947 refreshed all6SUCCESS.
Build verifier first x86 attempt tar-list fails PS5.1 nullExitCode before any
compile; original retained, explicit ProcessStartInfo runner/freshroot next.
LocalCOFF03 Windows-only prelude requires NTSTATUS for DDI units; fresh04 uses
original winternl for those units and Windows-only for private core/backend.
Read-only guessed native result/workflow/tools paths failed, no inputs modified.
New native-device and actual renderer controls remain mandatory; no new push.


Root follow-on bounds review identifies aggregate uint32 UP size can wrap across
streams/IB. Checked append now rejects overflow before allocation and keeps the
ordinary path; IB source/count products use uint64 clipping. Fresh portable02
GCC+Clang ASan/UBSan4050444 checks each; freshCOFF06 four core/copy objects PASS
zero diagnostics. First native packet closure incorrectly looked for conditional
Vulkan submodule header in root Git, stopped locally before archive/transfer;
failed empty01 retained, submodule-aware frozen-source preparation follows.
Agent probe6054985 adds18 real mixed/unused/perdraw repeated locked-buffer stages
98-115, expected1152newpixels/6d390bc5. Root review of callback414a flags borrowed
D3D9 table rereads after CreateDevice; agent asked to retain owned originals to
preserve production snapshot behavior before integration/native execution.


Protected teardown control added for a still-locked flagged borrowed VB after
PAGE_NOACCESS: DestroyDevice must discard the held lock without reading/uploading
borrowed bytes. Both local x64/x86 strict fixture COFF compile; native execution
pending. Native x867c2 caps544/API2800/adapter45802 pass, device7919 assertion fails.
Source audit identifies missing RT setup, not a capture switch (root initial
capture inference corrected to agent). Agent37e306e adds unbound rejection then
owned8x8 RT/bounded20-byte-stride60-byte FVF forwarding and exact cleanup.
Diagnostic4285518 corrects414 borrowed-table lifetime risk with owned22-slot
originals and protected/retired-table controls. Integrate both final slices and
freeze fresh root CPU source; original failed/native/local archives retained.


Integrated final probe605 as1124a52, protected teardown ase8120ad, DX8RT fixture
37e306e as36696a1 and diagnostics414/428 asc762aab/e7e1711. Original snapshots
retained; final forwarding uses owned22-slot original callback prefix throughout
lifetime. Callback CPU marker calls25/Vista22/function99/admission0; added to
sole consolidated build/package/ARM64 lists, now26 cases. Native execution not
yet claimed. Frozen f4 WARP88-input packet sources remain unchanged; fresh PS5.1
explicit-handle/raw-stream helper is prepared byDX11 before target handoff.
Native locked-buffer source preparer02 now resolves pinned submodule Git origins
for conditional Vulkan includes, and uses explicit ProcessStartInfo/raw streams.
First failed empty root archive-preparation01 preserved separately.


Root native ARM64 CPU4 packetb75-02 locally joins328 exactGit/submodule inputs,
sourcearchive4bd8b8a8fcadc7f24bff90177fbd1ed8ff8de3bc3a7d118dffee2115a86c650d;
no target parse/build/run yet. Actual EWDK x86b75 firstparty strict passes
544/2800/45802/584971, with observed unbound80070057/noDraw/0bytes and bound
S_OK/oneDraw/60bytes/stride20. Pinned dxbc vendor build first failed blanketW4/WX
(empty-macro/unused/shadow diagnostics); documented Meson vendor policyW3 and
conforming preprocessor now selected in a fresh helper, originalsource unchanged.
Root consolidated bounded fixture runner now rejects a missing ExitCode instead
of treating it as success, and drains stdout after wait. ARM64 runner already
compares explicit nonzero exit. Source/current driver goal remains active.


## 2026-10-07 integrated DX8/DX10/DX11 and bounded native runners

Root independently joins probe605 and callback428 original source/COFF evidence
with integrated sources. Fresh root-agent-integrations-02 verifies six strict
x64/x86 COFFs, the18-stage1152pixel6d390bc5 locked-buffer oracle, unchanged
callback caps/permissions/runtime probe and all nine original f4 packet files.
Attempt01 failed an assertion before saving a proof; it remains unaccepted.
Latest live affe CI37580729947 remains all6PASS; newer root source is unpushed.

SM4.1 agentc9c08fa is cleanly integrated as c9634b5. Logical10.0 rejects4.1
before the broader backend; actual new portable568 and typed10/10.1 fixture18
WARP draws/4608pixels are retained in the sole CI (28 native ARM64 cases).
Native new shader rendering is pending. Integrated ClangASan portable checks
are running under consolidated-linux-02 with raw logs and before/after source
hashes. Earlier source-path searches guessed .github/scripts and a zsh glob;
no files changed, and actual build/ARM64/Linux scripts are under scripts/.

Build agent reports exact b75 x86 six positives544/2800/45802/584971/52/4237,
34I386COFFs/9PEs, two intended semantic negatives and seven malformed CLI exits64.
Twenty first-party/W4WX compiles have zero warnings. Fourteen pinned vendor
units use repositoryW3/conforming preprocessing, retaining C4244/C4146 raw
warnings. Original collection/independent review and explicit target release
are pending; no hardware/runtime acceptance is implied.

Root freezes fresh CPU helper03 from unchanged b75 packet02/328inputs/four
fixtures. It reuses the tested f4 raw-process component and independent
failure-preserving finalization, and adds conforming preprocessing. Original
helper02 had unbounded kill/reap/pipe-drain calls and could lose its build
receipt if a final continuity capture threw;02 remains unexecuted and retained.
All fixture definitions, source/archive/compiler/SDK/library/candidate/desktop
pins remain unchanged. Target ownership stays verifier until explicit release,
then DX11 f4 WARP15, then root ARM64 CPU4 and the actual renderer/runtime gates.


Integrated consolidated-linux-02 completes PASS in31.73s: ClangASan/UBSan
SM4.1 568, existing shader147, boundedcopy4050444, privatechildren4029,
MSAA269, identity69/runtimeidentity180/backend49 and vertex342+negative.
Before/after integrated input hashes retain. Raw expected malformed-parser
stderr is preserved, with no sanitizer findings. Agent reports final frozen
c9 eight x64/x86 COFFs zero diagnostic bytes; root original review follows.
One combined production push starts the sole CI while native target execution
and original review continue; production admission and installed package stay
closed/unchanged. Later hardware testing requires successful exact CI artifacts.


## 2026-10-07 current published regression and native original review

Published b6cda5f sole CI37593963656 reproduces d3d11-device line54 failure in
both x64 and x86; ARM64 production, identity/shader CPU pass, native runtime
skipped. Raw failed-jobs log811269bytes SHAfca4eee4 preserved under
artifacts/dxvk-native-d3d9-locked-draw-20261007/ci-b6cda5f-failure-01.
Frozen f4 native WARP15 fails first fixture c0000409; original archive636bdf36
joins88 sources and11 retention flags with no finalization error. Fresh agent
diagnostic02 disproves the 32-slot layout hypothesis: 10.1/11 original/rebuilt
public WARP layouts pass. First compute shader callback16 returns80070057 after
169 checks; exact original/rebuilt comparison is next, no production fix yet.
DX11 retains exclusive target CPU; root issues no remote commands.

Root independent x86 original review03 accepts399 files/398 collection entries,
139 sources/133 Git origins/six supplementals,575 official compiler/header/lib
before/after joins,69 explicit commands,34I386COFF/nine I386PE, six positive
markers, two semantic negatives and seven CLI exits64. Observed FVF unbound
80070057/draw0/bytes0 then boundS_OK/draw1/60bytes/stride20. Original50,543,748-byte
archive61c38914 and root-native-x86-original-verified-03.json are preserved.
Root review01 guessed Name rather than actual lowercase desktop name/pid/start;
02 incorrectly demanded vendor-only Zc preprocessing in all first-party response
files. Both failed locally before writing acceptance;03 reads actual policy and
passes. Exploratory receipt reads used absent build-result.json and dict rather
than collection list; corrected to originals, no target changes.

Fresh CPU4 source/helper05 pins b6cda5f329inputs/13units including missing
umd_d3d8_compat.cpp in adapter/device. Original helper02/03 unexecuted; helper04
preparation correctly failed locally because its seed328packet omitted compat.
Failed04 metadata retained.05 prepared-input receipts pass, native parse/build/run
remain pending explicit DX11 release. New raw locked pixel print compiles strict
x64/x86 objects with machine8664/14c, zero stderr, resultPASS4.389s. No further
production push until the compute regression correction is validated.


Root follow-up source read corrects two resumed assumptions: existing605 probe
already prints all1152 D3D9_LOCKED_BUFFER_PIXEL raw values and final LOCKED_BUFFER
CLI marker. Redundant two-line D3D9_LOCKED_PIXEL trial is removed; its local strict
COFF originals remain as an unneeded trial, final production source unchanged.
SM4.1 root original review01 used an uninitialized worktree submodule (Git fell
back to parent); use actual initialized pinned root dxbc Git repository.02 then
used a guessed fixture filename;03 derives exact sm41 test names from original
manifest. Failed empty01/02 directories remain unaccepted. A runner read used the
checkout-relative workspace helper path; corrected to absolute workspace path.


SM4.1 independent root original review03 passes384 files/383 collection joins,
79 exact original Git/dependency inputs, raw GCC and ClangASan/UBSan568/147/35,
eight strict x64/x86 COFFs/zero diagnostics,365 compiler read inputs and28
original Microsoft SDK/header/library joins from four original packages. Root
supplemental integrated-source-original-joins-01 independently joins all four
actual COFF source paths (including tests/umd-d3d10-shaders.cpp) and shader header.
Native ARM64 SM4.1/WARP rendering is pending target handoff. A follow-up rg used
workspace rather than checkout tests; corrected through original COFF paths.

Fresh locked-runner-scripts-01 derives only new mode admission from frozen dynamic
runners, preserves ordinary1472 pixel gates and adds18stages/1152pixels/6d390bc5.
Three scheduled scripts retain LimitedUSER checks and process-local payloads.
Owned original raw-process componentd8cf replaces nullable Start-Process exit/
unbounded waits. Locked process65s, kill5s, pipes20s, task120s/result122s bound
failure cleanup; old frozen runners unchanged. Native parser/execution pending;
no hardware acceptance or target command by root while DX11 owns CPU.


Fresh locked verifier preserves prior1472 typed pixel/callback/readiness gates and
independently requires all1152 original pixel values in exact18-stage row-major
order, recomputesFNV6d390bc5, checks every locked resource/create/range/unlock/
retirement and bound raw child exit/pipe bytes. It requires pinned fresh desktop
baseline9eb3c586 and explicit actual LUID; no historical6e6c fallback. Python
parses; native PS parser/GPU execution remain pending. New unexecuted orchestrator
requires all six exact-source CI jobs PASS and local candidate bytes equal actual
original ARM64 ZIP, and guards independent cleanup/readiness/archive finalization.

DX11 diagnostic03 actual public WARP accepts both original compiled compute and
legacy rebuilt computeS_OK; generic decodeShader11 rejects before modern backend.
Original diagnostic archive023b5508 retained. Root source review of agent's fresh
repair worktree sees a22-line stage/operand/mask/duplicate validation addition for
four compute system input registers, excluded fromISGN; native/portable controls
are in progress. Old dx11-port worktree is frozen; use actual dx11-warp-repair
worktree from git worktree list. A reference artifact search guessed a worktree
artifact directory; actual original evidence stays in workspaceartifacts.


Root integrates10c831c compute input correction as555c83d, then exact malformed
binding preservation3f64065 as14f0a69. Root integrated ClangASan/UBSan646 checks
PASS16.48s, before/after input hashes unchanged. Independent original local review
joins two646 raw GCC/Clang runs, six actual strict x64/x86 COFF objects, zero logs,
and exact three integrated source files. Agent local receipts lack before/after
source hashes; this limitation remains explicit, fresh native Git packet provides
those actual inputs. Root direct diagnostic03 archive review independently joins
171members,88 retained sources,19 outputs,11 retention flags, realc0000409 exit,
original/legacy identical compute tokens and actual ThreadId00020012 declaration.

Corrected native3f first attempt still fails decodeShader11 on the new all-four-ID
shader after original and legacy public WARP succeed. Failure archivef25b2d20
retained; root holds CI. Actual next diagnostic confirms SV_GroupIndex uses
zero-component operand00024000, while other three input declarations use xyz
vector masks. Agent collects the original DXBC and will add only the dedicated
scalar zero-component form/effective-x overlap validation plus exact114-word
native FXC portable replay. No successful corrected native WARP15 yet; target
exclusive ownership remains DX11. Existing CI37593963656 remains failed exactly
b6 (fresh read confirms all six job states); no additional workflow dispatch.

Frozen GPU runner packet includes six original scripts/oracle/verifier inputs,
manifest33cceff1 and target prep helper48a8a0f6. Preparation/verification operates
only in new CUsersPublicDxvkLockedRunners-01; native parser/readback still pending.


Root resumes current096e024 with merged669 local sanitizer checks and actual
corrected compute96-thread/39malformed native controls passing. Later original
four-stream GS/SO creation remains E_INVALIDARG; packed mixed-type VS and original/
rebuilt public plain GS succeed. Local Microsoft stream declaration ABI confirms
stream/slot/register/xyzw mask fields. Declaration ordering remains a hypothesis
until direct original/rebuilt public GS/SO comparison, not an accepted root cause.

Fresh GPU orchestrator02 guards owned task cleanup after any remote preparation
attempt, including lost SSH response following registration. Original unexecuted
01 stays unchanged. Revision962a8194 and source receipt locked-orchestrator-source-
02.json retain exact diff and Python parse; no native execution or acceptance.
Empty findings apply_patch was rejected after successful helper02 preparation;
no plan change occurred until this explicit append. Root target access stays idle
until DX11 release. All6 CI states refreshed at exactb6, no new push.

Native81fa root original failure review02 verifies every163archive member,
88exact Git source/before/after/collection joins,19AA64outputs and11retention
flags. Three corrected shader/native fixture sources join current096e024 exactly;
all remaining source differences are recorded. Actual failure remains caller576/
2016checks/153callbacks/80070057, realc0000409 exit, bounded original pipes.
First ad-hoc review prep duplicated subprojects in the manifest's already full
submodule path; saved review01 then guessed standard COFF rather than actual
MSVC bigobj header. Corrected02 reads actual bigobj machine at offset6. Both
local validation failures preceded acceptance and imply no native state change.
A resume read also guessed nonexistent scripts/check-ci-state.py; actual GitHub
state is read directly through gh. No target call or production push by root.

Actual DX11 native GS/SO diagnostic03 isolates declaration ordering. Original
FXC and rebuilt public GS/SO both reject0,1,2,3,0gap withE_INVALIDARG/null and
accept0,0gap,1,2,3 withS_OK/non-null, unchanged slots/strides32/16/24/16 and
RasterizedStreamUINT_MAX. Single-stream raw/grouped controls pass. This confirms
an API conversion boundary rather than lost output signatures. Agent collects
originals then stable-groups public declarations byStream, preserving stream-local
element/gap order, exact byte/query oracles and malformed control behavior.
Root original81fa archive actually has163files/SHAe4632a68, correcting the earlier
plan typo171. Its three root-source differences are umd_ddi.cpp and SM4.1
umd_shader.cpp/.h; repaired compute sources join exactly, full-root native
acceptance remains a separate gate. No root target calls or production push.

Root freezes CPU4 native preflight helper6de68da8 and execution orchestrator
868831df, with original source/helper05 eight inputs checked locally. Native
preflight will pin prepared manifest32ade9cb and parse both original PowerShell
helpers on native ARM64 before build. Orchestrator preserves each raw SSH/SCP
command/stream and independently finalizes/joins the original archive after any
build attempt. Neither helper is executed; target still belongs toDX11.
DX8 agent local04 now preserves seven before/after source hashes, four strict
I386COFFs/three PEs, soleOpenAdapter export, GCC/Clang306positives and eight
semantic controls. Native CPU/build and genuine SysWOW64 runtime remain pending;
agent freezes final packet after updating legacy manual entrypoints for the new
separate modern-DDI guard unit. Core pins come from a future original CI DLL.

Root reviews/integrates exacte6bf08d six-line stable SO grouping asd153e818d61a.
Fresh root merged ClangASan/UBSan945 PASS17.44s/compile0/run0,13declared input
before/after hashes retained, result root-stream-output-d153e81-01. Nativeexacte6
strict build, GS/SO construction and all13 malformed pointer/private-byte/canary
controls pass. Later NativeQuery creation124 returnsE_INVALIDARG before the
four-stream draw; no full byte/query/pixel acceptance. Agent retains original
failure and compares exact public Query/Predicate/QI behavior for stream-specific
booleans before correction. Target ownership staysDX11, sole CI remains held.

DX8 freezes e99391a28-input packet (14harnessGit/11uncompiled b75references/three
licensed headers), local04positive306 and legacy18COFF/9PE compile gates. Root
finds its new native Run() still has unbounded Kill/WaitForExit and serial40s pipe
drains. Agent replaces only new helper with exact already-tested raw-process
component in a follow-up/fresh packet; e993 originals retained, no integration or
target execution yet. Root resumed Meson read guessed tests/meson.build; actual
root meson.build/src/umd/meson.build contain no DX8 runtime-probe target, confirmed.

Root independent DX8 local04 review02 passes seven exacte993 Git/source-before/
after joins,37original commands, four strictI386COFFs, three independently parsed
I386PEs, soleOpenAdapter frontend export, raw GCC/Clang306sanitizer positives and
eight exact semantic negatives. Imports remain the original system/CRT libraries.
Review01 guessed a positive command name rather than matching its actual
sanitizer compiler argv; rejected locally before proof write,02 selects the
original command structure. Native helper bounded-process follow-up pending;
e993 frontend code remains unintegrated until that fresh packet is ready.
Target scheduling: after DX11 current query correction attempt/collection,
explicitly release to rootCPU4, then frozen DX10SM4.1, then return to DX11 if its
remaining WARP15 stages need more work. No overlapping target calls or CI push.


Root integrates bounded DX8 followup77bbe79 asf897bc2 and per-stream overflow
query correction634d781 as2c1b0e2. Public WARP diagnostic shows CreateQuery succeeds
for9/11/13/15 while CreatePredicate rejects all four; ordinary Query owners are
kept for native typed predication. Exact634d native strict build and query/draw/
readback advance to later GraphicsShader decode assertion558, stage not captured;
lastHRESULTS_OK/checks2700/callbacks186. Original archive309694cb/88sources/21SDK/
2APIlibs/19AA64outputs/6ownedstages/11retentions are joined before explicit target
release. Root starts frozen CPU4b6-05 in orchestrator session5099; DX10SM4.1 follows
only after original finalization. DX11 prepares next code/stage diagnostic locally.
Merged root production/query/native fixture strict x64/x86 check is running
locally using retained07 compiler arguments and original before/after sources.

DX8 final handoff02/9b1804ee joins fresh77bbe29-input packet8c3a43b1 and independent
frozen-packet-cross-verification03/937952c3:880 compiled tool/header/library inputs,
four strictI386COFFs/threePEs. Nine compiled first-party inputs remain byte-identical
e993 to77; GCC/Clang306 and eight intended semantic controls retained. Native
PowerShell parse/EWDK CPU and genuine runtime execution remain pending; no core
DLL is required for CPU packet. Future CI core commit/run/path/hash stay null.
Build verifier now audits sole consolidated actual I386 private-loader config
locally, without target access, rebuild/relabel or CI dispatch.

Resume reads guessed nonexistent tests/meson.build and build-backend-umd.yml;
actual root meson.build/src/umd/meson.build and build-native-umd.yml are discovered
before follow-up. These failed local path reads caused no native state change.


Root native CPU4b6-05 completes in13.55s and raw archivee8c17473 is finalized.
Independent root-review02 verifies all409members/329Gitinputs/21SDK+five original
libraryZIPmembers/13AA64COFFs/fourPEs/eight actual process handles/exits/pipes and
11retentions. Adapter45802/device584971/copy4050444/callbacks243 pass. Ten unique
compiled units equal currentroot2c1b0e2; full-source differences umd_ddi.cpp and
umd_query.h remain explicit, so no full-root native acceptance is inferred.
First reviewer incorrectly assumed every positive line says PASS checks=;
callbacks original says verified checks=243 with25calls/22VistaCallbacks/99funcs.
Original failedreview01 and root-review-error-01 retained; corrected02 reads actual
marker, without rerunning native fixtures or altering their original bytes.
Merged query correction strict x86/x64 sixCOFFs pass with684root input before/
after hashes, raw zero stderr and original07 SDK/compiler flags. No target run.

DX8 independent root-frozen03-review02 passes all29source archive/Git joins,
880compiled before/after inputs, fourI386COFFs/threePEs, two older original ZIP
hash/CRC joins, actual PE imports/soleOpenAdapter export and nine unchanged root
compiled inputs. First reviewer guessed handoff key compiled_first_party_source_
joins; actual key compiled_input_links is inspected and used. Original failed01/
errorreceipt retained and no proof was written until corrected02. No native run.

DX10 frozen c9 actual strict first-party compilation/link,18AA64COFFs/twoPEs and
nativecontainer568 pass. Typed shader fixture rejects first expected S_OK at
expect30/E_INVALIDARG before any draw; no18draw/4608pixel acceptance. Agent preserves
167-member archive7301d4bd/three original FXC4.0 VS/GS/PS containers+token pairs/HLSL,
seven exited/drained owned children, all12retentions and completed collection/
transfer before explicit release. Target next belongs exclusively to DX11 for
its already frozen634d stage/code/class-linkage diagnostic; root and DX10 stay
local. After diagnostic finalization, root will run fresh77bbe DX8 CPU packet.

Fresh GitHub read: CI37593963656 remains exactb6 failure; identity/ARM64backend/
shaderCPU pass, x64/x86 backend fail, ARM64runtime skipped. Four unused workflows
remain manually disabled; offline/runtime-control are manual-only, consolidated
backend is sole production push trigger. Build verifier identifies all three
backend jobs omit VulkanLoader, so future production cores would search public
winevulkan/vulkan-1 despite existing module-local loader support. Narrow workflow/
config provenance fix will pin existing architecture-named private loaders.

## 2026-10-07 DX8 native success, hull repair and typed DX10 setup

DX8 exact7c1f545 native CPU PASS is retained as original127-member archive
9f7db177:28 owned stages all exited/drained,306 policy/14 CLI guards,4I386COFF/
3PE,29source/575compiler/8SDK/7library before/after joins and unchanged state.
No runtime/selector/core/hardware call is inferred. Actual EWDK bcrypt header is
shared/bcrypt.h; original77bbe precompile failure remains retained.

Root220b4c0 hull phase decoding passes merged1118 sanitizer checks. Exact14ebee
WARP15 native packet is root-owned after agent execution turn stopped under
automatic cybersecurity-content filtering without a result. Root directly runs
the prepared build/test, preserving original phases. Native corrected result
pending. DX10 ad13 proves all shaders create/bind; first Draw rejects missing
typed fixture state. Root merged fixture chain0256206/d536e58/3178900; immutable
production/HLSL/oracles retained. Native corrected e1bc suite pending.

Root integrates61e private-loader CI as a334e39; source/config local checks pass,
new CI native parse/execution pending. Separate bounded process repair assigned
to build verifier. No push/dispatch/installation yet. Workspace artifacts live
under /home/sunf/droidvm-repos/artifacts, not the checkout artifacts directory.
Two initial cat reads incorrectly used checkout-relative paths and failed; rg
confirmed actual original paths and handoff messages were explicitly corrected.
Microsoft local SetRenderTargets/SetViewports/ClearRenderTargetView docs were
read during fixture review; actual original SDK signatures remain authoritative.

## 2026-10-07 continuation: integrated modern source and exception fixtures

Root exact890 native attempt finalizes archivecebf778e49106fdf1036c72b7a2f686ef3d59718d374a5fbdde16cb678a40f27 (6644818bytes/290files):13 fixtures PASS, actual typed4147/callback430, query189/private4029/input41; final d3d10-formats strict compile fails oneC2220/sixC4702. All31 raw owned stages exited/drained and all11retention flags true. The original14 run had3832/367; do not transcribe poll-sensitive counts into890. Root release review errors01 (transcribed count) and02 (assumed homogeneous checks= format) remain preserved. Discovery command also initially used checkout-relative path for a workspace .planning helper; corrected through rg --files. Another exploratory host_commands[0] print assumed a list; actual DX10 schema is a mapping. No original guest evidence or source changed in these review corrections.

Root3c1ef05 changes only two fixture files: volatile runtime exception injection avoids optimizer proving callbacks noreturn; every CHECK site, caught exception, zero output and call-count oracle retained; production templates unchanged. Portable ClangASanUBSan269 and four strict optimized official-header x64/x86 COFF controls PASS; actual MSVC test pending. Root3868b01 integrates bounded SampleIndex fix a00, andc407197 integrates separate modern=true test export925e with guarded modern table/malformed creation controls. Public admission/capability bodies unchanged.

Native a00 completes9draws/2304 exact reference comparisons including SampleIndex256; next interpolation DrawE_INVALIDARG is captured. Agent original review227/79Git/21SDK/7libs/18COFF/2PE/7native stages/12retention PASS; archive d1b4e7f161619d89d5a28b1fb11abafebaa7cbd5047c937a29b78b471624bd93. Exact PS TEXCOORDreg0 conflicts with VS SV_Positionreg0/TEXCOORDreg1. Official D3D10 linkage requires register alignment; fixture-only position-first signature correction is next, not production fallback. All57/18/4608/3 oracles remain required. Root has exclusive target; c407 fresh packet88/15/archive8200a4614eb38648ec7a7b258d9d2120c5af0a18cca0f4c5ed1e861ecc6ab79f/manifest03d373966ef24af7902851e0f3ba5856ddcf9ead9104e4de6574f6784c7e0f46 is prepared and preflight running. bf6 full-script parse/wrapper smoke and c407 adapter native guards follow. No newCI/push/hardware acceptance.

## 2026-10-07 completed native WARP15 and bounded wrapper smoke

Exact c407197 full native WARP15 PASS. Original archiveb367f1dc987d7fec090d62ef3ce69afa729cd8a4db500d8fcb9121085768bf55 (7044979bytes/305files) and root-original-native-review-01.json join88 exact Git/archive/beforeafter inputs,34AA64COFF+15PE,34 exited/drained raw-process stages,21SDK+7libs against raw official packages, original helper/d8cf hashes, strict RSP flags and actual15 fixture outputs. Typed3977/callback396, native-entry2637/lifetime10834, SM51118, query189, private4029, input41, format3475/callback12/backend2 and MSAA269. Firstparty warnings0, unchanged pinned parser65. All11retention flags true, original source differences to root empty. Completed suite remains controlled WARP/CPU, no native hardware/ordinary runtime admission.

Native bf6 wrapper smoke PASS: full committedPS AST0errors, actual functionextent9af16c1856583de7b3d3d1569d9d794a42418b073cf6b9ebc9e5088fa5b2c155 unchanged; real success0, nonzero7, concurrent1048579stdout+1048587stderr and fixed30-second kill/reap30.0061365s controls all accepted. Original archive3435c36d04391c54fe14ed5038f271efe6b38c0a526b1df53ec5d3756041fa04 (17663116bytes/47files),46 indexed original joins,9source/575compiler/3SDK/4library beforeafter plus SYS/service/desktop identical. NativeI386COFF/PE/PDB; three compile/link/inspect+four wrapper cases+tarcollection=8 actual retained OS children all exited/drained (intentional timeout exit-1 expected). All five host transports complete0; root release proof finalized. No production core built/loaded, GPU runs0.

Root integrates fixture4a6 as85ae592; synthetic compatible-input control bug5/16 was then caught in agent original review and preserved. Correctiveb358 integrated111585a changes actual offsets5/15, asserts literal16 and compares all46 words; frozen763/147/35 and COFF4 PASS. HLSL2119/SHA7d658b unchanged after4a; all production/pixel/reference/57/18/4608/3 bodies unchanged. Isolated b358 packet differs from merged root at six sources including umd_ddi.cpp and umd_shader11.cpp; agent is preparing exact d2f native packet instead of relabeling isolated objects. Rootd2f0e0348b5ee13e7f6d140f00aba8dca036e491 integrates one CI adapter invocation via unchanged bounded runner9af/d8cf, keeping other26 calls, architecture/stdout/zeroexit policy. Five-sourcec9 actual nativeAST audit remains next. c407 adapter guard02 has13 source joins/8localCOFF, actual nativePS5.1parsePASS and nativebuild underway. Root retains exclusive target; no push/CI yet. Small exploratory filename/CWD/schema errors are discovery/reviewer failures only and never overwrite original receipts.

## 2026-10-07 native adapter guard and CI AST finalized; merged SM4.1 handoff

Modern c407 guard02 PASS94,208. Original archiveb3f3fd58d46756167c82b5e609679b76bbe9a4908ffb9c7a16367a4705c31c66 (995242bytes/62files), three actual OS children exited/drained0, fourAA64COFF+onePE, zero first-party warnings, source13 Git/archive/current-root/beforeafter/collection joins and all11 retention pairs verified. Seven local transport receipts finalized0. Root release is finalized; build verifier independently reviews official inputs locally. No core loaded or GPU/runtime admission.

Native PS5.1 c9 adapter AST audit PASS: full before/after parse0errors, bounded26-to27/direct adapter1-to0, byte-identical function9af/C#d8cf, outputadapter-test.txt/exit0/30000ms/architecture condition retained. Five source inputs beforeafter unchanged; upload/audit/transfer original raw receipts exit0. Root final release28274e0f joins transferred proof. The broad CI script was only parsed; its build and GPU actions were not executed by the audit.

DX10 merged d2f packet prepared3d753bf0/source732868ee/builder7b19b1ee, original local proof69a32b61 with GCC+Clang ASan/UBSan763/147/35 PASS. Root independently rejoins all79 inputs to current-root Git/archive bytes, including the changed DDI and shader11 compilation units. Explicit target handoff6f4fd6d4 gives DX10 exclusive ownership for native57/18/4608/3; all peer target calls stop until original collection/transfer and release. Root corrected a submodule-bool schema discovery after an exploratory TypeError; no originals changed or new target attempt.

Fresh GitHub API snapshot root-ci-current-state-d2f0e03-01 confirms latest sole automatic CI37593963656 failedb6, no fresh published candidate; four unused workflows disabled_manually, offline/runtime controls active but manual-only. DX11 local real-KMT compute probe performs384-word/1536-byte structured UAV readback with typed11 guards; source/strict/native link reviews continue before integration. No push or hardware/ordinary-runtime acceptance.

## 2026-10-07 complete merged-source SM4.1 accepted

Exact d2f native763 compiler/7483 typed checks,57 original shader pairs/HLSL7d658b,18 draws/4608 reference-equal pixels/3 expected malformed create callbacks PASS. Original archive5c2a71a26762c5202ee4cc6e9952e03b8860fff91d679b47268beb5da27c1fc3 (3371615bytes/275files). Agent original prooff96b6e1e and root-sm41-merged-d2f0e03-review-01.json independently rejoin all79 current Git/source inputs, actual7 retained/exited/drained native stages,18AA64COFF+2PE,57 exact DXBC/token chunks, all18 scene rows and preserved final markers. First-party warnings0/parser65 retained; full SDK21+libs7/raw package28 joined by agent. All12retention true, six local transport operations finalized0; release41d59f explicitly returns target to root with no pending target operation.

Both full CPU/WARP publication gates pass. Root bcedab5 changes only the stale ARM64 runtime-case marker568 to actual763; fixture inputs and all draw/pixel/negative oracles unchanged. DX11 compute-probe review caught an overrestrictive locked-allocation residency guard: matched Mesa keeps command/global BOs mapped through Render, while the proven D3D9 transport permits owned mapped references. Agent corrects/finalizes separate test probe/CI package slice before one consolidated push. Final native script parsing/probe compile-link and actual fresh CI artifacts remain pending; no hardware/ordinary-runtime success inferred.


## 2026-10-07 corrected compute source and final script audit

Root integrates eb3/23d as7b32aaf/da0a4ed; seven independently reviewed inputs match current committed Git. Corrected probe supplies DXGI runtime revision9 (Version0x00020009), preserves actual optimized D3DCompile/HLSL/DXBC/SHEX and384-word readback, and permits owned mapped BO residency. Independent read-only source proofca2795e1 verifies four original x64/x86 COFFs, typed10/11 callback signatures, guarded ownership/cleanup and separate GCC/ClangASanUBSan12482-check oracles with12288 bit rejects each. These are CPU/source checks; actual core/hardware pins remain null.

Final actual WindowsPowerShell5.1.26100.1591 audit PASS full before/after script parse0errors, original27 bounded invocations preserved and one compute-oracle call added, function9af16c/C#d8cf unchanged, ARM64 table28 and strict SM4.1 marker763 joined to original native stdout. Native proof7cc2b00a and releaseedd4a073 finalize all three host transports0; both current root scripts still match tested55a23df/cd3914 bytes. Four unchanged locked GPU runner scripts also parse0errors on nativePS5.1, proof3d6d68af/releasecf1d68d7; no script main, scheduled GPU task or core invocation occurred.

Root independently joins frozen23d packet19 exact Git/archive/current-root source inputs and13 prepared files. Native attempt01 passes three nativePS parses and originald8cf Add-Type, then LIB exits1146 because unparenthesized concatenations split each /DEF and /OUT scalar into prefix/path/quote response rows. Original archive3a42a11f (58files), failure reviewd62d5fbc and release0670d171 preserve the actual child retained handle/exit/drained pipes, source19 and13 retention pairs; all seven host transports finalized. No probe compilation or hardware occurred. Fresh helper02 parenthesizes all response-array scalar concatenations and asserts exact cardinality/paths; builder e7659b95 and preparedad99c2e6 retain identical19-source archive04e350c5, manifestaf1920f1, strict flags,12482-check oracle and eight60-second native children. Root review PASS and explicit e33ea960 authorization hand exclusive target CPU to DX11 for02; root and peers stay local until collection/transfer/release.

Fresh GitHub API snapshot root-ci-current-state-da0a4ed-01 confirms publishedb6 remains latest failed37593963656, precedingaffe37580729947 succeeds. Four unused workflows remain disabled_manually; sole automatic native UMD workflow and manual-only controls retain. Fresh-CI collector now checks exact six job names/five original ZIPs, all three generated private-loader configurations/source11, actual28 source invocation receipts and original DLL CodeView GUID+Age against the PDB. Actual historical three architecture pairs and swapped-original PDB rejection pass; Optional raw-CR ARM64 Hash/Path robustness and duplicate-row checks added; the original actual read_text path already normalized CRLF and parsed17 historical rows. Independent collector review4b8cb612 PASS; no fresh candidate has been accepted or hardware run.


## 2026-10-07 native compute probe publication gate complete

Exact23d native helper02 PASS and explicit release0a39d778 finalize all seven host transports0 and all eight native retained/exited/drained children0. Original archive4f495a3eb926da4bf2dcf055d3bd043542d72742ab48234737499fb51cf48c10 has1130775bytes/89files. Agent original review4ee76335 joins19Git/source/beforeafter/collected inputs,21SDK/7libraries/4tools against original Microsoft packages and all13 retention flags. Root independent413315b7 reopens all89 originals, current19 inputs, actual raw process exits/pipe lengths,2AA64COFF+2PE, required modernadapter/KMT/D3DCompile imports and CPUoracle12482 output. Strict /W4 /WX /MT /O1 /std:c++17 /EHsc /Zc:preprocessor retained; actual optimized compute workload remains present. Probe/core/GPU unexecuted, ordinary runtime admission and future hardware pins remain unset.

Root-publication-native-gates-da0a4ed-01.json verifies current88 WARP compilation inputs unchanged and78/79 SM4.1 source inputs unchanged; the sole difference is Meson metadata for the new separate probe, with all compiled SM4.1 units/oracles unchanged. Full native WARP15 and SM4.1 57program/18draw/4608pixel/3negative proofs remain accepted. Both final CI scripts still match nativePS5.1-tested55a23df/cd3914. Root commits this checkpoint and publishes one consolidated candidate; actual successful six-job CI, original five archives/private loader joins and target GPU/runtime acceptance follow. Target CPU is released to root; DX11 prepares the USER compute GPU runner locally and DX10 finalizes isolated typed10 probe locally.


## 2026-10-07 single consolidated candidate published

Root commits checkpoint930d7f7552d1a087ab12e6a2a3756e9556e2398a and pushes the existing work/native-dxgi-rotation-20261005 branch, actual push0 with preserved raw receipt. No manual workflow dispatch. Fresh GitHub exact-commit list confirms exactly one push-triggered native UMD run37630945784 (2026-10-07T13:44:41Z); shader-cpu succeeds, other backend/identity/runtime jobs pending. Four unused workflows remain disabled. Actual successful CI/current artifact/private loader/hardware pins are still pending; root retains exclusive target CPU.

DX10 isolated6b4cad7 followsf88 with named atomic SDK version constants because the SDK macro does not parenthesize its argument. Original local proof53c3c9b3/archive5c11e6bb retain15 Git inputs, original four optimized x64/x86 COFFs,4141 sanitizer oracle checks and unchanged peer transport33bec601/common implementation/completed57/18/4608 suite. Independent review continues. Root catches scalar-array response-file grouping in its prepared helper02 before any target execution; owner prepares helper03 with explicit scalar grouping/path cardinality checks. DX11 prepares interactive USER compute runner and independent384-word raw validator locally; both peers keep future core/run/hash pins unset until successful original CI artifacts are verified.

###930d7f7 CI failure and corrective work

CI37630945784 was the sole automatic push run. Identity/shader-cpu pass;
ARM64/x86 raw workflow blob identity rejects before Meson. x64 builds core
and passes26 owned native fixtures, then D3D9 backend rejection process
exits0xc0000005 with retained handle/exit/raw pipe drain. ARM64 runtime is
skipped. Original API state, all3 job logs, contracts ZIP13da0c3b and failed
x64 ZIP55fe9cf7 (47179bytes/92members) are retained under central
root-consolidated-930d7f7-01. Root original review9ba39f68 joins original
API source/run/digest/size/CRC plus all27 process receipts and raw streams.
No successful candidate or new hardware pin follows this failed run.

Root fixes null LibraryLoader::sym before LibraryFn member initializers,
retaining strict module-local private loader/no fallback policy. Actual
GCC/Clang sanitizer original-source negative controls each reproduce the
null call; fixed originals pass16checks. Full native identity PASS under
root-ci-repair-loader-02. Initial attempt01 retained19 pre-existing Unix
Win32 compatibility unused-parameter errors; only this diagnostic is
suppressed for the host regression, Windows warning policies unchanged.

Independent Git2.43/non-racy and official Git-for-Windows2.55 source
confirm stat-clean checkout-index returns before testing force; initial
racy control rewritesLF and remains separately retained. Explicit raw
ls-tree/cat-file helper bypasses this cache and conversion/archive attrs;
source12 includes this helper and existing byte/hash checks remain strict.
Independent raw-source helper proofc1923023 PASS:835 actual root regular
blobs plus synthetic export-subst, three text/auto/export-attribute controls,
12strictGitchecks/fivegitlinks retained/wrongCI-SHA-cwd/mutation rejection.
One corrected consolidated push follows; genuinehardware pins stay null.

Root reviewed DX10 helper03 source15/archive and9 prepared inputs,
root handoff9b02834e. Before any target call DX10 found undefined service
variables under StrictMode; root held the known-defective invocation and
requests fresh04/local audit rather than a knowingly failed native run.
DX10 still exclusively reserves CPU; DX11 GPU runner remains local-only.

Corrective42cf765c63e300b8ec058676e4243408a89741d6 is committed/pushedactual0.
Exactly one automaticCI37635552675 starts; no manual dispatch. Allfour
duplicate workflows verifieddisabled_manually. Seven committedGitinputs
join independentcanonical proofc1923023; currentnativecompiled joins are
WARP88,SM41 78/79onlymesonmetadata,compute18/19onlybuildPSmetadata.
No successful artifact/core/hardware pins are set yet.

### Target05 native CPU progress

DX10 exact04preflightfailure finalized: original40files/861973bytes
archive302a2574, threeownedcollector sidecars, source15/SDK23/libs7/tools4,
all8 hosttransportsactualexit/rawcapture and oneownedtar exit0/drain.
Release9d46c8f0 returns target; no compiler/fixture/COFF/PE/core/GPU.
Root verifies originalarchive digest/size/count. Source-order implications
from04emptystdout remain distinct from observed final preflight PASS.
Fresh05helper adds explicit System.ServiceProcess assembly load and actual
resolvedtype/fullname/location/file evidence; source15/flags/RSP/oracle4141/
CLI5unchanged. Rootdirect05handoffb4b733a8 hashes9prepared inputs/narrow
04-to05diff. Native05NoProfilePS5.1.26100.1591 ARM64 PASS fouroriginalprefix
markers plusJSON: parse2/errors0, RSP16/8/16/3/flags14, rawd8cf Add-Type,
resolvedServiceController assembly133960/SHA2b349085/actualRunning4.
SSH actual0/drain and priorSYS/binding/DWM1864/Explorer4464 preserved.
Native05 strict compiler fails: five full64-supported-version-to-UINT C4310
warnings become C2220 under unchanged /WX. Archivec1b09fa3/57files,
proof58c27a2a/releasef9ba9e4b retain source15/SDK23/libs7/tools4, all13retention
flags, three owned handle/exit/raw-drain receipts0/2/0 and eight finalized
host transports; original SYS/service/binding/DWM/Explorer stay unchanged.
No link/COFF/PE/CPUoracle/CLI/core/GPU was reached. Follow-on05ffdc2 masks
only the low32 Version values and proves full64 reconstruction locally.

CI37635552675 completes five successful jobs, including all three backend
builds. The ARM64 runtime failure is a stale stdout pattern: typed D3D11
actually exits0 and prints PASS4307/callback462 plus SM5 graphics/queries/
packed IA/streams/tessellation/classes. Harness expected callbacks followed
immediately by WARP controls. Stderr malformed-operand text is appended to
the exception as context and is not a failure predicate. Root changes only
that precise expected success line and retains failed originals before a
fresh consolidated push. Whole six-job success/private-loader/hardware pins
remain pending; DX11 has a short CPU-only frozen helper parse slot.

Independent full-line marker proofa754d516 PASS, including four genuine
marker/line-ending positives and18 rejection/exit controls. Producer and
genuine18-case malformed hull-shader rejection path/pinned dxbc parser join
exact42 originals. Original successful x64/x86 logs both match all28 corrected
patterns (56 total); old expression matches27 each, only typedD3D11 stale.
Proofd3ad8439 rejoins original raw job streams and actual gh exit0/size/hash
receipts; no new fixture/nativePS/GPU execution is claimed. Root's precise
one-pattern change leaves all actual fixture and runtime exit checks intact.
Large original API archives timed out at120s in retention01; truncated bytes
are preserved and excluded, and readonly fresh02 retries300s host downloads.

Fresh02 original retention completesactual0/receipt185f39c4. Independent
failed-run backend proof8599d2e8 verifies all5 APIoriginals,835 canonicalGit
rows per architecture/12 beforeafter configs/generated5/private3/3matching
PE-PDB identities,56 genuine marker outputs/56 bounded children. ARM64
originals have8 output pairs/7accepted hashes; failed marker exit0 and20
unrun remain distinct from accepted CI. Root publishes marker-onlye781175,
sole automaticCI37640080344; six-job success is still required.

Native frozen computeUSER helper02 passes PS5.1.26100.1591 ParseFile6 and
original d8cf Add-Type;15source/10prepared joins,23-file archivea3504520,
review1f25602c/release628cd196/3transports0 finalize. No helperbody orGPU.
DX10 source05ffdc2 is local strict4COFF/4141sanitizers/2positive4negative
narrowing-control PASS. Rootfrozen06 review623f066b joins source15/current
common10/9prepared bytes and442-file originals. Native06 preflight passes
unchangedflags/RSP/liveSCRunning4; single strict compiler/CPU attempt now
owns target. Hardware/ordinary runtime pins remain null.


ARM64 e781 timeout is distinct from the fixed marker: original41-file ZIP,
20 output pairs/19 accepted hashes and source/runtime prefix proofc4ba9a0e
retain the actual failure. Last SetViewports checkpoint does not establish
fprintf return or typed/backend entry. Identical native source passes18draw/
4608pixels in0.227s; CI crosscompiled release differs in toolchain/runtime.
The old ARM Start-Process harness lacks original per-case retained-handle
raw drain receipts. New exact d8cf migration keeps28markers and30s gate;
actual nativePS5.1 AST/Add-Type PASS9b313596, fixture execution still pending.
This strengthens capture evidence without claiming production stall fixed.

DX10 native07 now actually passes strictARM64 2COFF2PE/warnings0, CLI5,
CPU4141 and original import parser2positive14negative. Root2c4518cd joins
98original archive bytes/source15Git after agent811332a4; all14retention,
14native stages/8transports finalized. Earlier06 failure is a header filename
false positive; only module parser/record/path changes, no oracle weakening.

Actual System32 five payloads/raw15-file archive rejoin native5AA64PEs,
compiler4b68/beforeafter via root0dfdc7be. BCrypt remains a DX10 original
input requirement. Mesa8443 unsigned loader3001 differs from accepted mixed
signed51d9. Historical direct3e50 original3665 matches a signing-normalized
prefix only; signed parent API archives now NotFound. Preserve frozen02 and
all originals; fresh runtime03 with genuine matched8443 components needs new
native process-local validation before filling GPUready inputs. Never relabel
unsigned/signed full-file hashes or distinct loader/ICD/source/run identities.


Exact crosscompiled e781 shader EXE from genuine ARM API ZIP now passes
native WARP-only replay0.24375s/7483/57FXC/18draw/4608pixel/3callback.
Original126-file32a1 archive/root6a558 proof join114 DXBC/token bytes/HLSL
and unchanged SYS/service/desktop/System32. This narrows compiler/optimization
suspicions but does not prove old ARM CI timeout causality; current793 CI
still must pass all6. Source835/config12/PDB failed-e781 originals independently
verifiedeebedad8, whole acceptancefalse. BCrypt native264616 AA64 original
791feefc is now retained with exact beforeafter metadata for DX10 probe.


### ARM runtime checkout byte correction after793 CI failure

Exact793b7ca9/run37644615059 completes with five successful jobs and ARM
runtime112879776058 failure before any fixture starts. Original API runtime
ZIP11493618960/1509bytes/SHA748400ab3acc46f8a855d3e857dfcdb0c9a2fc7d2329de357809eaf74b28619c
contains only the retained raw runner. Actual original4798bytes/SHA46299450
is exactly Git4697bytes/d8cf with101 LF expanded to CRLF. Script31cf line20
correctly rejects that byte mismatch before runner receipt/Add-Type/loop;
zero fixture child executions, not a new shader timeout. Root byte proof
root-raw-runner-checkout-failure-verified-01.json and independent agent review
join raw archive/log/Git bytes. Exact checkout configuration causing newline
conversion is not retained; no production or timeout-causality claim follows.

The runtime job now reuses unchanged scripts/restore-native-ci-source.py,
checks its actual exit, and copies its original835-row canonical receipt into
candidate/arm64-native-canonical-source.json before fixtures. Existing upload
patterns retain that JSON inside the same artifact root; accepted runtime
shape increases87 to88 original members. Original ARM script31cf/C#d8cf,
28 ordered markers, PE/source/exit0/30s/CWD gates and all compiled sources
remain unchanged. The actual runner image20260924.168.1 official software
manifest includes Python3.13.15; no extra dependency action is introduced.
All5 failed ZIPs/6 job logs are being retained under root-consolidated-arm-raw-01/
failure-originals-02 using450s read-only host downloads; runtime deadlines
are unchanged. Frozen successful collector03/verifier54fc remain unexecuted;
a fresh88-member collector/verifier will bind the next exact source/run.
Hardware core/CI/source/hash/LUID pins remain null. Root owns target CPU;
DX10/11 finish local USER runner packets. Administrator management is allowed,
and graphics acceptance continues in the established limited USER desktop.


### Published canonical ARM runtime checkout correction

Root committed/pushed d7e5c7d46b8ce889e993bfab66a3b78b076c49d1 with
actual commit/push0 and clean worktree at publication. Exact four files are
workflow plus three authoritative plans; code delta is only six runtime-job
lines. Root narrow review and independent be21e650 pin workflow86deaa0d and
unchanged31cf/d8cf/1c7 sources,835 regular Git inputs,28fixtures/30s gates.
Exact raw REST list confirms one push event/run37648387721 for the new source
and sole build-native-umd workflow; no manual dispatch. Current run is pending;
no CI-success/core/hardware admission. Fresh88-member originals tooling binds
this source/run while frozen793 tools are preserved. Failed793 originals
retention continues separately. Root owns target; DX10/11 local USER packets.

### 2026-10-08: successful d7e5 exact CI and native USER runner readiness

Current d7e5/source CI37648387721 completes all6SUCCESS; root collector04 actual0
retains all5 original API ZIPs/six logs/three architecture canonical835/config12/
generated5/private3/CodeView-PDB identities and56 nonARM+28 ARM owned fixtures.
Runtime88 includes exact835-row canonical source receipt. Independent a780 ACTUAL
PID2347144 exit0 in3.146s, empty stderr; prooffcc99f9a and actual-process00e08c82
join all474 unchanged original CI files. Root collectorproof08d3fccd/admission217ee6e9
and fresh ARM64 candidatearchive50224855 authorize original candidate preparation
only; GPU and ordinary admission remain false. One push workflow remains enabled.
DX10 nativePS8 original archivea369745f/111 members: source15/common10, native5/
host7 exit0/drained, pending-readyfalse rejection, d8cf Add-Type and serviceRunning4
PASS; installed SYSd48/411040 and DWM1864/Explorer4464 remained unchanged. Root
original rejoin804357a2 PASS. DX11 native helper03 Parse11/Add-Type2 original
archivec11a6069/39 members/source21: native4/host3 actual0/drained, review1ae91827
and release9ebd27ef return target to root. Neither parser executes core or GPU.
A real ready-path incompatibility exists in frozen DX10 inputs schema versus
template/reader. Agent preserves frozen originals and narrowly corrects that helper
string in a fresh packet, without production changes or a needless new CI push.
Root stages fresh read-only limited USER/session1 identity capture with native30s
child/KMT desktop names/owned adapter/DC cleanup/SYS/service/desktop snapshots.
Runtime03 binding and actual process-local private module-load check are next;
DX8 ordinary runtime preparation continues locally. Hardware pins await originals.

### Current USER identity gate finalized

Fresh native limited USER read-only KMT identity PASS: original19-file archive,
six input bytes/sourcebeforeafter, native child PID9444/handle2312 exit0/drained
in1.069s and native tar collector8736 exit0. Five owned SSH/SCP phases exit0.
Actual USER SID1000/session1/non-elevated/elevation3/integrity8192 selects
LUIDec6b000000000000/source0, three original524-byte UMD-name queries with
zero open/query/close statuses and released desktopDC. Actual SYSd48/411040,
oem17/58624/PnPStarted/serviceRunning and Explorer4464 are unchanged. Limited
USER cannot read DWMStartTime (originalnull retained); separate management-after
original confirms DWM1864 and its prior exact start time unchanged. Root proof
eed7b7ce joins raw originals/selected identity18d461fc. Root owns target; DX11
locally binds all47 roles for the upcoming separately scoped module-load attempt.
Native ScheduledTasks omits optional default RunLevel in exported XML despite
explicit Limited and actual token3. Narrow follow-on offline readers accept that
valid default, reject explicitHighest/empty/duplicate, retain real token guards.
Official Microsoft task schema/security-context docs corroborate optional/default
low privileges; frozen readers and native helpers remain preserved. Root's first
offline comparisons assumed literal XML/default and observable limited-USER DWM
starttime; only review adapters changed, not native inputs or results.

### Runtime03 CPU-load originals fully bound for the first live attempt

Agent binds accepted d7/CI37648387721/artifact11495898065 with actual identity
18d461fc/LUIDec6b/source0 and all47 original roles. Readyfalse preview manifest
9227bc7b preserves unchanged native helpers; source19/config12/PE/PDB/original
archives/genuine unsigned3001 loader/1b0c ICD/5843 z reader PASS. Root invokes
the reader offline using only an ephemeral ready copy, rejoins all47 bytes and
19 raw Git blobs, proof42aa22f6 authorizes a fresh ready CPU manifest. No module
or GPU has run yet. Agent freezes d8cf-owned Prepare/Start/Result/cleanup and
collection wrapper before the separate root exclusive CPU-load handoff.
Narrow offline reader1c9c847e differs from frozen8bead only in optional default
RunLevel handling; wrapper9a912050 imports unchanged501/c239. Original controls
8e87a7ee: full original13125 self-test plus32 targeted checks pass, highest/empty/
duplicate and actual token mismatches still reject; original PT1M task remains
rejected by the compute PT2M deadline. All nativePS/C# inputs remain exact.
DX10 corrected schema/reader fresh preparation binds67/72 actualroles; fresh
160-byte privatecapability reply and runtimeCPUload originals remain pending.
A source-only owned30s read-only capability capture helper is being frozen;
it may not invent generation/WDDM/software/render fields from old data.
DX8 fresh isolatedd7 checkout prepares a separately gated I386 API8 device
forwarding frontend and real SysWOW64 D3D8 hardware workload. Strict I386
frontend local compile passes; actual WOW KMT slot/native build/device/render
readback and presentation are pending. Old readonly API8/CAPS12 remain preserved.

### Native PS5.1 runner failures and compiler recovery evidence

Read-ComputeJson returns a genuine array without pipeline enumeration on target;
use explicit [object[]] casts for source continuity rows. Do not relabel or alter
genuine before/after CI originals to fit a host assumption. CPU06 native preflight
passes exact12rows, so this correction is confirmed on target.
PowerShell variable names are case-insensitive: a local $authorization JSON object
is coerced by typed [string]$Authorization. CPU06 Run fails before core load. Fix
the local name, preserve CLI/security/hash guards; DX10 future hardware runner has
the same issue, while separate private160 query controller avoids it.
Mounted EWDK14.50.35717 libcpmtd.lib matches unchanged compiler-ready pin and owned
copy differs in only one byte/one bit. Both148-member archives/145I386COFFs parse;
the other574 files match575 original pins. Cause unknown. Restore only this file
from verified mounted original and require unchanged575-pin rehash before fresh
build. The existing failure was tool provenance, not a C++ compilation result.
Null-GS stream-output can use original active prior-stage compiled bytecode with
CreateGeometryShaderWithStreamOutput. Legacy DDI source slice caches actual VS
DXBC/rebuilds on prior-stage changes and rejects missing source outputs. Four
COFFs pass; native behavioral execution and nativeSM5 passthrough remain pending.

### CPU07 acceptance and merged SO/volume source, 2026-10-08

CPU07 limited USER private module load now PASS: root direct4740d980 rejoins
all119 original members/archivebc47fc02, actual PID11724/handle2324/exit0/drained
22.85s, six guarded HRESULTs, four owned module loads/reverse releases/absence,
all10 native children finalized and exact task removed. Explicit releasea1b5bc1c
returns target. Original180s SCP-9/50MBpartial preserved; separate complete
original transfer byte-joins native archive but interrupted host exit is unknown.
Zero VK/KMT/D3D/GPU counters describe explicit probe scope, not DllMain instrumentation.
No hardware acceptance or GPU-ready manifest follows from CPU loading.

Root reviewed frozen EWDK single-library recoveryf8ee4420 and authorizesa72350e3
after CPU07release. Actual repair575 PASS: original-readyc633 unchanged,
drifted018f backup and same-version mounted/staged48108e retained/rejoined.
Fresh unchanged33-source DX8 CPU02 native build fails probe line428 C3493
(color lambda capture), after genuine frontend I386 COFF/DLL success. Original
81-file/23582715-byte archive7d2ba781 and all raw failure receipts preserved.
Agent finalizes release then fixes exact compiler issue locally; repair is not repeated.

DX10 private16001 failed actual native PS5.1 DSC parse before Initialize/task/query;
original58-file archive56745a85/review63dd22f0/release4f6f5e94 preserved. Fresh02
Get-Private160Configuration and owned PreInitialize native AST/Add-Type gate are
root-reviewed073258eb:53 local refs/38 payload/48 fullpacket/37 master rows,
only3 changed control helpers/134 actual local checks exit0. Actual native prefix
and4/160/4 queries remain pending; target goes DX10 next after EWDK release.

Null-GS SO production commit21cdb35 now supports actual prior legacyVS and SM5
VS/DS bytecode with atomic native linking, genuine cache rebinding, retirement
owner and strict nonnullS_OK. Final current originalMS x86/x64 eight COFFs PASS
local-strict-coff-05. Fixtures add72-word raw/canary controls and1024-word public
DS comparison; native run pending. Texture3D agentcommit56d44ba adds bounded
10/10.1/11 resource/view/Map/XYZ transfer/mip behavior, local385547 policy and
six originalMS COFFs PASS, native27cases/9138voxels/945pixels still unrun.

Both commits cleanly integrate in isolatedworktree
reference/codes/dxvk-umd-so-volume-integration-20261008 HEADd821fc0; main/current
acceptedCI remain exactd7/all6success, no newpush. Root freezes combined93 rawGit
inputs and4 nativefixtures under artifacts/dxvk-so-volume-integration-20261008/
native-warp-d821fc0-01, descriptorae941b02/sourcearchivec02bd21d. Actual native
AST/build/30s fixture/309volume originals/readback are pending after private query.
Public admission masks remain unchanged. Real384 compute words/dual1792graphics/
ordinaryDX8-DX11 rendering and Present/DWM still required before completion.

### Actual adapter query, combined native failure and next target owner

ROOT private16002 directreviewbfb541eb hashes all134archive files/133collector rows/37sources+master/17native+15host exits and18raw queries/canaries. Genuine LimitedUSER7580/session1/8192 opens/closes ownedadapter/DC, queries4/160/4 pass and report LUIDec6b/source0/gen2/caps0/WDDM2200/rendertrue/softwarefalse. Fullnativeattempt remains failed Result1/Finalize1: liveNative telemetry44management/36limited changed, while SYS/binding/package/registration/service/desktop equal. No originals rewritten, query-only acceptance does not imply hardware.

Actuald821 combinednative packet passes nativePS5.1 AST/Add-Type,15production/dependency units and4ARM64fixture builds. LegacySO1252/SM5nullGS4193/domain1024 andvolume-policy385547 pass actualownedexit0. Texture3D profile0 initial/XYZ/dynamic/W-slice cases pass, but scopedmip readback6/mip3 exactword comparison fails; originalownedexit-1073741819 retained. First17savedrecords independently calculate1875matchingwords; failingword was not saved by originalCHECK-before-write. Agent prepares GPU-only scoped3D normalization and fail-first original retention, expected27/9138/945 unchanged.

All245archive originalsaf6b6e8c and93Git source originals join root00e716dd. All13native/9host children finalized, source/tool/21SDK/7libraries/5system/candidates/SYS/binding/desktop unchanged, no task created. ExactCIvolume additions(twofixtures/309originalretention) parse nativePS5.1 errors0, sourcecopies unchanged; noCItest execution or push. Rootreleased63ee07a toEWDK withfresh03authorization7e8a5187. Corrected171 probe nowcompiles, latercallback CPP strictMSVC fails; originals collecting, no repairreplay/validruntime/GPU. Root/DX10/DX11 target-idle.

DX11 locallybinds actualprivate160/rootproof plusCPU07/currentd7/Mesa47members to readyfalsemanifest52078756; real384words remains nexthardwaregate. GitHub run37648387721 exactd7 all6success andfourduplicateworkflows disabled verifiedlive. OrdinaryDX8-DX11 rendering/Present/DWM acceptance remains pending.

### GPU08 exclusive handoff and reviewed corrected source packets

ROOT ready GPU282df90a changes only readyfalse→true in boundmanifest52078756.
Rootf62421ef independently joins47 actualCPU07 payloadfiles/19rawGit/16genuine
ZIPmembers and5ARM64PEs; CPU07/rootprivate-query scope stays distinct from
actualGPU output. Frozen d312 hostrecipe/native8/helpers are reviewedc3796bb1
and authorized09f12ba9 against latestDX8CPU03releasebd943617/rootreviewe4e9b08c.
Native GPU08 prefix actualexit0 now parses7/helpers47/config12/12 and verifies
source/inputs unchanged; originalnativeowner PID11268/child9828/handle2532/exit0
drains1.172s. Separate LimitedUSER Prepare/Start/Result/Collect remain owned
byDX11; ROOT and peers target-idle.

DX8CPU03 failed actualC2872 ambiguouslog in callback fixture after4I386COFF/
3PE builds;107files/archive500b88c5 and13native+collector finalized, lastSCP255
retained and onlymissing-sidecars recovered by freshactual0. Roote4e9b08c
independently joins originals/closure/failure and SYS/stateunchanged. Exactfive
callbackTrace renamecommit6ae2f61 passesfocusedlocal609originalinputs/I386
compile-link; frozenCPU04 ec0967ba/archive73058ed0 rootprepared6eecd850 joins
33/30Git/3licensed and4unchangednativehelpers; actualCPU04 pending,no repairreplay.
PhasedgenuineI386 names/12CAPS/448offscreen/64screen preparationb21f724 remains
readyfalse pending actualnativeCPU04 outputpins; no targetcalls/installation.

Volume terminalmip proposedcorrection27df15d integrates2489b0f abovefa66393
CI changes; production normalized3D scratchchain and fail-first original
retention add108public observations withoutchanging27/9138/945 or309volume
originals. BothmergedcommonDDI originalMSx86/x64COFFs localstrict02pass0/
zerodiagnostics; rootprepared47f07bfe freezes93Git/newarchiveb1fb20a3/
descriptor68018da4. Actualcorrectednative02 pending afterGPUrelease; no newpush.


## 2026-10-08 actual real GPU08 compute accepted; corrected volume02 owns target

GPU08 originals archive93c1d6f8 (85,340,690 bytes/126files) is fully joined. Root
`review-gpu08-originals-root.py` independently checks all384 rawuints against
workgroup arithmetic and original384 trace lines; payload47 and Git19 match.
Actual limitedUSER/session1 controller11724 owns nativeprobe12144/handle2440,
exit0/drained24.363s. Callback tuple[108,1,1,6,6,5,5,1,24,6,6] is balanced, all
wrong-thread/cookie/error/leak/paging counters0. Native9/host9 finalizedactual0;
task absent. Root actual proof and release review are in
`artifacts/dxvk-native-dx10-dx11-20261007/guest-compute-gpu-d7e5c7d-08/`.
Ordinary-runtime admission and presentation remainfalse. Previous whole-registry
private16002 attempt remainsfailed; accepted rawquery5add/private rootbfb stays
query-only evidence. Fresh native02 descriptor68018da4/source2489b0f is authorized
against GPUrelease2a53b6b4; ROOT preflight running. No driver/registry/VM changes.


ROOT native02 preflight stopped at a PS parser missing newline `)if`; hostupload0/
preflight1 bothfinalized, no script body/source upload/build. Fresh native03 fixes
one LF in preflight, keeps source93/all8 payloads/build/parser/collector/d8cf exact,
and uses new original receipts. NativeARM64 PS5.1 preflight andAST2/Add-Type PASS;
actual strict-MSVC four-fixture build nowrunning. MainCI37648387721 liveall6PASS;
four redundant workflows remain disabled. Targetowner ROOT; peers local-only.


2026-10-08 ROOT actual corrected2489 native03 closed and integrated: archive
4,356,406 SHA d5fc7539/611files,93 rawGitinputs, native12+collector1 andhost7 exit0,
all11retentionpairs equal. FirstpartyW4WXwarnings0; pinneddependencyW3 retains65.
SO1252, typed3733/callback280/nullGS72+domain1024, policy385547, Texture3D12025/
27cases/9138voxels/945sampled; independent production309 wordsPASS, public108
36miprecords mismatchcounts30x0+3x1+3x8. SDKsupplement directraw21headers/2private
libs, all21header+7library receipt valuesmatch; five sharedlibs hashes native.
Main source ff5d77824 bytejoins all firstparty native compiled inputs. Added
volume fixtures to existing single matchingCI, allold28markers/deadlines/d8cf
unchanged. Target release1b1e4942/EWDKauth0129b236; CPU04 actualbuild running,
575 tool originals must remainexact/no repair replay. Ordinary runtime/Present
notaccepted. Consolidated push next; no newcore/probe may inherit d7 acceptance.


2026-10-08 source618ca58aae85c427eb63a4e79ac4a4d43b33043e pushed actual0;
exactly one automaticNativeUMD run37681929181. Identity/shaderCPU currentlyPASS;
threebackend jobs running, no newcandidate admission. Currentoldcore/d7KMT384
remain scopedhistorical. NativeDX8CPU04 actualPASS/root directreview saved:
archive988bdb3e/31,167,409/181files, source33/Git30/licensed3/575toolbeforeafter,
SDK10/libs9,5I386COFF/4PE,329policy/62callbacks+11forwarded/30invalidCLI,
48nativebuild+collector1/host7actual0 allfinalized. Original release1547258b
returns targetROOT; no core/validKMT/factory/GPU. Phasedc2ab manifestcad2 remains
readyfalse, actualprobe9009/front699b joined. FreshnativeAST5/AddType andeach
limitedUSER names→HALenum→448offscreen→64Present runtime phase stillpending.
DX10 USER05 frozeneb5e70/72roles/prefixqueued; DX11 peerpreparesD3D9locked2624
with genuine currentd7 originals. No additionalpush/manualCI/driver/VMchanges.


2026-10-08 continuation: user confirms passwordless Administrator available for
management/builds; GPU checks remain limited USER/session1. All six jobs of
sole automatic618/CI37681929181 now PASS. Fresh ROOT collector06 corrects only
ARM member summary88→511, pins unchanged independent volume reader Git SHA
b0c463995f012c5a9c5cf78cf52b233a8641a8170998fc65f2265f08cca5fb4d,
and starts original collection under host PID2544062/deadline1200s. Frozen05
not modified. DX8 parser-only handoff5319f9c0 is now exclusively owned by EWDK
agent; AST5/Add-Type only, no modules/KMT/GPU, original collector then release.
ROOT independently rejoins DX10 USER05 prepared descriptor dfd16e6a: 20files,
15prefix originals/152frozen archive/151collection/70known72roles/9helpers/
15probeGit+10coreGit; proof root-native-user05-prefix-prepared-direct-review-01
PASS, readyfalse. Names/HAL/offscreen448/Present64, DX10 real1792 and D3D9
real2624 remain pending; no driver or VM changes.


2026-10-08 ROOT CI618 originals admission: actual collector06 PID2544062 exit0,
five genuine originalZIPs/1721members, allsixjobsPASS/source853blobs+5gitlinks,
four exact canonical receipts/three DLL-PDB/config/private-loader chains,
ARM30/x64+x86 60 owned fixture children all0/drained. Independent complete
309production volume files/9138voxels/945sampled +108public files per archPASS.
New ARMcore6262784/a4053e44, x64 5918720/0f68ebcc, x86 5500928/d9536b79; these
are candidate-only and do not inherit d7 hardware acceptance. ROOT direct
review script review-ci618-originals-root.py and originalproofs retained.
DX8 parser actualPASS archive4ae29d77/20files/AST5/AddType; native2/host5 all0,
ROOT directreviewa594fc8c/release6340c17c accepted. DX10 USER05 nativeprefix
actualPASS archive41fb6692/46files/AST8/AddType/ServiceAssembly Running4/
pendingreadyfalse rejection; native7/host8 all0, protectedstatic34/rawSYS/
desktop retained, ROOT directoriginalreview saved, release891d243c accepted.
ROOT names-only admission23d6d374 changes onlyready/native_phase_parse/pending
from frozenDX8cad2; EWDK exclusiveauth995b506a now executes one I386 names
query and collection. No core/factory/GPU/installation permitted. D3D9 frozen
prepareddd53595b independently joins63files/32Git/helperTAR9/inputTAR15 and
acceptedGPU08shared47 exactly; unchanged probe/oracle gives native2624 total/
1152raw lockedpixels and checksums6d390bc5(word)/41e03dc5(byte). ActualD3D9
retry stillpending, readyfalse; target remainsEWDK names-only until release.


2026-10-08 actual I386 names01 FAILED beforeKMT: limited USER/session1 probe
2404/handle2256 exits1/drained0.0447392s, original196-byte stdout contains
USER gate then system-GDI32 guard failure. No adapter/namequery/core/factory.
TAR61c4793c/1961912bytes/38files preserved; ROOT direct failure/continuity joins
all source/input/systemDLL hashes, static34 and desktop; task removed, native2
closed/host7 finalized(result-command1)/outer1, explicitrelease53355f1d accepted.
Microsoft IsWow64Process documents FALSE for32bit apps on64bit ARM; exact
process-machine APIs are required. EWDK peer prepares freshprobe/frontend
IsWow64Process2/GetSystemWow64Directory2 fix and diagnostics locally, without
weakening system-path/I386 guards or changing oldfailure. No enumerate admission.
ROOT integrates fiveexistingDX8 commits into isolated branch
work/dx8-trunk-integration-20261008/head eeebfcf: source29/30 exactlyjoin native
CPU04; onlymeson.build differs by alreadyaccepted newSO/volume registrations.
Mainc903b4c recordsCI618/helperproof locally; no secondpush yet.
D3D9 exclusivehandoff b89230b6/rootreadymanifest508b97c8 authorizes one frozen
d7 typedlockedbuffer gate after release53355f1d. Nativeprefix parse7/config12/
hash63 and Prepare bothactualPASS; current GPU run/collection stillpending.
All ROOT/peer target calls suspended until DX11 explicitrelease. DX10 peer
continues concrete1792 USERpacket71roles, root72 pending.


2026-10-08 ROOT accepted actual D3D9 locked-buffer gate and released target:
raw archive13801813/85936635bytes/143files/199members,63roles/32Git,1152 independently
replayed pixels plus1472 unchanged native asserted pixels; word6d390bc5 and
byte41e03dc5,18stages; callback ownership/residency clean and static34 unchanged.
Actual probe7012/handle2288 exited0/drained2.076s, USERsession1/elevation3/8192.
Ten native and nine host receipts independently joined original streams and
actual exits, task removed, pending0, releaseee3c23b6 accepted. Ordinary runtime
and presentation remain untested; source618 does not inherit d7 GPU proof.
ROOT reviewer source review-d3d9-locked-originals-root.py retains reproducibility.
FrozenDX8CPU05 packet21898/manifest4c155/source5c420e4 directly rejoined33inputs
(30Git+3licensed) and four unchanged acceptedCPU04 helpers. Exclusiveauthc1c1c6f6
now runs one native EWDK build/collection; strict575tool before/after admission,
no repair replay. Fresh names05 remains separate and unexecuted.
ROOT DX10 USER05 review2f406791 independently joins71 original/stagedroles,
25Git/39archive members/80TARfiles and9unchangedhelpers against acceptednative
prefix4c42cd7e. New72-role sealer may prepare the actual1792pixel gate; no
hardware authorization yet. Latest618 local runtime candidate64roles is ready
for fresh CPU prerequisite preparation; newactualcorea405 proof is required.


2026-10-08 native CPU05 process-machine repair verified by ROOT: original181-file
TARc48fbc5d/31169398bytes,source5c420e4/33inputs/30rawGit,exactreadyc633 and575
compiler before/after,10SDK/9libs,5I386COFF/4PE strict W4WX zero warnings,
policy329/callback62+11forwarded/30CLI expected64. Actual48build children plus
collector4412/handle2560 exit0/drained2.071s; sevenhosts/outer exit0/rawfinalized.
ROOT proof042786e0 and separate release review accept release100c118d/pending0.
Newnativeprobe631808/2cef9cfa andfrontend440832/7b4e6b26 are current5c originals;
no validKMT/systemfactory/GPU executed. Isolatedtrunk68f83e6 additionallyintegrates
helper166f identity/trace validation; eight oldphasehelper bytes unchanged,
newnames05 admission stillpending. Target ROOT-idle before DX10 dualhardware
handoff. Live GitHub recheck confirmsCI618 stillsuccess and four redundant
workflowsdisabled; solematching automaticbuild remainsactive.


2026-10-08 trunk pushed once,3d39760 (618→3d39760 actualgitpush0): DX8 seven
native-tested commits merged5b51100 plus27controlfix274f429. DX10 threeprobe
commits integrated363d9cd/09b47d9/3d39760; actualtrunkprobe/oraclethreefiles
exactsource05ff andsharedtransport exacteb3a9d8, portableoracle4141/WerrorPASS.
An offlinejoin used05ff for separatetransport (pathabsent) and failedbefore
writingproof; correctedexplicitprovenance02 passesfourbytejoins. Production
SO/volume/CI618bytes unchanged. New CI on3d is pending; no hardware admission
for newsource. ROOT sealedDX10directreview joins38files/81TAR/72roles; manifest
5d9d198a adds onlyactualrootrole2f406791 to71 unchangedroles. Exclusiveauth
fb1acf22 toDX10d7USER05/10_0+10_1/1792pixels; management USERSSH knownworking,
limited USERsession1tasks unchanged. CPU05release100c ROOTaccepted; allROOT/
EWDK/DX11targetcalls remain suspendeduntil explicitDX10closure/release.

DX10 failed01 managementframe cannot be compared against limitedUSER's
DWMStartTime visibility. ROOT verified actual401bef3d/06e091c6 frames are equal
aparttimestamp, while USER20e6c984 sees sameDWM1864 butnullStartTime. Frozen
worker helpers retainUSER comparison; corrected02 controller separately binds
actualmanagement snapshot inmanifest/auth andchecks82staged originals.
This changes noprotected34/SYS/package/PnP/desktop lifetime guard.


2026-10-08 DX10fresh02 actual hardware acceptance is complete for immutable
source d7e5c7d/coreb0fcbc3a, probe05ffdc2/sharedtransporteb3a9d8, CI37648387721,
LUIDec6b. ROOT directprooffd8347aa verifies231 originalfiles/344TAR members,
1792 literalRGBA pixels (clear/red/Gather4/4xSampleIndex), actual shader words
and native raw KMT retirement. Both tasks/9native/12host owners closed;
protected driver/static34/desktop continuity retained, dynamic telemetry143
changes preserved. Separate management baseline fixed the cross-privilege
DWMStartTime comparison without weakening limitedUSER guards. Local missing
HLSL reader failure required only a fresh byte-identical reader closure; the
native render attempt was not repeated. Ordinary-runtime admission remains
closed and current3d candidate never inherits this d7 evidence.

Typed10.1 uses an inherited legacy CreateResource callback: strict singlecube
ArraySize6 now creates a genuine base10 cube, but blocks cube arrays. Local
10.1 cube SRV documentation explicitly allows First2DArrayFace+6*NumCubes
within Resource.ArraySize. DX10 peer owns a separate typed10.1 resource
boundary; base10strict6 stays. Modern cubeArray SRVs also require a distinct
GenerateMips policy branch, since current mipGenerationStatus recognizes
only singleCube/2D/2DArray; ROOT owns that small branch/new typed11 scope test.


2026-10-08 ROOT accepts names05 failure and readonly DLL release: original
names archive390a9fd7/1961779bytes/38files and native11944/handle2640/exit1/
drained1.823s stdout232 contain only USERidentity and explicit-process-machine-
APIs win32=127. No KMT/machine/directory/factory/core/GPU stage reached. Native
collector11864/0 and sevenhosts closed, taskremoved; ROOTproof9fa594fa accepts
release90252e9d. Subsequent readonly export archive50418477/1462431bytes/16files
joins five frozenhelpers, source/copy/source-after threeDLLs, native12196/0
andtar7700/0/drained, eighthosts closed (two wrapper-only guards retained),
SYS/desktop/static34 unchanged. ROOT boundedPE/APIset reader directly confirms
SysWOW64Kernel32 lacks Directory2W/A, IsWow64Process2 forwards to contract;
KernelBase Directory2W executable20ca80/ordinal834 and IsWow64Process2
executable14a5f0. NativeAPIsetv6/count973 WOW64l1-1-3 routesKernelBase. Direct
proof1cfbc585/2963bytes accepts release772fe285. FirstROOT reader draft assumed
all exports arecode and rejected dataWow64Transition; draft retained and no
proof written, corrected only to requirecode for the requested exact APIs.
Target ROOT-owned idle; EWDK develops narrow same-API resolution under missing
Kernel32 export only, explicit actualI386/pointer4/canonical provider guards,
and fresh nativeCPU source/build packet. No legacy architecture fallback.

Cube slices now committed locally791006c/1f2ffde/7669c3a/b5b863b; ROOT prepares
one combined isolated candidate and all Meson/CI raw readbacks. Typed10.1
resource policy97387 GCC/Clang sanitizer controls and8strictCOFF pass; modern
SRV production/fixture4strictCOFF pass. ROOT CubeARRAYGenMips fixture expects
5cases/450subresources/30690texels/900files, strictx64/x86 production+fixture
PASS and ARM64fixture strictLLVM COFF PASS. NativeMSVC/reference/hardware are
pending; unchangedofficial ARM64intrin.h prefetch incompatibility is retained.
CurrentCI3d37689715890 stillsuccess; liveGitHub confirmsfourredundant workflows
remain disabled and solematchingautomaticbackend active. No newpush/dispatch.


2026-10-08: Combined cube candidate frozen8528d91357255fe8f31138d5438e7313e2367fec
in reference/codes/dxvk-umd-cube-integration-20261008. Four production slices
merged with only Meson insertion conflict (both targets retained). Added modern
SRV target; fivefixtures build/ship/run under unchanged d8cf/30s and independent
four raw oracles on eacharchitecture, ARM64cases35. Resource oracle2e0da92
independently checks180subresources/3540texels/540files; all cube originals total
2086files. Combinedproduction strictactual2623291/x64 and2623292/x86 exit0,
emptydiagnostics/inputsbeforeafterequal; proof5f4e028d/19745bytes. PythonAST/
bashsyntax/35CIregistrations pass; actualnativeMSVC/reference/PS5.1AST pending.
DX10 prepares common160-source/23COFF/8PE packet locally, with oldview/1D/3D
regressions and unchanged immutablevolume readerb0c46399 (public108 observations
retained separately). Frozenold791/1f/7669/b5 packets remain exact. No push.

DX8 provider repairdb0e183 committed/frozen; ROOTdirectpreparedreview6419035c
(2540bytes) joins sourcearchive047375c5/105543bytes/33inputs/30Git+3licensed,
manifestbf1083f1/13614,13pins,886actual localinputscopies/beforeafter/4I386COFF/
2PE/12commands0,16actualsource mockselector cases eachGCC/Clang. Native CPU06
may execute exact Directory2W from alreadyloadedKernelBase only whenKernel32
exporterror127, preserving IsWow64Process2/effectiveI386/pointer4/canonical
providerandowner guards. Originalreadonly release772 andROOTexportproof1cf
accepted. Exclusiveauth5250125b/1840 hands target to EWDK for one fresh native
build/read-only processAPI observation thencollection; expected51nativebuild
children+collector,575tool/SDK11/lib9/provider3 unchanged,32malformedCLI,
policy329/callbackforwarded11. No repair replay/KMT/factory/core/GPU/install/
registry/VM changes. ROOT/DX10/DX11 remote suspended until reviewed release.

ROOT local Microsoft docs/header review establishes separate remaining cube
array RTV/DSV follow-on: D3D11DDIARG_CREATEDEPTHSTENCILVIEW explicitly contains
TexCube and independently preserved read-onlyFlags; RTV/DSV FirstArraySlice/
ArraySize use individualface slices, so validranges may crosscubes. CommonRTV
andlegacyDSV currentlystrictsingle6; modernDSV currently noCUBEbranch. DX11
records isolatedfollow-on, preserving8528. LocalrealKMT SO/volume probe work
continues; expected legitimate error/pending callbacks must be retained and
exactly joined, not reset or suppressed. Default transportbalanced() remains0.

## CPU06 failure originals directly accepted by ROOT

Source db0e183 native MSVC produced five I386 COFF objects and four PE files
with strict W4/WX and no diagnostics. The exact Directory2 resolver reached
KernelBase successfully, Process2 returned I386/AA64, and the readonly
observation then failed at canonical-I386-Kernel32-provider. Original stdout
3642 reports logical System32 names with I386 mapped headers; physical image
identity is not established by those names. Existing guards remain unchanged.

ROOT reader review-dx8-cpu06-failure-root.py directly rejoins original TAR
4a153ecf (31180717 bytes/118 files), Git30+licensed3, compiler575/SDK11/lib9,
postfailure hashes and all provider/SYS/desktop state. Actual16 build children
plus collector and postfailure hash worker (18), and10 host transports closed.
Policy/callback/32CLI were not executed. Explicitrelease831ea2c and ROOT proof
a1445895 (955 bytes) accepted; no KMT/factory/core/GPU or target mutation.
EWDK develops a focused two-object/one-PE mapped-image diagnostic locally.

Live GitHub recheck still shows latest37689715890/3d39760 completed success;
four redundant workflows remain disabled. No new push or CI dispatch.


2026-10-08 current target ownership: EWDK exclusive native mapped-image CPU07.
ROOT prepared proof26c96adb joins15inputs (12Git+3licensed),721actual compiled
inputs,2I386COFF/1PE/emptydiagnostics,3unchanged guard extents and oldfrontend.
Latest combinedcube native01 failed only firstlink LNK1104 uuid.lib after16
AA64COFF; archive253files/d04087fb and releasea5628043 independently accepted
by ROOT775db22e. Freshcube02 keeps source8528 unchanged and adds originalSDK
uuid.lib only; packet remains local pending review. CPU07 authorizes one
readonly observation of4modules/7filehandles and4malformedCLI; no KMT/factory/
core/GPU/repair/install/registry/VM. ROOT/DX10/DX11 remote suspended until
originals and explicit EWDK release reviewed. ROOT cube KMT probe7a47d03 is
committed test-only; local strictx64/x86 and GCC/Clang159292checks/154752bits
PASS, independent reader1positive/7negative PASS. DX10 independently reviews
probe locally. DX11 f96 SO/volume probe nativeCPU packet prepared locally;
separate moderncube RTV/DSV implementation proceeds from frozen8528. No push.
