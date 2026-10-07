# DX8 system-runtime progress

Created fresh categorized isolated worktree after worktree/state audit.
Native x86 CPU checkpoint is complete and root/DX11 exclusively own target.
All subsequent work here is local, with no shared production edits.

Strict local x86 cross compilation/inspection now passes four original COFFs
and three I386 PEs, with only OpenAdapter exported by the frontend. GCC/Clang
ASan+UBSan each pass306 positive checks and four intended semantic controls
(eight negatives total). Original local proof04 retains source before/after,
actual commands, compiler versions, logs and PE/import/export hashes.
Native Windows results and genuine system runtime/HAL execution are pending.
The original short invalid hex test literal and earlier host-verifier error
are retained separately; neither is a production or native-runtime result.

The checked-in portable policy runner passes306/306 and eight intended
controls, with source before/after receipts. Older DX8 entrypoints were adapted
to link the separate guard unit; their independent local cross gate passes18
COFFs and9 PEs across all three architectures, plus original caps/PE-parser
sanitizer checks and two caps controls. All target access remains deferred.
