# Findings

Each dispatch method emits one command after ApplyDirtyComputeBindings.
Indirect dispatch also uses SetDrawBuffers; BindDrawBuffers captures owning
backend argument slices. UAV binding chunks retain buffer/image/counter views.
CSSetUnorderedAccessViews counter initialization is a state operation and must
remain unconditional even when the dispatch is suppressed.

Direct zero group dimensions retain their original API early return. Native
controls will initialize counters before that no-op. Indirect arguments use a
nonzero byte offset surrounded by zero guards, so a wrong offset cannot produce
the expected literal CS payload. Query and resource owners must survive replay.

Production changes are confined to the two dispatch methods. Existing owning
argument/UAV/shader bindings remain unconditional and unchanged; per-action
cost is emitted only on execution. Functional Spec20.2 explicitly includes both
dispatch entry points; state and query operations are excluded. Actual all-End
prefix and exact-ticket evaluator remain unchanged. Local primary DDI docs
confirm zero dimensions, tightly packed XYZ/4-byte offsets and state counter
initialization (-1 preserves, other values set).

Native probe scope is compiled-only158 complete52-word snapshots:8216 words,
32864 bytes. Distinct shader values, untouched tails and count guards are literal.
Recorded shader/UAV/argument/query API owners are released before three replays.
Counter readback depends on the unchanged prior CopyStructureCount action.
Actual production ABI/CS/Vulkan, query/shader availability and module identity
must be established by a future native runner; no admission changes.
