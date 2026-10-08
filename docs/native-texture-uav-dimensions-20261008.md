# Native D3D11 texture UAV dimensions

The typed D3D11 view creator previously translated every Texture1D/Texture2D
UAV to an array view, including a resource whose actual `ArraySize` is one.
The embedded DXVK backend uses this descriptor to select Vulkan 1D/2D versus
array image views. The resource shape must therefore survive the native DDI
translation. Selecting a single slice of a three-slice resource still requires
an array view.

The typed creator passes that actual resource descriptor to
`textureUnorderedView11Desc`, which stages the single or array descriptor. It checks finite
slice bounds, the selected mip, UAV binding, and nonmultisampled Texture2D
shape before publishing the descriptor. Failed native creation keeps private
storage and existing bindings intact. Buffer element units, Texture3D views,
callback signatures and public admission gates are unchanged.

The original WDK [Texture1D UAV fields](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11ddiarg_tex1d_unorderedaccessview)
and [Texture2D UAV fields](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11ddiarg_tex2d_unorderedaccessview)
describe the mip, first slice and slice count. The original local copies are
under `reference/codes/windows-driver-docs-ddi`. The embedded
`src/d3d11/d3d11_view_uav.cpp` implements separate Vulkan view types and already
chooses the single dimension when deriving its default descriptor from a
resource with `ArraySize == 1`. Microsoft's [D3D11On12 view conversion](https://github.com/microsoft/D3D11On12/blob/master/src/view.cpp)
also selects single versus array from the resource's array size. Its buffer
conversion preserves `FirstElement`; conflicting buffer-DDI wording is not
used to change the existing buffer path.

`dxvk-umd-uav-texture-policy-test` compares the finite range decision with an
independent 64-bit endpoint oracle, including UINT32 overflow boundaries.
GCC and Clang ASan/UBSan each passed 49,631 checks locally.

`dxvk-umd-texture-uav-test` is a source-linked native D3D11 fixture whose factory
substitutes Microsoft's WARP backend. It creates 72 views across 1D/2D,
R32_UINT/R32_FLOAT, actual array sizes one/three, three mips, and single/multiple
slice ranges. Eight original FXC compute shaders separately address the four
texture dimensions and two scalar types. The fixture compares reconstructed
typed-DDI compute writes and typed clears with original public shaders and
public views. It reads every subresource, including untouched slices and mips.
Its 10,752 expected words produce 21,504 native/public observations. Eleven
malformed creations per resource profile preserve poisoned private storage,
canaries and the exact bound view; the 88 error callbacks also prove a live
core-callback replacement. Descriptor-only controls separately reject invalid
sample counts, missing bindings, mips and array shape without changing
the output.

The fixture writes 384 raw originals: 72 metadata records containing the
actual native/public `GetDesc` dimensions, four readbacks per view, and
HLSL/DXBC/SHEX for all eight shaders. `verify-texture-uav-originals.py` independently
enumerates absolute coordinates, checks untouched data and typed values, and
joins each original SHEX to its original DXBC. Local reader controls use
explicitly synthetic data and reject twelve mutations; they are not native
execution evidence.

Strict optimized local x64/x86 compilation uses the original Microsoft SDK/WDK
headers and retains COFFs, raw diagnostics, exact compiler argv and source
hashes. Native MSVC/WARP execution is pending. The future native marker is:

```text
D3D11 texture UAVs verified checks=<actual> views=72 words=10752 callbacks=88 original_files=384 WARP controls hardware_admission=0
```

For a source-linked native build, use the existing D3D11 fixture inputs:
`umd_ddi.cpp`, `umd_shader.cpp`, `umd_shader11.cpp`, `umd_runtime_query.cpp`,
`umd_allocation.cpp`, `umd_runtime_gpu.cpp`, the pinned dxbc-spirv dependency,
and `tests/umd-texture-uav.cpp`. Link `d3d11` and `d3dcompiler` plus the existing
verified static CRT/default-library closure. The portable policy target has
no Windows library dependency. Preserve actual source/tool/library/process
provenance separately from the raw-output reader's word oracle. No hardware,
normal runtime, presentation, registration or complete-profile acceptance is
claimed by this fixture.
