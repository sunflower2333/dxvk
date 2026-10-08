# D3D10 data-only stream output

Goal: implement real legacy UMD stream-output pipelines without SV_Position, while preserving ordinary rasterization guards and all native advertisement gates.

1. Inspect source and local Microsoft DDI contracts: complete.
2. Add position validation appropriate to creation and active raster/stream-output pipeline: complete.
3. Extend focused container controls and native SO byte/statistics fixture: complete.
4. Run meaningful portable sanitizer controls and strict official-header x86/x64 compilation: complete.
5. Commit and hand off exact source/results to ROOT for CI: pending.

No target VM operations or main worktree edits. Baseline c9f79a4; missing requirements remain0x17f.
