# ARM64EC intrinsic producer diagnosis and CPU07 preparation

Goal: diagnose genuine CPU06 ARM64X _mm_getcsr/_mm_setcsr EC thunk failures and implement a supported tiny producer fix, then prepare fresh CPU07 source and offline originals. CPU06 remains immutable. No target/MAIN/push calls or new agents; verify_ewdk_build owns KMD03 target.

1. Inspect actual CPU06 responses, libraries/header originals and local official MS docs: complete.
2. Identify supported helper/library/configuration correction: complete; genuine SDK ARM64 archive has A641 CSR provider and exact COFF weak-alias indices.
3. Implement source delta and meaningful local validation: complete locally; actual PS5.1 and native tool stages pending CPU07.
4. Prepare fresh CPU07 packet/offline reader with exact source pins: pending.
5. Freeze and hand off to ROOT: pending.

Errors: first lookup .planning/legacy-runtime-deploy-20261008 did not exist; actual plan is legacy-native-static-crt-cpu06-20261008. Whole packet text search returned excessive receipts; subsequent reads use exact original source/response leaves. Actual LINKREPRO RSP is UTF16; decoded reads will preserve original bytes. Collection top-level has no members field; use original reader layout before accessing it.
