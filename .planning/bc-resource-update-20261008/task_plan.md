# BC resource updates

Goal: make legal BC1–BC5 D3D10/11 UpdateSubresourceUP work with block-aware region and pitch validation, proved by native/public raw readback. Retain ordinary feature/version gates until full runtime proof.

1. Read implementation and local Microsoft block/update contracts (complete).
2. Add shared format block geometry and update layout validation, coordinate copy agent boundaries (complete).
3. Add typed native/public compressed-byte readback and malformed-update controls (source complete; actual Windows execution pending).
4. Strict local compile, portable policy verification and synthetic reader controls (complete); commit and handoff (in progress).

Local proof is not ordinary hardware completion. ROOT must integrate both Meson targets into the current NativeUMD build/run/shipping and archive the2520 originals before fresh CI execution. No remote target calls or MAIN edits in this scope.
