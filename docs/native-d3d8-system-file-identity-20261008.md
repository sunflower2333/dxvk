# System D3D8 image identity on ARM64 Windows

The native I386 CPU07 observation at source `f9722b3` establishes why a logical
module-name comparison stopped the previous names phase. Kernel32, KernelBase
and the already imported GDI32 are I386 images. `GetModuleFileNameW` reports
`C:\Windows\System32` for them. Their mapped NT filenames and the final paths
of both logical and explicit `GetSystemWow64Directory2W(I386)` file handles
resolve to the same files in SyChpe32. All three pairs have the same file ID,
length, SHA256 and selected PE headers; the seven read-only handles were closed.
The original 75-file archive is SHA256
`e30e49145377efaeda730903faa04b595a8fcb7660c2d2238d4fcee2f6b2df78`.

The repaired probe and diagnostic frontend share
`tests/umd-d3d8-system-identity.h`. It requires all of these observations:

- `IsWow64Process2` reports I386 process, ARM64 native machine and four-byte
  pointers; Directory2W is called with the explicit I386 machine value.
- The exact documented Directory2W API comes from Kernel32, or the already
  loaded KernelBase only after the Kernel32 lookup fails with error 127.
  Resolved API addresses must belong to these exact named loaded providers.
- Each named loaded module has bounded I386/PE32 headers. Two read-only file
  handles open its logical filename and its explicit I386 system-directory
  filename while denying write and delete sharing.
- Both handle final NT names equal the mapped image's NT name. Both full file
  IDs, lengths, independently calculated SHA256 values and selected PE fields
  agree. Hashes and file metadata remain unchanged while the handles are held.
- The loaded selected headers and named module handle remain unchanged. Both
  file handles close successfully before the helper returns true.

This check also applies to the actual Microsoft D3D8 caller and runtime image.
It has no SyChpe32 suffix whitelist or allowance based on a System32 string.
The existing private core path, source/hash pins, process permissions, limited
USER checks, adapter LUID, callback ownership and graphics oracles remain in
place. Production OpenAdapter admission and driver registration are unchanged.

ARM64 management's explicit SysWOW64 inventory and the actual I386 process file
view are distinct evidence. CPU07's I386 Kernel32/KernelBase/GDI32 hashes are
`c587716b...`, `8dc3ef47...`, and `f464459d...`; they are not relabeled as the
management inventory's copies. The module helper logs actual file-view hashes,
selected PE fields, mapped names, handle IDs and closes for independent review.

The focused native packet compiles four original I386 COFFs and three PEs:
frontend, probe plus guard, and the included shared predicate fixture. It runs
the predicate fixture, four malformed CLI cases and one read-only process/API
observation including the already loaded GDI32. It does not call KMT or a D3D
factory, load a candidate core, enumerate HAL adapters, create a graphics device
or render. Runtime names, enumeration, offscreen and Present remain separately
admitted steps. The old phase verifier's logical-path equality must be updated
to validate these complete per-module originals before any fresh names attempt.

Microsoft documents the relevant name and identity APIs in
[GetModuleFileNameW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew),
[GetMappedFileNameW](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmappedfilenamew),
and [GetFinalPathNameByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew).

The included predicate fixture is also registered in Meson, all three backend
builds and artifact copies, the bounded native ARM64 case map, and the existing
Linux sanitizer script. It tests shared parsing/joins with no Windows API call.

Local strict I386 cross compilation and portable predicate controls establish
source behavior only. Native MSVC and actual Windows API results for this
repaired source are pending a separate target handoff.
