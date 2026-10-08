# Same-owner ordinary D9/9Ex and D10 validation

`test-system-d3d10-binding.ps1` remains the single tuple owner. Its default
`-Api 10` preserves the reviewed D10 probe/typed entry controls and same-directory
core. `-Api 9` or `-Api 9ex` selects only the first native `UserModeDriverName`
string; `-D9Phase offscreen` is the default and `present` is a separate slice.
Readonly execution remains a raw tuple/genuine KMT snapshot with no mutation.
Production masks, capabilities, frontends, signed KMD and VM images are unchanged.
ROOT owns all future Windows execution; nothing was registered here.

The first/second tuple mapping follows the local Microsoft display registration
documents `adding-user-mode-display-driver-names-to-the-registry.md` and
`enabling-support-for-the-direct3d-version-10-ddi.md`. Their adapter registration
contract supplies no process-local override or live cache refresh guarantee.
The experiment remains adapter-wide. The old global lease and rescue-task
namespace deliberately remain shared with frozen648f, so a D9 owner cannot
capture a D10 owner's transient tuple. Authoritative backup follows lease and
outstanding-rescue exclusion. Under the same restoration mutex, exact prewrite
readback precedes closed/flushed schema2 intent and the first RegSet. Intent
names the owner, key and zero-based native slot0 or1; foreign owner/slot is
rejected. No intent still cancels without replaying an unused backup. Recovery
replays and reads the complete six raw values, not merely the selected string.

The D9 consumer is frozen739de05cbb8da62b783764e85c773cb109cb2cf3, not a caps
shim or a new frontend. Its argument contract has ten operands:

```text
9|9ex offscreen|present LUID front core private-loader ICD-DLL rawdir Local-event 60000
```

The ICD argument is the actual loaded DLL, resolved and hashed through the
separate pinned ICD JSON. `VK_DRIVER_FILES`/`VK_ICD_FILENAMES` still name the JSON
and affect only the worker/child. `-PrivateLoader` and `-PrivateLoaderSha256`
must equal the already reviewed actual `winevulkan.dll` sibling and its
`-VulkanLoaderSha256`. The production D9 frontend selects exactly
`<front-directory>\arm64\viogpudxvk.dll` for a native ARM64 probe or
`<front-directory>\x64\viogpudxvk.dll` for an x64 probe. The controller reads
the pinned EXE's bounded PE machine header and requires that exact child path.
This machine check is a layout boundary, not a claim of complete PE validation,
loadability or native build success. D10's sibling layout remains separate.

D9 has no `--entry-negative` mode. The worker records that the mode was not
executed. It still checks the actual desktop token/job and all three original
KMT names before readiness, then requires the selected effective KMT candidate
before the ordinary factory probe can succeed. KMT cache refresh/restart is
ROOT's separately reviewed operation under the existing bounded rescue path;
this controller contains no restart/reboot command.

D9 writes `<rawdir>.held.json` with FileShare.None, flushes and closes it before
waiting. The worker retries opening until closed, checks its exact schema/PID,
event, 60000ms limit and pending exit0/1, retains the actual process identity,
and remembers its published PID across polling. It releases only after the
protected complete raw-restoration proof and fresh original KMT query. The
retained runner receipt must have that same PID, exit0, drained/closed pipes,
no timeout or live child. A held failure still restores before cleanup; it
cannot produce a successful worker result. Event signaling proves no registry
restoration by itself.

After successful restoration/reaping, ROOT must run the frozen independent
`tests/verify-d3d9-system-originals.py` from739de05 with the exact API/phase/LUID
and all actual module paths. Its raw closure is three offscreen originals
(clear/draw/manifest), or five for Present (adds both literal screen frames),
plus the closed sibling held JSON. Controller stdout markers do not replace
that reader or KMD GPU submission/completion evidence. The unchanged D10
binding reader remains D10-only. No DX8 or D11 support is inferred here.

Current local evidence is two strict Mono C# builds/runs: the unchanged120 D10
helper controls and105 independent native-slot/PE-boundary/closed-file controls.
They invoke no Windows registry/process/KMT/GPU API. Source preflight stages
four Windows PS AST checks, both helper classes and the old tiny progress child.
The separate actual-source phase script exercises all five old cancellation/
intent scenarios plus foreign-slot rejection for both native slots, using an
in-memory registry double. Those Windows executions remain pending. The frozen
648f source/preflight packet and739de05 probe/reader bytes were not changed.

Hardware/production admission and default replacement remain false. Actual
adapter selection, SYSTEM factory, literal draw/readback/Present, restoration,
KMD completions and desktop continuity must be collected before replacement.
