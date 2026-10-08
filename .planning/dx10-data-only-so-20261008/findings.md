# Findings

- Existing Query/SetPredication uses a synchronous CPU/GPU correctness fallback; replacing it with Vulkan conditional rendering is a larger backend project.
- Concrete SO gap: buildShaderContainer and linkVertexOutputs unconditionally demand SV_Position. This rejects data-only geometry stream output and null-GS passthrough even though SO drawReady deliberately permits no PS/RT/viewport.
- Source createShader validates each shader before active pipeline is known. Position requirements must be enforced when rasterization needs them rather than guessing at VS creation.
- Local docs are present at reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi; independent contract review delegated.
- Memory registry quick-pass has no relevant hits; no memory facts used.

- Independent contract review confirms final raster producer boundary: VS without GS requires position; VS feeding GS need only satisfy actual GS input declarations; non-rasterized legacy SO GS and null-GS passthrough need no position. Present position validation is retained. Native11 with SO can rasterize a selected stream and is intentionally untouched.
- Actual fixture controls include192 buffer words across3 cases, SO/pipeline statistics,256 unchanged pixels after cached-VS raster rejection, and256 rendered pixels after an ordinary GS generates position. These are pending native CI execution.
