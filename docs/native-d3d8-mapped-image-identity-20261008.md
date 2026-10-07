# I386 system-image identity observations

The native CPU06 read-only attempt compiled all five COFFs and four PE files
with zero first-party warnings, then stopped at
`canonical-I386-Kernel32-provider`. Its original archive has 118 files,
31,180,717 bytes and SHA256
`4a153ecf3b66c106b6c0415dccd90344485398b66c14a62821f8d556f9d8bab0`.
IsWow64Process2 reported process `014c` and native `aa64`; the documented
Directory2W API was available through the already loaded KernelBase. Loaded
I386 Kernel32 and KernelBase reported logical `C:\WINDOWS\System32` module
names. The strict SysWOW64 path guard rejected that observation. Policy,
callback and malformed-CLI fixtures after that point did not execute.

Microsoft documents that [GetModuleFileNameW preserves the load-time name
format](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew).
That string alone cannot establish the physical mapped file.
[K32GetMappedFileNameW queries the file backing an address](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmappedfilenamew),
and [GetFinalPathNameByHandleW with VOLUME_NAME_NT returns the resolved NT
device path](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew).

`d3d8-runtime-probe --mapped-image-diagnostics` is a separate read-only mode.
It captures the current executable and already loaded Kernel32, KernelBase
and GDI32 modules. It records their original logical names, mapped NT paths
and selected, bounded PE-header fields. For the executable's logical path
and each system module's logical and explicit Directory2W(I386) paths, it
opens a read-only file handle without write or delete sharing. It records
the final NT path, volume/file index, selected disk PE fields and two SHA256
reads while that handle remains held. Every successful open has an explicit
CloseHandle result. This yields four mapped-image records and seven file
observations on a complete run.

Path, header and file-identity comparisons are observations. They grant no
runtime admission. The existing frontend, systemDirectory, GDI, genuine
System D3D8, KMT-name and hardware gates retain their original bytes. The
mode rejects active diagnostic permissions and an already loaded D3D8/core,
returns before KMT or graphics runtime branches, and creates no graphics
device. It does not load D3D8, a core, Vulkan, Mesa or WARP.

The focused native packet builds only the changed probe and unchanged guard
translation units: two original I386 COFFs and one EXE. It retains the exact
575-file compiler manifest, 13 selected SDK headers and nine official link
libraries, source and target-state snapshots, and the unchanged owned raw
process component. One read-only mode execution uses the original 30-second
deadline; four malformed CLI cases must return 64. Original policy/callback
fixture binaries and CPU06 failure receipts remain separate.

Local strict I386 cross compilation and link passed with zero diagnostics.
The actual bounded selected-header parser passed 24 checks under GCC and
Clang ASan/UBSan, including truncated/bad headers and output nonpublication.
Native MSVC compilation, actual mapped/file observations and any I386 KMT,
HAL enumeration, render448 or Present64 acceptance remain pending a separate
target handoff and original-evidence review.
