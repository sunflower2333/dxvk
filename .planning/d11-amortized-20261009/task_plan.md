# DDI11 amortized submission callback

Goal: implement the actual Microsoft DDI11 immediate submission callback contract without altering hardware admission.

1. Audit actual source and local Microsoft docs: complete.
2. Implement caller-thread coalesced operation behavior, dynamic callback slot and reentry/retirement handling: complete.
3. Run strict actual SDK compilation locally: complete (final03 production/final04 fixture, six COFF). New meaningful callback controls are ready but Windows execution is pending with ROOT.
4. Freeze source commit and bounded evidence handoff: complete; ROOT review/merge/native execution follow. No runtime or hardware proof claimed.

Errors: initial broad globs and stale local documentation path corrected through rg files; no source or target mutations.
