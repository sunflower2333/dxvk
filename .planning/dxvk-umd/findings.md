# Findings

- Native ResourceIsStagingBusy queries the whole resource and is legal while
  subresources remain mapped. Implement through embedded DXVK CS sequence and
  GPU access tracking, never Map again or return a constant. Pending buffers
  count busy without requiring submission. Untracked sequence sentinel must
  not be compared as a real sequence. Existing Vulkan barriers own actual
  read/write transitions; hazard DDIs validate native ownership without GPU
  idle. SRV hazard parameter order is device, resource, view.
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
