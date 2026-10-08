# D3D11 output binding on feature level 10

The merged ARM64 EWDK BGRA fixtures at `a6dd755` built and linked, then
`umd-dxgi-extended-blt.cpp:132` and `umd-dxgi-extended-primary.cpp:63`
observed a NULL public RTV after a successful native DDI binding. Their
retained stderr reads `rtv error=00000000`; the processes failed with
`0xc0000409`. These are failed runtime observations, not pixel passes.

The D3D11 DDI also serves logical feature levels 10_0 and 10_1. Its old
`setRenderTargets11` implementation always called
`OMSetRenderTargetsAndUnorderedAccessViews` with the complete eight-slot UAV
tail. A feature level 10 WARP backend rejects that combined binding, even
when those UAV pointers are NULL. The D3D10.1 DDI uses the legal
`OMSetRenderTargets` path already.

The corrected D3D11 DDI retains all handle ownership, RTV/DSV shape, RTV
range, UAV range and counter validation. It translates the complete UAV
state first. Below feature level 11_0, an actual non-NULL pixel UAV is
rejected before backend or tracked binding state changes; an all-NULL UAV
state uses `OMSetRenderTargets`. At feature level 11_0 the original combined
call, NULL tail, counter offsets and tracked shared-surface commit remain
unchanged. UAV update hints still do not limit the complete state binding.

This follows the local Microsoft
[SetRenderTargets(D3D11) DDI contract](../../windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d11ddi_setrendertargets.md):
RTVs and UAVs share binding points, and range hints describe changes while
the DDI binds the complete state. Microsoft's
[feature-level table](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro)
and [compute shader limits](https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader)
distinguish the optional single compute UAV at feature level 10 from the
eight pixel/compute UAV slots at feature level 11; the
[RWTexture2D contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-object-rwtexture2d)
requires shader model 5 for pixel UAV access.

The existing portable output-policy test covers an empty UAV state,
non-NULL UAVs independently in every slot, their return to NULL, and a full
non-NULL state for the lower-feature and pixel-UAV-supported paths. Strict
x86/x64 SDK compilation checks the affected production DDI translation
unit. The existing native BGRA fixtures and literal readers are byte
unchanged. Fresh native execution and unchanged raw pixel oracles remain
required; this source change does not grant ordinary runtime, hardware or
adapter-capability admission.

ROOT's fresh native ARM64 EWDK BGRA03 run at `47541bf2` builds 18 COFF objects
and two executables. Both unchanged fixtures exit successfully and produce their
complete original files, confirming that the previous NULL RTV failure is
removed. The unchanged literal reader still fails: `extended-blt-1-0-9` contains
24 zero words instead of `0xff808080`. That scene uses the ordinary fully typed
sRGB multisample resolve path on logical FL10_0. The corresponding profiles
10_1 and the PRESENT typeless resolve scene retain the expected bytes.

The [same-attempt source and closure review](../../artifacts/bgra-fl10-ewdk-20261008/independent-native03-review-01/same-attempt-native-originals-and-closure-verified-01.json)
retains the failed byte reader and closes all children while confirming the
installed driver, static registry values and desktop are unchanged. Pixel
acceptance remains false; the remaining resolve path needs a separate fix and
fresh native verification.

The subsequent [encoded multisample resolve fix](native-fl10-encoded-msaa-resolve-20261008.md)
also passes the original full BGRA oracle in ROOT's native EWDK run at
`189c473`. Both unchanged fixtures and the new encoded fixture exit successfully;
the independent readers verify 1836 original BGRA pixels and 3072 encoded
pixels. This closes the controlled WARP color regression. Embedded hardware,
ordinary factories and desktop composition still need current-core evidence.
