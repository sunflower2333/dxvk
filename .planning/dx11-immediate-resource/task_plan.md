# DX11 immediate resource copies

Goal: close actual native D3D10.1/D3D11 resource-copy format-cast semantics toward
ordinary Windows runtime operation; keep all production admission gaps closed.

- [complete] Audit Microsoft local SDK/DDI docs and exact production handlers.
- [complete] Implement typeless-family validation and correctly validated full/region copies.
- [complete] Add source-linked native/public readback and invalid-state controls.
- [complete] Strict optimized x64/x86 compilation; portable meaningful policy controls.
- [complete] Freeze source, evidence, documentation and commit for ROOT review.

Constraints: independent worktree only; no target calls; official SDK ABI; no
mask/version changes; no copied historical native execution claims.
