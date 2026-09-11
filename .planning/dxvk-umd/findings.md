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
