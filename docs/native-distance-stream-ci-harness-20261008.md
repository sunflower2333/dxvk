# Clip/cull distance Stream Output CI integration

The harness builds, ships and runs `dxvk-umd-distance-stream-policy-test` and
`dxvk-umd-d3d10-distance-stream-test` on x86, x64 and ARM64. The policy marker
requires 108 checks. The typed fixture produces exactly 77 HLSL/DXBC/token/
SO/query originals: 16 scenes, 32 buffers, 32 query frames and six
FXC programs. Its independent reader checks 2048 words across both roles and
192 query words, writing result JSON outside the strict raw directory.

Native directories are `d3d10-distance-stream-originals` and
`arm64-d3d10-distance-stream-originals`. The existing x86/x64 failure upload
retains the new directory recursively; the ARM64 always-upload adds its path.
All previous readers and raw paths remain, including depth and the three Blt
phases. The existing bounded runner and exit/output capture remain exact.

All 61 old ARM cases and markers are preserved; two new cases make 63. The
primary shipping group grows 46→48, plus two separate SRV fixtures, making
the mandatory model 48→50. The full shipping union grows 71→73. The earlier
CI61 collector packet retains its own immutable source and counts.

The production source and reader are owned and merged separately. Local
verification checks source preservation, frozen declarations, marker/reader
bindings, raw-directory isolation, uploads and derived selections. PowerShell
parsing and actual Windows execution remain pending CI; ordinary runtime and
hardware admission remain pending.
