# D3D10 stream output without position

The legacy UMD rejected data-only stream-output shaders because both the
reconstructed DXBC signature and producer linkage always required SV_Position.
A shader feeding a geometry shader, or writing only to SO buffers, does not
need to supply rasterizer position. The final producer feeding rasterization
still does.

This change keeps position validation at the active pipeline boundary:

- VS creation permits data-only outputs; draw preparation requires position
  when no GS is active.
- A VS feeding a GS must provide every register/component the GS actually
  consumes, including position when consumed.
- Ordinary GS output still requires position. The legacy GS-with-SO path
  explicitly uses D3D11_SO_NO_RASTERIZED_STREAM and may omit it.
- Null-GS SO passthrough may capture data-only VS output.

Present position entries still require the full four-component mask; malformed
registers, masks, declarations and tokens retain their existing validation.
Link checks run before shader cache reuse. This change does not affect native
D3D11 SO rasterized-stream selection or any public version/feature/threading
advertisement. Missing D3D10 requirements remain 0x17f.

## Validation

Portable GCC O2 and Clang ASan/UBSan runs pass shader-container171,
SM4.1-container763 and compute-container35 checks. The added container controls
cover position-free VS/GS output, original token/hash preservation, ordinary
raster rejection and malformed signatures.

Eight optimized official SDK/WDK-header x86/x64 COFF compilations pass without
diagnostics: production umd_ddi.cpp and umd_shader.cpp, Windows shader-container
fixture, and the source-linked native SO fixture. Actual retained children,
start receipts, closed raw output files and LLVM COFF reopens are under
artifacts/data-only-so-local-01 through -03 in the independent worktree.
The Windows shader-container SPIR-V branch cannot be compiled locally because
the pinned nested spirv_headers checkout is absent; existing CI builds that
branch with its materialized dependency. No native Windows execution is
claimed by these local compilations.

The extended native SO fixture preserves all previous cases and adds:

1. Data-only VS to data-only GS-with-SO, followed by null-GS passthrough and
   another GS-with-SO draw. It checks all64 words in each buffer, including
   payload bits and untouched padding, plus SO/pipeline statistics.
2. Reusing the cached data-only VS without a GS, with ordinary PS/RT/viewport
   prerequisites present: drawing fails and all256 target pixels and SO bytes
   remain unchanged.
3. The same VS feeding an ordinary GS that generates position: all256 target
   pixels become the expected rendered color and SO bytes remain unchanged.

FXC/WARP execution in CI and real embedded-backend/ordinary runtime execution
on the target VM remain required. This worker performed no target operations.

## Local Microsoft contracts

The workspace documentation includes:

- windows-driver-docs/windows-driver-docs-pr/display/stream-output-stage.md:
  SO captures data before rasterization; GS and SO can independently be off.
- windows-driver-docs/windows-driver-docs-pr/display/geometry-shader-stage.md:
  GS emits primitives to rasterization or memory with strip expansion.
- windows-driver-docs/windows-driver-docs-pr/display/rasterizer-block.md:
  rasterization consumes clip-space position.
- reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/
  ns-d3d10umddi-d3d10ddiarg_stage_io_signatures.md: runtime signatures can be
  unions containing unused registers. Actual token declaration filtering
  remains necessary and unchanged.
