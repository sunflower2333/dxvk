# Identity GPU Blt

Goal: fix CI54 first X8 Present identity-Blt pixel failure using correct raw GPU
copies; keep the exact X8 oracle and all ordinary-runtime gates unchanged.

- [complete] Audit validated Blt inputs, actual ownership and command-list state.
- [complete] Add noresolve/noStretch/unscaled/sameactualformat identity GPU-copy path.
- [complete] Add typed X8 raw/state/alias/negative/lifetime controls independently of opened-primary edits.
- [complete] Strict optimized official SDK x64/x86 compilation and local policy/original controls.
- [complete] Prepare exact source/local proof for commit and ROOT CI review.

Native Windows/GPU execution remains pending ROOT CI, not a local pass.

No target calls. Prior910261 worktree/source packet remains immutable.
Shared_present_port owns opened-primary failure-only actual-word diagnostics.
