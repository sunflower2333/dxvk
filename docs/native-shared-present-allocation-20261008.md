# Shared present backing and opened standard primaries

A plain shared present buffer now owns one linear KMD allocation. Its
`PresentSurface` pins the same `SharedSurface` owner held by render-target and
shader-resource views. Present, residency and allocation identity rotation all
refer to that allocation. DestroyResource drops its presentation reference;
it does not deallocate the backing while a view still holds the shared surface.
There is no second AllocateCb for BIND_PRESENT.

Opening a standard-primary allocation now attaches the same presentation
owner. The external primary remains borrowed; the previously implemented
internal staging allocation is the only allocation this device deallocates.
Present uploads through that staging using the existing primary copy command.
SetDisplayMode supplies the original borrowed primary handle, never the staging
handle. The callback changes scanout and does not transfer allocation ownership.

For shared presentation, a dirty cache publishes its newer local contents once.
A clean cache always refreshes at the presentation handoff boundary before the
unconditional synchronized upload required for presentation. Thus an opened
primary's first Present cannot overwrite its owner's frame with an unread
local cache. Flush/Present retain the existing epoch boundaries. A transfer
also rejects recursive Map of its staging texture from a runtime callback.
The normal resource/device liveness checks run after every callback boundary;
resource retirement cancels subsequent presentation even though the local
shared owner remains pinned to balance an in-flight map.

The supported new creation shape is the existing plain SHARED shape plus
BIND_PRESENT: DEFAULT Texture2D, one mip, one array element, one sample,
no CPU access, RGBA8 or BGRA8. Shared creation with pPrimaryDesc still fails:
the current created primary has a resource-associated primary/staging pair,
and opening that pair has no negotiated multi-allocation ABI. Stereo, flip,
keyed mutex, arbitrary shared-resource formats and ordinary version discovery
remain separately gated. This patch does not register a default UMD.

The existing typed opened-primary fixture retains its original three-profile,
three-format 36-image/1260-pixel and 78 rejected-open assertions. A second
phase exercises D3D10/D3D10.1/D3D11 tables with opened primary Present, external
updates after an earlier read in the same epoch, local writes followed by SetDisplayMode, shared present
creation, render-target-view lifetime, nested Present/Resolve, retired resource
handles, invalid shared-present shapes, and resource/device destruction during
LockCb, PresentCb and SetDisplayModeCb. It expects 48 images/1680 pixels and 60 additional negative controls.
The phase writes 240 actual files: native/public/KMD words, observed pitches,
handles and exact allocation metadata for each image. Its public D3D11
counterpart runs on the same WARP backend with the original caller pixels.

`tests/verify-shared-present-originals.py` independently reopens those files,
requires their exact inventory and allocation layout, and calculates expected
pixels directly from the seed and coordinates. The fixture uses a controlled
WARP backend and controlled runtime callbacks; passing it proves typed callback
and pixel behavior, not hardware/system-runtime/DWM admission. Native fixture
execution is pending. Local reader controls are explicitly synthetic and do
not stand in for those original native outputs.

The contract audit used the local Microsoft documentation checkout:

- `d3d10umddi/ns-d3d10umddi-d3d10ddiarg_openresource.md` and
  `ns-d3d10umddi-d3d10ddiarg_createresource.md`: runtime/kernel resource
  identity, one open allocation, and resource binding inputs.
- `dxgiddi/ns-dxgiddi-dxgi_ddi_arg_present.md` and
  `nc-dxgiddi-pfnddxgiddi_presentcb.md`: valid resource/allocation source and
  exact opaque DXGI context forwarding.
- `dxgiddi/ns-dxgiddi-dxgi_ddi_arg_setdisplaymode.md` and
  `d3dumddi/nc-d3dumddi-pfnd3dddi_setdisplaymodecb.md`: resource surface and
  underlying primary allocation selected for scanout.
- `dxgiddi/ns-dxgiddi-dxgi_ddi_base_functions.md`: separate DWM PresentBlt
  copy contract. It does not establish full DWM readiness by itself.
