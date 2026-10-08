# Finite render metadata diagnostic

Enable only with the existing `TU_WDDM_DIAGNOSTICS=1` worker environment.
Quiet API10/11 workers retain value0 and emit no added stderr. No admission,
mapping, callback, flags, returns, cleanup order or ABI changes are made.

For the first actual RenderCb only, the diagnostic records up to eight actual
allocation handles/write flags, allocation indices/offsets, full and relative
patch offsets, and supplied slot u64 values. A separate expected BO.Presumed
value is available only when bounded command7/native36+16*BO+32*command layout
matches. Availability is explicit; malformed layouts produce generic slot data
without pretending it is BO.Presumed. More than eight references reports the
omitted count and never dumps the command stream.

Signed c1b9 `wddmddi.cpp:7371-7378` requires relative patch44+16*i; BO.Presumed
must initially be zero. For six references, expected relative slots are44,60,
76,92,108,124. KMD patches a separate snapshot/DMA (`11971-11974,12035-12041`),
so input values do not prove KMD-patched GPU addresses. After RenderCb the
diagnostic records the exact HRESULT without dereferencing old runtime buffers,
which the callback may have replaced. There are at most ten before lines
(summary/count/eight rows) and one after line per RuntimeGpu owner.

ROOT's finite native verification recipe, not executed by this agent:

1. On the already prepared native SDK compiler view, compile only affected
   `src/umd/umd_runtime_gpu.cpp` with the existing strict `/W4 /WX /std:c++17`
   flags/includes. Use the current SDK and the existing SDK8.1/Win7 header
   views if this source is included in that published multi-view core. This
   verifies the unchanged callback layouts plus the added scalar logging.
2. Build and run only existing `tests/umd-runtime-diagnostics.cpp` as a native
   console executable, with strict `/W4 /WX /std:c++17 /EHsc /MT` flags. Its
   header is self-contained and needs no candidate DLL or graphics libraries.
   The output controls exercise disabled silence, raw64-bit values, six-row
   fidelity,1024-reference truncation to8 rows, per-owner suppression and exact
   callback HRESULT. Expected count is70 (previous45 +25 added checks), with
   `hardware_admission=0`; native result remains pending.
3. Freeze the source/compile/process originals for ROOT's chosen new source.
   Do not substitute or relabel the already accepted immutable96d core.

No factory, pixel, Present or runtime hardware result is inferred from these
CPU checks. ROOT decides if and when a fresh diagnostic core is selected.
