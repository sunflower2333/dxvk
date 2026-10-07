# Null-code stream-output port

- [x] Read original Microsoft DDI/API docs and embedded DXVK prior-stage behavior.
- [x] Implement D3D10 prior-VS and D3D11 prior-VS/DS null-code SO with actual bytecode.
- [x] Add raw-bit rebind, original public API, full-domain-buffer and negative controls.
- [x] Compile production and both fixtures with original MS headers for x86/x64.
- [ ] Build/run both actual native ARM64 WARP fixtures and retain outputs/cleanup.
- [ ] Integrate with Texture3D source in one trunk/one CI push after native review.
- [ ] Verify embedded Turnip/system-runtime hardware behavior; keep admission gated.
