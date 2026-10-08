# Ordered predicate and query records

This change implements recording prerequisites, not public conditional execution.
The SetPredication execution stub remains, no query wait is introduced, every
CS chunk executes, and hardware/ordinary-runtime admission stays closed.

D3D11CommandOrder holds an ordered stream of chunk references, exact predicate
bindings and accepted query Begin/End records. Production query references use
Com<D3D11Query, false>; predicate records preserve the original BOOL even for a
null pointer, and capture PREDICATEHINT from the actual query descriptor. Get-
Predication still returns the existing exact API state and a public AddRef.
No hinted predicate is polled or treated as guaranteed suppression.

Accepted query Begin/End and predicate transitions break CS chunk and draw-
batching boundaries. An implicit Begin is a separate recorded query operation.
Finalized begun queries each receive an End record. Immediate contexts break
the same boundaries but still use existing API predicate/query state; they do
not retain a second command-list operation stream. The new stream traverses
actual deferred/nested chunk dispatch. Query notices currently serve metadata:
EmitToCsThread still marks every recorded End pending before the first chunk.
End occurrence IDs identify positions in a recorded list, not historical GPU
result generations. Replaying a list must eventually allocate fresh execution
result tickets. Current metadata pins retain the query object, not old Vulkan
query data cleared by a later Begin.

Nested AddCommandList preserves every operation and private COM pin, relocates
chunk references using the same helper as tracked resources, and offsets End
occurrence IDs. IDs within an immutable list stay unchanged on repeated replay.
Resource tracking uses the actual recorded chunk count: the first nonempty
pending chunk is ID0, matching AddChunk, rather than the previous inferred ID1.
Once a chunk is recorded, an empty pending chunk refers to the preceding chunk;
a nonempty pending chunk refers to the next real index. Operation positions
are never used as resource chunk IDs.

Execution reset records the default null/FALSE binding without altering saved
API predicate state. Restore TRUE records that exact saved binding; restore
FALSE leaves API state at defaults. FinishCommandList records the reset in the
returned immutable list and restores the saved binding only in the next list.
These match Microsoft's [ExecuteCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-executecommandlist),
[FinishCommandList](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-finishcommandlist)
and [GetPredication](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getpredication)
contracts. The workspace local DDI SetPredication and QueryEnd docs additionally
require storing a null-predicate BOOL and unbinding a predicate before reissue.

The host test executes the actual production metadata template with shared_ptr
query owners. Its independent exact trace covers implicit/explicit query work,
null/noncanonical BOOL, hints, reset/restore and nested27-chunk/7-End ordering.
Repeated insertion4096 produces40960 ordered chunks and12288 unique End
occurrences;32 repeated traversals keep the recorded stream unchanged. Queries
stay owned after caller/child release and release after the final stream is
reset. Host ownership controls are not native COM or GPU execution. GCC and
Clang ASan/UBSan each pass155773 checks. Current production callers compile
against the official SDK; final originals are pinned in the source packet.
Existing native fixtures/readers/workflows are unchanged.

Remaining work includes retained results per issued GPU generation, ordered
API-side query issue advancement, guarded individual draw/compute/transfer
operations, source-data and CPU destination mutation handling, device-loss
wait cancellation, and the complete real-backend native matrix in
[the fallback design](dxvk-public-predication-fallback-design.md). Chunks can
still contain multiple predicatable and unconditional actions between the new
boundaries; skipping such a chunk remains invalid.
