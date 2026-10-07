# Native cube-array scoped mip generation

Modern cube-array shader views already reach the typed D3D11 table, but the
common mip-generation validator rejects `TEXTURECUBEARRAY`. Accept a valid
cube-array view only when the resource has automatic mips, render-target and
shader-resource bindings, a square single-sample cube shape, complete six-face
groups, and valid selected cube/mip ranges. Validate cube counts by division
before any face-count multiplication.

The source-linked `umd-cube-array-mips.cpp` fixture enters the actual typed
D3D11 production table with an explicit Microsoft WARP reference factory.
Each case creates three cubes and five mip levels. Every subresource starts
with a distinct face/mip/coordinate sentinel except each face's uniform mip1.
Cases select the middle cube's three levels, all cubes' four levels, the last
cube's two levels, and two cubes' single level. A fifth case rejects generation
on a resource without automatic mips and verifies complete unchanged images.

Actual staging copy/map readbacks save 450 word files and 450 metadata files,
covering all 30,690 texels, before checking the result. The independent Python
oracle derives each word from the face, mip, coordinate and selected scope;
it reads no fixture-produced expected buffer.

```
python3 scripts/verify-native-cube-array-mips-originals.py <native-output-directory> --output <fresh-proof.json>
```

Native execution is pending. Compile-only checks and Microsoft WARP references
do not establish real viogpu rendering or ordinary runtime activation. Public
`CompleteResources` and other incomplete contract bits remain closed. The
immutable prerequisite is source `791006c17a83cfe279138d078b2f3df26d034f4b`;
its single-cube fixture and native packet are unchanged.
