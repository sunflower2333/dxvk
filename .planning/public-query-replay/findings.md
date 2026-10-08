# Findings

The public current query ticket is replaced by every all-End prefix. Therefore
predicate bindings cannot consult that pointer after the prefix to select an
earlier ordered End result. Capture initial query ticket owners first, prefix
fresh End tickets in the existing execution order, then bind immutable ordered
predicate records to the most recent earlier End ticket or initial snapshot.

Nested command order append already relocates End occurrence IDs, and its End
order matches the existing m_queries execution list. Begin metadata can bind
its corresponding next End ticket; backend CS execution continues to consume
the accepted-End FIFO. Each execution needs a fresh owning view. Its API replay
lifetime is independent of captured CS query/ticket references and backend raw
query/event pins. There is no evaluator, wait, chunk suppression or GPU result
claim in this prerequisite.

The actual chunk dispatch now traverses the execution-owned operation view.
All vectors/snapshot copies are prepared before issuing the first End callback;
per-ticket allocation still follows the preceding accepted-End issue path.
Existing query FIFO/CS raw backend owners remain independent of API view life.
Public current ticket still points to the last prefixed End. An earlier record
binding makes result ownership precise, but does not prove CS/GPU availability.

Actual helper host controls pass 109673 checks under GCC and Clang ASan/UBSan,
including 4096 End relocations, 1000 fresh seventeen-End replays concurrently
completed by a CS model, 32 retained views and zero final query/ticket owners.
API objects/issue callbacks are modeled, no native COM/GPU action is executed.
Eight strict optimized SDK objects and eight LLVM COFF reopens pass with 573
current selected dependencies. No evaluator/action suppression/gate change.
