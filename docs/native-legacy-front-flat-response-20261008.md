# ARM64X frontend response-file correction

The original CPU04 attempt at source `9a2833ed7c802101d6e729b8338e856a17b2be2f`
copied all 73 selected compiler inputs, compiled the ARM64 frontend, linked its
native DLL, and compiled the ARM64EC frontend. The ARM64X link then returned
1104 because its outer response file contained another `@` response-file option.
The actual diagnostic names `@C:\Users\Public\DxvkLegacyFrontBuild-9a2833e-04\output\arm64-merge-inputs.rsp`
as the file it cannot open. Those failed originals remain unchanged.

Microsoft's [LINK response-file reference](https://learn.microsoft.com/en-us/cpp/build/reference/at-specify-a-linker-response-file?view=msvc-170)
points to the [response-file rules](https://learn.microsoft.com/en-us/cpp/build/reference/at-specify-a-compiler-response-file?view=msvc-170),
which prohibit embedding one response file in another. The producer now appends
the original `$nativeInputs` array directly between the EC object and the existing
ARM64X options. It still writes `arm64-merge-inputs.rsp` and retains the native
full-path response and hash ledger. Input order, quoting, object/library bytes,
link options, library-directory selection, seven tool stages, deadlines and
failure collection remain unchanged.

The original merge file contains two arguments: the ARM64 object and the selected
ARM64 `kernel32.lib`. Replacing its single nested option increases the observed
outer array from 13 to 14 arguments. Local original-byte checks preserve that
exact span and every preceding/following argument; the generated native full-path
response has a UTF-16 BOM and the preserved merge file is ASCII. The original
files are read according to those encodings and are never rewritten.

Native PS5.1 parsing and an actual corrected ARM64X link remain separate pending
gates. No frontend, core or view probe is loaded by this source-only correction,
and it changes no production admission or registration. Full compiler
attestation remains false. This legacy compatibility frontend serves DX8/DX9;
it is not a prerequisite for native D3D10/D3D11 semantics.
