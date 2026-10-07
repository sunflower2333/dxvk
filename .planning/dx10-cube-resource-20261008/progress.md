Initial translation is unverified and isolated. No public contract bits, registered driver, test packet or actual GPU admission changed. No target call.

2026-10-08: New fixture cross-COFF ARM64/x64/x86 compile PASS with Wall/Wextra/
Wshadow/Werror. Earlier local failures retained: WDKshared include discovery,
SDK/Mingw global header collision, and two fixture compile errors corrected
(shadowedtargetargs/constlegacyRTVclear). Production LinuxCOFF attempts stopped
at existing MinGW missing D3D11_VS_INPUT_REGISTER_COUNT; no header shim added.
Native MSVC and actual reference execution remain required. Independent arithmetic
oracle checks7readbacks/168subresources/10380texels, sixface initialization,
partialrectangle updates/crossface copies, faceclear, scoped mips anddepth.
No publiccontractbits changed. DX10 exclusively owns target foractualGPUfresh02.
