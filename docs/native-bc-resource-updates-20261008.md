# Native BC1–BC5 resource updates

`ResourceUpdateSubresourceUP` previously rejected every compressed texture
because its subresource lookup required a nonzero uncompressed texel size.
The backend could already create BC resources, so legal CPU uploads to those
resources failed before dispatch.

The update path now recognizes the8-byte BC1/BC4 and16-byte BC2/BC3/BC5
blocks, including their typeless, sRGB and signed variants. It validates
4×4-aligned left/top coordinates, right/bottom alignment or the exact logical
mip edge, padded block-row pitch, and the last source byte against the caller's
address range. Small1×1/3×2 logical mips still read one complete block.
The original pitches and texel-coordinate box are passed to the backend.
Empty or reversed boxes are no-ops after resource ownership validation.

This is a separate block contract: `transferTexelBytes` still returns0 for
compressed formats. Only UpdateSubresource opts into BC Texture2D metadata,
which also covers cube-face subresources. CopyRegion and volume creation keep
their existing block restrictions until their own geometry is implemented.
No format capability, feature-level, version admission, INF or default driver
registration changes in this patch.

`dxvk-umd-bc-update-test` links the production D3D10 and D3D11 DDIs to an
explicit test-only WARP factory. It uploads all15 BC1–BC5 format variants to
five mips/two array layers and separately exercises BC1 and BC5 cube faces.
Full uploads use padded, non-block-multiple source pitches; partial updates
include a6×4 mip edge and smaller-than-block mips. Per case, twelve malformed
updates and four empty-box calls must preserve native bytes and private storage.
The independently updated public D3D11 texture is read separately.

Expected native run totals are552 uploads,136 no-ops,408 rejections,840
subresource snapshots and130,944 compressed-byte observations. Each snapshot
writes original native/public Map bytes and actual pitches:2,520 files.
`tests/verify-bc-update-originals.py` reopens the exact file inventory and
reconstructs expected bytes without importing production geometry.
Its synthetic2520-file positive and ten corruption controls test only the
reader; they are not Windows native execution.

Local verification: production UMD, typed fixture and policy compile with
actual official SDK/WDK headers, optimized x64/x86 COFF and strict warnings;
all six exit0 with empty diagnostics. GCC and AddressSanitizer/UBSan Clang
policy runs each pass5,780,432 boundary/layout checks. Original start/process/
stdout/stderr receipts are preserved in worktree`artifacts/bc-local-01`.
Native fixture execution and ordinary hardware/runtime acceptance remain
pending CI/VM testing.

Contract sources: local Microsoft DDI snapshot
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_resourceupdatesubresourceup.md`
(byte pitches and source pointer),
[Microsoft BC storage and mip padding](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression),
[Microsoft UpdateSubresource box/no-op contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-updatesubresource),
and the existing upstream DXVK`D3D11CommonContext::UpdateTexture` and
`util::isBlockAligned` implementation (aligned offset, extent or exact mip edge).
