# Public depth/stencil and UAV clear predication

This slice routes ClearDepthStencilView and ClearUnorderedAccessViewUint/Float
through the existing exact-ticket action evaluator. It preserves ClearRTV and
counter-copy source/probes. Other draw/dispatch/copy/update/mip/resolve actions
and all ordinary/native/hardware admission remain pending. The outer UMD's
existing predication fallback is unchanged.

The [Microsoft Functional Spec 20.2](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#20.2)
explicitly includes all three entry points. Exact equality suppresses guaranteed
actions; null/hint evaluation has zero polls. Existing API-thread submission
and pending synchronization apply; no evaluator or wait runs on the CS thread.

Each of the original six backend commands remains an individual action with
its original owning attachment, image view or buffer slice/view. DSV writable
aspect filtering remains before emission, including zero writable aspects.
Uint raw/typed buffer paths, integer reinterpretation, A8/R11G11B10 conversions,
zero clears and image compatibility/recreation logic stay unchanged. Float
integer-format rejection stays before emission. Image recreation remains inside
the conditioned command. No original method had a resource-sequence-number
tail; none is added or removed. Transfer cost is charged for executed commands.

Every deferred clear, including default and explicit null bindings, uses recorded
predicate state. Immediate unbound commands retain direct CS emission. Query,
state and other unconditional chunks remain unconditional. Existing order,
replay, ticket/query/GetData and counter sequence helpers are unchanged. Backend
slice/view owners survive recorded API view release and are copied on replay.

Fresh GCC and Clang ASan/UBSan controls each pass 19,198 checks: 54 individual
modeled payload actions, 15 suppressed actions, 48 unconditional chunks,
24 modeled prefix submissions, 19,008 literal modeled fields and 12 replays.
They use the actual existing evaluator/ticket/order/replay helpers, retain
default/null BOOL and historical End selection, check nested restore and parent
state, and prove modeled resource/query owner release. View/COM objects, GPU
availability and payload mutation are modeled; this is not a GPU execution test.

Four new actual optimized official SDK/MSVC-target x86/x64 COFF objects compile
the common-context immediate/deferred instantiations and standalone native probe.
Four actual LLVM reopens validate architectures/current selected dependencies.
Original SDK/toolchain warning allowances remain, including inherited unused
lambda captures. No broader old compilation or control suite was repeated.
The copied compiler metadata selector included the old ClearRTV reader; those
original rows remain unchanged. Authoritative source joins retain its three
correct C++ before/after rows and the actual new-reader control before/after pin.

The unregistered standalone native probe loads an explicit matching DXVK core
d3d11.dll and matching DXGI module. It is designed for 83 snapshots, 19,256
defined fields and 73,040 raw bytes. Each snapshot contains D32_FLOAT_S8X24
depth/stencil, a raw Uint buffer, typed vector Uint buffer/image and typed vector
Float buffer/image. Values use literal exact integer/float bits. DSV output
retains all 32 depth bits and eight stencil bits per pixel; the format's 24 X
bits are not defined API fields and are not included in the five-byte record.
UAV output retains every raw word without masks. The seven DSV controls cover
zero/depth/stencil/both flags and read-only depth/stencil/both views. Other
special UAV formats are preserved by source identity, not claimed GPU-tested.

Immediate controls include both predicate BOOLs, visible/empty queries, hints
and null. Deferred controls cover list-default null and noncanonical explicit
null, first/second historical Ends, released recorded API query/views, three
replays, nested restore TRUE/FALSE and immediate parent restore TRUE/FALSE.
The query pipeline source is copied from the unchanged older probe. It does
not establish shader/query availability. No native probe was linked, executed
or registered with existing CI/Meson/harnesses.

The independent reader checks every defined depth/stencil field, all UAV bytes,
the exact file set and the full marker. Synthetic local controls pass the
83-file/73,040-byte positive input and reject 33 byte/aspect/history/null/restore/
file-set/marker corruptions. This validates the reader only; process/module/
target provenance must come from a future native runner. Native COM/BOOL/HRESULT,
queue/submission, Vulkan GPU values, device-loss and CPU fallback performance
remain pending. Existing threading/private query lifetime, all-End prefix,
exception boundaries and non-stream SO overflow stream0 limitations remain.
