# Fresh current-core D3D8 descendants

The actual new core must pass the existing complete CI source/core admission
before a new system D3D8 device run. The original INDEX creation correction is
`0bb412439d18c7297a004113a105a52dbe3e7269`; typed device controls already passed
for both x86 and x64 in the original CI52 packet. That result does not admit a
new core. CI54/32b failed the new X8 Present shared-backing check, so no current
core reference, phase packet or staging packet exists for that failed run.

`scripts/prepare-native-d3d8-current-descendants.py` fills the next preparation
gap after the existing reference, phase and five-payload stage materializers.
The prior native parser and four USER handoffs bind the old core/run, old
runner, old manifest/parser hashes and historical HAL paths. This recipe
derives fresh bindings from the actual admitted phase and prepared stage. It
does not collect or admit CI, build probes, transfer files to Windows, change
the driver registration, authorize a phase, or run a target workload.

Default execution validates the exact 32 predecessor templates and previews
all 17 parser/USER source transformations locally. It writes no packet and
constructs no core byte/hash tuple. The PowerShell native parser still requires
fresh actual ARM64 System32 PowerShell5.1 AST5/Add-Type originals. Its reader
preserves separate Git origins: the phase verifier/run helper are from `ad60a25`,
the unchanged device verifier is from `7d9cfd0`, the native probe/lifetime scope
is `fbd7afd`, the original ICD setup scope is `37b8a2d`, and CPU scope is `e3ac126`.

After ROOT has admitted all six actual successful CI jobs and produced the
existing current reference, phase packet and stage packet, materialize fresh
descendants with:

```sh
python3 scripts/prepare-native-d3d8-current-descendants.py \
  --phase-packet /absolute/path/to/actual-current-core-phase-packet \
  --staging-packet /absolute/path/to/actual-current-core-stage-packet \
  --out /home/sunf/droidvm-repos/artifacts/fresh-descendant-output
```

The output contains one native parser handoff and separate names, enumerate,
offscreen and present handoffs. All five hosts run once in local default mode
with actual PID/start/wait/capture receipts; no target execution switch is
provided. Each host retains its owned native child, task, transport and collector
workflow. Every authorization template stays false, each result output is
fresh, and the ready manifest remains absent until actual parser originals have
been independently accepted by ROOT. The three permitted ready mutations stay
`ready`, `native_phase_parse`, and `pending`.

The resulting descriptor names the future parser-ready manifest, fresh stage
proof/ROOT review, held-frontend HAL proof/ROOT review, and offscreen proof/ROOT
review paths. These are future paths, not fabricated pins or accepted proofs.
ROOT writes the actual ready manifest and per-phase authorizations after direct
original review. The current stage must be reviewed before enumerate; names,
HAL, offscreen and present advance only from their own actual original proofs.
All helpers and existing independent phase verification remain unchanged.

The public probe continues to load the actual system D3D8 runtime and call the
HAL public `CreateDevice` with hardware vertex processing. The seven 8x8 scenes
cover Clear, FVF, VS1.1/PS1.1, dynamic texture, Reset and PS1.4 for 448 actual
readback pixels. Present is a separate phase requiring the seven scenes again
and 64 actual desktop screen pixels. Historical HAL/offscreen results do not
admit any of these new runs. Mesa remains the default until ordinary public
runtime and hardware requirements pass.

Local binding checks and Python parsing are preparation evidence. Concrete
packet emission, native AST5, new HAL, all pixels and Present remain unverified
until a future actual successful current core is admitted and the target phases
complete.
