# Queued optional shared primary and MSAA copy harness

This harness starts at published `beb465cba602b80c941dc955f4e17e60a9926b3e`
and depends on the separate frozen source packet
`5f6277dcf40fa14cc43af8e9abaa52514272d70f` and MSAA color copy packet
`02e47e6e3bbe55e6593154b20708041dfd228742`. Both frozen source packets remain
unchanged. Integrate the source packets and this harness only in a later consolidated publication
after CI63 has reached an actual terminal state; this worktree does not publish
or execute workflows.

The build harness compiles and ships the optional shared fixture,
`dxvk-umd-msaa-copy-policy-test.exe` and `dxvk-umd-msaa-copy-test.exe`. It executes
them through the existing bounded x64/x86 runner and requires both frozen
independent raw readers.
ARM64 appends three exact markers and two fresh original directories with
their readers to the existing bounded runner. Both new directories are uploaded
recursively even on failure.

The exact native marker is:

```
DXGI optional shared primary PASS checks=<n> profiles=3 formats=2 images=24 pixels=840 negatives=12 raw_files=120 hardware_admission=0
```

Native raw directories are `dxgi-optional-shared-primary-originals` on x64/x86
and `arm64-dxgi-optional-shared-primary-originals` on ARM64. Each contains exactly
120 independent originals: 24 images with actual, public-reference, kernel,
metadata and allocation-wire files. The reader validates 2520 image observations
alongside pitches, padding, allocation identity and wire fields. Its command is
`python tests/verify-optional-shared-primary-originals.py --directory <raw-dir>
--output <outside-raw-dir>.json`. Reader JSON is always outside the raw directory.

The exact MSAA markers are:

```
MSAA color copy policy PASS checks=236 single_quality_ignored=1 hardware_admission=0
MSAA color copy PASS checks=<n> profiles=2 scenes=36 copies=32 snapshots=204 bytes=78336 rejections=84 noops=24 raw_files=612 hardware_admission=0
```

MSAA raw directories are `msaa-copy-originals` on x64/x86 and
`arm64-msaa-copy-originals` on ARM64. Each contains exactly 612 originals:
204 native planes, 204 public planes and 204 metadata files. The frozen reader
validates 78336 bytes per role, 156672 byte observations, 84 rejected requests
and 24 no-ops. Its command is `python tests/verify-msaa-copy-originals.py
--directory <raw-dir> --output <outside-raw-dir>.json`. Its result requires
`verified=true`, `profiles=2`, `scenes=36`, `copies=32`, `snapshots=204`,
`raw_files=612`, `bytes_each_role=78336`, `byte_observations=156672`,
`rejections=84`, `noops=24` and an exact `original_files` closure of 612.
The optional reader instead reports `passed=true` and an `originals` closure
of 120. Both readers keep hardware admission and registration false.

The preserved optional-only intermediate proves 64/51/74. The final combined
source selections increase from 63 to 66 ARM64 cases, from 58 to 61 fixed
backend fixtures (plus the existing five cube-loop fixtures), from 50 to
53 mandatory shipping fixtures, and from 73 to 76 total shipped executables.
All existing case markers, runner bodies, readers, raw directory mappings and
recursive upload paths are preserved. No ordered-helper cases are added.

Actual local source observer 3379670 exited zero and confirmed these counts,
all 63 old case patterns, and complete byte preservation of existing runner,
reader and upload bodies. Local validation is a source and frozen-original review only. The native fixtures
and public WARP controls have not run here. Successful future native CI controls
would still not prove ordinary DXGI sharing, primary scanout, VIOGPU execution
or Microsoft runtime replacement. Full created shared scanout pair support
remains unsupported; the frozen source only admits the optional NO_SCANOUT
fallback. Runtime and hardware admission gates remain closed.
