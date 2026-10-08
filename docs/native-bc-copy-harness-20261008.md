# BC regional-copy native fixture integration

The native development harness builds and runs the two targets declared by
frozen BC source `084d70a9fc837bf149ef982b947e309faf24993a`:
`dxvk-umd-bc-copy-policy-test` and `dxvk-umd-bc-copy-test`.
The policy requires 164777 checks. The native fixture requires two profiles,
20 scenes, 160 copies, 400 rejections, 120 empty no-ops, 180 snapshots,
3600 subresources and 237312 bytes per native/public role.

Each x86/x64 backend run uses a newly created `bc-copy-originals` directory.
The ARM64 run uses `arm64-bc-copy-originals`. Process receipts and reader JSON
remain outside the original directory. Every architecture must pass the frozen
`tests/verify-bc-copy-originals.py` independent reader and explicitly report
540 raw files, 474624 byte observations, 180 snapshots, 3600 subresources,
`hardware_admission=false` and `registration=false`. The ARM64 artifact upload
retains the raw directory recursively.

All existing 66 native case markers and reader branches are preserved. The
new case table, direct backend calls and shipping lists derive 68 ARM64 cases,
63 fixed backend calls plus five cube-loop cases, 55 mandatory fixtures,
78 binaries in the complete shipping union and 204 expected native children.
The module-local loader and owned process-runner source are unchanged.

Local source/original checks establish exact additive harness edits and pinned
fixture/reader/Meson declarations. Native Windows execution remains CI/target
work owned by ROOT. This packet changes no production resource handler,
feature/version discovery, registration or hardware admission gate.
