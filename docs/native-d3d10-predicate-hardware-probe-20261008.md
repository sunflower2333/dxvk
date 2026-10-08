# D3D10.0 predicate probe through real KMT

This standalone probe checks actual issued predicates through the named
development entry `VioGpuDxvkOpenAdapter10_2ForTest`, the exact
`D3D10DDI_DEVICEFUNCS`/DXGI 1.0 tables and unchanged KMT callback transport.
It does not change public exports, admission masks or installed driver
selection. The first bounded hardware slice is predication; SO/MRT and
ordinary Microsoft factory creation remain separate gates.

The source starts at `2fbb79131da999d52caa92f7341ac14aca12c212` in
`reference/codes/dxvk-umd-dx10-kmt-predicate-probe-20261008`. Transport
`tests/umd-kmt-compute-transport.h` remains byte-identical, SHA-256
`d0837445866a083eacfd46af8d7bd27e65cc69c9dc174a45ba347948b7b61f0c`,
last changed by `7a47d036294f1196c79fd643404e377b40120542`.

The command requires an absolute named core DLL, exact SHA-256, nonzero
selected LUID as 16 hex digits in byte order, `10_0` and a fresh absolute
payload directory. The loader verifies the supplied file and actual loaded
module path/hash before and after execution. The transport rejects software
adapters, verifies the 160-byte KMD identity against that exact LUID, requires
WDDM 2.0 and a real paging queue, and forwards real QueryAdapterInfo, context,
allocation, lock, residency, render and escape callbacks. Runtime cookies are
stable and distinct from raw KMT handles; callback tables remain alive through
all child/device/adapter teardown. No Microsoft graphics device factory is
imported or called in this probe.

Actual strict FXC compilation/reflection uses the original System32
`d3dcompiler_47.dll`, SHA-256
`4b68cd1fc3d482d0965910f1995513549989ae17a53d9be122de08437102a1d1`.
The two SM4.0 programs are a fullscreen SV_VertexID vertex shader and a red
pixel shader. Their original HLSL, DXBC and SHDR tokens are retained. The
HLSL is 229 bytes, SHA-256
`2cb4e70b6b12c94bde9667253db41c88ba1c96f5cf63b1fe98821d9d6abf9812`.
Reflection supplies owned writable native signature entries.

Every image is 16x16 R8G8B8A8_UNORM. Copy to staging, Flush, blocking Map,
bounded row-pitch copy, Unmap and saving original bytes precede the literal
oracle. All 256 pixels and every channel must match exactly.

| Frames | Workload | Literal output |
| --- | --- | --- |
| 00–01 | Unbound clear and fullscreen draw | Black, red |
| 02–09 | End-only false predicate, both comparison values and four draw entrypoints | Four black, four red |
| 10–17 | Begin/real draw/End true predicate, both values and four draw entrypoints | Four red, four black |
| 18–25 | End-only false reuse of the same query | Four black, four red |
| 26–29 | False predicate/value false: Clear, Copy, CopyRegion, Update | Black for all suppressed commands |
| 30–33 | Same predicate/value true: those four commands execute | Red for Clear/Copy/CopyRegion, green for Update |
| 34–35 | Unbound clear and draw after suppression | Green, red |

Four draws are Draw, DrawIndexed, DrawInstanced and DrawIndexedInstanced.
Indexed draws use an actual initialized `{0,1,2}` R32_UINT index buffer.
Predicates have flags zero, with no PREDICATEHINT. No CPU QueryGetData is
called between issuing a generation and any binding in that generation.
One post-use QueryGetData after blocking staging readbacks retains the actual
BOOL result, HRESULT and read count; the three results must be false/true/false.
The first binding follows the issued query and target reset without any CPU
readback or query polling. Each binding retains actual QPC start/end/frequency.
An actual unfinished interval is not guaranteed: clocks are observations,
and this probe does not fabricate S_FALSE, force a timing threshold or prove
the long-pending policy. The pending-policy source fix remains separately
owned by ROOT.

The comparison follows the documented DDI contract: equal query/value
suppresses subsequent rendering and resource commands. Predication can
legally bind before QueryGetData completes.
[SetPredication](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_setpredication).
End without Begin is an empty Begin/End interval; a bound predicate cannot be
ended. Every interval ends while unbound.
[QueryEnd](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_queryend).
These clauses were checked first in the local original DDI documentation
under `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/`.

The complete payload is exactly 45 files: one HLSL, four compiler originals,
36 `frame-00.raw` through `frame-35.raw`, and `queries.bin`, `bindings.bin`,
`ownership.bin`, `raw-close.bin`. All binary words are little-endian UINT64:

- `queries.bin`: three four-word records, generation/result/HRESULT/read count.
- `bindings.bin`: QPC frequency followed by 32 eight-word records,
  frame/generation/expected issued result/value/command/pre-read count/start/end.
  Commands 0–3 are the four draw entrypoints; 4–7 are Clear/Copy/CopyRegion/Update.
- `ownership.bin`: QueryAdapterInfo/context create/context close/allocation/
  deallocation/lock/unlock/render/escape/resident/evicted/wrong thread/bad cookie/
  core errors/malformed outputs/transport-balanced-before-raw-close.
- `raw-close.bin`: actual raw-close HRESULT.

Cleanup unbinds predication and the index buffer, then render/shader/raster
state, before reverse typed child destruction. Query, views and resources are
destroyed before the device and adapter. Ownership must balance before raw
KMT cleanup: one paired context, positive paired allocations/locks/residency,
actual render and completion callbacks, zero core errors, wrong threads, bad
cookies and malformed outputs, and no remaining allocations/residents/paging.
Fallback cleanup cannot satisfy that gate. Original stdout retains callback
counts and actual partial progress on failure. Saved failed readback bytes
remain available even when their literal oracle rejects them.

The final successful marker must be exactly:

```text
D3D10_PREDICATE_KMT_PASS profile=10_0 draws=27 pixels=9216 hr=00000000 pending_completion_guaranteed=0 ordinary_runtime_admission=0
```

`verify-d3d10-predicate-originals.py` independently checks the 45-file closure,
literal frames, original SHDR/container equality and bounded chunk closure,
actual issued results, all binding metadata/clocks, stdout count/image/clock
joins, zero remaining owners, successful raw close and the exact final marker.
Keep stdout/process receipts outside the payload directory. The caller must
also join the actual process exit, supplied LUID, named loaded core hash/path
and compiler originals. The reader alone does not establish their provenance.

Local receipts are retained in this worktree:

- `artifacts/kmt-predicate-sdk-freeze-03`: four optimized official-SDK
  x86/x64 COFF compiles and four actual llvm-readobj children, all exit 0,
  no diagnostics, reaped, no timeout. AMD64/I386 machines match. The 284-file
  source/SDK/MSVC/overlay/runner input closure is identical before and after.
- `artifacts/kmt-predicate-local-02`: GCC optimized oracle and Clang
  ASan+UBSan oracle both report 37,345 checks; independent reader passes
  92 synthetic controls. Actual exit/closed-output receipts are retained.
  Source before/after hashes agree with the final SDK freeze.
- `artifacts/kmt-predicate-local-01`: the superseded initial check retained
  two native Clear RTV const-pointer diagnostics; these were corrected with
  mutable FLOAT arrays before the successful fresh receipts.

These local checks establish source/ABI/oracle consistency. Native ARM64
EWDK builds, successful consolidated-core CI selection and real target
hardware execution remain pending ROOT. SO/MRT and installed ordinary-runtime
Mesa replacement are not admitted by this slice.

From an already initialized native ARM64 EWDK shell, with a fresh absolute
output directory and copied frozen sources, the standalone builds are:

```bat
cl /nologo /MT /O2 /EHsc /std:c++17 /Zc:preprocessor /W4 /WX /D_WIN32_WINNT=0x0A00 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /external:env:INCLUDE /external:W0 /I"%PREDICATE_SRC%\tests" "%PREDICATE_SRC%\tests\umd-d3d10-predicate-hardware-probe.cpp" /Fo"%PREDICATE_OUT%\predicate.obj" /Fe"%PREDICATE_OUT%\dxvk-umd-d3d10-predicate-hardware-probe.exe" /link /INCREMENTAL:NO /MACHINE:ARM64 kernel32.lib gdi32.lib bcrypt.lib
cl /nologo /MT /O2 /EHsc /std:c++17 /Zc:preprocessor /W4 /WX /external:env:INCLUDE /external:W0 /I"%PREDICATE_SRC%\tests" "%PREDICATE_SRC%\tests\umd-d3d10-predicate-oracle.cpp" /Fo"%PREDICATE_OUT%\predicate-oracle.obj" /Fe"%PREDICATE_OUT%\dxvk-umd-d3d10-predicate-oracle.exe" /link /INCREMENTAL:NO /MACHINE:ARM64 kernel32.lib
```

Only these source units are compiled. Use the existing owned bounded process
runner with a 60-second child deadline, retain actual exits and pipe-drain
results even on failure, and verify source/core/executable hashes and ARM64
PE/import originals before execution. Invocation and independent verification:

```text
dxvk-umd-d3d10-predicate-hardware-probe.exe <absolute core DLL> <core SHA256> <16 hex LUID bytes> 10_0 <fresh absolute payload directory>
python3 tests/verify-d3d10-predicate-originals.py <downloaded payload directory> --stdout <actual owned stdout.raw>
```
