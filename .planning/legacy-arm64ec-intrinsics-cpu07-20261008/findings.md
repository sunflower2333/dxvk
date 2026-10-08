# Findings

Actual CPU06 cl ARM64 and ARM64EC and native link pass; hybrid link emits _mm_getcsr/_mm_setcsr unresolved EC Symbol via libucrt _fenvutils/ieee members. Native full response includes object, ARM64 kernel32 and four ARM64 static CRT libraries; hybrid adds EC object and x64 kernel32. Existing producer sets LIB to selected ARM64EC support-object + x64-compatible CRT/SDK paths. Local original target14.50 support inventory includes ARM64EC objects, no intrinsic helper library has yet been proved.

Official local Windows SDK26100 softintrin.h exists; inspect its exact EC implementation and compare genuine EWDK headers/libraries before any source fix. No guessed stub or mixing native/EC library scopes.

verify_ewdk confirms the original EC object already has /DEFAULTLIB:softintrin.lib; llvm-readobj independently reads the exact original COFF-ARM64EC object with LIBCMT/OLDNAMES/softintrin directives. The current CPU06 archive contains library metadata, not softintrin/library bytes, so a new helper name is not proved. Local /home/sunf/winarm64-toolchain/crtlib/libvcruntime.lib contains real ARM64EC exit-thunk references to _mm_getcsr, but its5417000/a96ec519 bytes differ from target4707222/ea91b5f9; do not relabel it as target.

Official ARM64X build guide requires matching CRT settings and combining each ABI inputs; EC permits x64-compatible libraries, native ARM64 does not. Comparing Microsoft-owned SDK28000.2526 arm64/x64 NuGet archive symbols locally (comparison only, not same target pins) may identify the missing provider. No source fix chosen yet.

Real SDK range/COFF comparison proof11517/b99f16ce passes; 10 genuine closed LLVM children; A641 CSR definitions and exact weak alias indices read from actual original bytes. ARM64 helper1136438/e140f4af; x64 helper544350/57aad423. Source selects same-SDK native um/arm64 sibling, only explicit hybrid input; original EC search unchanged. Target mounted library remains unproved until freshCPU07. Source dumpbin gate checks original A641/member/section-backed definitions/weak aliases; exact alias auxiliary mapping is independently rejoined by CPU07 offline COFF reader.
