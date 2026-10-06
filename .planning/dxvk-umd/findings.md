# Findings

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
