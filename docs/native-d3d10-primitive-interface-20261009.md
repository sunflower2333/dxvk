# Controlled D10 GS/PS PrimitiveID interface

The retained ARM64 native attempt from418853f built successfully and completed
model40 without GS. In model40 with GS, both the reconstructed shader and the
original public WARP shader produced7 in the primitive lane at all128 covered
pixels. The existing literal requires44. Equality between the two paths did
not satisfy that literal and is not a successful draw proof.

The old source used an unqualified uint PrimitiveID member in G, while the PS
used V plus a separately declared PrimitiveID argument. Its original GS
signature placed PrimitiveID at o3.x; its PS signature placed it at v2.y.
This observation motivates correcting the controlled fixture interface; it
does not establish a production-only defect or prove the correction works.

Microsoft's [HLSL structure contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-struct)
defines member interpolation modifiers and requires nointerpolation for int
and uint interpolation. The [system-semantic contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)
requires GS to supply the PrimitiveID consumed by PS. The local D10 stage IO
DDI contract separately explains that full signatures describe the upstream
and downstream register layout and declarations describe actual use:
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_stage_io_signatures.md`.

The fixture now uses G for both GS output and PS input, explicitly marks its
integer PrimitiveID constant, and reads input.id+7. GS still writes primitive+37.
The no-GS scene still requires generated ID0 plus7; the GS scene still requires
37 plus7 =44, for both SM4 and SM4.1. After actual FXC compilation, the fixture
also requires scalar uint PrimitiveID signature rows and matching GS-output
and PS-input register/mask before shader creation.

Every old pixel, UINT/SINT, raw NaN-like word, depth, clipping, instance,
FrontCounterClockwise, public equality, rejection and state-preservation
check remains intact. The original reader changes only its retained HLSL
source SHA256 pin; its complete validation body and literal image oracle are
unchanged. Original attempt01 shaders, images and errors remain immutable.

Local x86/x64 strict SDK compilation checks C++ and typed DDI usage only.
It does not compile HLSL, execute FXC, run WARP, or admit hardware. Fresh FXC
register packing and all four native/public scenes require the separate
complete native attempt02, including the original49-file pixel reader.
