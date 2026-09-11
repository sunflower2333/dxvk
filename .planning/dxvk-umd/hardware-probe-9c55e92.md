# ARM64 generic varying and immediate-constant shader probe

Source: `9c55e9274338f04e500fe45df803940611585575`.
CI: https://github.com/sunflower2333/dxvk/actions/runs/34603595256 — all five jobs passed.
ARM64 artifact10265677966,3599846bytes, archive SHA256
`fea5d12ecb19e7fd5b88c038077ee12b4ca9866fa86f0a4f6496a380841159ef`.
Local directory: `.planning/dxvk-umd/probe-9c55e92-arm64/` in this checkout.

| File | SHA256 |
| --- | --- |
| viogpudxvk.dll | 9412f2e4b7d4f1796defad253629f7a5464d3f12890a3599649a6b9b5fe74353 |
| dxvk-umd-backend-probe.exe | 6d7bc40c002023e565ecbcbcec0179ae24d72f7c9dcb8f03085000c7994ac831 |
| dxvk-umd-ddi-probe.exe | 585164810cce6ecb6f0fbf429390bb6d13d25e44db9cd19706c7737afe21054b |

Copy the matched three files into an isolated folder. The new DDI run is:

```text
dxvk-umd-ddi-probe.exe <current OS adapter LUID as 16 memory-order hex digits> --native-copy
```

Parent's previous OS LUID was`2A58000000000000`; refresh it if Windows restarts.
Use the existing process-local Turnip ICD with the same identity. No driver
registration or installed Mesa replacement is required. Parent owns execution.
The unchanged backend-only clear test need not be repeated merely for this
candidate; the new DDI workload exercises the changed production shader path.

The linked VS/PS now exchanges interpolated UVs and flat integer/raw float
payloads. The PS checks interpolated UVs against SV_Position and verifies
NaN, negative-zero and signed-infinity bit patterns exactly before sampling
the texture and applying the constant-buffer color. The VS obtains its integer
pattern from a dynamically indexed immediate constant array. A linkage or bit
preservation error emits green, violating the final red-pixel requirement.
The DDI inputs contain only native registers/system values/masks reconstructed
from Microsoft reflection; semantic strings/types are not passed privately.

Require exit0;64-byte buffer copy with0mismatches;depth/stencil occlusion
0/4096/0;4096final red pixels with0mismatches;event1/error0;and actual
KMT_ALLOCATION_PUBLICATION4096pixels with0mismatches. Retain parent desktop
and host fault/retirement checks. This proves the new standalone DDI/backing
path only. Native Microsoft-runtime activation, sharing and visible Present
remain acceptance gates.

Production source matches87697ea;61bc32b/9c55e92 fix only the fast CI link
filename and runtime. Do not substitute rejected8f7bce4, whose final-SPIR-V
test discovered the input read-use-mask bug. Latest CI checks the generated
SPIR-V types, vector widths, locations and Flat decorations independently.
