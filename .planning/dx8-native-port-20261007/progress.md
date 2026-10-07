# DX8 progress

Created isolated worktree work/dx8-native-port-20261007 from c0ea296 after
worktree audit; shared/main sources and all reserved files preserved.

Implemented caps converter, genuine system8 probe, bounded PE32/PE32+ import
parser, native ABI/semantic fixtures, independent pixel oracle and native x86
CPU packet/build preparation. GCC/Clang sanitizer positives543/4237 PASS;
both exact SM2-cap regression negatives compile then exit1 at intended CHECK.
Strict local cross compiler/linker PASS15 COFF/9 PE across x86/x64/ARM64.
Original Microsoft mapped image parser PASS, I386 slot b11d4. Oracle synthetic
selfcheck PASS4 rejects, explicitly not hardware evidence. Source before/after
hashes and original warnings/logs retained under local-verification02/01.

Final local review and reserved-file identity checks pass; branch committed and
frozen x86 CPU packet is emitted from that exact commit for parent handoff. No target transfer,
build, selector, registry write, hardware or CI action; native EWDK execution
awaits exclusive target release from root/DX10.

Second slice authorized against b61: immutable8/9 identity and caps12 are
implemented with callback/metadata snapshots and strict API matching. Real
backend uses non-Ex8 parent then D3D8 flag;9 default remainsEx. New typed
fixture cases retain every old9 case and exercise bounds/lifetimes/mutations.
Final cross05 compiled33COFF/linked6fixturePE including real private
parent/backend and Windows SM1 regression onx86/x64/ARM64. Malformed8query,
all reserved flags and nonzero FVF triangle byte forwarding are included.
Both typed fixtures also pass source Wshadow/Werror; original shadow review
warning and earlier failed attempts remain separate immutable evidence.
GCC/Clang actual SMconverter47 and APIbounds2800 PASS. Four intended semantic
negative controls compile0/exit1 PASS. No target action/PE execution/push/CI.
