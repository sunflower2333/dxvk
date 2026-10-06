# Target EWDK driver build, 2026-10-07

The existing Windows ARM64 VM built and independently verified exact production
KMD source `c1b9ea69fa31037c5feeb1cfdb77500118b0ae37` with MSVC14.50.35717 and
WDK10.0.28000.0. All nine compile/archive/WPP/resource/link/inspection steps
passed in9.1133401s: VirtIO1.1700107s, KMD7.303272s.

The ARM64 Native `viogpuwddm.sys` is410624bytes, version100.6.101.58624, SHA256
`b773c84e8df806293a87a6e27c019c4e0995f51cbc660c871b826ce60947ae9e`.
Imports are only `ntoskrnl.exe` and `HAL.dll`. The verifier links118Git/tree/
source-archive/after-build files, the native kernel PE, map/PDB/output hashes,
all86compiler files and before/after desktop/installed-driver metadata.
The result is unsigned and uninstalled; signed58624/oem17 and the existing
DWM/Explorer processes remain continuous through the build.

The supplied ISO mounts read-only atD:. Actual initial ARM64/x64 builds fail
C1510 because the selected `clui.dll` extents18283870208/18287994880 lie beyond
the physical17912889344byte file. Its UDF metadata and many tools are present.
An owned native compiler copy preserves67readable original files and adds19
missing files recovered from exact byte ranges of the
[official Microsoft ISO](https://download.microsoft.com/download/27248da3-70d5-453c-ba2b-c20f784597e5/EWDK_br_release_svc_im_28000_260714-1640.iso).
Fifty-five retained recovery metadata ranges match that image; all86compiler
hashes remain unchanged after building. Original ISO/mount are preserved;
complete-ISO hash and arbitrary IDE/MSBuild acceptance are not established.

Use these existing target build parameters with a fresh owned output root:

```text
-EwdkRoot D:\
-CompilerHost arm64
-CompilerBin C:\Users\Public\EwdkNativeCompiler-01\bin
```

Headers/libs are under `D:\Program Files\Windows Kits\10\{Include,Lib}\10.0.28000.0`.
Helper: `C:\Users\Public\build-guest-ewdk-kmd.ps1`. Source archive and receipt:
`C:\Users\Public\ewdk-source-c1b9ea69.tar.gz` and `.json`. Existing evidence roots
`VioGpuEwdk-c1b9ea69-01/02/03` must not be reused.

Workspace evidence: `artifacts/dxvk-ewdk-target-build-20261007/`, particularly
`kmd-c1b9ea69-03/ewdk-build-verified.json` and `ewdk-build-handoff.md`. Both initial
failed builds and independent receipts remain retained. This checkpoint proves
target driver building; GPU and ordinary DX8-DX11 runtime admission require
their separate rendering/presentation acceptance.
