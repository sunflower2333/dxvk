# Findings

The CPU08 probe passes a valid handle and non-null output/callback pointers,
then Interface=0xffffffff. Historical accepted18452d7's real OpenAdapter checks
invalid Interface before the null query callback and returns
D3DERR_NOTAVAILABLE. Its original public adapter fixture explicitly requires
that HRESULT for 0xffffffff. The CPU08 probe instead expected E_INVALIDARG,
which would cause exit7 after correct forwarding. Its null call must continue
to expect E_INVALIDARG.

The next probe is limited to explicit original frontend LoadLibraryEx,
GetProcAddress OpenAdapter, the null guard, one invalid-interface call,
unchanged argument/function-table canaries, exact view-specific core module
path and frontend FreeLibrary. No valid adapter query, runtime factory,
device creation, rendering, Present or registration is established.

CPU08's eight compiler/linker/inspection stages build two executable views.
Those original executables have the old expectation and have not executed.
They remain immutable and cannot be relabeled as passed forwarding controls.
