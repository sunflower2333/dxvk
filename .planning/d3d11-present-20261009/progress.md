# Progress

2026-10-09: Created isolated worktree from 7865936; coordinated with ROOT and port_dx11. Read workspace rules and planning skill. Memory lookup found no relevant entries. No target process or registration changes.

Inspection: exact D10 probe filename is umd-system-validation-probe.cpp (initial guessed name absent). Local bounded wrapper is workspace artifacts/root-run-owned-local-20261008-01.py (initial guessed main artifacts path absent). No target actions.

Implemented separate Present probe/reader/control files. Frozen offscreen files are untouched. Direct public docs were consulted only for Present/CreateSwapChain return and buffer behavior; corresponding links will accompany the documentation. Native Present/hardware results remain pending.

Wrote the runtime ownership, exact evidence contract and concrete native ARM64/x64 eight-stage CPU follow-on. New native link needs user32.lib; no D3D11/DXGI/D3DCompiler import libraries are required. Frozen frontend reuse must still join its original source/dependency/toolchain/PE proofs. No target calls.

Local attempt01 passed: supervisor PID3689026 exited0/reaped, two strict optimized official-header COFFs (x64 PID3689028 and x86 PID3689032), two LLVM inspection children, and reader controls PID3689037 all exited0/reaped with zero stderr. 87 meaningful synthetic rejections cover occlusion, device removal, Present parameters/count, owned HWND/PID/session, descriptor, cleanup, and all inherited pixel/typed-negotiation/held/module/lifetime controls. Source inputs before/after are identical. No Windows or actual Present execution. Independent peer review is running.

Fixed peer-confirmed windowed-scope blocker by disabling factory Alt+Enter/window changes and observing non-fullscreen state before and after each Present. Preserved attempt01 originals. Fresh attempt02 uses the same focused two COFF/two LLVM/control sequence, with six additional meaningful reader rejections.

Fresh attempt02 passed with supervisor PID3692937 exited0/reaped, two strict optimized COFFs/two LLVM inspections and 93 focused reader controls. All five child receipts exited0/reaped/closed without timeout and compiler stderr remained0. Separate final independent peer review02 is running. No further source changes are planned.

Final independent peer02 passed with no remaining blocker. Review note3055/SHA b85efea59b22452078f18eef9beed4f04965ab8b730fbea10c0bf0e6a44f3630 and finite pin packet13615/SHA b4d5b2b5870c491925ecd98f49ad7b8e0447e39437d3e0a773987a4bdc96a65b join corrected source, doc, saved original sources, local proof, two objects, five child receipts/raw files,93-control proof and supervisor. Reader subreview independently confirmed the guard predicates. No source/test/native/target rerun. Preparing only the seven new source/doc/own-plan files for commit; MAIN/push stays ROOT-owned.
