# Findings

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
