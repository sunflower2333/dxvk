# Native D3D9 runtime interface, 2026-10-07

The native9 bridge now reports the original Vista DDI (`0x000c`) and writes
only its 99-pointer device table, through `pfnRename`. Both the outer adapter
and inner device publication use that boundary. Modern SDK types remain local
superset storage; newer callback slots belong to neither this interface nor its
caller output. D3D10/11 interface negotiation remains separate.

The actual Microsoft System32 runtime exposed the mismatch in
`device-lifecycle-01`: exact frontend `889acc3` forwarded both HAL9 and HAL9Ex
creation to the verified `c8fbd55` core from CI `37569563644`. Both private
creates returned `S_OK`; Windows immediately destroyed them and the public API
returned `8876086a`, before any resource or render-state callback. This run
verified restoration and retained evidence, with no rendering acceptance.

Original evidence is under
`artifacts/dxvk-native-d3d9-caps-20261007/` in the workspace. The 42-member
`dxvk-system-runtime-device-lifecycle-01-evidence.tar.gz` SHA256 is
`2e40fa9f916456059a15d2c4f3b6a3ba5e09d3a69a4dfc7b03ac2ec99ffed485`.
The unchanged system DLL SHA256 is
`0658854da684062f722bd96135f40109f7beb7d724866d55cd4f8194e3d7d248`.

Its matching public PDB places native `ValidateUMDeviceFuncs` at RVA `0668d8`
and `IsDDIVersioningInvalid` at `062378`. At driver version `>=0x4002`, the
validator requires old BufBlt/TexBlt/VolBlt/CreateResource callbacks to be null
and their newer counterparts to be non-null. At version `>0x6000`, it also
requires AcquireResource and ReleaseResource. The old native9 table reported
the SDK default `0x11000` while providing the original resource/blit callbacks
and omitting those synchronization callbacks. That combination is rejected.

The local official SDK defines Vista as `0x000c`; its original device table
has exactly 99 pointers. [Microsoft's adapter contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddiarg_openadapter)
distinguishes the caller runtime build from the driver's returned interface.
The implemented DDI uses the original layout. Unsupported newer interfaces
remain unclaimed, including newer asynchronous resource ownership.

Native fixtures now put exactly that table at the end of a writable page and
protect the next page. They exercise both adapter and direct-core creation,
verify the leading canary, flush and destroy each real fixture device, and
check that newer storage remains unchanged in ordinary current-SDK callers.
Native build, consolidated CI and a new actual runtime lifecycle trace are
required before accepting this change's target behavior.

The production OpenAdapter export and ordinary runtime DX8-DX11 rendering,
visible presentation, reset, sharing and full Display+Render acceptance remain
open. Earlier typed GPU checkpoints retain their existing scope.
