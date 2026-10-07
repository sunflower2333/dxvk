# Genuine I386 process API providers

The limited USER names05 probe failed before its first KMT call: the combined
`GetProcAddress` guard reported error 127. The original failure and release
remain retained; that run supplies no I386 UMD filename or graphics admission.

A separate read-only collection copied the guest's genuine SysWOW64 providers
and native API-set schema. Source, copy and source-after bytes match:

| Original | Machine | Bytes | SHA256 |
| --- | --- | --- | --- |
| SysWOW64/kernel32.dll | I386 | 679024 | 51a5f4434691ea46e074957c1d41be6bd69f6050389600801cf1043915272237 |
| SysWOW64/kernelbase.dll | I386 | 2672464 | 612baaac75dcb1de8bd62815aec4c211abbf3b7fd6be0ede3ec27fac8885efbf |
| System32/apisetschema.dll | ARM64 | 140192 | e9418b4e61bfadc862bfc9e4d09813a333e18e9eb3dfdcb3104737c8922599c0 |

Kernel32's 1,656 named exports omit `GetSystemWow64Directory2W/A`.
`IsWow64Process2` forwards to
`api-ms-win-core-wow64-l1-1-1.IsWow64Process2`. KernelBase exports the actual
Directory2W implementation at RVA 0x20ca80 and IsWow64Process2 at RVA 0x14a5f0.
The native schema's WOW64 contract `api-ms-win-core-wow64-l1-1-3` names
KernelBase as its host. Independent LLVM and bounded raw PE reads agree.

The frontend and probe capture both Kernel32 lookups and errors before owner
queries or trace output. Only a missing Directory2W symbol with
`ERROR_PROC_NOT_FOUND` selects that same exact documented API from the already
loaded KernelBase. The explicit process/native-machine API remains required.
Both resolved address owners, Kernel32 and the alternate KernelBase provider
must have I386 images and exact paths within the returned canonical I386 system
directory. Legacy IsWow64Process remains diagnostic output only. The actual
D3D8 caller and GDI paths retain their original strict comparisons.

`--process-api-diagnostics` observes loaded providers, exact lookup errors,
resolved owners and the canonical directory; it makes no KMT query, graphics
factory or core load and publishes no admission. Native CPU build execution
of this mode is separate from later limited USER names, HAL enumeration,
offscreen448 and Present64 phases.

Originals: workspace
`artifacts/dxvk-native-d3d8-system-device-20261008/guest-wow64-export-originals-02/`,
16-file TAR SHA256
`504184770a846a14bcfcdd304594ee3e4fdfb696993d9a316e7d29595205ef14`.
The read-only worker and tar process exited and drained; the signed SYS,
package, service, PnP, static34 registration and desktop remained unchanged.
Two initial host path-spelling guard failures ran no worker and are retained
separately. Actual Directory2W invocation through the repaired resolver awaits
the separately authorized native CPU build.

Official signatures and public API requirements:
[IsWow64Process2](https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-iswow64process2),
[GetSystemWow64Directory2W](https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-getsystemwow64directory2w).
