# D11 SYSTEM runtime CreateDevice gate

Goal: correct typed DDI11 FL10_0 negotiation and actual GPU behavior for DXVK UMD; do not bypass proof.

1. Inspect actual originals and frozen front/core/probe and local Microsoft contract. Complete.
2. Implement evidenced exact Windows7 DDI11 revision contract in isolated worktree. Complete.
3. Local controls and strict SDK compilation. Complete: 12 COFFs, reader79/113, GCC+Clang ASan/UBSan1,310,988each andoriginalbodyfail655415.
4. Freeze commit/handoff and ROOT source review/native build/hardware retry. In progress.

ROOT alone owns VM access; no target calls here. ActualSYSTEMtyped11creation/pixels/Present remains unproven.
