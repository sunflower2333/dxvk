# Native UMD Texture1D continuation (implementation only)

Base: `2f0c9205524667c9291ebc7ee2198ebf87419a8a` on
`work/native-offline-contracts-20260915`. This continues the resource contract
work without changing the driver-parent source pins or runtime admission.

## Implemented source paths

`D3D10DDIRESOURCE_TEXTURE1D` now reaches the embedded device's
`CreateTexture1D`, not a height-one Texture2D substitute. Mip count, array
count, logical width/mip progression, usage and the subset of bind/misc flags
are checked before the initial-data array is traversed. Initialization keeps
native subresource order (mip + arraySlice * mipLevels). Unused height/depth and
physical padding do not redefine the logical one-dimensional width.

SRV, RTV and DSV production DDIs now select the actual resource interface and
the matching Tex1D/Tex2D union. Texture1D SRVs normalize UINT_MAX mip/layer
counts to the remaining range before backend creation. RTV/DSV counts are
finite, nonempty ranges. Format compatibility and usage-specific restrictions
are additionally checked by the embedded D3D11 implementation.

View creation stages the COM view and retirement node locally, then publishes
runtime private storage only after success. Failure releases staged objects
and the temporary resource reference before invoking SetError; no later
DestroyView callback is assumed. Existing Texture2D creation uses the same
transaction. Successful DestroyView still uses the existing deferred child
retirement path.

GenerateMips handles an actual Texture1D/array SRV only when its resource was
created with matching autogen and bind flags and CheckFormatSupport permits
mip generation. The CPU does not generate texels or read GPU data back.

The output-merger describes Texture1D effective mip width, slice range and
unit sample count. OutputShape now records intrinsic dimensionality, rejecting
mixing Texture1D and height-one Texture2D views despite equal extents. The
existing sparse-slot, size/sample compatibility, overlap and one-shot binding
checks are retained. This does not add new MRT color formats or shader outputs.

The bounded CopySubresourceRegion and UpdateSubresource paths recognize 1D
mip/array subresource indices for the same RGBA8_UNORM/BGRA8_UNORM formats
already handled by the 2D path. They use a single-row/single-depth box and do
not multiply a mip width by an array count. Existing Map/Unmap, whole-resource
CopyResource and staging-busy forwarding operate on the real resource object;
they have not been replaced with new synchronization or copy emulation.
UpdateSubresource now rejects depth-stencil destinations before recording.
Two-dimensional subresource mip shifts are also bounded explicitly.

## Not completed or enabled

This is not complete resource, shader, MRT, or native system runtime support.
Texture3D/cube creation, shared OpenResource backed by the same kernel resource,
legacy pipeline/text callbacks, broader format-sensitive copies, primary/flip
Present, and native target acceptance remain separate tasks. No VKD3D source
changes belong to this commit. The existing missing-requirements bitmask,
feature-level admission, KMD, Mesa ABI and driver package registration are
unchanged. No installed driver or candidate source pin is updated.

## Validation status

Compilation, unit tests, WARP, embedded DXVK execution and target GPU tests
were intentionally **not run** for this continuation. `[skip ci]` is used for
this commit; permanent build/test workflows remain unchanged. The prior MRT
commit's green results are not results for this revision. Only source review,
patch formatting and byte/hash identity checks were performed. Any temporary
source-transfer job only reconstructs and stores Git blobs; it neither builds
nor executes the UMD or its tests.

Before later admission, exercise immutable/staging/dynamic/default resources,
non-power-of-two mip chains, array slices and all-remaining SRVs, 1D/2D mixed
OM rejection, DSV and SRV/RTV alias hazards, failed view creation and reentrant
device retirement, GenerateMips, partial updates and staging readback. Follow
with native-runtime/Turnip tests; a software reference is not target proof.

## Public contracts used

- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_createresource
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddi_mipinfo
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_tex1d_shaderresourceview
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture1d
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-updatesubresource
- https://learn.microsoft.com/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openresource
