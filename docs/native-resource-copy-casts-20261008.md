# Native immediate resource bit copies

The previous D3D10.1/D3D11 `ResourceConvert` and `ResourceConvertRegion`
handlers rejected different format values before issuing any backend copy.
`ResourceCopyRegion` did likewise. The Microsoft copy contract permits
reinterpreting bytes within one DXGI typeless group; UINT/FLOAT, UNORM/SRGB
and typed/typeless formats do not require a rendering or numeric conversion.

The shared native handlers now accept those format casts. An explicit format
family classifier keeps equal-size, unrelated groups separate: RGBA, BGRA,
BGRX, R32 and R16G16 cannot be mixed merely because each texel has four bytes.
The D3D10.1 exception for `R32_UINT/SINT` and `R9G9B9E5_SHAREDEXP` is
bidirectional; R32 FLOAT and TYPELESS are excluded. XR bias is classified in
its documented R10G10B10A2 group, but this classifier does not advertise or
prove backend XR resource support.

Before the void backend `CopyResource`, the UMD checks real resource type,
width/height/depth, mip and array counts, sample count, multisample quality,
destination writability and format compatibility. It preserves shared-source
refresh and destination publication only after validation. A region retains
index, dimension, immutable-destination, source-box and overflow-safe destination
extent checks. Reversed as well as equal source-box axes now produce the
required empty no-op before geometry or shared-state updates, with ownership
validation retained. The typed Convert slots reuse these real copy handlers.

`tests/umd-resource-copy-cast.cpp` links the production DDI with a fixture-only
Microsoft WARP device factory. Exact D3D10.1 and D3D11 tables each exercise
whole Copy/Convert and offset CopyRegion/ConvertRegion, independent native and
public API resources, 1D/2D array mips, 3D volume mips, and unstructured buffers.
Source patterns include ordinary float bits, signed zero, a NaN payload and a
denorm bit. Additional 2D controls cover BGRA/BGRX sRGB casts. Native R9
execution cases cover UINT→R9 and R9→SINT; the portable policy covers all four
directed pairs. Untouched destination regions and source bytes are checked.
Ninety-two invalid calls require one caller-thread error and unchanged bytes;
six empty-axis cases per profile/resource kind preserve destination data.

The planned native run emits 320 observations, 640 raw files, and 134,624
bytes per native/public side. `verify-resource-copy-cast-originals.py`
reconstructs every byte from fixed format IDs, geometry and source/destination
patterns. It checks exact manifest metadata and rejects missing, extra,
renamed, symlinked or corrupted outputs. Individual rejected calls and source
preservation also require the actual producer stdout/exit/closure evidence;
the external reader retains final rejection sentinels per kind/profile.

Local optimized official SDK/WDK x64/x86 compilation passed for production,
native fixture and format policy (six COFF objects), with empty diagnostics
and actual LLVM machine/directive reopens. GCC and Clang ASan/UBSan runs each
passed 74,036 checks across 36,864 format pairs. Reader-only synthetic controls
passed one 640-file positive and twelve deliberate mutations. These are local
source/compile/reader results, not Windows execution or phone GPU proof.
The original first fixture compile errors and first Clang policy-link error
are preserved in artifacts alongside their successful corrections.

Meson targets are `dxvk-umd-copy-format-test` and
`dxvk-umd-resource-copy-cast-test`. Run the native fixture in a fresh directory,
then run `python tests/verify-resource-copy-cast-originals.py --directory DIR
--output RESULT.json`; keep all raw files and the producer process originals.
ROOT coordinates CI/hardware and owns all target calls.

Production admission, feature/version discovery, and the missing-requirements
mask are unchanged. Typed↔BC geometry conversion, BC/depth/MSAA region
contracts, mapped-state tracking, structured-buffer stride/alignment validation,
and legacy D3D10.0 cube/2D metadata distinctions remain separate gaps. Ordinary
system-runtime rendering, Present and desktop replacement remain unproven.

References are the workspace's local Microsoft DDI documentation:
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/`
`nc-d3d10umddi-pfnd3d10ddi_resourcecopy.md` and
`nc-d3d10umddi-pfnd3d10ddi_resourcecopyregion.md`, plus Microsoft's
[format-conversion table](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#format-conversion-using-direct3d-101)
and [XR casting contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/casting-ability-of-xr-formats).
