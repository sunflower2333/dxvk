# Findings

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
