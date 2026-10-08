# Scoped cube mip generation

The original native ARM64 reference attempt from `8528d91` reached real WARP
and passed cube initialization, updates and face access. Its nonzero-base cube
SRV selected mip 1 through mip 4, but the final 1×1 mip retained the initial
word `ff040001` instead of the required uniform `ff000000`. The saved metadata
and words independently match 4,228 preceding texels; only that final word
differs. The failed attempt, crash exit and all original outputs are retained
under `artifacts/native-cube-integration-20261008/guest-native-8528d91-03`.

Microsoft specifies that
[GenerateMips](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-generatemips)
uses the largest selected mip to generate recursively through the smallest
selected mip. The
[D3D10 DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_genmips)
operates on the supplied shader-resource view and allows an error callback for
invalid flags or mip type. This fixture's interval and flags satisfy the
existing validation; the observed output does not satisfy the mip oracle.

`generateTexture2DViewMips` creates a typed single-slice GPU texture whose mip
zero has the selected source mip's dimensions. It copies each selected face or
array slice into that texture, generates the normalized chain and copies only
the generated levels back. The source mip, excluded tail and other slices keep
their original bytes. Both scratch owners are allocated before commands are
recorded. Ordered GPU copies and backend command retention hold asynchronous
ownership; the production path performs no CPU readback or global idle.

The helper uses the existing 2D/cube range and format admission. Cube-array
face multiplication follows validated cube bounds, and slice/subresource
arithmetic is bounded by the official texture-array and mip limits. A one-mip
view returns without generation. The existing 1D and 3D helper bodies and
their oracles remain byte-identical to `8528d91`.

The original cube oracle remains 7 readbacks, 168 face/mip subresources,
10,380 texels and 336 raw files. Before its mip scenarios, separate direct
public API observations retain all six faces and five levels for the
remaining-chain, two-level and one-level views. Their 90 subresources comprise
270 metadata/actual/expected files and three view records named
`public-cube-direct-*`. The public mismatch counts describe backend behavior;
they do not replace or weaken the production oracle.

Read the direct public observations independently with:

```text
python scripts/verify-native-cube-public-mips-originals.py <originaldir> --stdout <rawstdout> --output <freshproof>
```

That reader checks all 6,138 public words against literal face/mip/index
arithmetic and verifies saved expected words, view ranges, dimensions, file
closure and printed mismatch counts. It reports mismatches in the source,
generated interval and excluded levels separately. Synthetic reader controls
cover both matching and missing-terminal observations, wrong dimensions,
joint actual/expected tampering, wrong stdout counts and an unlisted file.

Local optimized x64/x86 COFF controls use original Microsoft SDK/WDK headers,
`-Wall -Wextra -Werror` and the unchanged pinned dependency. They compile the
common DDI plus the cube and cube-array mip fixtures. Actual native MSVC and
WARP validation of this correction remain pending a separately authorized
combined attempt. Hardware and ordinary runtime admission remain closed.
