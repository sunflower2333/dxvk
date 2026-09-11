# Native staging and hazard checkpoint

Engine`5b0983d3a3d64919b030efd1a8e991bd48e1a92a`.
CI34612034025 ALL5PASS, ARM64/x64/x86 plus shader/identity checks. ARM64
artifact10268959166,3607249bytes; published archiveSHA256
`93967e0a73b014d33a53387c9b8ace6d18008b6defc70187f36aac3f7cf41982`.

Only the matched files under`.planning/dxvk-umd/probe-5b0983d-arm64/` are needed:

| File | SHA256 |
| --- | --- |
| viogpudxvk.dll | ee017f63070b403d046b807721ba70c41ddf2bbf71b6628e8ba095b513c034ca |
| dxvk-umd-ddi-probe.exe | 16db5c11d81e19de0844cd254bb1b8101fa7e13b0b723f3c591eb0e95acc0f29 |

STATUS.txt identifies exact5b0983d/ARM64; both are ARM64 PE. Run the usual
`<fresh memory-order 16-hex LUID> --native-copy` with current process-local ICD.
No installed driver replacement or registration. Parent owns execution.

Existing64-byte copy,640MSAApixels,4096shaderpixels/depth/stencil/KMT backing
must all pass. New output requires mapped=0 in both DDI_STAGING_BUSY lines,
including mapped_subresources=4 and invalid_rejected=1. The pre-query busy
result may be0or1 because completion can race the query. This query does not
map again or modify existing maps. The shader samples a GPU-cleared texture
and indexes a GPU-updated buffer after native RAW notifications; existing
full-image checks verify those transitions. The native SRV hazard signature
is device/view/resource as established by actual WDK, despite local markdown.

Target execution is pending while parent Windows VM is stopped. CI and this
standalone DDI path do not establish Microsoft runtime or visible Present.
