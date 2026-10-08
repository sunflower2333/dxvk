# Findings

Prior-sourceaudit: DXVK D3D11 SetPredication stores a query/value but never
gates GPU commands. Modern UMD currently implements CPU suppression after
query resolution; backend/public-context behavior is still incomplete.

Do not implement graphics-only Vulkan conditional rendering and call it
complete D3D predication: Copy/Update/Resolve/compute and inverted/hint/query
semantics must be audited separately. Preserve ordinary-runtime/hardware gates.

Current dxvk_device_info.{h,cpp} has no conditional-rendering feature/extension
entry or enable/query chain; only Vulkan loader function declarations exist.
DxvkQuery::getData aggregates multiple Vk64 pool slots on CPU, and D3D11Query
GetData computes occlusion/SO-overflow booleans there. No GPU32 per-generation
predicate reduction/storage exists. Exact target hardware extension support is
unverified because this task makes no target calls.

Official Vulkan EXT conditional rendering coversdraw/dispatch/attachmentclear,
explicitly excludes copies/blits. CopyBuffer and UpdateBuffer are unaffected.
Simply wrapping graphics commands would break public D3D Copy/Update/Resolve
predication. A correct backend requires feature enablement, query-slot GPU
reduction+reuse lifetime, invertedboolean generation, secondary/renderpass
inheritance/reset/restore, and conditional transfer shader/fallback semantics.
No partial extension flag will be advertised as complete support here.

Continue authorized bounded internal nonshared rotation and Present readback
correctness using fresh private command lists and existing owner live checks.

The bounded internal production change is confined to a new private-transfer
helper and the existing nonshared rotation / Present resolve-readback callers.
Fresh lists begin with default predicate/bindings and ExecuteCommandList(TRUE)
restores the application state. Maps remain immediate synchronized completion
barriers. All preflight, allocation publication, resource reservations, nested
Present/rotation exclusion and retirement tails remain active.

New native fixture phase: 24 snapshots, 840 pixels, 48 raw files. Its independent
reader checks all four bytes and owned backing guards, rejects extra/missing/
linked originals and changed markers, and leaves Texture2D DepthPitch as an
observation rather than inventing an API requirement. Existing profile and
identity bodies are byte exact; legacy reader AST and identity reader bytes
are unchanged. The existing Blt reader entry requires all three phases.

Original local review pid 3224775 exit 0 reopened all eight optimized compiles
and eight LLVM COFF processes. Source bytes remained identical across compile;
selected dependency pins were rejoined after the actual LLVM reads. Synthetic
reader control pid 3225873 exit 0 rejected 56 mutations and rejoined all three
phase counts. No native GPU execution or target-capability inference occurred.
