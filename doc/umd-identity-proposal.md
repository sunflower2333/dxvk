# Native runtime adapter identity proposal

This is a proposed private contract and a tested decoder only. The current KMD
does not produce it, and the native DXVK UMD is not registered with Windows.
No KMD implementation is changed in this branch.

`D3D10DDIARG_OPENADAPTER.hRTAdapter` is an opaque runtime handle used only with
runtime callbacks. It is not a KMT adapter handle and cannot be converted to
an LUID. `pfnQueryAdapterInfoCb` returns the miniport's private adapter data for
that exact runtime adapter. Its buffer is output-only, so placing a request
header in that buffer is not a valid query discriminator.

The documented identity source is `DXGK_START_INFO.AdapterLuid`, supplied to
`DxgkDdiStartDevice` since Windows 8. A future coordinated KMD change must
capture it for the successful adapter start, invalidate it on stop/removal,
and return it only while the adapter is ready. A GPU reset generation is not
an adapter LUID. No generated identifier or hardware-ID match can replace it.

## Proposed compatible reply

Keep the existing 128-byte `VIOGPU_WDDM_ADAPTER_INFO` v0 prefix byte-compatible,
including `Header.Version=0`, `Header.Size=128` and the zero reserved fields.
Append an optional independent 32-byte trailer only when the caller's output
buffer is at least 160 bytes. A smaller caller receives only its bounded
prefix, exactly as before. The trailer is not a new interpretation of v0's
reserved fields and does not change the allocation/submit ABI version.

All integer fields use little endian byte encoding, independent of pointer
width. Offsets below are relative to the start of the full reply.

| Offset | Bytes | Value |
| --- | --- | --- |
| 128 | 4 | Magic `0x44494c56` (`VLID`) |
| 132 | 4 | Trailer version 1 |
| 136 | 4 | Trailer size 32 |
| 140 | 4 | Flags 1: LUID valid; no other bits |
| 144 | 8 | Exact `DXGK_START_INFO.AdapterLuid` bytes |
| 152 | 4 | Node mask 1 for the current single-node adapter |
| 156 | 4 | Reserved, zero |

The future UMD must zero a 160-byte buffer, call `pfnQueryAdapterInfoCb` using
the original `hRTAdapter`, validate the complete prefix/trailer and nonzero
LUID, then require exactly one Vulkan physical device with `deviceLUIDValid`,
the identical 8 bytes and `VK_DRIVER_ID_MESA_TURNIP`. No adapter-index fallback
is allowed. Callback failure, old KMD, truncation, unknown version/flags,
zero LUID, unsupported node mask or duplicate Vulkan matches must fail.

`src/umd/umd_runtime_identity.h` implements the byte decoder with no Windows
handle casts; adversarial tests include all truncated reply lengths and an
old-KMD reply with a zero trailer. `umd_runtime_query.cpp` implements the
callback consumer as a development export. Its Windows CPU test supplies
mock callback replies only, checks the exact opaque runtime handle, verifies
the output buffer is zeroed and propagates callback errors. Wiring it to
OpenAdapter and implementing the KMD producer require a coordinated ABI
review and lifecycle validation.

## Microsoft references

- [OpenAdapter arguments](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_openadapter)
- [QueryAdapterInfo callback data](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/ns-d3dumddi-_d3dddicb_queryadapterinfo)
- [DXGK_START_INFO and AdapterLuid](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dispmprt/ns-dispmprt-_dxgk_start_info)

These signatures and field availability were checked on September 11, 2026.
