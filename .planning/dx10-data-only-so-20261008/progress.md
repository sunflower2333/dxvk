# Progress

2026-10-08: Created categorized independent worktree from c9f79a4, read production plan and planning skill, inspected SO/query/shader creation and draw linkage. Selected data-only SO scope and informed ROOT.

Lookup errors: nonexistent guessed umd_objects.h and umd_shader_container.cpp; discarded these guesses and inspected actual umd_ddi.cpp / umd_shader.cpp. SoSetTargets docs filename differs from guessed filename; use file search for exact path. artifacts directory is absent in new worktree and will be created for this work.

Implemented position checks keyed to final rasterized output in shader container/linkage and legacy DDI creation/draw. Added portable container controls and native SO-only GS/null-GS bytes/statistics/raster rejection cases. Existing fixture sequence remains intact. Next: strict compile and portable controls.

Additional benign lookup error: new worktree has no pre-existing artifacts and main repo artifacts are workspace-level, not inside main checkout. Resolved to /home/sunf/droidvm-repos/artifacts/dx11-uav-texture-20261008/local-validation-04.

Local01 portable controls passed in GCC and Clang ASan/UBSan: shader171/SM4.1 763/compute35. Optimized official-header production umd_ddi/umd_shader x86/x64 compiled without diagnostics. Native fixture compile exposed const signature-entry pointer and old WARP argv unused warning; corrected both. Native shader-container SPIRV branch could not compile locally because pinned submodule spirv_headers is not materialized; compile ordinary Windows branch locally and retain full SPIRV execution for CI. Original failures are preserved, not overwritten. Added positive data-only VS→position-generating ordinary GS raster pixels.

Local02 shader-container Windows branch compiles x86/x64 without diagnostics. SO compile found overly broad replacement accidentally modified the prior passVertex helper with undefined mode; restored that prior helper exactly, leaving the new dataShader mode branch only. Will compile only the affected SO fixture in fresh Local03 outputs.

Local03 corrected native SO fixture compiles optimized x86/x64 with zero diagnostics; actual child3112203/3112204 exit0 and actual LLVM reopens3112261/3112262 exit0. Final accepted set: four production COFFs Local01, two shader-container COFFs Local02, two SO COFFs Local03. All6 portable execution controls are exit0; the shader-container43-byte stderr is its expected malformed extended-token diagnostic, identical for both compilers, not sanitizer output. No target/native execution occurred. Added documentation and preparing commit.
