# Legacy ARM64X entry

`umd_legacy_front.cpp` supplies a genuine typed `PFND3DDDI_OPENADAPTER`
forwarding entry. An ARM64 view loads `arm64/viogpudxvk.dll` and the ARM64EC
view loads `x64/viogpudxvk.dll` beneath its own directory. Original cores and
PDB identities retain their filenames. Each core keeps its original private
Vulkan loader beside it. The WoW64 runtime loads `x86/viogpudxvk.dll` directly.

The entry copies the resolved function pointer into the exact SDK type and
forwards the original argument pointer and HRESULT. It does not create or
rewrite callbacks, function tables, capabilities, runtime handles or device
owners. A core of the same basename already loaded from another directory is
rejected. Missing libraries/exports fail with real Win32 HRESULTs. A null
argument is rejected before loading a core. The loaded core is retained for
the process lifetime because its published DDI tables outlive `OpenAdapter`.
The frontend performs no work in DllMain.

`build-native-legacy-front.ps1` must run in an actual ARM64 MSVC developer
environment. `-Arm64EcLibraryDirectories` supplies the reviewed matching
ARM64EC support-object directory, x64-compatible static CRT directory and x64
SDK import/UCRT directories. ARM64EC's support directory contains objects; it
does not need its own `libcmt.lib`. The EC library search contains no native
ARM64 library directory. The original native ARM64 inputs reach the hybrid
link solely through the captured native response. This follows Microsoft's
[ARM64EC library compatibility](https://learn.microsoft.com/en-us/windows/arm/arm64ec)
and [ARM64X build route](https://learn.microsoft.com/en-us/windows/arm/arm64x-build).
Seven retained-handle
native tool stages compile ARM64, link the native view and capture its actual
full link inputs, compile `/arm64EC`, merge `/MACHINE:ARM64X`, and inspect
headers/load configuration, exports and imports. Each tool has a 60-second
deadline and original response array, stdout, stderr, handle/exit/drain
receipt. Compiler and linker invocations are separate. The producer executes
no module/probe, runtime factory, kernel query, GPU work or registry mutation.

An ordinary Git checkout is supported. A partial native source packet must
instead supply `-SourceManifestPath` and `-SourceManifestSha256`. Its explicit
`native-legacy-front-source-input-v1` manifest contains exactly four rows for
the frontend CPP/DEF, producer PS1 and unchanged owned C# runner, with raw Git
blob, SHA256 and size from a reviewed commit. The producer independently
computes each actual Git blob SHA1 (`blob <bytes> NUL <content>`), hashes the
files before/after, and retains the exact source mode. No guest Git checkout
or Git executable is assumed in this explicit mode.

Actual hybrid linking and both native/emulated-x64 forwarding controls are
required before native default registration. A plain AA64 DLL cannot replace
the shared native UMD-name slot. The separate `umd-legacy-front-probe.cpp`
performs CPU-only null/unsupported calls through the typed frontend, retaining
argument/table canaries and requiring the exact view-specific core path. It
does not provide a kernel callback or explicitly create a device. These view
controls run only under their own later authorization; the build packet does
not execute them.

Local strict optimized original-SDK compilation passes for the ARM64 view.
Clang 18's ARM64EC original-header path fails at intrinsic declarations; the
original failures are preserved. No macro, header or ABI shim was added.
Actual MSVC ARM64EC compilation, hybrid link/view execution, ordinary
Microsoft runtime rendering/Present and system default registration remain
pending native original evidence.
