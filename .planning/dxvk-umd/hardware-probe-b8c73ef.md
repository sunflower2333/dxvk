# Corrected ARM64 native shader linkage probe

Source: `b8c73ef944ee5f73aae18b2163dc44efe9851044`.
CI: https://github.com/sunflower2333/dxvk/actions/runs/34606380427.
ARM64 artifact10266488436,3601008bytes, archive SHA256
`62f866c144d799e996e63c004c6262aa521b8f658baa6ab0b7ef2f1719a8da98`.
All five CI jobs passed, including ARM64, x64 and x86.
The independent Microsoft WARP job passed887checks.

Copy only these matched files from `.planning/dxvk-umd/probe-b8c73ef-arm64/`:

| File | SHA256 |
| --- | --- |
| viogpudxvk.dll | f0b61c668d306affd423606ced67b0ebe91c51f86840ed9e7e1fc42f9d2c1934 |
| dxvk-umd-ddi-probe.exe | f61cbcb031913f964f87ae5909a20bf1df6398d26fb86ff29b5ef9e291cd436b |

Run `dxvk-umd-ddi-probe.exe <fresh 16-hex memory-order LUID> --native-copy`
with the existing process-local Turnip ICD. Parent owns remote execution.
No installed UMD replacement or unchanged backend-only rerun is required.

The previous9c55e92 failure was diagnosed independently using Microsoft's
WARP implementation. FXC folded the shader's expected negative-zero constant
to positive zero even in the original bytecode. The new shader obtains its
negative-zero bits from runtime constant-buffer data and binds that buffer
to both VS and PS. Production shader bridge code remains87697ea. The strict
NaN, negative-zero, infinity, integer, interpolated UV and immediate-array
acceptance checks remain enabled.

Original and reconstructed shaders on WARP both produce these exact words:

```text
80000000 fedcba98 ffffffff 00000001
7fc01234 80000000 7f800000 ff800000
12345678 87654321 00000000 00000001
3e480000 3c000000 41680000 3f000000
```

Require exit0, buffer64bytes/mismatches0, depth-stencil0/4096/0,
final4096redpixels/mismatches0, event1/error0 and KMT publication4096pixels
with0mismatches. A failed image now reports DDI_FIRST_PIXEL and per-category
DDI_LINKAGE_FAILURES, then one bounded diagnostic draw returns the sixteen
actual payload words. Diagnostics cannot turn failed acceptance into PASS.
Native runtime activation and visible Present remain unproven.

Parent resultseven-dxvkb8-ddi-03: exit0/249ms; buffer64bytes, depth/stencil,
4096final pixels and KMT publication allPASS. Exact files, OS LUID581E,
process-local ICD9AA5, KMD58386 and paired hostVirgl28a. Original DWM2088/
Explorer5820 remained live. The host trace began one second after this DXVK
process completed, so this checkpoint has no correlated GPU-trace evidence.
