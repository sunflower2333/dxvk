# FL10_0 fully typed sRGB MSAA Blt

Actual native WARP diagnostic 02 establishes why the old helper returned zeros:
at FL10_0, all 12 whole-resource/whole-subresource, immediate/deferred,
typedUNORM/typedSRGB/typeless MSAA copy alternatives leave their magenta
sentinel unchanged. Native typedSRGB resolve reads `ffbcbcbc`, while the
independently drawn typeless/UNORM resolve reads `ff808080`. The unchanged
production helper reads zero. At FL10_1 these copies and the helper work.

These are actual diagnostic observations, not debug-layer messages or hardware
admission. The original archive and failed diagnostic01 remain immutable under
`artifacts/bgra-msaa-diagnostic-20261008`. Existing extended BGRA fixtures,
literal readers, adapter masks and capability claims are unchanged.

Microsoft [MSAA copy conformance documentation](https://learn.microsoft.com/en-us/windows-hardware/test/hlk/testref/bb8a1e51-8425-46f8-887d-96a62e2f7603)
requires feature level 10.1 for CopyMultisample. Its
[resource typing contract](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro#strong-vs-weak-typing)
requires BIND_PRESENT before an ordinary fully typed resource can expose a
different family view. The [functional spec §5.6.6](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#MultisampleResolve)
requires sRGB resolve arithmetic in linear space. Resolving typedSRGB first
therefore cannot implement the encoded-domain average by later copying bits.

The new private GPU path applies only to actual FL10_0 fully typed RGBA/BGRA
sRGB sources which need UNORM resolve. It requires the native source SRV bind,
one mip, one array element, subresource 0 and the full source extent; unsupported
descriptors return before creating or executing the private command context.
Sample counts 2/4/8/16/32 generate separate SM4.0 programs with declared sample
counts and literal `ld_ms` indices. The current typeless PRESENT path and
FL10_1+ multisample-copy path remain unchanged.

DXVK's backend makes the RGBA/BGRA UNORM/sRGB family images mutable and admits
the exact private UNORM SRV. When that view succeeds, the shader reads encoded
normalized channel values directly. This private backend behavior does not
admit a new public ordinary typed view. If the native renderer rejects that
view with E_INVALIDARG or DXGI_ERROR_UNSUPPORTED, the helper retains the
original SRGB SRV and reconstructs the encoded channels in the shader. Other
failures propagate before command execution.

The reconstruction uses the piecewise sRGB inverse, an explicit white endpoint
and integer 8-bit quantization per sample. It sums integer channel values, rounds
the mean to nearest-even, and normalizes for a UNORM RTV. Alpha remains linear.
The [sRGB conversion tolerance](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#SRGBtoFLOAT)
prevents a universal guarantee that manually reconstructing decoded samples
recovers every raw byte on every renderer. Native WARP all 256-channel controls
are a required target-specific precision check; they do not establish generic
inverse-sRGB bit exactness or hardware support.

All shader/view/intermediate allocations precede command recording. The fresh
deferred context draws the resolve at source extent, releases the scratch RTV
before binding its SRV, then performs the existing rotation/stretch/conversion
pass and final GPU copy. ExecuteCommandList(TRUE) preserves application state
and predication. The production path never maps or reads back pixels on CPU.

The new CPU test interprets the actual rebuilt DXBC for both view interpretations,
all five sample counts and all 256 channel patterns: 10240 channel observations.
It checks container hashes, actual sample indices, declaration counts, instruction
limits, endpoint means and invalid counts. This is CPU semantic evidence.

The new native fixture creates a real FL10_0 WARP device and validates five
shader specializations. It writes all 256 channel values to every physical sample
of typed RGBA29 and typed BGRA91 SRGB 4x 16x16 images through independent point sampling.
Per family four raw
observations reconstruct individual source samples; a fifth records the actual
production helper's encoded mean. Per family three early unsupported controls
(missing SRV
bind, nonzero source subresource, mismatched source extent) preserve the original
RTV binding and an independently cleared magenta destination, observed in a sixth
snapshot. The reader uses fixed writer formulas and integer means, never observed
source pixels as the destination oracle.

Exact native marker:
`Encoded resolve native PASS profiles=1 families=2 shaders=5 samples=4 images=12 pixels=3072 rejections=6 hardware_admission=0`.
The 24 raw files contain 12288 pixel bytes and 384 metadata bytes. Each metadata
record is eight little-endian uint32 values:
`case,16,16,4,sourceFormat,(case<4?readFormat:sourceFormat),observedRowPitch,0xa000`.
Fixed family 0 uses source 29/read 28; family 1 uses source 91/read 87.
Native execution and the existing full BGRA literal reader remain required before
claiming this target passes. No installation, registration or mask clear follows
from local source, CPU or compiler evidence.

The build and ARM64 runtime harnesses now require both new executables and
collect the 24 native raw originals. Counts derived from these exact source
name sets are 74 ARM64 cases and 74 bounded x86/x64 backend fixture children per
architecture (70 call sites, including one five-member cube loop). The canonical
mandatory shipping set contains 61 executables: the first shipping array 59 plus
the separately shipped SRV-range and Texture2D-SRV-remaining fixtures. The full
shipping union contains 84 executables plus the production DLL and its PDB.

Local validation passes four strict SDK COFF compilations (affected DDI and new
native fixture, each x86/x64), 10240 CPU channel observations, and an independently
generated literal-reader dataset. The prior one-family 39-mutation proof remains
immutable; the final two-family reader controls reject 76 meaningful mutations. The first
CPU shader
compile's third-party unused-private-field warning is preserved; the corrected
recipe marks the upstream include as a system header and recompiles only that
TU, reusing ten unchanged successful CPU objects. These are source/compiler/CPU
and synthetic-reader results. Native WARP execution remains pending ROOT's owned
VM build/run; no target calls were made during this local preparation.
