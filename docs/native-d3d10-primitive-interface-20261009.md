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

The fresh f639b66 ARM64 attempt02 confirms that the shared scalar interface
still compiles with GS PrimitiveID at o3.x and PS PrimitiveID at v2.y. It
completed model40 without GS, then stopped at the added matching check before
creating or drawing the GS scene. No GS1 pixel result exists for attempt02.

The follow-up keeps the shared interface and matching check, but reserves the
entire ordinary DATA0 row in both V and G using nointerpolation uint4. VS
explicitly broadcasts the original instance value to all four lanes; GS copies
that vector; PS consumes only the original .x lane. This follows the
[HLSL packing contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules):
four-component vector boundaries constrain shader IO packing. The intended
result is a separate PrimitiveID row in both stages. Actual new FXC signatures,
the retained matching check and full pixel oracle must confirm that result.

Native attempt03 now passes on ARM64 and x64 under the target Windows VM.
Actual FXC SM4/4.1 signatures put GS output and PS input PrimitiveID at
register3/mask1, while DATA0 occupies register2/mask15. Each full fixture passes
2894 checks. The unchanged 49-file reader runs once per architecture and passes
18494 checks, including all 18432 literal observations across four scenes,
24 frames and 12 FXC programs. GS0 remains7 and GS1 remains44. ARM64 and x64
reader originals are retained in the workspace artifact packet
`artifacts/d3d10-system-shader-native-20261009/84e19a4-03/`.

This is controlled native DDI/public WARP verification. The separate local
strict SDK compilation establishes C++ and typed DDI usage; neither scope
admits ordinary hardware rendering or default deployment. Failed attempts01
and02 remain immutable with their distinct failure prefixes.
