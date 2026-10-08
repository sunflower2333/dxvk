# Legacy frontend invalid-interface result

The CPU-only frontend probe supplies Interface=0xffffffff, non-null adapter
handle, callback pointer and guarded function-table pointer. The accepted
18452d7 core returns D3DERR_NOTAVAILABLE for that unsupported Interface before
examining the absent query callback. The public adapter fixture already tests
this exact invalid Interface and HRESULT. The probe now expects that real
result instead of E_INVALIDARG. Its null OpenAdapter call still requires
E_INVALIDARG before any core is loaded.

This is a test-only correction. The original frontend export, typed SDK ABI,
forwarded argument pointer/HRESULT, core selection and core implementation
remain unchanged. CPU08's successfully built frontend and original view
executables stay frozen; those executables have the previous expectation and
have not been executed. A new corrected-source ARM64/x64 view build must
precede its own bounded module-only forwarding attempt.

That later attempt may establish original DLL loading, export resolution,
native/emulated architecture selection, the two negative results, unchanged
canaries, the exact view-specific core path and frontend release. It cannot
establish a valid KMD/runtime handshake, a device, GPU rendering, Present,
ordinary Microsoft runtime admission or default registration.

Local strict optimized original-SDK ARM64/x64 compilation passes with empty
diagnostics and actual AA64/8664 COFF machine values. The retained original
commands use the established local Clang toolchain; these results do not claim
MSVC native executable execution. The original18452 core adapter/API/public
fixture Git bytes and all107 frozen CPU08 preparation inputs are retained and
unchanged in the local evidence.
