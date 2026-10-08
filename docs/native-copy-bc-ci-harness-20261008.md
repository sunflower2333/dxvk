# Native copy, BC and shader CI integration

The NativeUMD harness builds, ships and executes six additional tests on x86,
x64 and ARM64: copy-format policy, typed resource-copy cast, BC block-transfer
policy, typed BC update, D3D10 shader profile and typed D3D10 system shaders.
The existing bounded process runner, raw exit/output capture and 30-second
deadlines are preserved.

The typed copy fixture writes 640 native/public payloads plus its manifest in
`resource-copy-cast-originals`; its independent reader derives 320 observations
and 134624 bytes for each side. The BC fixture writes 2520 originals in
`bc-update-originals`; its reader derives 840 snapshots and 130944 byte
observations after 552 uploads, 136 no-ops and 408 rejections. The shader fixture
writes 49 HLSL/DXBC/token/readback originals in `d3d10-system-shader-originals`:
24 frames, 12 FXC programs and 9216 words per native/public role. Reader output
is outside each strict raw directory. ARM64 uses corresponding `arm64-`
directory names, retained by the existing always-upload step; x86/x64 failure
retention already recursively covers the three directories.

All 54 old ARM64 cases and markers remain exact; six new cases make 60.
The rotation shipping group grows 39→45 and retains the two separately shipped
SRV cases, giving the collector's mandatory shipping model 41→47. The complete
EXE shipping union, including probes and other fixture groups, is 64→70.
Existing 384 UAV originals and opened-primary recursive path remain; that path
also covers shared-present's 330 old+new files. The unchanged DXGI Blt marker
and recursive raw path also cover its independently added identity-copy phase.

Local verification reopens three actual baseline Git outputs and compares the
complete source bytes: edits are additions plus six names appended to the
existing shipping line, with every old body preserved. It checks exact old
case/marker ordering, source Meson target declarations, new reader arguments,
output locations, regexes against fixed recipe totals, and YAML equality
except for three new ARM raw paths. Shader source is owned and merged
separately from this harness. Local PowerShell is absent, so native parsing
and actual Windows execution remain pending CI. These checks do not admit an
ordinary VM runtime or replace the default UMD.
