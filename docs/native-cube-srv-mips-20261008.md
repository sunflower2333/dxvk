The typed D3D10.1/D3D11 cube SRV path now accepts `MipLevels = UINT(-1)` and
selects the resource mip chain starting at `MostDetailedMip`. Previously its
finite-range check rejected that documented value. The conversion validates
the starting mip before subtraction and validates the face/count range in
cube units before publishing a complete public descriptor. `NumCubes` remains
a finite count; failed conversion leaves the caller's descriptor unchanged.

The local Microsoft DDI source documents the remaining-mip meaning in
[D3D10DDIARG_TEXCUBE_SHADERRESOURCEVIEW](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_texcube_shaderresourceview).
[D3D10_1DDIARG_TEXCUBE_SHADERRESOURCEVIEW](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10_1ddiarg_texcube_shaderresourceview)
defines `First2DArrayFace + 6 * NumCubes <= Resource.ArraySize`, and
[D3D11DDIARG_CREATESHADERRESOURCEVIEW](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11ddiarg_createshaderresourceview)
uses that exact newer cube structure. The installed original WDK header
10.0.26100.0 supplies the actual native tables and structures in this fixture.

`tests/umd-cube-srv-mips.cpp` links the production DDI into an explicitly named
WARP reference fixture, using separate typed D3D10.1 and D3D11 function tables
and live runtime callback owners. It creates one cube with four mips through
10.1 and three cubes with five mips through 11. All 102 view cases compare the
production result with an independently constructed original public runtime
view and an absolute resource face/mip oracle. The original FXC compute shader
samples every selected face/mip with point filtering. Each path checks 2,244
words plus the untouched readback-buffer tail; 310 original HLSL/DXBC/metadata
and native/public word files are retained for independent byte inspection.

The fixture also checks 36 failed creations across both interfaces: out-of-range
starting mips, zero/oversized mip counts, misaligned/out-of-range first faces,
zero/oversized/overflowing finite cube counts, incompatible format, null
descriptor, plain 2D resource, and missing SRV binding. Every error must reach
the caller through the updated live callback. The callback observes unchanged
poisoned private storage and the exact previously bound backend view; the same
storage remains usable for a later valid creation without destroying a failed
object. Private and native table guards remain intact through cleanup.

`tests/verify-cube-srv-mips-originals.py DIRECTORY --stdout STDOUT_RAW` reads the
original files without modifying them and recomputes all expected float bytes
from absolute face and mip coordinates. It requires the complete 310-file set
and the actual 102/2,244/36 stdout marker. It does not claim physical hardware
execution from these WARP observations.

Local original-SDK optimized Clang MSVC-target compilation passed for x64 and
x86 production `umd_ddi.cpp` and the new fixture, with strict `-Wall -Wextra
-Werror` and empty diagnostics. The first retained fixture compiler attempt
exposed the C++17 anonymous-union constant-evaluation restriction in a new
test assertion; the field-layout assertion stays at compile time and the
descriptor conversion control runs in the native fixture. Actual strict MSVC
`/W4 /WX /O1 /MT` compilation, public WARP execution, and independent original
word verification are pending the separately frozen native packet and root's
exclusive target handoff.

This slice is based on the separate `791006c` base10 cube resource prerequisite.
The 10.1 resource callback on that baseline still accepts one six-face cube;
the separate typed10.1 cube-array resource work is owned by the DX10 port. This
change does not modify resource creation, public capability admission, exports,
registration, or the accepted immutable d7/618/3d hardware/core evidence.
