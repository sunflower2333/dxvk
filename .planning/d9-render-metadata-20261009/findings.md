# Findings

The published RuntimeGpuDiagnosticInfo retains only generation/context/queue,
reference count, stream length, lock counts and cleanup counts. It lacks the
exact allocation handles, write flags, patch positions and input slot values.

Signed c1b9 native request36 + BO16 with Presumed offset8 requires relative
patch44+16*i. Six actual rows require44/60/76/92/108/124; a484-byte wrapper has
stream offset256 and full patch300/316/332/348/364/380. Predicate:
wddmddi.cpp7371-7378. KMD copies and patches its own snapshot/DMA; runtime may
replace command buffers on RenderCb return. Reading old buffers afterward
would be unsafe and could not prove the miniport's patched values.

Snapshot actual allocation/list scalars and generic supplied slot before the
callback. Read the independent expected BO.Presumed field only if command7,
length, BO count and native36+16*BO+32*command layout identify that field within
the stream. These diagnostic checks do not reject or alter a submission.
After callback, emit only normalized HRESULT and exact original callback HRESULT.

Existing TU_WDDM_DIAGNOSTICS=1 enables this; other values cause no added output
or snapshot reads. First callback only; eight references maximum, omitted count
explicit. Actual six references fit fully. No command dump or runtime handle
dereference; values are scalar opaque-handle numbers.

No local compiler, parser, tests, readers, targets or GH calls are authorized.
Validation remains source-only until ROOT executes the finite native recipe.
