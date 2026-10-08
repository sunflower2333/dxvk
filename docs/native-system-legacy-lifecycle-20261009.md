# Ordinary D9 and D10 through the existing lifecycle binding owner

The optional `-RootAuthorizeLifecycleRestart` mode now admits API9 and API9Ex
with their actual D9Phase `offscreen` or `present`, and the existing API10
route. API11 retains its existing offscreen-only lifecycle restriction. Apply
is required; external `-WaitForReviewedRefresh` remains excluded. No default
mode, production adapter factory, capability mask, KMD or registry owner is
replaced by this extension.

The native tuple slot remains 0 for D9/9Ex, 1 for D10 and 2 for D11. All three
effective names must match the exact expected forward tuple. The worker gives
the protected current post-restart LUID to every probe. The immutable backup
LUID and fresh restored LUID remain independent. In a run without lifecycle
refresh, the probe LUID is still the original LUID and its argv is unchanged.

Use `invoke-system-legacy-lifecycle-owner.ps1` with ROOT authorization, pinned
owner/22-field argument JSON hashes, `-Api 9` (or `9ex`) and `-Phase offscreen`
(or `present`). API10 accepts only the existing compatibility selector
`offscreen`; its unchanged native probe actually creates a visible swapchain
and performs two Presents. It is not an offscreen-only rendering test. The
separate D11 host is byte-for-byte unchanged. Both hosts propagate the actual
owner exit code and refuse unreviewed fields or implicit authorization.

The existing ordinary D9 probe remains unchanged: exact SYSTEM factory,
HAL/HWVP, nonsoftware 1af4/1050 adapter, private paired160 and current names,
512 literal clear/draw readback pixels, exact approved module census and full
device release. Present adds exactly two Present/PresentEx calls and 512
literal desktop RGB pixels. The existing literal reader must receive the
actual forward LUID and exact API/phase. Existing D10 and D11 readers and
per-API retained runner contracts also remain unchanged.

Recovery code is unchanged: raw six-value replay/readback and release,
completion of the retained original worker job and owned probe, then reverse
exact-device restart if needed, fresh original Mesa selection and healthy
original desktop SID/session. No GPU oracle survives that reverse transition.

A new pending `held-module-observation.json` retains actual loaded paths and
the original PID/start/handle plus source/payload pins before strict module
validation. This preserves fallback diagnostics on failure. It declares
`validation_pending=true`, `passed=false` and `hardware_admission=false`;
the original strict census and process-lifetime checks still decide success.

Source-only verification uses 163 pure helper checks, the existing 16 actual
recovery cases with the selected API slot, and actual-source argv/census
controls. The latter test six argv cases with distinct backup/forward LUIDs
and five memory process censuses, preserving the pending paths when strict
selection or original lifetime fails. ROOT must run native PS5.1 and these CPU
checks before a concrete hardware attempt. Local strict C# passed; native
CPU, ordinary GPU/readback/Present and default replacement remain unproved for
this source extension.
