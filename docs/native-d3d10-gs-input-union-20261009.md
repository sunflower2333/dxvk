# D3D10 GS input union correction

CI68 source 90ff834/run 37820635406 passed the model40 non-GS scene, then both
x86/x64 native shader fixtures reported E_INVALIDARG at the next expected DDI
success. The retained FXC01/04 VS, FXC02/05 GS and FXC03/06 PS are respectively
byte-identical. Shader creation therefore already worked in the completed
scene. Source and actual token replay attribute the next rejection to
DrawInstanced's VS-to-GS signature link, before backend drawing.

The original GS declares `v[3][1].xyz` and `.w` with ordinary dcl_input. Its
original ISGN and runtime union identify those same masks as ClipDistance and
CullDistance. The old reconstruction kept them ordinary/Uint32, so the linker
could not match the upstream VS's typed Float32 system outputs. Reconstructing
those names from the actual union resolves that mismatch without changing the
shader instructions or the native rendering oracle.

The legacy GS creation and legacy GS/SO paths now validate the union and apply
system names only to actually declared components. Clip/cull become Float32;
disjoint ordinary varyings retain their register, masks and raw Uint32 type.
Unused union rows stay unused. Repeated same-register/system rows merge their
disjoint masks into one semantic. Duplicate/overlapping/conflicting masks,
missing coverage, incompatible explicit system declarations or types,
unsupported stages/versions, and malformed array/dedicated declarations reject
before committing the staged input vector. Existing clip/cull ordinals and
all profile/raster position checks still apply after annotation.

The raw declaration validation checks canonical immediate array indices,
actual primitive vertex count and canonical dedicated PrimitiveID. It rejects
malformed shapes before parser access; it does not allow union metadata to
repair invalid shader operands. Annotation is confined to legacy Geometry:
ordinary D11 and non-GS creation behavior is unchanged.

Local Microsoft contracts read:

- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_stage_io_signatures.md`
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_signature_entry.md`

The stage signature is a full union, potentially a superset of actual shader
use; each entry carries its system name, register and xyzw mask. These fields
are needed by this backend's reconstructed signature even though an implementation
that does not reorder or reconstruct registers may ignore the full union.

Focused verification in fresh diagnosis03:

- GCC O2 and Clang ASan/UBSan each pass 2302 portable profile checks, including
  the exact 144-word GS, all 81 ordinary/clip/cull component partitions, unused
  and repeated union rows, malformed operands, atomic rejection and legacy
  stage/version/type limits. The two shipping marker checks now require 2302.
- Both hosts replay all six genuine retained FXC containers. They reproduce
  two old VS-to-GS rejections and accept four corrected VS/GS/PS links. All six
  reconstructed code chunks remain byte-exact to the original SHDR instructions.
- Four strict official-SDK x86/x64 UMD/profile compilations pass with empty
  stdout/stderr, and LLVM reopens their four correct-machine COFF objects.
  Twenty current compiler/link/run/reopen children have retained closed originals.
  The unchanged portable decoder objects were reused only after exact source,
  actual dependency and compiler byte comparison; no redundant rebuild was needed.

Diagnosis01's cast compile failure and diagnosis02's expected malformed-parser
diagnostics are preserved. The final canonical precheck resolves those local
issues; final controls have empty stderr. No native fixture, HLSL, literal pixel
oracle, native error expectation, raw reader, capability mask or admission gate
was changed. Complete native four-scene WARP execution, real Vulkan/KMT ordinary
runtime execution and production/default replacement are still pending.
