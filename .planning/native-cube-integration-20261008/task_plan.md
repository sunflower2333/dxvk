# Combined native cube porting

Goal: integrate legacy D3D10 cube resources, typed D3D10.1 cube arrays,
typed10.1/11 cube SRV mip ranges and scoped cube-array GenerateMips into the
native embedded DXVK UMD, with original native and CI readbacks.

Source checkpoint: four local commits791006c/1f2ffde/7669c3a/b5b863b merged in
an isolated worktree based on ROOT review checkpoint3d6fb93. Meson insertion
conflict resolved by retaining both targets; production headers merged cleanly.
Main production branch and all old frozen source/native packets remain exact.

- [x] Merge the four production slices and register every fixture target.
- [x] Add five cube fixtures to the existing backend/ARM64 CI runners, preserving
  the unchanged d8cf owner and30s deadline. ARM64 fixture count becomes35.
- [x] Compile combined production with strict official SDK/WDK x64/x86 inputs:
  actual2623291/2623292 exit0/emptydiagnostics/correctCOFF, before/after equal.
- [x] Merge DX10's independent resource original arithmetic oracle2e0da92.
- [ ] Freeze combined source and native reference packet with compiler/header/
  library/source/oracle original pins; parse actual WindowsPowerShell5.1 scripts.
- [ ] Native ARM64 MSVC build/reference run: five cube fixtures and old view,
  Texture1D/Texture3D regressions. Independently verify every original readback,
  retained child exits/raw pipes, static34/SYS/desktop and explicit release.
- [ ] Integrate accepted source into main, push once, verify sole automatic CI
  original artifacts for all architectures. No separate workflow dispatch.
- [ ] Rebuild and run real viogpu GPU cases from exact current source; ordinary
  runtime/public admission/registered replacement/Present/DWM remain open.

Target currently ROOT-owned idle. EWDK prepares the DX8 missing-provider repair
and fresh nativeCPU packet locally. No target operation follows from local
cube compilation. Native public contract mask remains closed.
