# Public Dispatch and DispatchIndirect predication

This slice adds individual predicate actions to Dispatch and DispatchIndirect
on the frozen c27aaf0 view-clear baseline. Counter-copy and clear actions remain
unchanged. Draw, other copy/update/mip/resolve actions, native acceptance and
ordinary/hardware admission remain pending. The outer UMD's existing predicate
fallback is unchanged; this core API work is not a sole Mesa replacement gate.

The [Microsoft Functional Spec 20.2](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#20.2)
explicitly includes both dispatch entry points. Guaranteed predicates suppress
an action on equal query result and BOOL value. Null and hint bindings do not
poll. The existing exact-ticket evaluator submits preceding query work and
waits on the API thread, with no timing-based escape or wait in a CS lambda.
State/query operations remain unconditional. Section 20.3.6 prohibits issuing
the bound predicate; the probe unbinds before reissue and counter readback.

Both original backend lambda bodies and captures remain intact. Dirty compute
bindings precede the action. Indirect argument binding also precedes the action;
its existing CS chunk captures the owning backend argument slice. UAV binding
chunks retain buffer/image/counter views. CSSetUnorderedAccessViews counter
initialization is unconditional state work, including for a suppressed dispatch.
Direct dispatch retains the original zero-group early return. No original
method had a resource-sequence-number tail; none is added or removed. GPU cost
is charged for emitted actions. Every deferred dispatch records an individual
action, including list-default null and noncanonical explicit null BOOL.
Immediate unbound dispatch retains direct emission. Query generation, tickets,
all-End prefix, historical selection, nested relocation and restore are unchanged.

Fresh GCC and Clang ASan/UBSan controls each pass 7,001 checks with the actual
existing evaluator/ticket/order/replay helpers: 108 modeled actions, 30
suppressed actions, 216 unconditional chunks, 120 state bindings, 48 modeled
submissions, 5,292 literal fields and 24 replay walks. They verify independent
argument ownership, released recorded payload/query owners, direct/indirect
dimensions, default and explicit null, historical Ends, nested restore, parent
predicate state and zero remaining modeled owners. Bindings, shader execution,
GPU query availability and payload mutation are modeled. They are not production
COM/Rc/CS/Vulkan execution tests.

Four new optimized official SDK/MSVC-target x86/x64 COFF objects compile the
common immediate/deferred instantiations and native probe. Four actual LLVM
reopens validate both architectures and 581 current selected dependency pins.
Original compiler warning allowances remain, including inherited unused lambda
captures. No old successful compile/control suite was repeated. All four
compiler before/after source rows select the correct current sources/reader.

The unregistered standalone native probe loads an explicit DXVK core d3d11.dll;
a future runner must verify its matching DXGI/module/source identity. It is
designed for 158 snapshots, 8,216 complete uint32 words and 32,864 raw bytes:
79 snapshots for each dispatch entry point. Each snapshot contains all 24 typed
R32_UINT output words, all 24 append-buffer words and all four counter-copy
destination words. Twelve 1x1x1 compute invocations use groups 3x2x2, distinct
recorded/parent shader values, and a fixed append value. An indirect argument
offset of four bytes has zero guards before/after the XYZ tuple. Three zero
dimension controls cover each entry point. Exact counter value five demonstrates
unconditional state initialization; successful execution appends twelve values
and yields seventeen. Every untouched tail and counter destination guard remains
an exact literal. Counter snapshots use the existing unchanged CopyStructureCount
action with null predication, so native acceptance also depends on that path.

Immediate controls cover both BOOL values, visible/empty queries, hint and null.
Deferred controls cover default/explicit null, first/second historical Ends,
released recorded query/shader/UAV/indirect-buffer API owners, three replays,
nested restore TRUE/FALSE and immediate parent restore TRUE/FALSE. Exact compute
shader/UAV API state is checked before and after restore. Distinct parent shader
values catch stale inherited bindings. Query graphics pipeline source remains
byte-identical to the older clear probe. HLSL compilation occurs only at native
execution; C++ compilation establishes neither shader nor query availability.
No native probe was linked, executed or registered with CI/Meson/harnesses.

The strict reader verifies the full marker, regular original file set and all
52 words per snapshot without masks. An independently written synthetic
158-file/32,864-byte input passes; 44 word, tail, counter, zero-dimension,
history/null/restore, file-set and marker corruptions are rejected. This proves
reader behavior only. No native queue/submission, COM/BOOL/HRESULT ABI, Vulkan
GPU values, device loss or CPU fallback performance is claimed. Existing API
serialization/private COM-owner requirements, all-End prefix, exception
boundaries and non-stream SO overflow stream0 limitations remain.
