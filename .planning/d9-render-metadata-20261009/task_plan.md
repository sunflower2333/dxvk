# D9 render metadata diagnostic

Parent: a0dab8b24d9a78a13cd4e0d8f592276382635c99. ROOT owns MAIN, pushes,
native execution and target access. The selected96 hardware core is immutable.

- [x] Confirm existing diagnostics lack exact allocation/patch/BO values.
- [x] Check signed c1b9 field layout and callback buffer lifetime.
- [x] Add a first-callback, eight-row opt-in scalar snapshot and raw result.
- [x] Extend existing output controls for quiet, bounds and suppression.
- [x] Independent source review; isolated attributed commit prepared.
- [ ] ROOT native compile/output-control execution (not authorized here).

Preserve all callbacks, returns, flags, guards, ABI and cleanup order. This is
diagnostic evidence, not a proposed Render failure fix.
