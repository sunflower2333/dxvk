# Findings

Actual base is e2081913ed2df8d7218363dbede146e3b45f90d8.
ROOT actual CI54 report: CI37761389362 terminal failure, first profile0/X8
Present backing comparison after source update4; pfnBlt returnedS_OK. The failed
word was not saved; no actual alpha-channel difference can be claimed.
Source bltTexture2D always draws from sampled scratch into an RTV even for
same-format, identity unscaled copies. Direct raw CopySubresourceRegion can
avoid interpretation of unused X8 channel and unnecessary shader work, while
private deferred recording/ExecuteCommandList(TRUE) retains application state.

Existing bltData owns a device serialization worker, resource reservations,
pinned backend/context/resource/surface references, rotationActive and live
checks. The optimization must remain inside that established ownership path.
Current blt fixture keeps30 snapshots1536pixels and original raw-reader oracle;
new controls should add an independently identified phase without masking words.

The optimized path validates actual-format equality, rotation1, noflags1/4,
whole-selected-source extent. A private deferred raw copy executes with TRUE;
exact COM texture/subresource alias is a no-op under validated whole bounds.
The existing outer ownership/shared publication/Present tail is unchanged.

Actual DXVK predication is still a stub. Native WARP central-helper controls
are explicit; immediate shared refresh/publish predication remains a distinct
preexisting gap and cannot be admitted from those controls.

New phase:3typedprofiles,4formats,120images15624pixels,240originals;24images
are native helper-only sameimage sub1to3 alias supplements because DDI
presentable sources currently require1mip/1slice. X8 words compare4bytes.
Existing legacy30images1536pixels/68original byte+program oracle retained.
