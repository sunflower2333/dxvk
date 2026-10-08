# Native depth and private-transfer CI integration

The harness builds, ships and runs `dxvk-umd-d3d10-depth-test` on x86, x64 and
ARM64 with the existing bounded process runner. Its new raw directory contains
exactly 65 HLSL/DXBC/token/depth/query originals; the independent reader writes
its JSON outside that directory. Native directories are `d3d10-depth-originals`
and `arm64-d3d10-depth-originals`, retained recursively by existing x86/x64
failure upload and the extended ARM64 always-upload path.

The original 60 ARM cases and markers remain exact; the depth case makes 61.
The primary shipping group grows 45→46, plus two separate SRV fixtures, making
the mandatory model 47→48. The complete fixture shipping union grows 70→71.
These counts are derived from current canonical selections; the earlier
60-case collector packet retains its own immutable 60/47/70 counts.

Private-transfer source commit `8d245293` adds a third mandatory phase to the
existing Blt reader. Both existing harness paths invoke that wrapper, and its
existing recursive Blt upload retains the additional originals. The production
source supplies the sibling reader; canonical restoration includes all tracked
files. This phase adds no fixture or ARM case. Depth source `754c3bcf` and private
source are merged separately from this harness-only change.

Local review checks exact old source preservation, frozen reader imports,
source declarations, markers, raw-directory isolation, upload paths and derived
counts. PowerShell parsing and actual Windows execution remain pending CI.
The change does not establish ordinary runtime or hardware admission.
