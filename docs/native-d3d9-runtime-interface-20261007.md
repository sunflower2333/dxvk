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
Strict native ARM64 fixtures for exact `c7e8953` now pass 43,067 adapter
and 538,869 device checks with zero warnings. The exact first stdout record
confirms both protected table publications executed. Root independently
verifies the original 35-member archive, 88 Git/Vulkan inputs, nine ARM64
objects, two executables and retained installed driver/desktop state in
`artifacts/dxvk-native-d3d9-interface-20261007/root-native-device-interface-fixtures-verified-02.json`.
A stale host stdout assertion failed in review01; that original error remains
preserved and review02 checks the correct new guard record.

The separate exact `c0ea296` runtime frontend/probe also passes native compile,
ten malformed CLI controls, three null guards and read-only SSH enumeration
with zero warnings. Its lifecycle path reads only the negotiated Vista prefix.
The native script parser accepts the staged lifecycle family. Root independently verifies consolidated CI `37573265151`: all six jobs, five
original ZIPs, three architectures and15 native ARM64 fixture executions pass.

Actual `device-lifecycle-02` with that exact core now reaches back-buffer creation
through Microsoft System32 HAL9 and HAL9Ex. Both16x16 X8 requests use documented
DiscardRenderTarget (`0x1000`) with RenderTarget/NotLockable, pool3 and mip count0.
The current creation mask rejects that hint, and both APIs return `80070057`.
Both devices close; original signed SYS, desktop,34 registration/58 critical
fields, raw KMT names and process-local selector restore. This advances the
runtime gate but accepts no API device or pixels. The original42-member archive
SHA256 is `5c8740b37566a32c5ff30c4533b8dd1a2b918cc37539aa72874bf65e88ac2ed2`;
root restoration review is `root-system-runtime-device-lifecycle-02-verified-01.json`
in the same evidence directory. Next implement and verify the target discard hint.

The production OpenAdapter export and ordinary runtime DX8-DX11 rendering,
visible presentation, reset, sharing and full Display+Render acceptance remain
open. Earlier typed GPU checkpoints retain their existing scope.
