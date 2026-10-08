# Public per-action render-target clear predication

This slice adds ClearRenderTargetView to the existing public core exact-ticket
action evaluator. Counter-copy behavior and its 22-case/88-word native probe remain
unchanged. Other draw/dispatch/clear/copy/update/mip/resolve classes stay pending,
and all ordinary-runtime, discovery, threading and hardware gates remain closed.
Outer UMD already has its own fallback; this public feature is not the sole
Mesa replacement blocker.

## Recorded clear and ownership

The [Microsoft Functional Spec 20.2](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#20.2)
explicitly lists ClearRenderTargetView as predicatable. Guaranteed timing cannot
allow an otherwise suppressed clear; hints may execute unconditionally. The
existing equal-result suppression, ignored but retained null BOOL and hint
zero-poll rules continue through the generic evaluator.

The original clear lambda still owns its converted color and DxvkAttachment
image view plus optional buffer shadow. Immediate unbound clears keep their
original CS emission. Bound immediate clears evaluate before GPU mutation.
Every deferred clear, including default and explicit null, is an individual
action record. This deliberately tests the recorded default/null state instead
of relying on unconditional chunks to ignore the immediate parent binding.
Command lists start with null/FALSE, nested lists have their own reset/default
records, and restore TRUE/FALSE records the saved/null predicate respectively.
Replay passes those exact records and historical result owners to evaluation.

Action view/shadow Rc owners survive API RTV release and are copied into each
new replay command. State/query chunks remain unconditional; no entire chunk
is suppressed. The original clear had no resource-sequence tracking tail, and
this slice introduces none. Command-list action relocation, costs, flush hints,
counter sequence tracking, ticket/query/backend helpers and public GetData
remain unchanged. Only the invalid-action log text becomes generic.

Pending guaranteed results still use the existing API-thread prefix submission
and synchronization. No wait is added to a CS lambda. Null/hint bindings have
zero polls. Invalid/device failure never becomes unconditional execution. The
existing context protection/application serialization and private query owner
requirements remain. Existing allocation/issue exception boundaries and CPU
yield performance limits are unchanged; no rollback/performance guarantee.

## Actual local checks and pending native acceptance

Actual GCC and Clang ASan/UBSan runs each execute 2,318,014 checks in the unchanged
production evaluator/ticket-read/order/replay helpers: 4,500 individual actions,
1,250 suppressed actions, 4,000 unconditional chunks, 2,000 modeled prefix submissions,
1,152,000 exact modeled RGBA/shadow words and 1,000 replays. Controls bind a suppressing
immediate parent but retain recorded default/null clear behavior, preserve a
noncanonical null BOOL, select an earlier End across reissue, cover nested
restore TRUE/FALSE, and release modeled attachment owners to zero. The query,
API view, GPU availability and attachment payload are modeled here; these are
not actual COM/CS/Vulkan/GPU clears.

Six actual optimized official SDK/MSVC-target x86/x64 COFF objects compile the
common-context explicit immediate/deferred instantiations, immediate caller
and new standalone native probe. Actual LLVM reopens verify their architectures
and the current selected compiler dependency closure. Original SDK/toolchain
warning allowances remain, including the unchanged historical common-context
unused lambda captures; original failure receipts are retained in the base
packet. No source warning cleanup or new allowance is introduced.

The new unregistered tests/d3d11-predicate-clears-native.cpp is designed for 80
raw cases and 12,320 words, loading an explicit matching DXVK core d3d11.dll and
matching DXGI module with a hardware device. It covers cold visible/empty
queries, both BOOLs, hints/null, buffer RTV shadows, default/explicit-null
deferred clears under a suppressing immediate parent, two historical End
results, three repeated list executions, nested and parent restore TRUE/FALSE,
and released recorded API RTV/query owners. Query pipeline shader source is
copied unchanged from the old counter probe. Compilation establishes neither
shader/query availability nor hardware behavior. No native probe was linked,
executed or registered with existing CI/Meson/harnesses.

tests/verify-predicate-clear-core-originals.py independently checks all 80 raw
files, exact 12,320 little-endian RGBA words and the full single-line marker. It
does not accept masked color channels and cannot establish native process or
backend identity. Actual local reader controls use explicitly synthetic output,
verify 49,280 bytes, and reject 23 bad pixel/alpha/default/null/historical/restore,
size/file-set/symlink/marker controls. This is parser validation only.

Native COM/BOOL/HRESULT, actual queue/submission, device-loss/performance, texture
and buffer-shadow GPU clears and target identity remain pending. Both the old
counter probe and this clear probe remain unexecuted; all broader action/native
admission claims stay closed.
