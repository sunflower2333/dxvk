# Texture2D SRV remaining ranges

The shared Texture2D SRV conversion previously rejected UINT(-1) in MipLevels
and ArraySize. Microsoft explicitly defines these values as the mip levels
and array slices remaining after MostDetailedMip and FirstArraySlice.
The original local documentation is
reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_tex2d_shaderresourceview.md,
Remarks; the online primary page is
[D3D10DDIARG_TEX2D_SHADERRESOURCEVIEW](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_tex2d_shaderresourceview).

The new resolver checks the first index before subtracting it from the total,
accepts a positive bounded finite count or the all-remaining value, and retains
the output argument on failure. Texture2D conversion publishes the resolved
finite counts. Multisample mip validation uses that resolved count, retaining
the existing requirement of mip0 and one level. The shared path serves the
exact10.0,10.1 and11 view callbacks. Texture1D,Texture3D,cube and target/depth
views are unchanged.

The portable policy fixture compares145579 requests against an independent
64-bit interval oracle, including UINT boundaries and guarded unchanged
failure output:436737 checks. It passed both GCC and Clang with ASan/UBSan.

The new native fixture invokes the three exact WDK tables through their
separate source-linked factories; each owns its live core callback table for
the complete device lifetime. It substitutes the private renderer factory
with an explicitly labelled Microsoft WARP reference, using the existing
private renderer minimum FL11. This does not register a system adapter or
establish hardware/runtime acceptance.

Each profile covers168 views:144 single-sample views and24 multisample views.
All three cover504 normalized native/public descriptors. For single-sample
views, separate original public descriptors retain the requested sentinels,
and two public compute shaders load four corners from each exposed mip/slice.
Their readbacks are checked against an absolute resource-coordinate oracle:
5184 native words and5184 public words. Multisample views compare actual
public descriptors and enforce the original one-mip semantics; they do not
claim sampled multisample-pixel coverage.

Each profile rejects21 malformed requests, requiring exactly one live runtime
error callback per rejection. Private output remains byte-identical even
inside the error callback, and the exact previously bound COM view remains
bound. A later valid view reuses the failed private storage. Destruction checks
all storage and table canaries. Expected actual callback total is63.

The fixture retains1380 original HLSL/DXBC/metadata/native-word/public-word
files. The separate Python reader reconstructs every requested range and
corner value without using the production resolver, joins the complete file
set, and checks the original cs_5_0 container headers. Its21 local mutation
controls passed; those control files are synthetic parser tests and do not
represent native execution.

Local official-SDK optimized O2 COFF checks passed with empty diagnostics for
the production DDI, typed fixture and portable fixture, on x64 and x86.
Native MSVC compilation, public WARP execution and original readback acceptance
remain pending. Production capability/admission bits and exports are unchanged.

## Native reference recipe

Meson targets are dxvk-umd-srv-range-test and
dxvk-umd-tex2d-srv-remaining-test. The latter uses the existing native entry
dependencies plus d3dcompiler.lib: umd_ddi.cpp,umd_shader.cpp,umd_shader11.cpp,
umd_runtime_query.cpp,umd_allocation.cpp,umd_runtime_gpu.cpp and the pinned
DXBC/util dependency objects. Use actual release static-library dependency
closure, including UUID/RuntimeObject/AdvApi32/Synchronization where requested
by the original selected archives. First-party units require /W4 /WX /MT
/std:c++17 /EHsc /Zc:preprocessor and separate compile/link commands.

Run the typed fixture in a fresh owned output directory with native bounded
process ownership and retain all1380 generated files plus original process,
compiler,source,SDK and library receipts. The expected stdout matcher is:

~~~text
^native D3D10/D3D10\.1/D3D11 Texture2D SRV remaining ranges verified checks=[0-9]+ views=504 words=5184 callbacks=63 WARP controls$
~~~

Then independently run:

~~~sh
python3 tests/verify-tex2d-srv-remaining-originals.py --directory ORIGINAL_OUTPUT --output FRESH_REVIEW.json
~~~

The reader's word scope complements actual compiler/process/source joins; it
cannot establish those joins or authorize target execution itself. ROOT owns
native sequencing and CI hooks. Existing fixture definitions and original
packets are retained unchanged.
