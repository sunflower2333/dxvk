# Progress

2026-09-11: Local vertex-input extension sanitizer checks now PASS:
identity69, trailer176, shader53. Native layout maps registers with explicit
R32 float/uint/sint vector formats; generic VS compilation waits for the
bound layout's actual scalar types and caches that variant. Vertex-buffer
DDI and probe now require real positions. Windows WDK/build validation next.

2026-09-11: Register-based layout/vertex-buffer and typed deferred-VS
implementation under development. New sanitizer test with nonzero input
registers caught constructor registerIndex/streamIndex argument inversion
at check43; corrected against actual upstream signature declaration.
Depth b47f955 remains the next stable paired checkpoint, independent of
this uncommitted extension. Main requested standalone probe files/hashes.

2026-09-11: Added Texture2D depth/stencil views (including array and MSAA
descriptor translation), independent stencil-face enables, depth/stencil
state and clear DDIs. SetRenderTargets now atomically updates depth even
for zero color targets, as Microsoft specifies. Probe requires exact
occlusion sample counts0/4096/0 for depth rejection/pass/stencil rejection;
additive saturation alone could hide wrongly accepted draws. Pending CI.
Blend b0ea010 and scissor b9b6a03 CI both ALL PASS on three architectures.
Paired60004 has reached Sign+package. Correct documentation path is
doc/umd-native-status.md; a guessed doc/viogpu-umd.md read was absent.

2026-09-11: Resumed own DXVK assignment after compaction. Child6bed28b
CI34596463268 ALL PASS across three architectures; --native-copy still
has no target execution. Paired60004 CI34596574202 passed regression and
Mesa; KMD/sign/catalog job103256746352 is running. Blend/indexed b0ea010
passed x86; latest scissor b9b6a03 architecture jobs remain active. Next
bounded implementation is depth/stencil views/state and tested occlusion.
One read guessed tests/umd-ddi.cpp; actual file is umd-ddi-probe.cpp.

2026-09-11: Added SetScissorRects with count/clear bounds and translated
signed rectangles. Probe enables scissoring and requires its bound64x64
rectangle, alongside new indexed/additive-blend pixel path. CI pending.

2026-09-11: Continued required DDIs with D3D10.0 blend states, index buffers
and indexed/instanced draw variants. The GPU probe now requires additive
half-red plus half-red blending through an indexed triangle to produce full
red; texture/CB/blend/draw omissions fail the existing4096pixel criterion.
Source pending CI. Paired60004/1ab0151 CI34596574202 is independently
building validated9eb; newer KMT publication probe6bed28b CI34596463268
remains active, preserving a stable validated package checkpoint.

2026-09-11: Child9eb4361 CI34595689478 ALL PASS onARM64,x64,x86, including
production native allocation/Present, constant-buffer/SRV/sampler DDIs.
x64/x86 CPU fixtures report217allocation/present,715adapter,69query and
57real-HLSL/container/reflection checks. Paired60004 now committed/pushed
with exact9eb child, production-allocation ABI checker and existing Mesa4ace.
Newer6bed28b --native-copy probe CI34596463268 is separate and pending;
it is not in60004 and has not been run on target.

2026-09-11: Implemented optional --native-copy device probe. Harness-owned
runtime callbacks now use real KMT device/allocation/context and Lock/Unlock;
production PresentDDI reads DXVK output and writes the actual kernel backing.
The final callback verifies4096red pixels without changing screen contents
or claiming Microsoft runtime/visible Present proof. Source not yet built
or run on target. Paired60004 remains pinned to earlier9eb until validation.

2026-09-11: Latest child9eb4361 nativeCI34595689478 running; superseded
73d01e7 CI34595600097 cancelled because it retained the already diagnosed
const initial-data probe compile failure. Paired parent60004 locally pins9eb
through depth1 fetch; actual allocation ABI,377identity and full contract
checks allPASS. Keep it uncommitted until childCI passes. INF still Mesa4ace,
no KMD implementation changes and no device tests by this agent.

2026-09-11: Native-memory callback fixture217checksPASS on x86 in b40ccb1
CI34595019708, alongside715adapter and69query checks. Production UMD
compiled, but later probe object rejected const initial-data arrays because
WDK pSysMem is mutable. Corrected both probe arrays; future CI compiles DDI
and probe objects early to catch such issues before the full backend build.

2026-09-11: Added single Texture2D SRV and VS/PS sampler/resource bindings.
Pixel shader now samples an immutable white texture and multiplies it by
the bound constant-buffer color. New build pending. Paired checker compares
actual extracted child allocation declaration to actual KMD header; local
size/16fields/defaults/formats checkPASS. Parent checker remains uncommitted
until latest child passes and60004 pin is prepared; no KMD edits.

2026-09-11: Upload/copy signed-box fix1f4595d CI34594094209 allPASS.
Native allocationfdce901 CI34594671352 caught fixture field spelling:
D3DDDICB_LOCKFLAGS.DonotWait (implementation compiled). Corrected fixture.
Added VS/PS constant-buffer binding and changed real-HLSL pixel probe to
read its color from an actual constant buffer; target runtime still pending.

2026-09-11: Added native present-source allocation ownership, synchronized
GPU readback/kernel Lock+Unlock publication and real DXGI PresentCb plumbing.
Only development windowed blits are enabled; shared/opened resources,
primaries and flips remain rejected. New CPU fixture checks trusted bounds,
padding, callback ordering/failures, stale-publication refusal and ownership.
Source review checked actual SDK callback signatures and UINT_PTR DXGI
handles. New CI pending; no device execution. Previous828b535 failed signed
WDK-box narrowing; fix1f4595d is running replacementCI34594094204.

2026-09-11:60003 pairedCI34592189757 now allPASS at7051011; signed package
artifact10260421279,11528743bytes, SHA256
5c5acc14f9fa612dc67360d6ceb7e276cfeff28c27feaf9aec2de6bd28f3d8af.
Logs confirm DXVK DLL/two probes, Mesa DLL, KMD SYS/CAT signed and packaged.
Query childbc31fb9 CI34592884124 allPASS on all three architectures.
Latest828b535 build still running. Main informed of identity and old Mesa4ace
pin, so this package must not silently replace newer active Mesa GPU fixes.

2026-09-11: Implemented bounded ResourceUpdateSubresourceUP/CopyRegion for
buffer and RGBA8/BGRA8 Texture2D subresources. GPU probe now performs an
offset buffer update/copy and checks all64bytes before draw. Query commit
bc31fb9 CI34592884124 ARM64PASS, x64/x86 still running. Main reports user's
backup completed and existingVM now6GiB; only main resumes device actions,
this agent stays isolated/local/CI until a coordinated window is granted.

2026-09-11: Continued mandatory DDIs with four actual backend query types,
begin/end/ownership, busy/removal status translation and no partial output
publication. Added CPU completion-helper failure tests and required a real
GPU event in the4096pixel device probe. New Windows CI pending; no GPU run.

2026-09-11: Producer WDK34591724161 allPASS at1f99078; main authorized
cherry-pick12bbe0c/1f99078/33f94fa. Integrated as c481d57/cd77ed9/f09ee0f.
Local paired377checksPASS with actual decoder; parent7051011 pins child
eb27a08 (docs/comments-only over allPASS11d889d), version60003, starts
paired34592189757. RegressionPASS, Mesa+DXVK pending. Follow-up checker
now verifies explicit development source version and exact five package
files; full contractPASS. Main requires continuing mandatory native DDIs.

2026-09-11: child31f0215 CI34590877589 allPASS acrossARM64,x64,x86;
x64/x86 executed715adapter lifecycle checks plus runtime-query consumer
and57real-HLSL checks. All3artifacts uploaded.11d889d KMT-mode CI remains
active. Parent12bbe0c producer failed its newMSVC/WX fixture before KMD
compilation (SAL redefinitions); main owns the fix and explicitly requests
holding cherry-pick until it passes. Own paired parent is prepared with
11d889d pin/version60003 but remains uncommitted and unpushed.

2026-09-11: New child31f0215 adapter lifecycle and11d889d optional --adapter
KMT probe are pushed. Native CI handles34590877589 and34591067184 have
passed sanitizer and real-HLSL CPU jobs; architecture builds remain active.
Read-only reviewed parent's in-progress KMD producer: all160-byte fields,
legacy128-byte preservation, atomic64-bit LUID and stop/reset refusal agree
with this consumer. Parent will provide a reviewed commit; no cherry-pick
or device operations yet. Old CI watcher62752 completed and was drained.

2026-09-11: Paired801fde5 CI34589164956 ALL PASS. Logs confirm60002,
both DXVK probes,viogpudxvk.dll,Mesa viogpud3d.dll,KMD sys and cat signed;
final droidvm-arm64-drivers artifact uploaded (11522740bytes). Existing INF
still selects Mesa. Added development OpenAdapter adapter/device lifecycle
and mocked CPU tests; real WDK signatures verified against Microsoft docs
and SDK header. Local identity69/trailer176/shader41 sanitizer tests pass.
New adapter test will run before expensive backend compilation in Windows
CI; its result is pending. No remote hardware operations performed.

2026-09-11: Accepted dedicated DXVK UMD ownership. Read workspace safety rules,
planning skill and current Direct3D audit; verified independent clean branch.
Initial inspection was followed by the implementation and CI results below.

2026-09-11: eaf3f078 implements embedded exact-LUID Turnip factory and real
WDK resource DDIs. CI34586456839 passed all3architectures and identity69
ASan/UBSan cases. Independent driver parent94a27498 pins the DXVK submodule
and builds/signs/packages it beside Mesa/KMD without changing the active Mesa
UserModeDriverName. Paired CI34586690632 has passed regression/Mesa/DXVK jobs;
its driver signing/packaging job is still running. No remote tests performed.

2026-09-11: Added restricted SM4VS/PS container bridge, shader ownership,
rasterizer/viewport/target/topology state and DrawDDIs. Updated probe to require
4096red pixels after drawing over a green clear. Local shader41 and identity69
sanitizer cases pass. Windows CI additionally compiles HLSL and independently
reflects the rebuilt containers with Microsoft's D3DCompiler. That new CI is
pending; this is not native runtime activation or general DXVK UMD support.

2026-09-11: a52df2b compiled across3architectures, but the real Windows shader
CPU test caught token re-encoding differences. Fixed by preserving the raw
runtime tokens; b76de1d fast D3DCompile/container/D3DReflect test passed.
Added proposed identity decoder+callback consumer in7a373289 (no KMD producer)
and176sanitizer decoder cases. a9c69c0 also fixes DDI/API DO_NOT_WAIT bit
translation. Final child CI34589123522 is running. Paired parent801fde5 pins
a9c69c0 and version60002; CI34589164956 is running. Initial Inf2Cat file-list
failure was repaired in vcxproj; main KMD implementation remains untouched.

2026-09-11: b76de1d CI34588836455 is now allPASS: Linux sanitizer cases,
Windows fast HLSL/container/reflection57checks and ARM64/x64/x86 full target
builds. x86/x64 also ran57shader checks and the mock runtime-adapter callback
consumer successfully. a9c69c0 differs by the nonblocking Map flag fix and
coverage documentation; its final CI remains running, as does parent801fde5.
Removed the two disposable /tmp reference-header/test-binary files after use.

2026-09-11: Final child a9c69c0 CI34589123522 allPASS on ARM64/x64/x86,
including57Windows real-HLSL checks and mocked runtime callback consumer.
All3architecture artifacts uploaded successfully. Parent801fde5 paired
CI34589164956 has passed regressions and Mesa compile/link and is now building
its exact pinned DXVK prior to the KMD/sign/catalog job. No hardware tests.
