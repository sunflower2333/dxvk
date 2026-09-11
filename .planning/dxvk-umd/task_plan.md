# DXVK native Windows UMD

## Goal
Embed DXVK translation in a real VIOGPU D3D9/10/11 UMD, loaded by the Windows
runtime, with correct hardware rendering and full Display+Render integration.
ARM64, x64 and x86 are required; an app-local runtime is not the target.

## Next Step
Audit DXVK device construction and the local Windows DDI header/build environment.

## Phases
- [in_progress] Audit and implement an internal exact-LUID DXVK backend factory.
- [pending] Implement native DDI resource/shader/draw/readback coverage.
- [pending] Build and validate with personal-fork GitHub CI on supported architectures.
- [pending] Coordinate real-device offscreen runtime proof with main.
- [pending] Complete sharing, synchronization, presentation and full native acceptance.

## Constraints
Own only this independent checkout. No remote access until main grants a test
window. Preserve existing driver and desktop. Parent owns all shared worktree
and main-plan edits. This thread pins PWF_PLAN_ROOT to this directory.

## Errors
- Initial patch context for src/meson.build mismatched; no partial edits were
  applied. Corrected the context and applied successfully.
- Two guessed filenames were absent; used rg results to locate actual files.
