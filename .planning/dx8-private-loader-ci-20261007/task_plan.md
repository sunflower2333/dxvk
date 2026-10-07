# DX8 future CI core configuration

Pinned baseline: 2c1b0e2fdf3d35ff8495c39c979b36370ead918b. Root integrates and
pushes; this checkout performs local work only. Target CPU remains reserved
for the other owners.

- [x] Audit the sole workflow and original I386/Mesa source and artifacts.
- [x] Select architecture-specific private Khronos loaders in the existing workflow.
- [x] Retain generated Meson configuration, source snapshots and original DLL identity.
- [x] Compile the actual private loader source locally and verify configuration/PE joins.
- [x] Prepare precise deferred native parser/build/collection commands for frozen77bbe.
- [x] Commit and hand off exact paths, hashes, limitations and required future CI joins.
