# Public per-action render-target clears

Base 6a64bf283c45c52b808f49c4018e7e83d80fb00d. ROOT owns MAIN, CI and target.

1. [done] Implement ClearRenderTargetView using the exact-ticket action
   evaluator and independently owned attachment view/shadow. Deferred default
   and explicit null actions must have recorded predicate state.
2. [done] Verify helper/replay/raw RGBA controls and official optimized
   x86/x64 SDK compilation, preserving the existing counter 22-case/88-word probe.
3. [in progress] Document scope, freeze exact source and get independent review.

All ordinary/native/hardware admission claims remain closed; no target, MAIN,
push, workflow or child-agent action. Preserve prior frozen packets.
