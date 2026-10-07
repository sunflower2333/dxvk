# Native D3D11 hull phase instance declarations

The original FXC hull shader in the typed D3D11 WARP fixture failed in
`decodeShader11` before a native shader creation call. The decoder did not
recognize register 23 (`vForkInstanceID`) or register 24 (`vJoinInstanceID`).
These are scalar inputs to individual hull phases; they do not belong in the
graphics or patch signature register files.

The correction tracks the active hull control point, fork, or join phase. It
accepts a plain `dcl_input` instance declaration only in its matching hull
phase, checks its canonical scalar operand shape, excludes indices,
modifiers, selection bits and extra operands, and rejects a second instance
declaration within the same phase. A subsequent fork or join phase has a new
instance register. Hull phase markers are rejected in other shader stages.
The original instruction words remain unchanged in SHEX, and the existing
implicit identity control point signature path is preserved.

The SDK defines operand types 23 and 24 as the fork and join phase inputs.
Microsoft documents the scalar register and phase boundaries in
[hull shader registers](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/registers---hs-5-0),
[the fork instance declaration](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-input-vforkinstanceid--sm5---asm-),
and [the join phase](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/hs-join-phase--sm5---asm-).

## Original native evidence

The single frozen diagnostic over `634d78110d84752b94505bf80195661aff0ea956`
ran on the existing ARM64 Windows guest. Native PowerShell 5.1 parsed all
three helpers without errors. First-party production and fixture units
compiled with `/W4 /WX /MT`; the pinned external parser retained its existing
`/W3` policy. The first 13 graphics shader instances decoded successfully,
including the signed two-stream geometry shader. The fourteenth instance was
the failing hull shader:

- Stage 3, profile `hs_5_0`, 54 code words, 796 original DXBC bytes.
- Original `CreateHullShader` returned `S_OK` and a non-null object both with
  and without class linkage.
- `decodeShader11` returned false with the runtime error state still `S_OK`.
- The exact declaration at words 12 and 13 is `0x0200005f, 0x00017000`.
  It occurs after `hs_fork_phase` and its instance count declaration.

The retained archive is
`artifacts/dxvk-native-dx10-dx11-20261007/guest-warp-634d781-diagnostic-01/native-warp-diagnostic-634d781-01-evidence.tar.gz`,
SHA256 `f9c4f03414d3ab39870ff826297ff66e42e93dd82661dd29fee4fc250a801991`.
Its independent review joins 88 base source files and the sole fixture delta,
21 headers, two copied API import libraries, 19 ARM64 outputs, six exited and
drained native children, all five completed transports, and 11 retention
controls. The driver, desktop and existing candidate pins remained unchanged.
Target CPU ownership was explicitly released after collection and joins.

The captured hull original DXBC has SHA256
`6b9a871aa97f433eacc90c10a0c4acf0692569041712b14ccf77a440aa9493e9`.
Its raw SHEX has SHA256
`d3f215bf793999e88af72a9b6f4fbf333efadd89af196ceae432a38c58d7e65a`.
A local Clang ASan/UBSan replay of the unchanged decoder reproduces rejection
in the default register case at source line 376 of `634d781`.

## Controls and remaining native verification

The portable fixture embeds those exact 54 words and checks reconstruction,
the unchanged instruction stream and DXBC hash, all four floating point
triangle patch factors, and the absence of an instance register in ISGN or
PCSG. Separate fork and join controls cover canonical zero-component,
one-component and x-mask forms, repeated independent phases, wrong stages,
wrong phases, output and system-value declarations, indexing, modifiers,
selection bits, unused operand fields, and duplicate declarations. Rejected
decodes clear all staged metadata.

GCC and Clang ASan/UBSan runs both pass 1,118 checks. The two deliberately
unknown extended-operand negatives emit two existing parser diagnostic lines
per run; no sanitizer diagnostic occurs. Six strict official-header x64/x86
COFF controls pass for the production shader unit and both modified fixtures.
Their original receipts and unchanged before/after source hashes are under
the isolated worktree's `artifacts/hull-phase-repair-01/` directory; the final
aggregate receipt is `local-hull-repair-controls-02.json`.

The typed native fixture adds 18 malformed hull creations derived from its
original FXC bytecode. Each must report one `E_INVALIDARG` callback, preserve
every private storage word and canary, and keep the exact previously bound
`ID3D11HullShader` pointer. The existing tessellation draw, pipeline statistics,
cyan pixel assertion and missing-domain-shader rejection remain intact.

The corrected production slice and its new native controls still require a
fresh strict ARM64 WARP run. The full WARP15 suite has not passed. Normal
D3D11 runtime admission and real hardware/presentation acceptance remain
closed; no installation, registry, GPU, push or CI action is part of this
correction.
