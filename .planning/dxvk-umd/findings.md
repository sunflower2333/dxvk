# Findings

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
