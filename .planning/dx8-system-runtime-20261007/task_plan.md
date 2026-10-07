# DX8 system-runtime slice

Base: b75d6d583aa587da2f78b4e7183d3da14e5b373f, isolated categorized worktree.
Root owns all target operations, D3D9 lifecycle, integration and CI pushes.

1. Completed: audit original I386 Microsoft D3D8 image, Interface8/CAPS12,
   current typed core, old ARM64 frontend dependency and x86 private loader.
2. Completed: separate x86-only, process-local read-only frontend and exact
   module/name selector, with original caps passthrough and blocked CreateDevice.
3. Completed: meaningful portable selector controls, strict x86 cross compile,
   original COFF/PE/export/import inspection and all source hashes.
4. Completed implementation: reviewable native x86 packet/helper and portable
   policy runner. Freeze exact committed objects and hand off original hashes
   plus the explicit pending native/runtime/hardware gates.

Actual production core identity is pending the future exact-source consolidated
I386 CI DLL. The b75 native checkpoint built CPU fixtures only. This frontend
verifies the original future DLL path/hash against an independently joined CI
source receipt; it does not rebuild or relabel a candidate. CPU-only builds
need no core. Native PS parse/compile and genuine runtime trace remain pending
exclusive target coordination with root.

Production admission/caps/exports remain unchanged. No target commands,
hardware/selector execution, registration, installation, push or CI dispatch.
