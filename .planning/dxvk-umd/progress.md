# Current continuation checkpoint (2026-10-08)

## Current checkpoint: native DX9/DX11 builds and CI68 shader correction

VM default UMD remains Mesa. Ordinary SYSTEM DX8-11 hardware creation, draw,
readback, Present and DWM/Explorer replacement are incomplete. ROOT alone owns
target calls; preserve the single registered8192MiB VM. Production capability
masks and default registration are unchanged. Historical18452/CI37743643139
hardware evidence does not admit current ports or a current core.

Published0ccc3a2fb228afd9cb8a224c7aaa473177d56610 triggered one Native UMD run
37814911005. CI67 is completed FAILURE: identity/shader-cpu/ARM64backend pass,
x86/x64backend fail in d3d10-system-shader-test, ARM64runtime SKIPPED. Both
original failure ZIPs and18026members are retained and reviewed, with all owned
local readers closed. Proof21657/SHA0cd6018b is at
artifacts/dxvk-trunk-integration-20261008/ci67-status-port_dx11-01/
CI67-failed-originals-and-owned-read-closure-reviewed-01.json. Both shader tasks
exit-1073740791, no timeout; lastError!=ddiResult(hr80070057). Exact FXC replay
isolates scalar PS SV_PrimitiveID packed at v2.y: production profile incorrectly
requires component mask1. Reviewed79898e is merged as94f58db. It admits exactly one-hot scalar UINT32
register components while preserving dedicatedx, version/type/stage limits,
HLSL, raw pixel and negative HRESULT oracles. Exact failed VS/GS/PS tokens
replay successfully; focused615 controls and four strictx86/x64 COFFs pass.
Independent3727241/0 finite sourcepeer PASS pins five Git/current/TAR files. The frozen success
collector and success reader are UNEXECUTED; no current core admission.

Native D10CPU03 remains accepted17tools/fourCOFF/fourPE, ARM64X5d410dbc plus
ARM64/x64 probes, release3800/SHA71941132. Native D9CPU02 now passes10tools,
twoCOFF/twoPE, selected538rows/317unique stable; ROOT3692291/0/66.090s.
Archive6413451/SHA507b957f has89originals. Finite3698174/0 proof9042/
SHA96f02722 joins14native lifetimes+observer/9transports/7parents/realouter.
Explicit ROOT release6850/SHAcb6e3f25 closes CPU02. Both native probe images
were only built; no factory, frontend/core module or GPU call occurred.

Native D11CPU01 source786 now passes all17tools, zero warnings, fourCOFF/
fourPE, selected139rows/118unique/30headers stable. ROOT3699709/0/80.328s;
archive12135355/SHAb389e43d,151originals/162members. ARM64X frontend1464832/
SHA7b5a6094 exposes OpenAdapter10_2 and VioGpuDxvkD11ValidationInfo, actual
CHPEv2/dynamic1224+8 mapped. Finite3704530/0 proof37491/SHA4987a8ca at
artifacts/system-runtime-validation-20261009/
independent-D11-success-build-host01-review-01/
D11-native-build-originals-and-closure-reviewed.json joins21absent exactnative
lifetimes+observer/11transports/7parents and REAL closedROOTouter. All pending
counts0. ROOTrelease3712944/0 writes2472/SHAdd09c9e6. Frozen positive reader01
incorrectly omits the realouter while claiming finalized; it remains UNEXECUTED
and unusable as release proof. No native compiler/probe rerun occurred.

Native BGRA189 is now independently accepted and released: ROOT3684203/0,
9tools, threeCOFF/threePE; original BGRA literalreader171raw/1836pixels and new
encoded reader24raw/3072pixels/12672bytes pass exactly once. Finite3691530/0
proof11997/SHA68e87b5a joins originals/ownership/static34+SYS+desktop. Explicit
release1346/SHA51abf6c2 closes the WARP reference-only attempt. Generic inverse
bit exactness remains false; current embedded GPU acceptance is pending.

ROOT actual D10module-negative attempt now passes ARM64+x64 fivechecks each,
core_loaded0, using the exact CPU03 three binaries and no core payload. Outer
3713844/0/58.931s owns all ten phases; unchanged reader3714896/0 joins8native
children/11transports/10phaseparents/static34+SYS+desktop. Independent3719885/0 finite proof24030/SHA4f8939a2 joins the realouter/reader
and all closed lifetimes. ROOT3721492/0 file-only release953/SHA25edc0e5
closes the attempt.
Originals: artifacts/system-runtime-validation-20261008/
native-d10-entry-negative-0f4f5e-01/native-execution-01. No registry mutation,
KMT probe, factory or GPU workload ran during this negative gate.

Current local MAIN94f58db38e8e1c4691505a673b760243f9c48425 adds the reviewed
final binding020f line atomically as34c2a83 and Present8d0 as8f8edc1; no push
has occurred since0ccc. Binding pure93 local metadata controls/sourcepeer pass,
exact CI source/config/core/private-loader/ICD/dependency tuple and held module
census PID/start join are required. Native020f PS5.1 extended18restore scenarios, eightAST files and93tuple
controls are frozenCPU-only at artifacts/system-binding-native-preflight-
20261009/020f66e-01; actual native execution remains pending. NoApply/core/hardware
admission. Presentsource2strict optimizedSDKCOFF+93synthetic reader rejects and
peer02 pass; actual Present CPU host04 ROOT3726163/1 stops at native PS5.1 AST parsing
before any compiler: derived producer line50–51 starts -and on a new line.
Collection/transfer/separate observation are complete; failed originals and
realouter closure are under finite review. A minimal syntax correction and
fresh native attempt are being prepared from the actual786CPU17 prerequisite. Actual twoPresent/DWM proof
remains pending; offscreen source/oracle remains frozen.

Next: publish this reviewed shader correction, binding and Present source in
one push, then record the actual sole NativeUMD run. Finish fresh CPU-only
binding and Present builds, preserving the failed AST originals.
Then require all six actual CI jobs SUCCESS and genuine core/config originals,
readonly actual private Vulkan inventory, ROOT-approved current module tuple,
and bounded ordinary SYSTEM D10/D9/D8/D11 held render/readback/Present/rawrestore
attempts. Mesa/DWM replacement follows successful actual hardware gates.

## Previous checkpoint: physical BC correction and native runtime continuation

VM default UMD remains Mesa. Ordinary SYSTEM DX8-11 hardware creation, draw,
readback, Present and DWM/Explorer replacement are incomplete. ROOT alone owns
target calls; preserve the single registered8192MiB VM. No capability masks
or default registration changed. Latest accepted core is historical18452d7 /
CI37743643139; it cannot admit the newer unpublished ports.

Published2fbb79131da999d52caa92f7341ac14aca12c212 has one NativeUMD run
37787018299, completed failure. Identity/shader/ARMbackend pass; x86/x64 BC
update fails0xC0000005; ARM64runtime skipped. Both R9volume originals pass72721
checks. Each arch executed42fixtures/41passed before BC failure;23 later
fixtures unexecuted. Actual CI66 failure originals are admitted by
artifacts/root-CI66-failure-originals-admitted-20261008-01.json. Four unused
workflows remain manually disabled; no additional workflow dispatch occurred.

Current local MAIN8359f6aee1aa79d8e2471a1ab4b4cbe1682a25bf includes all reviewed
private query/pending retirement/CopyStructureCount/BGRA/packedDS/1DDS/ClearRTV/
DSV+UAV predicate ports. New reviewed merges: physicalBC0a5->4ccde8b,
FL10output47541bf2->966c7696, Dispatch44f0f975->54173e4 and Vulkan
asymmetricBC50640c1d->7fdd97e5. ROOT read all production diffs and independent
source reviews. Fresh mergedcontext/DDI x86+x64 strict optimized4COFFs pass
3596033/0, diagnostics empty,641 actual dependencies/850selectedinputs stable.
BC backend frozen2COFFs pass; current TU/454actualdeps/compiler match3597563/0.
Historical explicit overlay hash was not captured, so full compiler-input
reuse equivalence is not claimed. Full next CI build remains required.

BC native01 W4/WX allocation shadow failure fixed10ca6fd; native02 fixture
failures retained. Actual39 native WARP diagnostics establish27 physical/NULL/
composition passes and12 logical-boundary failures, not a complete BC gate.
Physical boundary correction preserves all original byte patterns/rejections.
NativeBC03 ROOT3587342/0 builds19ARM64COFF/3PE and runs12owned children: policy,
copy and update allpass. Exactly-once unchanged literal copy3593819/0 verifies
540files/180snapshots/474624byteobservations; update3593820/0 verifies2520files/
840snapshots/34cases/130944compressedbytes. Frozen consumer3593821/0 plus finite
closure3595143/0 joins99sources/25inputs/allstreams/desktop. Localwrapper count
failure3593816/1 preserved; outputs are19COFF+3PE+8RSP. Proof
artifacts/bc-native-originals-reviewed-20261008-03/
native-BC03-literal-originals-and-owned-closure-verified-01.json.
ROOT explicit release target-release-03.json918bytes/SHA79fe350f. WARP only.
Separate Vulkan asymmetric copy uses retained DEVICE_LOCAL transferbuffer and
existing barrier helpers;16actual public llvmpipe cases/96raw passed and peer
3591177/0 reviewed. Embedded DXVK/targetGPU execution remains pending.

Merged a6dd EWDK reference02 builds21ARM64COFF/5PE. PackedDS unchangedreader
3568930/0 passes150files/4100pix/49200byteobservations. Original1DDSreader
3568931/1 had R16_TYPELESS54 instead of actual SDK53; ROOT d2707a2 changes only
two metadata constants. Correctedreader3571385/0 passes204files/1360pix/8160
byteobservations;38corruptioncontrols3571756/0. No native rerun. Reference02
source/closure peer3581281/0 passed; released5b90ec0d. BGRA originally aborted
on NULLRTV for logicalFL10_0; no pixel acceptance was granted.

FL10output fix excludes unsupported pixel UAV slots and preserves FL11's
original combined call/counters. Two strict DDI builds and672policy checks
pass; independent source review finds no blocker. Fresh BGRA03 ROOT3596363/1
builds18ARM64COFF/2PE, both native fixtures exit0. Unchanged literalreader
3597151/1 fails scene extended-blt-1-0-9 pixel0,0: actual0 vsff808080. Its24
words are allzero; same sceneprofiles0/2 and all scene8 encoded resolves pass.
No byte admission. Originalarchive3834811bytes/SHA846ec542,338regularfiles.
Peer3599005/0 closes8native+TAR/11absentlifetimes/6transports/reader/outer,
static34+SYS+desktop unchanged. ROOT release target-release-native03.json
1016bytes/SHA3c1e177b. Source owner diagnosing fullytypedSRGB/MSAA FL10 path;
fixture/reader/expected bytes remain immutable. Native rerun of the original full fixture only after fix.

Modern ARM64X source857a5fe is separate module-forwarding work. Modern02 built
frontend but SDK C4201 prevented views; accidental dependent ARM64 call was
rejected before probe launch and all original failure records were retained.
Modern03 scopes warning4201 only around official DDI include, keeps/W4/WX,
and host gates require actual successful prerequisite originals. ROOT exact
BGRA03 release binds modern03 auth1804bytes/SHA9b5e0e2a. Upload3600435/0,
stage3600943/0, sequential3601880/0 pass parse/build/views/ARM64/x64 module and
negative forwarding. Modern03 original reader 3610976/0 and finite source/closure review 3612893/0
passed. The frontend is 1282048 bytes / SHA64939f5c; all 25 native children,
13 transports and 11 ROOT outers closed. Independent proof is
artifacts/modern-runtime-deploy-20261008/independent-modern03-review-01/
modern03-source-and-complete-outer-closure-verified-02.json, 16709 bytes /
SHA9e77d162. Explicit ROOT release is 604 bytes / SHAcbcbb7b6.
Historical18452 payload is negative-module only: no GPU, ordinary factory,
registry or hardware admission. No same-attempt driver-readiness snapshot
was collected by this module-only packet.

Ordinary D10_0 validation8eeb20 source/probe is frozen: actual SYSTEM d3d10,
exact KMT160adapterLUID, forwarding original runtime handles/callbacks, literal
16x16 draw/readback+Present and bounded held restoration protocol. Native
ARM64Xfrontend+ARM64/x64probe CPU source packet remains immutable8eeb20.
Host01 native stage fails before parse/build: ROOT3630343/1 reaped, exact packet
extract5096/0, but PowerShell5.1 wraps the ReadJson ledger array so the controller
Count20 assertion fails. Failed01 collection/transfer/observation is complete: archive50144/SHA7fe5ce23,
30 originals, finite3654735/0 proof9880/SHAa024f055, ROOT explicitrelease3234/
SHAfd673a95. Corrected host02 actualROOT3656018/1 is reaped: native PS5.1
Arm64 one/two/twenty-row shape and AST pass; frontend ARM64 CL passes, but
LINK fails LNK4070/LNK1218 under /WX because DEF LIBRARY name differs from
intermediate OUT filename. Archive810330/SHAe6478aa5 and complete observation
are saved. Finite3666563/0 proof17559/SHA6919e372 passes; ROOT
release3795/SHA21468570 closed host02. Fresh DEF-only0f4f5e removes the
conflicting LIBRARY directive and preserves both exports. Actual CPU03
ROOT3668478/0 passes all17tools, builds four COFFs/four PEs including ARM64X
CHPEv2 and ARM64/x64 views, with zero warnings. Archive12133483/SHAa0222d24
has152regular originals; finite3675099/0 proof29475/SHA416385c6 verifies
selected139/118unique snapshots and21 absent exactlifetimes. ROOT release
3800/SHA71941132 is closed. ROOT3680868/0 merges8eeb+0f4f as68f9d3d/c387ac0.
No probe/factory/core load occurred; both original failures are preserved. D9 host01 shares the array bug and must not execute;
corrected host02 and fail-prefix-aware ROOT sequential driver are prepared. All native source/build failures remain immutable. Single
reversible binding controller preserves raw native/WoW tuples and independent
SYSTEM watchdog/startup rescue, with real KMT before/bound/restored checks.
Two-line separately pinned progress runner7def540f solves held-line visibility;
portable held/exit/drain controls pass, Windows AST/task/job/restore pending.
Binding source97bb has a concrete stale-backup rollback race and must never
run Apply. Fresh648f612c acquires the lease before authoritative backup and
uses protected, flushed mutation intent before RegSet; absent intent cannot
replay the backup. Independent648f source review3623246/0 passed; five actual-source Windows
in-memory phase controls now pass actualROOT3679124/0: nativePS5.1 six
ASTs and five scenarios/29checks, all selected inputs unchanged. Archive27182/
SHA91292c6c is retained. Finite3682864/0 proof6630/SHA728e2e42 passes;
ROOT release1360/SHAae425894 closes the preflight.
NoApply, registry/task/KMT/GPU/core calls occurred. Same-owner DX9/9Ex extensionac77
also passes finite3651470/0 source review and225 portable controls; Windows
AST/12 phase controls remain pending. No target registry writes occurred.
DX9/9Ex ordinary SYSTEM probe739de05 is frozen with 512 literal readback
pixels, optional two Present calls/512 desktop RGB observations, exact loaded
module tuples and externally held final device release. Three strict local
COFF builds and four reader layouts/15 rejection controls pass. Independent source review3626709/0 passed210 joins/69 originals; ROOT3627858/0
merged739de05 as97836d9 after full CPP/reader/producer review. Native
ARM64/x64 CPU build is pending. The historical
fog-cap clear-only attempt is not compatible current proof.
DX8 existing448pixel/64Present gate remains immutable and requires new accepted
currentCIcore. New KMTpredicate build/run packet preparation is also pending
successful currentCIcore and matching Vulkanpayload; all hardware gates closed.

ROOT native BGRA MSAA diagnostic01 (2026-10-09) compiled successfully:
CL8924/0, zero stderr, actual AA64 bigobj188772/SHAddba069b. The controller's
ordinary-COFF-only machine check rejected that valid bigobj before LINK or
execution. Failure originals and archive63636/SHAb0da39b3 are preserved;
collection and exact lifetime observation passed, outer3620322/1 was reaped.
Finite independent3624082/0 reviewed6475/SHA227e4491; ROOT release596/SHA60f5f669.
Fresh diagnostic02 reuses the compiled object and only LINK2640/0 + run10940/0;
ROOT3625015/0 reaped. Archive196809/SHAe7c2ff91 includes60 raw files/30 snapshots/
720 pixels. Existing inspector3625409/0 ran once, proof25600/SHA5d5dc09b.
Actual FL10_0 all twelve MSAA copy controls leave the magenta sentinel;
native typed-sRGB Resolve isffbcbcbc, separately drawn typeless encoded Resolve
isff808080, and unchanged helper returns zero. FL10_1 controls/helper pass their
respective observed colors. These are diagnostic observations, no pixel admission.
Finite independent3628627/0 closes all recorded lifetimes/transports/outer,
10049/SHA bf7c9e7f. ROOT release596/SHAe84bf813 precedes D10 CPU ownership.
The official local spec establishes typed-sRGB Resolve operates in linear
space and inverse-sRGB reconstruction has finite decode precision. Source
bounded FL10_0 shader fix189c473 is frozen, finite source peer3657069/0 passes,
and ROOT merged it as1a1a5e5. Exact private UNORM MSAA SRV is preferred;
native E_INVALIDARG/UNSUPPORTED uses original-SRGB bounded reconstruction,
with generic inverse bit-exactness explicitly false. Four strict COFFs,
10240 actual DXBC CPU channel observations and3072 syntheticpixels/76rejects
pass. Fresh actualROOT3684203/0 completes41s: native9children0, three
COFF TUs/threePEs, unchanged BGRA reader verifies171raw/1836pixels and new
encoded reader verifies24raw/3072pixels/12672bytes. Originalarchive4097159/
SHAbf01d23b is collected; exact lifetime observation passes. Independent
source/closure review and explicitROOTrelease are underway. Native packet
recompiled productionDDI, unchanged extended-blt directhelperdependent and
new fixture; reused16originalobjects with no observedheader exclusion.
Existing fixture/reader bytes and expectations remain immutable. WARP
reference slice passes; currentembeddedGPU acceptance remains pending.

Next: finish D9/D11 CPU builds and same-owner binding Windows controls; execute
fresh encoded-resolve fixture plus original full BGRA fixtures; then one
consolidated CI push. Current explicit source sets derive74 ARM64 cases and74
bounded backend children each x86/x64,61 canonical mandatory and84 total EXEs
plus DLL/PDB. Earlier prepared72/59/82/216 CI67 collection remains unexecuted
and must be replaced for actual pushed source/run. Dedicated D11 FL10_0
validationa103 passed finite3649032/0 source peer and ROOT merged6b40a0f;
production-strength callback, real factory and current identity checks remain.
It exposes only exact11.0 DDI version/pipeline bit1; production/generic masks
remain unchanged. Separate SYSTEM D11 frontend/probe and same binding-owner
Api11 extension5b35 source peer3671781/0 passes. Separate frontend/probe
7865936 fixes exact approved private-loader handling; finite829 plus786
source reviews passed, four changed-source tool children and59controls remain
closed. ROOT3681172/0 merges0151+829+786 ascd62888/3a5d995/8359f6a. Its
ordinary512pixel probe honestly records presents=0; native17stageCPU packet
is in preparation. ROOT identified old binding owner hardcodedwinevulkan.dll
and ICDformat1.0.0 contradict genuineCIviogpu_gl_loader_${arch}.dll and
USER06 format1.0.1. One-owner tuple correction is in progress; noApply.
Fresh74case CI67 preparation3672536/0 is local-only with futureSHA/run null;
old72 packet remains unexecuted. Current-core private Vulkan payload inventory,
typed11 baseline hardware, ordinary factories/pixels/Present/restore and DWM
proof are still required before default replacement. Goal remains active.


## Previous checkpoint: five reviewed ports, CI61 failure retained, CPU08 pending

VM default UMD remains Mesa. Ordinary system-runtime DX8-11 draw/readback/Present
and DWM/Explorer replacement have not passed. Latest hardware-accepted historical
core remains18452d7/USER06; no failed/new source is promoted to that evidence.

CI61/37772962325 for e7954d7 is terminal FAILURE: identity/shader/ARMbackend pass,
x86/x64 fail resource-copy-cast-test, ARM runtime is skipped. ROOT3298650/0
reopens11500 original ZIP members,80 native children,1106 blobs+5links,
9 GH transports/2 collectors and all completed literal readers. Proof2662711/
9f176b89 is artifacts/root-CI61-failure-originals-independent-review-01/actual-CI61-failure-originals-reviewed-01.json.
Only48 paired copy observations exist; the manifest is empty. Exact failed
exit is -1073741819, checks9950/callbacks14/HRESULT88760870. The first reversed-X
public call is located by source CHECK accounting, not a recorded phase trace.

Reviewed source integrated: distanceSO f54ab4a->2e9cd67, minimal harness
7e91e14->77c0ae6, query-generation prerequisite4551189->9dc3e72,
actual dumpbin CSR guard100f0ea->ef8af0f, copy fixturee8b0500->4e178f4.
ROOT3298560/0 joins the distance/query/harness frozen source/compiler/helper
originals52397/168ce63a; ROOT3302100/0 passes four merged strict O2 official-header
COFF compilations plus four LLVM reopens102401/1dd56c04. ROOT3327487/0 joins
both focused failure fixes and genuine originals. New harness is63 native ARM
cases/50 mandatory/73 complete shipping union; next push is one consolidated
Native UMD run after CI61 terminal. No workflow activation/manual dispatch.

CPU07 b5ee276 final04 actual stage/native PS5.1 parse/copy pass; five native
tool stages pass (ARM64 CL/LINK, EC CL, both dumpbins). Producer rejects an
unprinted archive pathname before hybrid LINK. Actual copied ARM64 helper
is1136438/e140f4af and has genuine A641 definitions/AntiDependency aliases.
All189 originals,1366 forensic checks,11 native children and10 host transports
are closed and reviewed; native_build_passed=false, hybrid/views unattempted.
ROOT3279723/0 admits only that failed attempt and explicit target release9954eb58.
Target is now released to ROOT; fresh CPU08 packet from100f0ea is being prepared,
with identical native15/native6/hybrid19/frontend9/views8 policy. Old attempts
remain immutable. Corrected target mixed DLL build/load has not passed.

Next work: actual63-case CI outcomes and raw reader admission; CPU08 native
build and views; ordered predication replay plus retained query generations;
optional shared primary ownership and full color MSAA regional-copy semantics.
Ordinary feature masks, full public predication, scanout pair ABI, actual target
resource/shader/threading coverage and default replacement remain incomplete.

## Previous checkpoint: CI60 correction and KMD build

Active continuation (2026-10-08): actual CI61 x86/x64 have failed
resource-copy-cast-test at lastError==S_OK line24; ARM backend is still pending.
Only one Native UMD run37772962325 exists for published e7954d7. Frozen success
collection remains unexecuted; genuine failure logs are being reviewed before
any fix. No current core/ordinary-runtime admission is inferred.

ROOT CPU07 final04 preparation independently passed575 checks/113refs
(actual3265074/0). One-attempt authorization371c8b7d binds the accepted KMD03
release; actual upload3265998/stage3266673/native parse3267421/copy3268132 pass.
Actual build3268312 fails, views are stopped; same-attempt collect passes and
transfer/closure/original review are ongoing under ROOT sole ownership.
Do not retry CPU07 or change its frozen113 inputs. Exact producer error pending.

Distance SO f54ab4a and minimal harness7e91e14 are frozen for the next batch,
with63/50/73 source-derived counts; query-generation prerequisite4551189 is
also frozen. ROOT source review is in progress. Full public predication,
ordinary DX8-11 draw/readback/Present and Mesa/DWM replacement remain incomplete.

Actual publication: ROOT3247639/0 pushed e7954d7d3a5cb18ecedff7c447e3d1a8529c9a1a once. GHAPI3248247/0 returns exactly one push workflow, Native UMD backend development37772962325, in_progress; raw12682/7ad3fdee. No additional dispatch or workflow activation. CI61 has61 native ARM cases/48 mandatory/71 total shipped fixtures. Target remains released to ROOT while CPU07 packet is prepared.

The default VM UMD is still Mesa. Latest fully accepted core remains18452d7;
its isolated real-KMT D10/10.1 USER06 draw/readback/Present passed1792 exact
pixels. Ordinary system-runtime DX8–11 and desktop replacement are pending.

Published fae491c / soleNativeUMD37768987344 is terminal FAILURE: x86/x64
both fail opened-primary line703 with callback887b0002; identity/shader/ARM
backend pass, ARM runtime is skipped. ROOT3242783/0 independently reopens
1390 original ZIP members,38 actual native children,7 GH transports,
2 collectors and1084 exact source blobs. Proof715425/3ef34e7f is
artifacts/root-CI60-failure-originals-independent-review-01/actual-CI60-failure-originals-reviewed-01.json.
Saved legacy opened90 and partial shared80 files per architecture pass their
literal oracles; Blt identity120/15624 and shared predicate21/735 also pass.
This is original failure/pixel evidence, never a successful current core.

Source b3c7caa now integrated as ec73890 requires the documented exact
DXGI_DDI_ERR_UNSUPPORTED only for the shared descriptor carrying pPrimaryDesc;
first five DXGI_ERROR_UNSUPPORTED checks and all no-allocation/storage/pixel
oracles stay exact. The raw log has no descriptor index; branch5 attribution
is source inference. Production primary rejection remains unchanged.

Depth/null-PS754c3bc and private rotation/Present8d245293 are integrated as
4265dc6/8195410. ROOT3239634/0 joins frozen source/originals30844/f9a29f0d;
ROOT3240940/0 completes6 merged strict optimized x64/x86 compiles and6 COFF
reopens110811/3e129018. Harness1c4d7ad integrated370661b preserves60 old ARM
cases and adds depth65 raw originals plus independent reader:61 cases,
48 mandatory and71 full shipped fixtures. ROOT3242660/0 joins the corrected
rejection/harness originals19529/83bdf310. New native execution is pending.

Target EWDK KMD03 actually passes11 tool stages and all19 retained native
children/11 host transports. ROOT3234895/0 independently joins242 TAR members,
source118,compiler86,selected14,PE/imports and protected34/desktop continuity.
New unsigned SYS410624/a05bde85 remains uninstalled. ROOT3238396/0 accepts
only historical fdfd8f99/C1b9 source and explicit target release:1235/765baa27.
No newer-trunk KMD, package signing or hardware admission is inferred.

CPU06 ARM64/ARM64EC compile and native LINK pass; hybrid LINK fails actual
_mm_getcsr/_mm_setcsr. Official same-SDK comparison originals identify A641
CSR definitions and their weak aliases in ARM64 softintrin.lib; x64 lacks
these providers. ROOT3243588/0 rejoins the real HTTP206/COFF originals:
17280/31f48bc1,comparison only. Source4b2b3da integratedabebcfb explicitly
selects this same-SDK helper for hybrid LINK, preserves native6 inputs and
requires actual library copies/symbols/stability. CPU07 native packet is being
prepared; no target library/build success is inherited from comparison.

Final merged opened-primary fixture compiles2 and COFF reopens2 also pass ROOT3244319/0: 75977/96e8fa96. The next push includes the five reviewed source/harness commits and this checkpoint.

Next: one consolidated push through
the sole active NativeUMD workflow; prepare the exact61-case current-source
collector. Review and run one fresh CPU07 frontend build after explicit target
handoff. Continue clip/cull stream-output and generation-safe backend predication.
Then stage an accepted current core and verify fresh DX8 HAL/offscreen448/
Present64, ordinary DX9/D10/D11 hardware coverage, DWM and dynamic Explorer.
All ordinary0x17f/0x37f and registration gates remain closed.

## Earlier checkpoint: X8 presentable resources and native CRT linking

Latest accepted core remains18452d7 / NativeUMD37743643139, all six jobs
successful. ROOT00139eed joins five genuine ZIPs/14168 members,1012 Git blobs,
five links and150 actual native fixtures. Ordinary VM replacement remains
pending; the installed default UMD is Mesa.

Published a6ca946d21572a23a49d7edc396179cccc1be88a / soleNativeUMD37753759333
is terminal FAILURE. The corrected typed INDEX16/32 lock fixture now passes
597007 checks on both x86/x64. Each job reaches18 passing fixtures before
opened-primary fails at line281 with887a0004; ARM64 runtime is skipped. ROOT
replayer3074934 reopens the same506 original ZIP members, actual API/logs,
seven closed GH transports and1023 Git blobs/five links. Only profile0/format1
BGRA output exists: four images/140 pixels plus padding per architecture.
The failure is not successful CI52/core admission; its success collector never
executed. The raw constructor assertion cannot distinguish opened/staging/source
creation. Source inspection identifies a deterministic rejection of format2
B8G8R8X8_UNORM presentable creation without a primary descriptor, followed by
the same unsupported Blt format; this explanation is an inference.
Local Microsoft DDI documentation explicitly permits X8 display-source Blt.
DX10 owns the bounded production X8 creation/same-format Blt correction;
all existing three-format pixel/negative/lifetime oracles remain required.

CPU05 actual ARM64 CL/native LINK/ARM64EC CL passed; the corrected flat14
ARM64X LINK now fails4294: missing native load-config symbol, with LNK1218
under unchanged/WX. ROOT3064127 rejoins185 actual files/1456 checks and
accepts only original failure/explicit releasefdd4dbf0. The actual native
full-path response contains only the native object and ARM64kernel32.lib,
omitting CRT default-library inputs. Candidate bd601ea explicitly resolves
and pins four static CRT libraries in the original ARM64 LIB directory order,
requires their presence in the actual captured response, then preserves them
in the flat hybrid link. Native6-input/18-argument CPU06 parsing/linking/header/
import/export/view validation remains pending; no warning is suppressed.

DX10 current184 native CPU08 strict build remains ROOTbd3e58aa accepted:
4141 oracle checks/five malformed CLI controls, original probe194048/b51f7c6a
and archive1247319/7d41cba4. USER06 helper prefix now actually passes native
Parse8/Add-Type/ServiceAssembly Running4/pending-input rejection. ROOT3074935
replays46 same originals/7 retained native children exactly, and b34ccbee
independently accepts8 host/8 outer closures and explicit release45c8fbf8.
Protected34 values/SYS/binding/service/desktop are unchanged; full raw frame
difference is timestamp only. No core/probe/KMT/GPU/task ran in this prefix.

ROOT3076088 independently reopens71 input roles/81 stage files and fresh actual
management frame. ROOT3076294 seals the genuine final self-review as role72:
sealed-role72-ROOT01 has82 stage files, input ready=true, hardware_authorized=false.
The1792-pixel real KMT dual-profile protocol is being reviewed/frozen before
fresh exclusive target execution. Historical05ff/d7 pixels and binary provenance
remain historical. Target is released to ROOT; no overlapping target owners.

Reviewed X8 source159e0e2 and diagnosticaddf2db are integrated as54eb93c and
488b484; native CRTbd601ea as c559f73 and texture UAVed8492b as c9f79a4.
The UAV fix preserves single Texture1D/2D versus actual array dimensions and
buffer units. ROOT3085770 directly reviewed its original source/local packet;
ROOT3086077 independently replayed the original X8 source controls. The old
X8 diagnostic compiler receipts lack separate start originals; retain that
limitation. Fresh ROOT3100472 now captures actual starts/raw closure/reaping
for six merged-source optimized official SDK compilations: production UMD,
texture UAV and opened-primary fixtures, x64/x86 each. All six exit0 with
empty diagnostics; all362 actual selected compiler dependencies are unchanged.
Proof19292/b7965934fd746ad0d445c23d8861630a115f84ca2cde463111a2cbd815c87c8d
is artifacts/root-merged-x8-uav-20261008-01/merged-source-compilation-verified.json.
This proves local compilation, not native execution or hardware admission.

New harness retains all52 old ARM cases, adds two UAV cases (54 total), and
ships the two new EXEs (mandatory shipping model39 to41). Native x64/x86/ARM
runs retain384 raw UAV files and invoke their independent compute/clear/
public-description reader. ROOT complete diff and peer48a8e494 review preserve
old bounded runner/calls/oracles; real new native execution remains pending.
Only the existing NativeUMD push workflow is active. One consolidated source
push is next; no additional dispatch or workflow reactivation.

DX8 heldfrontend HAL remains accepted93fdbf8a; failed public offscreen85c7c07b
still has no448-pixel/64-screen proof. The creation-hint correction needs a
successful current core and fresh staging/names/HAL/offscreen/Present sequence.
D10/D11 requirements0x17f and ordinary version/feature discovery remain gated.
Complete system-runtime resource/shader/MRT/SO/predication/shared/primary/
threading and D11 immediate/compute-UAV/tessellation/interface hardware proof,
then DWM and dynamic Explorer validation, are still required for replacement.

## Active execution update: integrated CI60 candidate and accepted isolated D10 hardware

Actual publication update: ROOT3207966/0 pushedfae491c360a47fee15f45b681cfe96c19496faf0 once. GHAPI3209084/0 confirms exactly one push NativeUMDrun37768987344, in_progress; original12696/2125b9f9. No extra workflow or manual dispatch. ROOT now accepted CPU06 failed build originals and explicit release1030/d16a2c05; hybrid LINK fails actual EC _mm_getcsr/_mm_setcsr, after both CL stages and native LINK pass. Views stayed skipped. ROOT3215473/0 grants exclusive KMD03 ownership to verify_ewdk_build with1532/b81fa683 authorization; all other actors make no target calls.

The consolidated candidate includes SO e208191, same-family bit-copy e2cf240,
shared presentation bcf2ec9, BC1-5 updates b5e7200, identity X8 Blt c06479d,
D3D8 fresh descendant preparation 135f273, decoded D10/10.1 shaders baee5b4,
integer RTV scalar linkage a331718, six-test harness 9fcc83d and internal shared
transfer predication isolation 31397d0. Public admission masks remain closed.
ROOT queued source/original joins3193292/0 cover commits31fb/c138/a5c:
30258/6ba2f36d. Final shared-transfer joins3206993/0:13485/f2c7cbb0.
ROOT source review caught missing clip/cull SO ordinals; final c138 includes
that correction. Separate integer RTV fix removes the obsolete float-only
binding guard and commits all eight realized scalar classes atomically.

ROOT3196251/0 completes20 actual strict optimized SDK x86/x64 compiles and20
LLVM COFF reopens with actual dependency closure captured before/after:
170769/5b42fd1e. Earlier3194813 preserves20 successful compiles but its observer
omitted four assert/stream header pins; that failed attestation is retained,
not relabeled. After shared-helper integration ROOT3207022/0 separately
compiles the two changed units for both architectures and reopens all4COFFs.
ROOT3199835/0 independently replays frozen harness originals:16913/169710ea,
60 ARM cases,47 mandatory fixtures,70 total shipped executables. All54 old
case bodies/markers and bounded ownership runners remain. New raw readers
cover copy640+manifest,BC2520,shader49; old recursive UAV/opened-primary/Blt
and shared-resolve paths remain, with the new shared predicate phase42 files.
Actual native CI60 execution and new source/core admission remain pending.

Published32b7bbb / soleNativeUMD37761389362 is terminal FAILURE: identity,
shader and ARM backend pass; x86/x64 fail at opened-primary line74, first X8
Present shared-word equality after Blt S_OK. ARM runtime is skipped. ROOT3157240
independently replays518 ZIP members,38 native children,7GH transports and
1033 source blobs/5links; proof31842/7b244c84. Seven passing images245 pixels
per architecture remain; failing word was not saved and no alpha cause proved.
The next publication is one consolidated push through the sole active workflow.

USER06 same-attempt archive195973674/28e1e4b4 is now complete. SCP3122155/-9
and relative-path SFTP3157163/1 originals remain unchanged. Recovery02 owned
3172010/0 and its four actual transports complete the absolute /C:/ tail and
three external tar sidecars; ROOT collection amendment2595/4766b34e permits
only recovery, never a workload rerun. Original reader03/owner04 completes
3187393/0, pixel reader3187435/0; proof45357/7d738dce and explicit release
24653/d37eaf72. ROOT3190874/0 independently replays the unchanged literal
oracle1792 pixels,4draws,2profiles,7images,9native children,14host transports.
ROOT3197235/0 accepts source184/CI37743643139 hardware and explicit release:
1683/f72b5d74. Real isolated KMT hardware passes; ordinary system runtime,
production profile admission and default Mesa replacement remain unaccepted.

CPU06 reader02 preparation ROOT3157241/0 preserves135 focused controls,
1535/c7fbd196. Fresh ROOT authorization2809/a351fe15 binds descriptor07f86969,
unchanged bundle52/source73, corrected f958 reader and latest d37eaf72 release.
ROOT now exclusively owns target CPU execution. Upload3198504/0, stage3199557/0,
parse3200169/0 and copy3201851/0 pass. Build3201913/1 is an actual failed producer;
views are skipped. Same-attempt collect3201946/0 and transfer3202068/0 retain
41953468-byte archive; observe/original review/release continue. No compiler
failure is repaired, retried or promoted to success within that attempt.
Queued bounded KMD fdfd8f9 packet4fafdf5b remains local preparation, unsigned,
uninstalled and historical; its native execution waits for actual CPU06 release.

Remaining replacement work includes successful current-core CI/original review,
fresh DX8 AST5/staging/names/HAL/offscreen448/Present64, ordinary hardware
DX9/D10/D11 resource/shader/MRT/SO/predication/shared/primary/threading coverage,
D11 immediate/compute/UAV/tessellation/interfaces, DWM and dynamic Explorer.
Depth-only/null-PS D10 draws and actual renderer predication are separate source
work in progress. This checkpoint does not open0x17f/0x37f or replace Mesa.

## Historical evidence and checkpoints

D10/D11 replacement status: the mandatory D10 callback table is complete,
but production D10 requirements remain0x17f: stream-output, predication,
opened/shared resources, complete resources/shaders/MRT, primary/DXGI and
runtime threading. Some bounded paths are implemented; complete semantics
and ordinary target validation are still required. D10.1/D11 add their exact
interface/immediate/compute-UAV/tessellation/shader-interface requirements.
Public supported-version discovery and pipeline levels remain unadvertised.
Latest corrective CI37736830999 on ec6209e is terminal FAILURE (x86/x64
allocation regression; ARM runtime SKIPPED); prior accepted CI37724958522
remains CPU/fixture evidence, not ordinary VM runtime or desktop acceptance.
No default VM UMD replacement has been accepted.
Latest published18452d7 soleNativeUMD37743643139 is running with50/37;
actual newnames accepted666e5fe1, freshheldfrontendHAL in progress.

Previously accepted source 1b378e61f9b4e852cb3deffc305cc652aabfeafa integrates corrected
typed DXGI Blt and preserves all45 prior ARM cases/32 shipped fixtures, adding
one to each (46/33). ROOT b1806c72 joins all10 Blt files to frozen a3 source,
all current DX8 helpers remain unchanged, and the owned push2821662 closed0.
Sole NativeUMD CI37724958522 completed all6 SUCCESS. ROOT061b134b directly
rejoins five original ZIPs/9758 members/994 Git blobs/five links,138 closed
native fixtures,18 cube/three volume/three Resolve/three Blt raw reviews.
ROOT623f72e8 independently binds the terminal API and closed collector; new
ARM Resolve and Blt raw originals are retained and accepted. No extra run.

Previous d650 CI37721066402 all6 succeeded, but ROOT 2d8db135 accepted ARM
Resolve only at runner-proof scope because the upload whitelist omitted its
36 raw originals. Five original ZIPs/9503 members/989 Git blobs/five links,
four canonical receipts/135 closed native fixtures and18 cube/three volume
reviews remain accepted; independent x86/x64 Resolve pixels passed. Preserve
that partial original review and do not relabel missing ARM pixels as accepted.
New CI46 independently accepts the fresh ARM Resolve/Blt originals; prior
d650 partial evidence stays at its original scope.

Frozen Blt a3c7409 corrects97a58e5's erased-table callback. ROOT db824203 joins
2839 originals/241 Git+dependency inputs/657 compiler inputs/253 portable
joins/four optimized x86/x64 COFFs/47 closed children. GCC optimized and Clang
ASan/UBSan each passed340240 controls; independent synthetic reader accepted
one positive/rejected18 mutations. Primary work is separate and local. Frozen
939ae931 plus frozen451bd978 are now integrated together as13 exact source
files. ROOT51639d8d rejoins4530 original TAR members/250 Git inputs/340 compiler
inputs/eight optimized x64/x86 COFFs/21 closed compile-control children, plus
peer source reviewfa6257b5. GCC/Clang each5637 and literal-reader1positive/
eight negatives passed. Failed nonterminal unlock is retained/retried before
new map/copy/rotation; terminal unresolved unlock reports failure without
mapped Deallocate or callbacks after DestroyDevice. Synthetic terminal
runtime3 releases/two map closures stay separate from UMD successes. Preserve
all old46 ARM cases/33 shipped fixtures, add two (48/35), and retain new ARM
primary raw originals. Native execution24images/768pixels/72files and actual
ordinary runtime/scanout remain pending; gamma/shared/VidMm gates stay closed.

DX8 CPU10, parser01 and fresh de72 payload staging remain ROOT-accepted with
actual split CPU originals, AST5/Add-Type and17 staged payload originals.
The current admitted manifest dbe5e1a9 changes only ready/native_phase_parse/
pending. Names01 was rejected before Start by the invoke script's stale owner
whitelist. ROOT765cf75d directly joins36 narrow cleanup originals: exact task
Ready/neverrun267011/XML matched, no execution markers, only that task removed,
old attempt/helper files unchanged and both native/fresh4host/old6host/both
outer receipts closed. Failed names01 is preserved and never accepted.

Fresh names02 ROOT-principal authorization succeeded once. ROOT70ba8098 directly
rejoins38 originals/TAR290177f7, genuine I386524-byte/260-word name query,
physical runtime identity, two native children/seven hosts/outer/task removal
and actual releasebaef3450. The installed DriverStore name remains the old
viogpud3d_x86.dll. No new default binding or HAL acceptance from names alone.

Actual enumerate01 advanced past the former frontend denial: system
Direct3DCreate8 object1/adaptercount1, new core CreateDevice forwarded once.
HAL caps8876086a FAILED because private DXVK Vulkan instance initialization
failed; exact612-byte log enables only VK_KHR_win32_surface. ROOTb30be87e
joins41 original files/TARf7f95210, unchanged six production inputs, selector
restore, failed native probeexit1/collector0/seven transports/outer closed,
exact task removed and explicit release6572ff3a. That failed attempt does not admit HAL; current enum02 is accepted separately below.
Frozen37b8a2d is integrated locally as0bbdf18 (six exact source files). It
changes only the owned ICD JSON path to .\\viogpu_gl_vk_x86.dll
and adds explicit -icd02 candidate/payload pins. ROOT9e67e94a directly rejoins
509 portable controls,seven derivation negatives,three changed local I386
COFF/twoPE and exact6a loader path cases3. The old local guard object stays
excluded; native02 independently rebuilt the current guard. ROOT448a90c8
manually reviewed all helpers/host/outer/reader and35 committed/licensed
inputs before authorizationd142b1c5. The actual native build passed4COFF/3PE,
509policy/frontguard/five malformed CLI controls with19 closed children.
The whole attempt remains FAIL: posthash compared50 preexisting historical
Native telemetry values. Current before/after/post state is exactly identical;
static34 and selected575 compiler/13SDK/9library inputs are unchanged.
ROOTfa9f189f directly replays scoped reader06/all136 TAR originals and accepts
native-build scope plus failed-attempt explicit release, preserving original
posthash/host failures. Fresh five-payload -icd02 staging actually passed
once under ROOTc236b4a9. ROOT32c9f843 independently rejoins18 TAR originals,
four original API ZIP/CRC/PE payloads and the separate148-byte derived JSON,
static34/SYS/desktop, two native/five host/outer/offline-reader closures and
explicit release8e434536. Current phase40d is integrated as six exact
production files. ROOTf83d9675 rejoins every Git/snapshot file and preserves998
other files, then independently runs all136 portable protocol controls under
closed child2922037. The archive dispatcher now carries the explicit joined
core tuple; offscreen/Present accepts the exact -icd02 root with the derived
JSON lock while retaining448 readback/64 screen pixels and allocation lifetimes.
The prior enumeration fallback already matched de72; this repair does not
reinterpret the real Vulkan failure. Fresh packet03 and parser/names/HAL02
remain disabled until their respective actual receipts pass. Old packet01/02
and unexecuted parser01 recipes stay preserved. No HAL/device/draw/Present/
default replacement is accepted from staging or synthetic controls.

Consolidated primary939+451, ownedICD37b and phase40d is published as
 a37c72d3bc9af0f64a57c1cbb034eb9e5dce10d2. Owned push child2922823 closed0;
only NativeUMD run37733967652 triggered. Terminal result is FAILURE: shader,
identity and ARM64 build PASS; x64/x86 primary fixture fails at line455 with
callback status88760870; ARM runtime is SKIPPED. Correct callback uses
D3DDDIERR_DEVICEREMOVED, while the two fixture comparisons expected DXGI.
Frozen dc53288 corrects only these two comparisons (455/623); ROOT rejoins379
originals/273 compiler inputs and optimized x64/x86 COFFs. Direct HRESULT,
ownership, mapping, release and24image/768pixel/72raw checks stay exact.
Fresh native execution remains pending the single corrective CI push. Existing46 ARM cases/33
shipped fixtures stay present and two are added (48/35), with primary raw
originals in the ARM upload whitelist. No extra CI run or plan-only push.

Fresh current parser02 actually PASS under ROOTc64e8abb. ROOTa1433c3b directly
replays all20 original TAR members and native02's136 originals; AST5error0,
unchanged CSharpd8cf, actual2native/5host/outer closed0/drained and release9562969b
are joined. The three-field manifest admission52f7abd1 changes only ready,
native_phase_parse and pending; admitted manifest15f40829 keeps all original
source/binary/CPU/payload scope and no hardware/default admission. Fresh names02
alone is authorizeda3185558: limited USER current I386524-byte KMT name query,
two native/seven host/outer/task removal. DX8 agent owns target until same-attempt
originals and explicit release are ROOT-reviewed; all other agents remain local.
Fresh current names02 actually PASS; ROOT2c13fdc3 independently replays all38
originals and the exact166291-byte proofd9dc890e. Genuine I386524-byte/260-word
query and physical runtime, static34/SYS/desktop, actual2native/7host/outer and
exact task removal/explicit releasee7ddd36d are accepted.

Fresh genuine system8 HALenum02 actually PASS after ownedICD path repair.
ROOT6d36b26c independently replays all41 originals and exact177424-byte
canonicalproofc5f481fd: HALcaps=true, interface8,212capsbytes, internalcoredevice1,
ownerteardown1, publicCreateDevice/draw/Present=false, initializationRender0.
Original12814-byte log6c78b49b shows VK_KHR_surface enabled and actual Adreno830
Turnip26.2.99 device construction with matching LUIDec6b000000000000. Old
612-byte initialization failure stays immutable. Actual2native/7host/outer
closed0/drained, task removed and release98cd5faa accepted. Public workloads,
448offscreen/64Present pixels and default VM replacement remain pending.
C2 read-only observation01 actually PASS; ROOTb3be5b4f directly replays25
originals, with only retained-original equality count886-to911 added. All70
before/after source observations match; genuine msvcdis140.dll,msvcp140_1.dll,
pgodb140.dll ARM64-output copies are captured. Native5/host9/outer are closed
and explicit release4c1eeeb5 accepted. No compiler/module/repair/PATH/registry
or original ISO/EWDK mutation occurred; compiler readiness/load cause remains
unproved. Target handed to DX8 under ROOT03e15dcf for one frozen offscreen03
workflow: seven scenes/448readback/noPresent, currentHAL/manifest/staging
required; native2/host7/outer/task/selector/lifetimes must close before release. New D10 shared-primary and D11 2D SRV
remaining-range production slices continue locally with all caps closed.

CPU02 actually FAILED at the first recovered resource after17 readable ARM
copies. ROOTb14c2be1 directly rejoins86 originals/17 unchanged source copies/
33 original resources/native Parse6,allfive native andnine host owners closed
and explicit release3dc11393. No compiler/link/view build stage ran. The
ConcurrencyCheckui string14.50.35723.00 differs from fixed14.50.35723.0;
ce067879 explicitly compares recovered-resource fixed fields while retaining
raw strings/hash/machine pins. ROOTca0e10c0 directly rejoined33 original PE
version blocks; ROOT prepared replay236 and authorizationd403895e bound fresh
CPU03 after actual DX8 native02 releasefa9f189f. CPU03 Parse6 and all67 copies
passed. Its first ARM64 CL failed2 with D8027: cannot execute the copied
Hostarm64/arm64/c2.dll. Later frontend/view stages were not attempted; no
module/probe ran. Original151-file TAR686df426 is retained; agent original
reader1110 and ROOT replay1261 pass original-integrity/closure only, preserving
actual build=false. ROOTa0de4e20 accepts seven closed native children/ten
closed host transports and explicit release1f554914. The ROOT comparison's
initial check-count mismatch is preserved; fresh scoped review accounts for151
already-retained original equality checks without changing any raw evidence.
CPU03 and staging03 releases are historical; latest accepted target release
is C2 read-only ROOTb3be5b4f; current HAL ROOT6d36 remains workload prerequisite.
DX8 offscreen-only window03e15dcf has closed FAIL and ROOT37f91309 accepts
closure-only release; target is idle ROOT-owned. DX10/DX11 remain local. Local exact CPU03 PE imports reveal
omitted eager msvcdis140.dll and MSVCP140_1.dll, plus delayed pgodb140.dll;
actual source readability/search resolution and OS-load cause remain unproved.
A narrow native read-only dependency capture is being prepared without build,
module execution, repair or original EWDK/ISO/PATH mutation.
This ARM64X frontend is the DX8/9 legacy OpenAdapter entry, not modern D10/11
runtime admission; modern mandatory semantics/hardware proof remain separate.

ROOT c14006d5 independently replays1949 original-only joins for33 same-version
resources, including genuine17 native TAR members/32 Microsoft HTTP206 ranges
for16 x64 resources, exact target-specific clui bytes/versions and native
inventory continuity. No original D: compiler/ISO change or full ISO hash claim.
ROOT b69be46f manually reads all CPU02 producers/controllers and joins51 packet
members/59 frozen references/seven Git inputs/34 readable originals+33 resources
and selected actual SDK/libraries before authorizing a1767ca. Full compiler
attestation remainsfalse; actual ARM64X/hybrid and view execution are pending.

The installed viogpud3d default remains unchanged. Actual system-runtime
DX8-DX11 hardware rendering, Present and DWM/Display+Render are not accepted.

# Progress

2026-10-08 de72 original recovery02 PASS: collectcd7bcad4 closes all3 API,
fiveZIP andsixlog subprocesses, download333/383/331sec. ROOT72723054 directly
joins9380 originals/969Git+5links/fourcanonical/123actualclosednativefixtures,
18cube and3volume readers, allPASS. ROOT standardcore originale05dcef3
independently reads all3 rawZIP/API digests, executable standard OpenAdapter/
OpenAdapter10_2/private legacy entries, no applicationfactories/security0 and
actual CodeView/MSF7 PDB GUID+age. Newx86 core5517312/ba60b53f; ARM6279168/
45934284; x645931008/803265b5. Agents receive exact originalsource/run/path/
SHA; no target staging/loading yet. Initial reader draft private-export label
was corrected against originalGit DEF before execution; actual reviewPASS.
Main57 CI37713183282 final FAILURE: x86/x64typedresidency fail, ARMbackend
compilePASS, nativeARMexecutionSKIPPED. Preserve fullfailed APIs/diagnostics;
legacy de72 candidate remains independent. ARM64X forwarding/build-only
slice2e875aad frozen locally; ROOT manually reads allCPP/producer/docs and
requests source/tool packet, no native execution/registration yet.

2026-10-08 actual CI43 x86 failure confirmed: originaljob113103594071/log
851213/SHA31669a43, collector PID2746686 closed0/stderr0. Real residency
transaction102085 PASS; typedDXGI fixture line335 requires owned.empty() &&
!callbackAction && errors==0 and FAILS. This matches pinned ownedallocation
outliving Device::close/serviceclose. DX10 separately implements terminal
allocation ledger cleanup on DestroyDevice caller; preserve assertion and all
failed original bytes. de72 all6PASS remains independent legacy candidate.

2026-10-08 01:44 UTC continuation: published main57b3131 is clean; sole43-case
CI37713183282 runs once, no manual dispatch. Standardentryde72dc2
CI37711793677 all6 SUCCESS. Original collector01 retained three450sec killed
partialZIPs (x64/arm64/x86; no CI failure), both small archives complete.
Freshcollector02 changes only outputroot and bounded download timeout900;
ROOTprepared originalsourcee2f9b327/readers9a4e6ecd, active local downloads.
ROOT additionally prepares pure originalPEexport/CodeView/MSF7 PDB identity
reader for all3 genuine newcores; no module load or target call.
EWDK constructor correction remainslocal: permit system8 internal initialization,
immutable version/interface/flags contract, typed draw/Clear/Blt/Present denies,
HALfailure exit1, freshcore tuple from separately admitted phase manifest.
CPU09 preparation no longer waits for core ZIP; source freeze/nativepacket next.
DX11 actualARM64X compiler/linker producer and normal System32 D3D9 probe
remainlocal and unregistered. DX10 audit found terminal allocation ownership
can outlive RuntimeService close; separate narrow ledger/teardown fix requested
before shared Resolve integration. Actual target owner ROOT idle; no default
replacement or success/hardware claim. All original failures remain retained.

2026-10-08 latest: actual enum789c3d73 child0 observationally completes, but
strict855 original reader rejects before HAL admission. Direct3DCreate8 calls
one internal CreateDevice with Interface8/version69632/flags0/callbacks1/
functions1/obsolete buffers0; readonly frontend returns8876086a and forwards
zero core CreateDevice calls. Core DLL/open succeeded; CAPS12/device/draw/
Present never reached. Original40 files/TAR309c2bfb/stdout113180 retained;
ROOT exact reader produces byteidentical120c82a6. ROOTd12d1109 accepts explicit
release702c0c9d/native2/taskremoved/7host/outer/9localowners absent. No retry.
EWDK now local-only narrow constructor/teardown/no-draw boundary repair, HAL
failure exit and real standard entry/new core pins. Oldd7 lacks documented
FOGINFVF0x2000, but this was not the pre-caps failure cause; prefer newde72core.

CI90 genuine collectorf6c35d07 closed all downloads/logs. ROOT91411b62 directly
reopens9368 files/fiveZIPs/966Git blobs+5links/fourcanonical/core-private-loader
configuration and120 closed owned children. Independently repeats18 cube raw
readers and3 volume arithmetic reviews on actual originals, allPASS. NewARM
core6279168/d7d71695 is hardware-unrun. Local reader draft incorrectly assumed
ARM andnonARM fixture sets identical; corrected to exact35 build calls+5cube
loop and distinct40ARM patterns after inspecting two CPU-only differences.
No CI/native rerun. Newpublicentry de72dc2 pushedonce/soleCI37711793677 running.

ROOT manually reviewed realresidency/priority sourcecc878273, then original
501-file TAR/72Git/338compilercopies/10O2COFF/14closedprocesses/rawGCC+Clang
ASan+UBSan102085 PASS, ROOTd80e5b49. Local reviewer initially assumedO1; actual
commands showO2, corrected without changing evidence or rerunningcompilation.
Merge d8689ac retains8exactcc implementation/doc files and6legacy/contract
files; Meson parse/41prior+2newARMcases43 PASS. Sole conflict was shipping-list,
resolved exactunion preservingpubliclegacy andaddingbothresidency executables.
Native43 CI stillpending, no extra push yet. DX10 prepares real shared-resolve
handoff/pixel/lifetime work in separate branch, gates stillclosed.

2026-10-08 current: original I386 staging underd6cd4674 PASS, original TAR
63e67db2 has17 files/all4 original payloads. ROOT direct staging reader PASS,
admission equals46471352 after normalizing only2 CLI-derived relative path
strings; initial exact-string comparison failed locally with no target action.
ROOT346022f6 accepts release19d2203c/native2/host5/all7local owners absent.
Default registration/SYS/desktop unchanged. ROOT preparedenum4e1daea2 joins
12 pins/unchanged11 names helpers/full host recipe/local no-target plan.
EWDK now exclusively runs one enumerate-only attempt789c3d73, unchanged
07c/names2bda/staging4647; no CreateDevice/draw/Present/default/VM action.

Standard legacy OpenAdapter318d330 locally merged without conflicts. ROOT
directsource/localoriginalreviewe37e639b passes8 Git files/2source controls/
15strictCOFF/5preservedARM failures/6LLVM/current303deps, explicitly no
historical whole-toolchain or native runtime claim. Actual merged Meson parse
PASS, exact5 implementation/doc files preserved, all40 previous ARM cases plus
new public-entry case41 and both public cube hooks retained. CI37709079286
at90fb093 all6 SUCCESS; original fiveZIP/sixlog collector running locally.
DX11 separate deployment/probe branch prepares unmodified normal System32
D3D9 path and registry-kind-preserving conditional rollback. Genuine ARM64X
frontend remains required for shared ARM64/x64 slot; default replacement open.
DX10 residency/priority slice remainslocal and keeps ordinary modern gates
closed while primary/Blt/mode/shared resolve/render allocation ownership gaps
are implemented. Full remote acceptance and broader goal remain active.

2026-10-08 live DX8 names milestone: CPU08 release368bc2d5 accepted ROOTbbd8a239.
Native phase parser08 one-attempt auth2ed4101c PASS (AST5/Add-Type,20 originals,
native2/host5/outer closed); ROOT051b2c65 accepts release6aff3997. Manifest
07c46de5 admits only ready/native_phase_parse/pending. Names one-attempt
auth78235c19 PASS under actual limitedUSER/session1: I386 KMT ec6b/source0,
ABI524/260 words/version0, exact actual DriverStore viogpud3d_x86.dll.
ROOT independent frozen855 archive reader returns byteidentical admission
2bda9e1c on38 original files/TARc4077d19; ROOTca61a0ff accepts releasec50a177c
with probe/task/collector/7host/outer closure. No factory/core/render/Present
yet. EWDK prepares original payload staging and next runtime phases locally.
Single push90fb093 launched onlyCI37709079286; Linux andshader alreadyPASS,
three architecture backends pending. CPU04 helper-only ROOTdelta85f9ac8c PASS;
local review drafts failed from substring/slice mistakes before proof output,
corrected after inspecting actual source; no target retry or mutation.
DX10 local audit now targets ordinary-runtime10/11 admission. DX11 owns
normal legacy OpenAdapter and documented fog-cap mapping implementation.

2026-10-08 current continuation: prioritize actual VM DX8/DX9 enablement.
CPU08 native parser/build/collector and7 host transports all0; ROOT945dba7b
directly rejoins107 original TAR files,21 sources,4I386COFF/3PE,18 closed
native build children,91 predicates/4 malformed CLI and3 physical pairs/6
closes. EWDK finalizes explicit release before855 system names attempt.
CPU03 SO/volume actual oracle invocation failed D9002/LNK1104; ROOTa7a6ce97
accepts156 originals and release0175b8ff, with3COFF/2PE including partial
oracle output. CPU04 helper-only separate compile/link is frozen locally.
ROOT6b source/local-original reviewd1b48152 passed; the prior inline reviewer
incorrectly required exit0 for negative controls. Corrected persisted reader
joins their actual expected exit1 and retained stderr. Six local strictCOFF
and production oracle boundaries unchanged. Aggregate3d19c53 merged into
main, adding6b mip correction to855/5624/66b/f96/7a47/8528 integration.
Merged73 physical phase controlsPASS;40 runtime registrations retained.
CI public-cube reader hooks and ARM raw-artifact retention added. One push
queued for combined source; actual native6b/5624 and real GPU probes pending.
DX11 local audit found no standard legacy OpenAdapter export; implementation
and reversible deployment preparation assigned. No default registration yet.

2026-10-08 checkpoint: ROOT accepted combined cube02 failed originals and
release4021b90e with directproof f1b472e9; target ROOT idle. Failure is missing
release RuntimeObject import library, before any reference fixture. Complete
member scan also finds AdvApi32/Synchronization, so fresh cube03 includesall3.
ROOT acceptedf96 CPU03 prepared originals99d9b777 and locally reviewed owned
CPU03 controller/host1bbb7e5a; queued after cube03, no target authorizationyet.
ROOT aggregate3213fda has cube/SO/volume real-KMT probes and38 ARM cases.
DX8 agent freezes shared physical-identity66bfbdf, nativeCPU08 preparation
ongoing. Local strict4COFF/2PE and91included GCC/Clang controls pass. Latest
publishedCI37689715890 remains green; four redundant workflows disabled,
no new push. User authorizes passwordless Administrator when needed forbuild;
actual graphics limitedUSER1 unchanged. Full goal remainsactive.

2026-10-08 runtime03 continuation: root current native GPU staging PASS with
original50224855 tar/coreb0fcbc3a/compute8efb72c1/probe54ed0539/statusae8c823d;
rootstageproof5ff88170. Root review2d691375 admits only the frozen CPU05
module feasibility attempt (descriptorc5757fd5/payload06590733/helpers95f96982),
authorizationfd5acd14, exclusive target ownerDX11. Uploads and native stage0;
Prepare controller1 before registration/load with all helpers unchanged,
fresh guard0 and failure cleanup0/taskabsent. Collect original failure and
diagnose exact input guard; no success/GPU claim. DX10 frozen follow-on03 and
private160 local packets received; agent prepares a separate bounded USER
read-only query task. DX8 isolated currentd7 slice prepares actual Interface8
CreateDevice forwarding and genuine SysWOW64 offscreen/Present gates.

## Native D3D9 resource continuation

Previous goal turn is progress: production Mesa/DXVK commits, verified CI
artifacts and three real target lifecycles change the next action. Full goal
remains active; empty lifecycle does not close DX8-DX11 system-runtime gates.
Recovered clean b5b45b3 worktree and authoritative f648 source CI all4workflows
successful. Windows22/Android8022/ADB5555 respond. Auditing typed resource,
render-target, clear and readback contracts against retained guest SDK headers
and Microsoft specifications, followed by real nonempty target GPU work.

Resource fixture worktree01 compiles/runs on native ARM64:77449checks,
3.6520244s compile/0.0572746s run. All52 source hashes, archive/EXE/AA64
and unchanged driver/desktop independently verified. Added three-stage real
GPU byte/padding/subresource oracle to the KMT probe. Worktree02 preflight
failed because Windows RPC defines small as char; rename rectangle smallArea
and use fresh03 roots. Preserve the failed02 evidence and regenerate unused
semantic controls from the corrected probe, so controls fail behaviorally.

## 2026-10-06 diagnostic build verification continuation

Resumed at exact diagnostic KMD c1b9ea69. Windows22 and Android8022 are
independently online. Downloaded the completed native guest build receipt,
SYS/PDB/MAP and logs for independent verification; reported runner5.429254s,
VirtIO0.754602s and KMD4.367460s. Receipt retains installed58623 and desktop
DWM1644/Explorer5828. This remains unsigned, uninstalled compile evidence.
Added a parameterized Git/archive/output/PE verifier and read-only guest
signing-tool/certificate inventory. Verification and packaging remain active.
Corrected discovery paths after absent flat-inf-spec.json, a worktree-relative
workspace-script lookup and a zsh unmatched roadmap glob; none changed source.
Independent c1b9ea69 verification nowPASS118 source files and all reported
output hashes, ARM64 Native PE/import policy and installed/desktop continuity.
Pushed exact c1b9ea69; CI37426954641 is running. Added parameterized build
version and compiled identical diagnostic source as58624 in5.741236s.
An initial rebuild referenced an absent remote source-receipt filename;
transferred both exact local inputs under explicit paths before the successful
retry. Guest package preparation copies the complete active DriverStore
payload for rollback, preserves runtime hashes and uses distinct test signing.
No diagnostic installation or reboot has occurred yet.
Guest CurrentUser certificate creation failedNTE_PERM(80090010) in SSH.
Three concurrent download sessions hit banner timeouts during that attempt;
the next script transfer succeeded. Independent daemon/root checks confirm
the same sole VM PID3980 and6656MiB, with Android SSH still being checked.
Switched signing preflight to a distinct LocalMachine software-CSP certificate
before copying another package, and serialized further guest transfers.
The local58624 verifier correctly refused because its download was incomplete.
Serialized download succeeded, and independent58624 build verificationPASS.
Distinct LocalMachine software-CSP signing preflightPASS thumbprint
DF9791D9362A8E2FA962C1683625D74D4DAE5D05; no private key export.
CI37426954641 failed in inherited vblank fixture MSVC /WX: SAL macro
redefinitions and production local `late` shadowing a fixture peer member.
Guarded existing SAL definitions and renamed that peer member; applied the
same guards to the new readiness fixture. These edits affect fixture inputs
only; c1b9ea69 production KMD inputs and guest diagnostic build are unchanged.
The second package attempt reached public-certificate export but CurrentUser
Root import requires forbidden UI in SSH. Changed only that trust destination
to the machine Root store used by existing CI/installation scripts; signer
stays the distinct nonexportable machine-store key. No KMD installation yet.

2026-09-11: Added API-only WARP control matrix to the existing CPU fixture;
it does not use UMD translation helpers and runs before state changes. Tests
default/selected SRVs and full/explicit mips to locate the ignored operation
in one run. All mandatory full-image checks remain unchanged and gating.

2026-09-11: 4ba6a19 x86 WARP still old-green with an active empty InfoQueue.
Next controlled change requests MipLevels0 full-chain allocation per Microsoft
CreateTexture2D and verifies all8subresources around the selected2mip range.
Typed/typeless comparison did not identify a cause. Production unchanged.

2026-09-11: Typed e98896b control repeats unchanged green; actual descriptors
and caps are correct. This is not a typeless-only behavior. Added Microsoft
debug layer/InfoQueue output for the original creation and GenMips operation;
fallback is only DXGI_ERROR_SDK_COMPONENT_MISSING. Production unchanged.

2026-09-11: ef4b174 CI34615056755 ALL5PASS. c40dc0d diagnostics show old
green mip1 untouched, not a rounding error. Read official GenerateMips typed
format/support contract; changed the mip oracle to an isolated typed RGBA8
resource with queried MIP_AUTOGEN, preserving old typeless clear/sample tests
and exact all-pixel assertions. Input descriptor/cap fields are now logged.
Typed control result pending; production c9e389d GenMips code unchanged.

2026-09-11: c9e389dCI34615423153 x64/x86 compile production and test but
WARP view fixture fails check391 pixels. Prior ef4b174 x64/x86 bothPASS.
Added first mismatch subresource/coordinate/expected/actual and stage labels
to identify whether mip generation or subsequent shared-fixture state failed.
No relaxed assertion or production fix based on an unlocalized assumption.

2026-09-11: c9e389d GenMips push completed; NativeCI34615423153 running.
State-reset ef4b174 CI34615056755 remains active. Synchronized public
umd-native-status with the code and ordinary-runtime/ARM64X acceptance gates.
No device operations and no candidate runtime registration performed.

2026-09-11: ef4b174 CI34615056755 running. Continuing mandatory GenMips DDI
with native auto-mip creation flag translation, owned resource/view validation
and existing DXVK GPU mip generation. WARP verifies selected-array isolation
and exact lower-mip pixels; target GPU workload remains unchanged. New source
is not yet compiled or published. A guessed workflow filename returned404;
resolved actual .github/workflows/build-native-umd.yml before retry.

2026-09-11: cf492c9 CI34613950771 ALL5PASS. New continuation implements
native UNDEFINED and other D3D10 topology setters, full16viewport replacement,
null-viewport holes and unconditional zero-count viewport/scissor unbinding.
Added independent WARP state getters to the existing view test, preserving
the backend GPU workload. CI pending; no ordinary-runtime or hardware claim.

2026-09-11: Resumed cf492c9 push complete. Native CI34613950771 identity and
shader-cpu PASS, ARM64/x64/x86 builds running. Frozen5b0983d/61edc56 artifacts
remain the delivered candidates. Parent supplied independent ordinary runtime
f6da604 probe CI34613737738PASS. Coordinated ARM64/x64 loader contract with
OpenCL agent and read official Microsoft Arm64X/WDDM INF documentation.
Updated activation gap count20->16 and recorded architecture selection rules.

2026-09-11: Runtime activation continuation closes four10.0 table slots:
default constant-buffer update, table relocation and counter capability/query.
The first reuses actual backend update; relocation keeps no stale table pointer;
counter result is the embedded DXVK device's real absence of performance
counters, without device-loss queries. This reduces the inventory20to16 but
does not expose an incomplete feature level. Existing probe merely exercises
these initialization operations; no new backend workload count or hardware
rerun request. Parent's ordinary runtime probe is the next acceptance path.

2026-09-11: DXVK5b0983d CI34612034025 ALL5PASS; exact ARM64 DLL/probe
downloaded, hash/architecture/source verified and recorded in hardware guide.
VKD3D61edc56 CI34612036226 ALL4PASS; ARM64 all10manifestentriesPASS and
standaloneguidewritten. Parent requested handoff then actual runtime wiring.
Created doc/runtime-activation-gaps.md with current export/negotiation/device,
resource/fence and Present dependencies. Parent builds a separate system-D3D
application acceptance tool. No new device actions or predication code.

2026-09-11: Paired97e7b00 finalartifact10268526395 downloaded and all40
candidate/finaldriver manifest entries match. Exactsource97e7/bc61/Mesa4ace;
INF stillselectsMesa. Guide records published archivedigest and independently
verified signed DLL/probe hashes, keeping them distinct from testedstandalone.
DXVK5b0983d replacementCI34612034025 and VKD3D61edc56CI34612036226running.

2026-09-11: Early970f14a WDK compile caught a docs/ABI mismatch: real SRV
hazard signature is device/view/resource, not the local docs' argument names.
Fixed production and probe argument order plus mutable clear color array.
New VKD3D45eb12b validates UAV data/counter handle registry; fixture zero-flag
spelling normalized to actual field type before replacement CI. No target
files delivered from these unvalidated sources.

2026-09-11: Implemented native whole-resource staging-busy query through
embedded DXVK CS/GPU tracking, no remap or GPU idle. Added buffer and SRV
hazard notifications with ownership/view-resource checks. Target fixture now
queries one mapped buffer and four simultaneously mapped texture subresources,
checks fail-closed nonstaging input, and requires actual clear-to-sample and
buffer-update-to-index transitions. New implementation CI/runtime pending.
Paired97e7b00 CI34610190479 nowALL7PASS; final artifact verification next.

2026-09-11: Parent1a7cf3b target PASS exit0/259ms,640MSAApixels and4096draw
pixels correct, copy/depth/stencil/KMTpublicationPASS, original shell retained.
Parentbc61de9 target PASS exit0/1167ms,17workloads/17408words including two
native root32 updates; original shell retained. Exact runtime identities and
host-proof limits recorded in respective hardware guides. Standalone CI all
passed; paired97e7b00 CI34610190479 still running. Continue real native
staging-busy and hazard interfaces. Resume searches guessed two absent paths;
use rg file discovery for backend/engine source locations before opening them.

2026-09-11: Typed vertex inputs7b30c07 CI34598775633 ALL PASS. Dynamic
maps/error translation272a067 CI34599288562 ALL PASS on ARM64/x64/x86;
x64 logs confirm715adapter,100query/map,217allocation/present and74real
HLSL/reflection checks. All production DDIs/probes compiled. No target run.
Paired60005/a27eb3e/b47f955 passed Mesa/DXVK; KMD job103264521935 running.

2026-09-11: Stable depth b47f955 ALL PASS. Prepared/pushed paired60005
a27eb3e (ABI,377identity and full contractPASS), CI34598876655 running.
Standalone three ARM64 binaries downloaded from childCI; paths, SHA256 and
minimal usage recorded in hardware-probe-b47f955.md and delivered to main.
New vertex child7b30c07 CI34598775633 running. Continuing Map busy/removal
status translation and mandatory dynamic buffer/texture Map DDIs; new CPU
failure tests prevent partial output, and GPU probe uploads its vertex data
through DynamicIABufferMapDiscard. These additions are outside60005.

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

2026-09-11: Following8f7bce4, added bounded immediate-constant-buffer and inert
comment/debug block preservation. The ordinary-opcode length field must not
be used for these blocks: their second dword carries full length. Added
truncation/alignment/duplicate/unknown-class tests, real-HLSL dynamic lookup
and GPU-probe lookup-table usage. Local shader119PASS; new WindowsCI pending.
An initial patch context missed the word 'itself'; no files were changed until
the exact-context retry succeeded.

2026-09-11: Implemented generic VS/PS register linkage, typed producer variants
and pixel declaration-derived interpolation/raw32 types. Added real-HLSL
probe with interpolated UV validation plus flat unsigned and NaN/-0/Inf bit
payloads. Local identity69/trailer176/shader102 sanitizersPASS; Windows HLSL,
SPIR-V generation and new target execution still pending. New code is not
part of parent-tested272a067 and has not been installed.

2026-09-11: Parent reported real272a067 hardware ALL PASS on58386, current
LUID2A58000000000000, process-local56bd30c Turnip ICD9AA5. Backend exit0/224ms,
4096magenta/0mismatch. Native DDI exit0/195ms: actual adapter query/create0,
64byte copy0mismatch, depth0/4096/0, KMT backing4096pixels0mismatch,
4096finalred/event1/error0. DWM2028/Explorer5780 retained with no new
appfault/timeout. Exact three probe hashes matched. Parent's host-trace
summary remains pending. Native runtime/application/display Present still
unproven; no unchanged backend rerun is needed. This agent made no remote calls.

2026-09-11: Resume: paired60005/a27eb3e/b47f955 run34598876655 ALL PASS
reverified via gh. Signed final artifact10264495517,11542329bytes, SHA256
7557f872ba8c6b38c97eb52f138a2efc93c2e69f3427edc43be65d667f8614e7.
Parent will test standalone272a067 after GLES with LUID2A58000000000000;
this agent continues local shader linkage/DDIs. Full native D3D9/11 runtime,
applications and visible Present remain mandatory and unproven.

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
2026-09-11 resume: Read source/plans, confirmed clean68d6bb4 and completed
x86/x64 failures in CI34617903693; ARM64 compilation still running. Replaced
strict mip oracle's selected slice1 with slice0 and retained every exact pixel
assertion, original typeless slice1 tests and outside-view mip checks. Removed
temporary API-only diagnostic matrix; its decisive results remain documented.
No production workaround added. Delivered VKD3D61edc56/paired97e7b009 and
ordinary-runtime/ARM64X handoff to the new independent agent. Replacement
DXVK CI pending; no remote commands or hardware workload performed.
2026-09-12: Added GS shader/create/bind and three GS resource-binding slots,
actual VS->GS->PS relinking and native union-signature handling. Local ASan/
UBSan passed69identity/176runtimeidentity/144shaderchecks before the extra
union cases; final rerun and Windows original/rebuilt WARP+SPIR-V CI pending.
6dfb092 mip replacement x86/x64 CI34619152294 now PASS,488WARPchecks each,
debugqueue empty; ARM64 still compiling. No target backend workload changed.
2026-09-12: Final local GS/union-source ASan/UBSan PASS: identity69,
runtimeidentity176 and shader147. The first union rerun caught an obsolete
negative expectation and it was corrected to the Microsoft documented
unused-input behavior; missing/invalid declarations still fail. Mip6dfb092
CI34619152294 now ALL5PASS. Committing GS continuation for Windows CI.
2026-09-12: GS7c0b9e3 CI34620365858 fast shader CPU PASS2257checks.
Original and rebuilt VS->PS and VS->GS->PS each match all16payloadwords;
the GS fixture correctly changes fixedBits.x from12345678to03254769 via
the bound b1 XOR mask while preserving NaN/-0/Inf and sampled/interpolated UV.
Per-vertex GS input and producer/consumer final-SPIR-V types pass. Identity
job also passes; all three full production build jobs remain in progress.
2026-09-12: GS7c0b9e3 CI34620365858 completed ALL5PASS. ARM64/x64/x86
compile/link, PE/import/export checks and artifact upload all pass. x64/x86
execute2257shader/WARP and488view/mip checks. Downloaded only the GitHub
ARM64artifact10272751005 into probe-7c0b9e3-arm64, verified STATUSsource/arch
and DLL/probe hashes. Existing frozen artifacts and VKD3D source/pins unchanged.
No device actions, new backend GPU run or runtime-activation claim. Final docs
checkpoint describes the remaining SO/shared/negotiation/presentation work.

2026-10-05: Resumed per user's DX8-DX11 DXVK UMD priority. Selected the
existing dxvk-umd plan explicitly and recovered current source/package state.
Main driver tree is dirty with unrelated work; DXVK and paired UMD checkouts
are clean. Auditing primary/DXGI ownership as the next concrete source gap.

2026-10-05: Implemented published DXGI rotation with full-chain preflight,
stable runtime resource handles, reusable scratch copies for private/present
textures, and old-backing publish plus invalidation for shared surfaces.
Added production DDI/WARP regression and wired Windows/ARM64 build paths.
Loadability/integration checks and 172822 transfer/1125 shared-policy checks
pass. Local ARM64 syntax check initially lacked WRL winrt include path;
correcting the header path rather than changing production code.
User authorizes personal CI builds and target testing. Android SSH responds;
Windows SSH port22 currently times out at banner, so inspect the existing VM
and forwarding state before guest testing; never start another VM.

2026-10-05: Production rotation/DDI/allocation plus regression pass the local
ARM64 syntax check against the actual extracted SDK/WDK. Disposable lowercase
WRL aliases fixed Linux filesystem casing and were removed after the check.
Prepared work/native-dxgi-rotation-20261005 for personal CI. Authoritative device
control confirms the registered Windows VM is stopped; connectivity worker is
starting only that VM with its live saved configuration unchanged.

2026-10-05: Pushed rotation and review repairs through4f59adf to
work/native-dxgi-rotation-20261005 on sunflower2333/dxvk. Personal native offline
and full native UMD workflows dispatched. Review found/fixed reentrant private
storage reads and final identity commits; added callback retirement/poisoning,
nested rotation and exact deallocation-ownership checks. No remaining concrete
review findings. Local x64 syntax fallback is blocked by extracted new MSVC STL
using __builtin_verbose_trap unsupported by local Clang; use actual MSVC CI.
Windows guest is running, SSH22 restored; installed driver58623 and Mesa
b36366b remain unchanged. Current saved RAM6656MiB was preserved by daemon
start. Await architecture runtime checks and exact ARM64 artifact.

2026-10-05: Native offline CI37310474949 SUCCESS on exact4f59adf: Linux
sanitizer contracts, x64/x86 production-DDI WARP execution, ARM64 production
DDI/fixture compilation. Full native UMD CI37310476652 has passed identity and
shader CPU jobs; backend architecture compilation remains in progress.
Downloaded artifact is pending native ARM64 guest regression execution.

2026-10-05: Exact ARM64 rotation artifact11345428856 verified by SHA256 and
PE machine before guest execution. Passed1281checks/43synchronized locks,
exit0. The same production-DDI checks pass on x64/x86 in offlineCI37310474949.
Remote runner recorded desktop PIDs and explicit WARP/no-install/no-hardware
acceptance limits. Pulling JSON/stdout/stderr into retained local evidence.
Full native backend CI37310476652 is still building all three architectures.

Guest evidence collected and checked: source/hash/exit/stdout match; stderr is
empty; DWM5384/Explorer3704 retained. PowerShell Get-Content attached filesystem
metadata to its string, bloating the raw JSON. Preserved the original and wrote
a compact normalized receipt; fixed future runner reads with File.ReadAllText.
No identical workload rerun. Paired audit found that the ARM64 fixture-copy list
must gain rotation.exe alongside both source pins; preparing this candidate
integration without changing its older58522/Mesa baseline or installing it.

2026-10-05: Full native UMD CI37310476652 SUCCESS on exact4f59adf, all six
jobs: embedded backendARM64/x64/x86, identity, shaderCPU and nativeARM64runtime.
Paired worker is preparing the exact source pins and ARM64 fixture staging,
then personal signed-package CI; installation remains out of this checkpoint.

Promoted the DX8/DX9 source audit into docs/native-dx8-dx9-roadmap-20261005.md
so the missing typed adapter bridge, exact-adapter D3D9 core, system-runtime
DX8 proof and packaging entry points survive outside local remote-test notes.

Paired5798e077 pushed to personal work/native-dxgi-rotation-20261005 and direct
signed-package CI37312228456 dispatched. Both exact source pins and ARM64
rotation fixture staging are included. Existing73package tests and diff check
pass. Package version58522/Mesa pins/admission gaps remain preserved. Retain
receipts under workspace artifacts/dxvk-native-rotation-20261005/paired-ci-37312228456/.

2026-10-05 continuation: Mesa artifacts in historical34757244563 had expired.
Exact-source rebuild37312838033 passed ARM64/x64/x86 and source regressions,
with three fresh artifacts. Paired d5e482c4 updates the dependency run-ID in
both workflows and opengl/package.py, preserving source/version. Replacement
CI37318070394 passed all dependencies including OpenGL ABI and DXVK matrix,
but final build job failed; paired worker is diagnosing its completed logs.

DXGI audit found callback entry points may move between UMD calls.8dbcc93 now
retains the runtime-owned DXGI table; allocation regression replaces PresentCb
after initialization. CI exposed fixture table lifetime errors; af84b74 makes
rotation's table a fixture member, keeps ddi-probe table in device scope, and
stops clearing DXGI in the native-entry fixture. Generic push builds PASS;
new native/offline CI dispatched. No driver installation or additional guest
workload performed. Service503 interrupted three workers; all resumed.

2026-10-05 resumed after compaction: recovered exact source/CI and agent state
from selected plan. Offline x64/x86 production DDI fixtures now PASS onaf84;
full architecture/native execution is pending. Paired final-build failure is
VS18 versus pinned WDK17 tasks, before source compilation. Delegate minimal
cross-build/native-check split based on proven gg-zero-copy implementation.
Read-only contract review confirms live DXGI table requirement and finds a
separate pre-existing reentrant Present private-resource lifetime defect;
delegate focused production/regression repair before final paired promotion.
Clarified kernel/core copy versus live DXGI table in runtime documentation.
Project catchup emitted no additional recovery context; memory lookup had no
relevant registry hits. No remote configuration/install changes performed.

Offline37335070338 is now SUCCESS onaf84b74 for Linux sanitizers plus actual
x64/x86 DDI fixture execution and ARM64 compilation. Reviewer retains exact
source/artifact receipts; full37335069804 remains active without failures.
Reviewed paired CI diff: StaticOnly retains catalog/file/signature/installer
checks on x64, while separate signed-runtime-abi on Windows ARM64 runs every
native/EC/x86 GL/CL/D3D/DXVK load. New ARM64 functional job uses the exact pinned
DXVK script and source guard. Requested always-retained output/hash artifacts.

Paired e0449321 now preserves native functional/signed runtime checks in
separate ARM64 jobs, and selects matching VS17/x64 MSBuild for ARM64 targets.
All73package tests, YAML parse and diff check PASS. Agent dispatches a direct
CI run to validate this toolchain repair independently of the pending Present
source repair; current paired DXVK pin remains4f59adf. Added callback-table
lifetime and exact offline validation to the rotation checkpoint document.

Refreshed stale runtime-entry implementation sequence against actual source:
mandatory hardware D3D10 slots exist, bounded SO/predication/shared-open paths
are implemented, and DXGI has2/7callbacks. Corrected the inaccurate contract
comment that OpenResource always fails; production gap mask is unchanged.
No new feature level or hardware/runtime acceptance is claimed.

Exactaf84 full source37335069804 SUCCESS, all six jobs, including native ARM64
runtime fixtures. The pending Present repair now covers separately owned
allocation/readback and live resource reservation; LockCb/final PresentCb tests
destroy and immediately poison private storage, requiring balanced locks,
single deallocation and no post-retirement Present. Independent reviews found
nested Present/readback reuse and mutual Present/rotation identity hazards;
implementer adds busy/rotation guards before final review and CI dispatch.
Paired37336134109 exposed the inherited checker assumption of one ARM64 job;
agent repairs job-specific contracts and verifies all checks before rerun.

Further independent review confirms that a device-wide Present guard is needed
to protect shared publication from nested different-source Present, and a
bounded resource-liveness validator must run after identity/CreateContext
callbacks before PresentCb. These are explicit cancellation/ownership cases,
not hypothetical warnings. Paired full contract checker now PASS after its
job-specific repair; postpone repeated paired dispatch until final source pin
is ready so source and package validation can proceed concurrently.

Paired61d03f05 now enforces VS17/x64 product build, mandatory native ARM64
functional and signed-load jobs, and full non-StaticOnly runtime invocation.
Full contract checker,73package tests, YAML parse and diff check PASS.
Known-failed37336134109 was cancelled as superseded by its diagnosed checker
repair, avoiding redundant4fbackend builds; no observation timeout was used.
No new paired dispatch until the reviewed final DXVK source commit is ready.

2026-10-06: Final focused Present diff reviewed independently by root and
rotation reviewer: separate allocation/readback owners, registry reservation
checks after identity/context/Present callbacks, device-wide nested Present and
rotation exclusion, and pinned shared sweep. Actual production fixtures now
cover LockCb/CreateContextCb/PresentCb retirement with immediate0xccpoisoning,
no early-retirement PresentCb, balanced release and nested calls. Diff check
PASS; local WDK loadability policy101slots and identity sanitizers69/180PASS.
Full local native suite initially found an uninitialized tracked shader-parser
submodule; initialize its exact gitlink before finishing local checks.
Full af84 ARM64 artifact evidence is retained and its ten fixture hashes agree.

Tracked shader-parser213d2b859e83d91670ada15cc773e2c90c7c8b61 is initialized
and matches the gitlink. scripts/test-native-umd.sh now PASS under ASan/UBSan:
69identity,180runtime identity,147shader checks. WDK loadability policy passes
101/101 mandatory device slots, DXGI2/7 and unchanged closed admission.
Independent reviewer and root final source review have no concrete remaining
findings for this repair. Windows fixtures still require exact-source CI;
prepare source commit and concurrent final paired candidate validation.

Source34ff484ac87b66f77e1c9f6d8db914783b24788e committed with real multiline
attribution and pushed. Dispatched exact offline37338141939 and full native
37338144307; generic push37338074483/37338074581 also active. Reviewer handles
source CI and verified artifact retention; paired worker advances both source
pins and validates contracts before direct paired dispatch. Remote worker
prepares changed ARM64 lifetime fixture runner and read-only system-runtime
DLL inventory; execute only after verified artifact arrives. No installation.

34ff source CI initial results: offline Linux sanitizers PASS; full identity
and shader CPU PASS. All architecture fixture/backend jobs continue without
failures. Paired final34ffpins and73tests/YAML/diff pass; full checker pending.
Bounded next-gap audit confirms residency needs a native resource-to-backend
BO identity link; the Present publication allocation and Vulkan image interop
metadata do not expose all rendering BO handles. Save this dependency for
actual QueryResidency implementation rather than claiming unconditional residency.

Paired b6bf4c4f849f5ca597637fc2ce591f23a826f158 pushed with both DXVK pins
at34ff484. Full checker,73package tests,11-job YAML and diff check PASS.
Direct paired37338579561 now runs concurrently with exact source CI; signed
package/native-load evidence remains pending. Guest scope includes both changed
rotation and lifetime fixtures because CreateContext cancellation lives in
rotation; no repeat of the eight unchanged fixtures is needed.

Exact34ff offline37338141939 SUCCESS, all four jobs: Linux sanitizers, Windows
x64/x86 production DDI execution and ARM64 compilation. Reviewer downloads and
verifies artifact source/PE/hash before handing changed fixtures to remote
worker. Full native37338144307 and paired37338579561 continue without failures.

Remote system-D3D inventory retained read-only: native ARM64 Windows26100 has
System32 D3D9/9On12/10/11/DXGI but lacks D3D8; SysWOW64 supplies all six x86
runtimes. Added current DX8 acceptance architecture evidence to roadmap, with
DDI/renderer activation still unproven. Prepared transferred guest runner parses
successfully; waiting on exact rotation/native-lifetime binaries and hashes.

Offline34ff exact logs PASS1428rotation checks/44locks on x64 and1400/44 on
x86 (private-storage byte assertions reflect architecture size). Verified
ARM64 rotation.exe SHA256a03ba1df30eb753394ab248ee0d7702985101b6d1274b731a4065da4bcde8cd6
has source34ff and nativePEaa64; remote worker executes it once now. Full
native-lifetime fixture remains pending. Paired repaired full-contract/Zink,
GL/CL ABI and package tests all PASS, with architecture builds still active.

Exact34ff native ARM64 rotation guest PASS1428checks/44locks, exit0, empty
stderr, no timeout. Before/after DWM1644 and Explorer5828/start times preserved.
Receipt binds source34ff, offline37338141939 and a03bexecutable SHA256; compact
stdout/stderr/JSON retained locally. Requested copies into guest-34ff484 artifact
directory with digests; keep remote-plan originals. New lifetime guest fixture
is pending full ARM64 build, and full/signed package CI remains active.

Root inspected compact guest rotation JSON and offline evidence receipt:
source/hash/run/check output match, exit0 and empty stderr, same desktop process
identities. Offline archive source and all eight ARM64 EXE machines are verified
by reviewer; exact-source review is embedded in the receipt. guest-34ff484 copies,
runner scripts, inventory and SHA256SUMS are retained with originals preserved.

Paired37338579561 exact34ff DXVK ARM64/x64/x86 builds and native ARM64
functional execution now SUCCESS. Source full x64/x86/identity/shader PASS,
with ARM64 backend still building. The one guest lifetime run may use the
verified paired fixture input instead of waiting on duplicate source artifacts;
receipt must bind actual source/artifact/run, and no second run is needed.
Audited current package invocation: verify_bundle.py correctly keeps distinct
D3D Mesa eae74a and GL/Turnip3e50 sources; obsolete verify-joint-package.py is
not the active workflow verifier. No package source/pin change is needed.

Paired native ARM64 logs PASS10834lifetime checks and1428/44rotation. Verified
fixture input11357009608 from paired37338579561 binds STATUS34ff, nativePEaa64
and lifetime SHA2561a5618ccbb46c6f7c753ab1a0e572140e85242bf1839c9b7a1c359b1e9e88615.
It matches native runner validation hashes. Remote worker receives that input
for the one changed lifetime guest execution; no source-artifact duplicate run.

Exact34ff native ARM64 lifetime guest PASS10834checks, complete-contract-fixture1,
backend-calls5, exit0, empty stderr/no timeout. Paired37338579561 input11357009608
local/remote PE/hash verified. DWM1644/Explorer5828 retained session/start times
and remained continuous from rotation through lifetime. Compact lifetime receipt
and logs added to guest-34ff evidence; no full-backend fixture rerun is needed.

Exact34ff full37338144307 SUCCESS all six jobs, including ARM64 backend/native
runtime. Reviewer retains full source/runtime/hashes independently of paired
build. Both changed guest fixtures completed once with desktop continuity.
Paired input arm64/x64/x86 DXVK manifests verified exact source34ff/parentb6bf,
all file hashes/PE machines/private loaders and unchanged eight gaps. Product
job now compiling/signing on repaired VS2022 host; final package still pending.

Paired product111865802664 passed driver compilation and reached Sign+package:
matching VS17/x64 host resolves the prior WDK precompile failure. Full source
and guest validation is complete; final catalog/load/receipt remains required.

Product111865802664 SUCCESS: compilation, PE/exports, signing/package,
actual catalog/common signer, exact joint receipt and artifact upload pass.
Signed ARM64/native+EC+x86 runtime loading remains final gate. Full source
receipt inspected by root: all15ARM64EXE/DLLs AA64, all10runtime hashes match,
STATUS34ff, rotation1428/44, lifetime10834 and allocation5164PASS. Independent
guest bundle source/hash/desktop checks also pass; no duplicate workload.

Delegated native D3D9 bounded design saved to roadmap: typed in/out opaque
handles, exact identity query owner and conservative GetCaps/close harness;
production bareOpenAdapter/CreateDevice rendering stay unavailable until
embedded D3D9/runtime-owned factory exists. Root checked actual WDK layouts
and current D3D10 factory before recording next implementation dependencies.

Root live poll confirms paired37338579561 completedSUCCESS, including final
signed-runtime-abi on Windows ARM64 after product success. Source/offline/full,
guest changed fixtures and paired signed load checks all pass. Agent retains
final signed package/source/manifest/PE/catalog receipts, then documentation-only
checkpoint without redundant full build. DXVK/VKD3D unregistered,58522older
candidate baseline, installed58623/Mesa and VM configuration preserved.

Paired final CI logs prove53actual Windows catalog members/common PE signer,
52-file installer receipt and64GPU/9installer joint bundle identities. Signed
ARM64/EC/x86 GL/CL/D3D and all three DXVK/private-loader/closed-gate loads PASS.
Root inspected finalrun/source-verified receipts: parentb6bf, DXVK34ff, all
architectures unregistered and exact eight-gap set. Final138534327-byte artifact
archive download/hash verification is pending before documentation commit.

User explicitly suggests KMD builds in the target Windows VM for faster
iteration. Delegate toolchain/disk audit and exact b6bf isolated KMD compile,
with setup/compile timings and source/PE/output receipts; no driver installation
or configured VM changes. Continue already authorized DXVK milestone artifact
verification. Paired worker503 interrupted after91%download; resume original
session rather than duplicate138MBtransfer. File plan remains explicitly selected.

Downloaded paired artifact verification now COMPLETE PASS: archive SHA256
1aad40d95c607738fe1e36b64488c7b4ce50e2010e77d3fb1f103db875f8f919 matches
GitHub metadata/upload. Exact b6bf/34ff/version58522 and older Mesa/CLVK pins
verified; local64GPU/9installer hashes/PE/source/gate receipt equals uploaded
joint receipt. Windows53catalog/common signer/fullABI logs retained. Final
candidate-evidence.json and signed-package-verified.json are reviewable; cleanup
and docs-only paired checkpoint proceed. User-requested guest KMD build audit
is active separately; installed driver and system configuration preserved.

Resumed from the explicit file plan after compaction. Read workspace rules,
agent.md and selected planning skill; session catchup has no additional output.
Memory registry contains no matching project evidence. Paired documentation
checkpoint09ec84ca is complete/pushed/clean. Guest build bootstrap proceeds
with minimal official native ARM64 MSVC instead of a full VS installation;
root and reviewer avoid duplicate remote transfers. No configuration changes.

Independent final DXVK receipt reviewPASS; absolute guest clock chronology
anomaly recorded in guest README and its SHA256SUMS refreshed. Matching
desktop identities before/after remain valid; no unchanged fixture rerun.
Portable nativeARM64 compiler/linker and SDK26100.1rc/tracewpp acquired from
official packages. Reviewer extracted exact WPP/CL/RC/LINK flags from successful
paired CI so guest direct build reproduces actual KMD policy and dependencies.

Root clarified roadmap initial audit pins vs current validated34ff/b6bf,
and added absolute-clock limitation to rotation documentation. Reviewer
verified all294guest-source archive files against exactb6bfGit; source transfer
and native portable build script preparation continue.

After interruption, only root remains active. Existing transfer completed:
78,305,838 bytes in 26.655s; source/tools had not yet been extracted or built.
Resumed portable setup successfully in 51.224s with native ARM64 tool banners
and hash checks. Found arm64rt.lib in Microsoft.Windows.SDK.CPP.arm6426100.1
instead of the compiler/WDK packages; archive and library digest recorded.
Adding its isolated library path before running the exact-source build.

First guest compile reached native cl but failed VirtIO compilation: portable
include order let MSVC crtdefs.h shadow the kernel CRT definition, causing
_CRTIMP_ALT/__ALTDECL errors in WDK string.h. Preserve attempt-1 logs and
correct kernel/km-crt header precedence before retry. Driver source unchanged.

Corrected include precedence: VirtIO CL and LIB both PASS,0.576s/0.111s.
Windows PowerShell5 then rejected Measure-Object on OrderedDictionary keys;
change timing aggregation to emit numeric dictionary values first. Preserve
attempt-2 logs; this is runner bookkeeping, not a compiler/source failure.

Third attempt: VirtIO and both WPP stagesPASS; all ten KMD source files
compilePASS in2.640s. RC fails RC1109 creating quoted resource-output path
when arguments are provided via response file. Reproduce CI direct RC argument
dispatch instead; preserve attempt-3 logs. Final link remains pending.

Guest exact-source b6bf buildPASS: VirtIO CL0.562947s/LIB0.084819s,
WPP0.035853s/non-owner0.018827s, KMDCL2.568962s, RC0.053833s,
LINK1.435689s. Native ARM64 SYS/version100.6.101.58522 and PE import checks
PASS; total runner5.114122s(Stopwatch). Collecting output/log/continuity
receipts for final verification. No installation/signing/package acceptance.

Downloaded SYS/PDB/map/compiler-PDB hashes all match guest outputs. Independent
LLVM inspection confirms nativeARM64/subsystem, kernel-onlyNT/HAL imports,
CFG/NX/ASLR, version resource and stack cookie; link map confirmsGsDriverEntry.
build-verified.jsonPASS; every stageexit0/empty stderr, desktop1644/5828 and
installed58623/oem10.inf preserved. Base setup51.224s/support library0.269s.
Saved guest build guide, compact evidence and recipe snapshots. Runtime
admission/eight gaps remain unchanged; next implementation is nativeD3D9.

Final compact guest artifact checksum setPASS(86files), recipe snapshots
preserved and unused local486MBtoolchain assembly staging removed. Provisioned
guest tools/source remain reusable; local archives/manifests retained. Final
documentation-only commits preserve binary producers34ff/b6bf and source pins;
no duplicate CI/package/backend fixture run is needed for these documentation
changes. Both implementation and guest-build continuation checkpoints complete.

Recorded exact CI timing baseline; delegated official matching arm64rt.lib
package lookup after remote discovered missing link support library. Guest
compiler/linker/WPP/rc source/toolchain preparation continues with native tools.

2026-10-06 continuation: recovered existing DXVK plan explicitly and verified
clean f003ed6 checkpoint. Read workspace image/VM safety rules and D3D9
roadmap. Memory registry has no matching project history. Begin the typed
D3D9 adapter bridge; no remote state or installed driver changed.

Added typed D3D9 adapter source/header, shared raw runtime identity query,
development export and Meson target. Windows read-only health check confirms
existing DWM1644/Explorer5828. Fixture implementation and compiler verification
remain next; the new target does not yet have its test source.

Added D3D9 fixture for malformed identities/exact S_OK, unchanged canaries,
literal interfaces/opaque runtime build IDs, callback-table mutation, nested
caps/create/open, exception boundaries, callback close/reopen, sticky changed
identity/removal, concurrent close during a blocked query, and stale-token
reopening. Wired offline MSVC compile/execution, embedded Meson production
build, published fixture and native ARM64 CI execution. Compiler checks pending.

Local existing sanitizer suite PASS: identity69/runtime identity180/shader147.
Prepared standalone guest D3D9 runner and small release CRT/SDK supplement
assembler using retained verified archives. It uses /MT so no system CRT
installation is needed. Source and library transfers are isolated from the
existing KMD toolchain. Corrected a fixture mutation which had changed a valid
nonzero generation at open rather than making the reply malformed.

Guest native MSVC compiled all three D3D9 source inputs, but /W4 /WX rejected
C4201 anonymous unions in Microsoft dxgiddi/d3d10umddi headers. Isolate external
header warnings with /external:anglebrackets /external:W0; retain fatal source
warnings. First attempt receipts/logs retained before the corrected compile.

Corrected guest compiler pass compiled bridge/fixture/query warning-free; link
reported missing uuid.lib. Added official SDK ARM64 Uuid.Lib from the same
retained, hash-verified package. Preserve second attempt compiler receipts.

Third guest link found OLDNAMES.lib absent from retained portable payloads.
This fixture uses no legacy alias API, so exclude only that default library
and require all symbols to resolve. Inspected static CRT COFF directives and
added official synchronization/runtimeobject SDK imports before the next link.
Normal production CI build flags/toolchain remain unchanged.

Guest fourth attempt showed CL ignoring the linker-only default-library flag
when /link was inside the multiline compiler response file. Corrected runner
to put /link /NODEFAULTLIB:oldnames.lib on the actual command line after the
compiler response file. No driver source change; record failed attempt too.

Corrected guest build PASS42429 D3D9 adapter checks. Source compiles warning-
free and links static CRT with all symbols resolved; downloaded receipts/
binary/import logs retained. Compile18.006s/run3.503s/runner28.599s. Driver
58623/oem10.inf and DWM1644/Explorer5828 retained. Added checkpoint doc and
updated roadmap for the implemented adapter slice. Prepare exact source CI.

Committed/pushed aac5172b1c0983dbe58b4f6fe4c8a1a3cd7e7e03 with actual
multiline session trailers. Guest verification PASS all nine source hashes,
EXE hash/ARM64 PE and driver/desktop continuity. Offline37408748554 and
full37408749906 run exactaac5172; upstream automatic package/API workflows
also triggered by push. Continue required native CI to completion.

Native CI Linux contracts, identity and shader CPU jobs already PASS; Windows
architecture jobs continue. Read-only next renderer audit confirms public
D3D9 creation enumerates/uses direct ownership and InitialReset's implicit
swapchain. Private parent/offscreen initialization and common raw identity
are the next construction work. Retained exact source-aac5172.zip locally.

Offline37408748554 ALL4PASS: Linux contracts, x64/x86 execution, ARM64 compile.
Full37408749906 has identity/shader CPU PASS; three embedded builds continue.
ResetState initializes render state separately from ResetSwapChain, so a
private offscreen initialization can reuse defaults while omitting the
implicit swapchain. Do not expose swapchain-dependent COM methods in the DDI.

Retained offline x64/x86 logs confirm exactly42429 D3D9 checks each with
normal /MD toolchains. This independently validates the /MT guest fixture
result; no driver workaround was needed. Full embedded DLL/import/export and
native ARM64 CI remain pending. Release local generated library staging;
keep reusable support/source archives and all failed/success receipts.

Resumed with the explicitly selected DXVK plan; catchup adds no context and
the memory registry has no relevant history. Collected full37408749906:
all six jobs PASS on exactaac5172, including three embedded builds and native
ARM64 execution. x64/x86/ARM64 each report42429 D3D9 adapter checks. Retained
full status/logs; downloading architecture and native validation artifacts.
Next source work is the shared runtime selection and offscreen D3D9 core.

Full CI artifact verification PASS:148retained files hashed, each source pin,
native PE architecture and export/import policy verified. Actual x64/x86 and
ARM64 fixture logs each show42429D3D9 checks; six CI jobs all success.
Added a renderer-neutral raw adapter owner and common GpuBackend factory,
copied callback snapshot, embedded D3D9 static core excluding API entry source,
private parent and offscreen state/submission initialization. Development
construction/teardown helper publishes no COM renderer or typed device DDI.
Added portable ownership and actual DLL rejection fixtures; wired full builds
and ARM64 execution. Local sanitizers69/180/49/147PASS. Architecture CI next.

Adapter docs checkpointd2c2e05 and embedded sourceeffb654 pushed. Requested
full/offline CI for sourceeffb. Cleanup audit then found a constructor helper
leak on late D3D9 allocation failure; add local ownership guards before the
final architecture validation. Successful destruction remains unchanged.
Removed only the11verified generated support libraries and empty staging
directories; retained reusable archive, guest toolchain and all evidence.

CIeffb654 full37411400537 and offline37411402396 failed canonical WDK type
compilation after neutral RuntimeGpu header removed a transitive D3D10 include.
The raw query header needs d3d9types before d3dumddi. CPU ownership and shader
jobs passed. Fix that explicit dependency, retain failed diagnostics, and run
the final source including e46f10f constructor guards.

Resumed the embedded D3D9 continuation with the pinned plan and clean9b4168b.
Collected offline37412714285 ALL4PASS and full37412713033 failure: ARM64,
x64 and x86 all compiled then failed the same missing annotation singleton.
Moved its definition into d3d9_annotation.cpp, shared by public/embedded
targets. Retained full failed diagnostics. Guest output transfer completed;
receipt49PASS/0.807s compile/0.060s run awaits independent input/PE checks.
No installed driver, guest desktop or VM configuration changed.
Two combined patches failed on wrapped plan/progress anchors; neither changed
files before the corrected patch applied.

Pushed annotation link repair2a4095c with session trailers; replacement
full37415595130 runs that exact source. Identity jobPASS; architecture builds
continue. Added reusable guest and full CI verification scripts. Guest
verificationPASS: exact Git input hashes10, executable hash/nativeARM64 PE,
49checks, empty compiler/run stderr and installed58623/desktop continuity.
Drafted embedded renderer checkpoint and updated adapter/roadmap links;
final architecture/DLL/nativeARM64 evidence remains pending.

Full37415595130 identity/shader/x64/x86 jobsPASS; ARM64 build continues.
x64/x86 artifacts11390954382/11390859803 are published and downloading.
The standard API build also completed its x86 phase and is building x64,
providing regression coverage for the moved shared annotation definition.
Recorded official next-device/resource contract requirements in findings.

Full37415595130 ALL6PASS at exact2a4095c, including native ARM64 execution.
All three architecture logs confirm42429adapter/49owner/45DLL-rejection checks.
Normal public API Windows37415565405 and package37415565396 alsoPASS at2a.
Downloaded x64/x86 and native ARM64 validation outputs; ARM64 core download
still running. Retained full logs/status/artifact metadata (four artifacts).
Independent PE/import/export verification is the remaining validation step.

Independent final verificationPASS: three actual DLL PE architectures,
required development exports, absent production OpenAdapter/public D3D factory
exports and forbidden factory imports. All13nativeARM64 executed EXE hashes
match downloaded artifacts;166retained CI files hashed. Guest49check receipt
was already verified. Finalized embedded checkpoint/roadmap/planning records
and marked the bounded embedded continuation complete. Next remains actual
typed D3D9 device/resource/rendering implementation and target GPU proof.

Continued at clean c1ec9e0 with PWF_PLAN_ROOT pinned to this plan. Skill catchup
and memory quick pass add no project history. Read adapter/backend/runtime
service and existing WDK next-device findings; began typed creation/destruction
audit with no source changes yet. Independent runtime/callback and teardown
fixtures precede real target construction and resource/rendering gates.

Both Windows22 and Android8022 SSH are currently reachable. Implemented typed
D3D9 device construction/Flush/destruction, monotonic tokens and runtime-handle
reservation, pre-callback input snapshots and post-construction identity
publication. Narrowed backend header to an opaque state for isolated lifecycle
compilation. Added controlled backend fixture exercising actual RuntimeGpu
context/allocation/map/terminal unlock-deallocate-context order, input mutation,
duplicate creation, nested/concurrent Flush/destroy, stale tokens and cleanup
after failure/reset/close. Wired full/offline/native ARM64 checks. Compiler
verification is next; resource/rendering and target construction remain open.

Guest corrected fixture PASS63962 checks then lifetime-gated fixture PASS66400
checks (/W4 /WX, compile3.040s/run0.063s). Local identity69/runtime180/owner49/
shader147 sanitizer suite PASS. Added real-KMT-only target lifecycle probe;
it opens the chosen display, drives the actual typed adapter/device DDIs,
verifies original-caller callback execution and balanced context/BO/map
teardown, and presents nothing. Guest compiler preflight and exact source
CI follow. Installed Mesa is older than paired37312838033, so target setup
may require isolated process-only ICD selection, without driver installation.

Guest fourth worktree attempt compiles both lifecycle and real-KMT probe
with /W4 /WX. Final fixture66403PASS; compile2.871s/run0.062s. Its new fence
cleanup control rejects adapter escapes after CloseAdapter. Source review
complete; prepare exact-source commit, architecture CI and guest receipt rerun.
Evidence stays under the workspace artifacts/dxvk-native-d3d9-device-20261006,
outside the checkout. No installed driver or desktop configuration changed.

Committed/pushed af9ecd52ee2fac7ceacc1cdceeb2ad2a9712ffd5 with session
trailers. Full37419569213/offline37419571615 run that exact source. Guest
exact-source rerun66403PASS, compile2.906s/run0.054s; receipt/hash verification
and CI remain in progress. Added standalone verification and hardware-runner
scripts. Hardware staging uses only process-local signed Mesa loader/ICD copies
from paired37338579561; preserve their final package hashes (source-identity
lists pre-signing hashes). Source Mesa pin3e50dd4b/run37312838033 is unchanged.

Resumed af9ecd5 with this plan explicitly pinned; recovery and memory search
add no project history. Offline37419571615 is ALL4SUCCESS. Full37419569213
identity/shader jobs pass while all three embedded builds remain running.
Prior pending staging scp completed. Exact guest66403 checks/input/archive/
executable hashes are already verified; next collect final CI and run the
real-KMT lifecycle probe with the process-local pinned Mesa payload. A workflow
search using an absent zsh wildcard failed before execution; use rg on the
workflow directory instead. No source, installation or VM settings changed.

Added a device CI verifier for66403checks, the14thnativeARM64execution hash
and all three hardware-probe PE architectures. Added hardware staging with
archive/source/finalMesa hashes checked before execution. Windows Public
inventory confirms existing guest build roots; the assumed C:\dxvk-umd-20261005
path does not exist. Two guessed ARM64 script names also do not exist; actual
CI runner is scripts/test-native-arm64.ps1. Use explicit known paths next.

Full37419569213 ARM64/x86 build jobs are now SUCCESS and their artifacts
11392248793/11393295519 are downloading. x64 still runs; nativeARM64 execution
depends on the full build matrix. Offline logs independently confirm66403
device checks on both x86/x64. Added final hardware receipt/input verifier.
An absent roadmap wildcard failed before execution; rg --files identifies
docs/native-dx8-dx9-roadmap-20261005.md as the actual roadmap.

Full37419569213 is ALL6SUCCESS, including nativeARM64 execution. Hardware
staging passed archive/source/architecture and pinned finalMesa hashes.
First real-KMT probe returned d0000001 before KMT_ADAPTER/OpenAdapter output:
D3DKMTOpenAdapterFromGdiDisplayName(default DISPLAY1) failed before the UMD
ran. Retain the failed evidence and inspect active/session-visible adapter
enumeration before changing or rerunning the probe. Installed driver and
desktop continuity receipts are being collected; no configuration changed.

Independent af9ecd5 CI verification PASS: all full/offline jobs,66403device
checks per architecture,14nativeARM64execution hashes and three native probe
PE/import policies. hardware01 receipt retains58623/DWM1644/Explorer5828.
An existing adapter control reports zero outputs in the SSH session; it also
attempts ordinary system D3D11 creation (hardware fails, WARP succeeds) and
does not establish candidate rendering. Corrected the probe to require exact
LUID bytes, list/close KMT adapter handles and report open/device failures.
Offline CI now compiles the probe on all architectures. Prepared a fast guest
probe builder with six malformed CLI controls and a pinned SDK Gdi32 library.
Guessed test/compiler path searches failed; actual offline runner is
scripts/test-umd-mrt.ps1. Production UMD source remainsaf9ecd5.

Committed/pushed probe-only0b8dea5; offline37421518064 now runs exact source.
Guest native probe compilation passed/W4/WX but first link failedOLDNAMES:
CL treats /link inside this multiline response file as compiler flags. Move
linker arguments to the process command line as in the proven guest fixture
runner; retain DxvkD3D9Probe-0b8dea5-01 and use a fresh02build root. af9ecd5
public API Windows37419538950 and package37419539028 are also ALLSUCCESS.
Independent full artifact verifier hashes184retained files.

Guest probe0b8dea5 build02PASS with all six malformed CLI controls. KMT lists
d36b/686b LUIDs, both with zero presentation sources in this SSH session.
Add existing private identity decoding/matching to listing output before
choosing the exact hardware LUID; preserve original backend/callback path.

Probe bfd7e77 guest build PASS: compile/link0.6198779s, full build/CLI/list
runner1.3083707s, six malformed CLI controls. Raw identity audit shows physical
LUIDd36b private queryNTSTATUSc00000a3 (DEVICE_NOT_READY); LUID686b replies
with software-driver data. Corrected native hardware02 explicitly selectsd36b:
KMT open/device creation succeed, typed D3D9 OpenAdapter returnsd00000a3 before
renderer construction. Diagnose the existing KMD readiness refusal; do not
claim GPU construction or change admission based on the successful CI fixtures.
Android8022 remains online. Offline0b8dea5 was superseded/cancelled by the
workflow concurrency policy; final bfd7e77 run37421820113 is in progress.

Final probe offline37421820113 ALL4SUCCESS. Logs confirm actual probe compiler
preflight on x64/x86/ARM64 and unchanged66403device fixture checks on x86/x64.
Readiness registry inventory hit the display-class Properties subkey's denied
ACL; constrain enumeration to numeric driver subkeys and ignore that unrelated
protected key. The failed receipt scp had no source because inventory stopped;
retry only after a successful corrected inventory. No driver setting changed.

Continuation recovered bfd7e77 source with only planning findings/progress
modified. Pending readiness scp84417 completed. Final bfd7e77 offline CI is
ALL4SUCCESS. Refreshed plan with exact lifecycle CI and confirmed KMD refusal.
Read-only collector now resolves active PCI Enum Driver key, service and hash;
historical registry entries are retained separately. An unquoted guessed
docs/native-kmd glob failed; use rg file discovery for actual paths.

Active collector's first execution assumed System32/drivers/service.sys and
failed before evidence publication; active binary is DriverStore based.
Resolve Win32_SystemDriver.PathName instead. Found retained exact58623 signed
SYS/MAP/PDB under .artifacts/destroy-diag-58623-ci-37242831438/download.

Active readiness receipt01 collected and downloaded with exact PnP/service/
binary binding. Exact SYS hash and ARM64 BLs confirmed. Stored first-reset
and first-timeout locations differ; no timing attribution claimed. Git package
receipt pins KMDd10f9ba6. Historical CI37242831438 lookups404 on personal and
Droid-VM repos; use retained signed receipt rather than more guessed endpoints.
Guessed Android log directory absent; resolve source/actual paths next.

Recovered bfd7e77 with the DXVK plan pinned; skill catchup and memory query
found no additional context. Current source worktrees match the handoff:
DXVK has only planning edits, paired GG is clean, primary KMD has unrelated
user edits. Both remote SSH services are online. Traced exact installed
standard-resource unmap/destruction and selected diagnostic admission
provenance as the next bounded KMD slice, followed by guest compilation.
One combined file search included an absent guessed artifact directory;
use the known package/evidence paths rather than retrying that path.

Independent bfd7e77 probe verificationPASS: all51Git/local input hashes,
archive/EXE hashes, ARM64 PE/import policy and six malformed CLI controls.
Both hardware receipts now verify exact separate probe/UMD provenance,
all9input hashes, final signed Mesa payload and driver/desktop continuity.
Their outcomes remain explicit GDI-open failure and D3D9 DEVICE_NOT_READY.
Added an isolated d10f9ba6 worktree under reference/codes for diagnosis.
New queue admission/first reset/2D-destroy immutable records share the timeout
clock. Existing queue fixture1219PASS; timeout overwrite negativePASS.
Focused destroy/reset fixture and native guest compile remain in progress.

Focused production destroy/reset fixture24PASS plus four compiled semantic
negativesPASS. Queue lifecycle/admission1220PASS, first-timeout overwrite
negativePASS, full Native Context contractPASS and diff-checkPASS. Wired the
new controls to both KMD CI paths. Reused the proven guest direct compiler
recipe with separate exact-source/evidence roots and existing toolchain;
prepare the committed diagnostic source for guest ARM64 compilation next.

Readiness continuation recovered installed diagnostic source c1b9ea69 and
fixture-only followup fdfd8f99 in the isolated readiness worktree. Both are
pushed. Guest native ARM64 builds verified all118production input hashes,
output/PE/import policy and desktop continuity:58623runner5.429254s and
distinguishable58624runner5.741236s. Host fixture corrections preserve all
production inputs; vblank lifecycle plus nine semantic negativesPASS.
Initial CI37426954641 failed inherited MSVC fixture warnings; corrected
replacement37428593406 remains in progress at this recovery checkpoint.

Signed58624 preparation independently verified47retained runtime files,
50catalog members and51exact58623rollback files. Installation receipt
readiness-install-58624-20261006.json reports PnPUtil0/PASS, active device08
version100.6.101.58624/oem17.inf, no reboot and no VM configuration change.
Signed SYSd48e118a89b83df49e1da2f4b26e57b13d7f40ee6a42ad19ff6ea0328989650a;
package-resultf836257b318828f068627fd780a9af2d81cef80673c0c1f359e8e3d88766a989.
Both SSH services and ADB are online. Next cleanly stop the existing guest,
verify daemon STOPPED/no crosvm, start once with the reviewed helper and bind
fresh common-clock diagnostics to the exact loaded58624binary.

Clean guest shutdown succeeded; daemon STOPPED/PID-1/no crosvm verified.
Reviewed local/remote start helper hashes match; the sole Windows VM started
once with PID10130 and6656MiB. Replacement CI37428593406 ALL2SUCCESS at
fdfd8f99, including1220queue/24readiness/foursemantic negative controls.
Early SSH closed during boot, then returned; two collector attempts stopped
because Explorer was absent. Adjusted read-only collection to persist an
explicit desktop_processes_complete flag even when processes are missing;
desktop recovery and new readiness interpretation remain pending.
One queue/DDI search guessed the wrong subdirectory; discovered actual files
under viogpu/common and viogpu/viogpuwddm and continued there.

Continued from58624boot and hardware03 with the persistent DXVK plan pinned.
Both Windows22 and Android8022 SSH are live. The unchanged bfd7e77probe
opened KMT adapter/device and D3D9 adapter on physical6e6c, but DXVK device
construction returned8876086a while loading installed DriverStore Mesa.
Added structured process-token evidence and an owned Limited interactive
scheduled-task runner with source-script hashes, exact Explorer owner/session,
30second probe/55second task deadlines and task removal. No candidate DLL,
KMD, registered ICD or VM configuration edits are part of this probe.

Hardware04 completed under measured USER/session1Limited token; pinned local
ICD selected, same8876086aCreateDevice refusal,0.5892604s. Desktop/58624oem17
continuity passes; owned task removed. Full03/04payload downloads continue.
Source audit finds pinned Mesa3e50dd4does not contain the MWD property/device
runtime bridge. Added property-only support diagnostic and native guest
compiler recipe; the existing runtime-integration source is clean and can
be brought onto the candidate once the actual zero reply is measured.

## 2026-10-06 resumed backend bridge diagnosis

Recovered current bfd7e77 source, af9ecd5 lifecycle and fresh58624 evidence.
Live GitHub verification confirms37419569213 all6PASS and37428593406 all2PASS.
ADB5555 and both SSH services are reachable; Android lists one registered crosvm.
Hardware04 already measured USER/session1Limited, pinned local Mesa selected,
and CreateDevice8876086a with desktop/installed58624 retained. Next run the
property-only support diagnostic, then integrate the proven missing runtime
bridge into an isolated exact-source Mesa candidate and validate it.
A shell quoting error invalidated an unnecessary Windows CIM query; no mutation.

Measured and independently verified missing private runtime bridge: pinned
ICD reply all0, Limited USER/session1, property query only, desktop/58624
retained and scheduled task removed. Verified35 source/build input hashes,
ARM64 EXE and both03/04nine-input hardware payload receipts. Porting existing
bridge into isolated exact3e50dd4 candidate; preserve residency and resolve
second patch teardown fixture fields before source tests.

Ported existing runtime ownership bridge into exact3e50dd4 Mesa and committed/
pushed8443c71a5ab32b9d58b904fa51f4bf2f9089db8d on personal
work/dxvk-matched-runtime-20261006. No production source outside13explicit
paths changed. Preserved residency and protocol header matches DXVK exactly.
Allocation2795/residency72sanitizer checksPASS, three allocation and six
residency semantic negativesPASS, seven additional existing regressionsPASS,
full policy/YAML/diffPASS. Source extraction fixtures required the new runtime
header, shared helpers and a bounded external Escape peer; they compile the
actual production functions and preserve the original residency oracles.
Dispatched three-architecture OpenGL/Turnip candidate and transport CI; source
SHA/status/artifact verification and actual hardware construction pending.
Also prepared seven exact Git source inputs and reusable portable native
ARM64 transport build/controlled-dispatch fixture in existing Windows guest.

Exact Mesa8443c71 native guest production transport compiles with /W4/WX
and executes the controlled direct/shared dispatch fixturePASS. Independent
verificationPASS all7Git/archive/source hashes, ARM64 PE/hash and
desktop/installed58624continuity; compile1.0699098s/run0.8524681s.
This is controlled dispatch evidence, not actual GPU construction.
Mesa CI37453381660 source-regressionsPASS; three architecture builds active.
Transport CI37453384744 real SDK/WDK ARM64 compilation and command-stream/
runtime semantic checksPASS; full Turnip ARM64 compilation/link still active.

Full ARM64 Turnip CI37453384744 all3jobsSUCCESS, exact8443c71. Downloaded
ICD artifact11408122116 and independently verified all13payload checksums,
AA64 core DLL/dependency, actual exports/imports and embedded8443c71 source
string. Native ICD1b0cb01a requires only native z-1 plus Windows system DLLs.
First stage guard caught unused x64 CRT files inherited from the earlier
probe payload; imports prove UMD/probe/loader and new ICD are statically linked
and do not use those DLLs. Prepare fresh native-only3file stage rather than
weaken the PE rule. A prepared-but-unrun interactive task is being canceled;
no GPU execution or driver/registration/configuration change occurred.
Exact scripts from completed property05 were reconstructed and independently
hash-verified against the original task config, retained under scripts/.

### Matched runtime support07 and actual device08

- Fresh exact three-DLL native ARM64 process-local stage02 verified successfully.
  Canceled the prepared old support06 task without starting it.
- support07 under Limited USER/session1 reports3152574d/version1/size88/flags1,
  supported1. Owned task removed; no installation or registration.
- device08 uses unchanged af9ecd5 UMD/bfd7e77 probe. KMT/open/create/flush/destroy
  all S_OK; context1/1, allocations3/3, locks3/3, wrong_thread0, render0.
  The render>0 lifecycle oracle returns80004005; preserve failed evidence.
- Recovery guessed tests/umd/d3d9_device_kmt_probe.cpp and nested artifacts;
  rg discovered tests/umd-d3d9-device-probe.cpp and workspace-root artifacts.
- First combined plan patch referenced an absent findings heading; patch rejected
  and replaced by an exact task-plan patch and append-only findings/progress.

- Empty submission audit: InitializeNativeOffscreen only resets/binds state;
  Turnip tu_wddm_queue_submit_locked suppresses entry_count0 render callbacks.
  Probe271a83c removes only that invalid render>0 lifecycle expectation and is
  pushed; offline CI37455724595 started. Existing lifetime checks retained.
- Independent property07 verifier passed8input hashes,3wrapper hashes, exact
  oldUMD/loader and newICD/zlib hashes, AA64, Limited session1 and58624oem17
  continuity. Probe271a83c build verifier passed51sources/exe/archive/CLI.
-09 is a new failure after successful creation: Flush88760868 and DXVK command
  submission device loss. Task removed; captured verified compressed evidence.
- One extra poll of completed SCP49312 returned Unknown process id; no transfer
  retry performed. Full downloaded property07 payload independently verified.

### D3D9 startup callback repair f648a1a

- Closed startup deferral window in RuntimeService constructor and typed D3D9
  ownership. Added before-first-pump/between-pumps/caller-affinity/closure and
  synchronous rejection checks to the actual typed lifecycle fixture.
- Guest ARM64 production fixture compile3.3824182s/run0.0547275s/66410PASS.
  Compiled negative-deferred-disabled control exits1 at result==S_OK.
- Source f648a1a pushed. Offline37456996101/full37457000846 started; monitor
  both and validate exact rebuilt hardware candidate before acceptance.
- Diagnostic device10 af9+Mesa8443passes in1.8902364s;08and09failed receipts
  remain immutable. Saved wrappers and eight exact linked payload hashes.
- Readiness snapshot initially assumed newer active_device schema; guest
  script was older registry-only schema. Narrowed extraction to actual keys;
  do not claim it independently resolves active binding or09cause.

- Startup offline37456996101 all4SUCCESS. Full37457000846 identity/shader
  passed; all three backend builds still active. Watched with30s intervals.
- Independent positive/negative guest receipt verification:52source files each
  match f648a1a, with the declared deferral mutation only in the negative.
  Failed build script omits source hashes on failure; captured a separate
  after-run source receipt instead of changing its original failed receipt.
  Negative verifier initially expected the omitted sources key; corrected to
  use the separate measured receipt.

### Startup candidate CI and target completion

- f648a1a full37457000846 all6SUCCESS, offline37456996101 all4SUCCESS;
  normal API37456982139 and package37456982302 also SUCCESS. All three
  architecture downloads verified for source, native PE/import/export policy,
  66410device checks and14native ARM64 execution-to-download hashes.
- ARM64 UMD/probe staged from exact CI artifact11410126793. Guest archive and
  three input hashes/AA64 checks PASS; stage is process-local only.
- device-f648a1a-58624-11/12/13 all PASS; diagnostics1/0/0; measured durations
  0.9432165/5.2373924/2.3940590s. Every stage S_OK and each lifetime balances
  context1/1, allocations3/3, locks3/3, with wrong-thread0/render0.
- Collected compressed evidence serially, verified each archive hash, all eight
  payload hashes and three wrapper hashes per run, limited USER/session1 token,
  actual local Adreno830 ICD, and driver/desktop continuity. Saved exact wrappers;
  all owned tasks removed. Binary links point to retained CI and Mesa payloads.
- New active-binding snapshot14 verifies0002/statusOK/PnP0/running service/
  58624oem17/exact SYSd48e118a; DWM1552/Explorer4184 unchanged. ADB5555 and
  both SSH services respond; root ADB confirms only existing crosvm PID10130.
- A status redirection and helper compile initially used nested-project relative
  paths for workspace artifacts/scripts. Corrected to workspace root and both
  checks passed. Termux pgrep/pidof exit1 comes from process visibility; retained
  successful id output and use root ADB for the sole VM process, no VM action.
- Updated current plan, device evidence and roadmap. Exact binary source stays
  f648a1a/Mesa8443 after documentation-only checkpoint. Typed resources, actual
  GPU pixels/nonempty submission, Present/reset and ordinary DX8-DX11 admission
  remain open; do not infer prior09loss cause or long-term stability.

### Native resource fixture verification, latest worktree03

- Verified all52 source input hashes against the current source files, ARM64
  EXE e44e2380, both archive hashes and original native receipt:77449checksPASS,
  probe/compiler exits0, compile3.3734562s/run0.0504742s.
- Three separately compiled semantic controls exit1 at their intended behavior:
  post-create identity refusal(check10654), NotifyOnly tag validation(check4648),
  and descriptor snapshot against callback mutation(check2331). Each verified
  all52 after-run inputs and exactly the declared source mutation.
- Preserved worktree02 compiler failure caused by Windows RPC small macro.
  Corrected probe identifier in03; no compiler failure counted as semantic proof.
- Driver58624/oem17 and DWM1552/Explorer4184 retained. These CPU/backend fixtures
  do not prove actual GPU pixels or ordinary D3D runtime admission. Full native
  core build and target typed clear/readback remain next gates.

- Reviewed eight source paths committed/pushed as dde00ed3d255d24073d111c90dc91e69247bba93.
  Full37464752821 and offline37464754567 dispatched; ordinary API/package
  runs37464732394/37464732439 also active. Resources/rendering remain in progress.
- Updated owned interactive wrapper with explicit render mode and independently
  computed expected192pixel checksumffefa655. New render verifier retains
  byte-linked payload, native PE, limited token, driver and desktop evidence.
- Recovery guessed obsolete documentation filenames and nonexistent Mesa glob.
  Both read-only searches failed; rg--files located native-dx8-dx9-roadmap and
  src/freedreno/vulkan/tu_knl_wddm.cc before further work.

### Full resource CI and target failure16

- dde00ed full37464752821 all6SUCCESS; offline37464754567 all4SUCCESS.
  All3downloaded candidates independently verify source, PE/import/export,
  77449device checks per architecture and14native ARM64 execution hashes.
- Exact ARM64 artifact11415095322 UMD6f81586e/probe1b3d76dd staged with
  archive0da2105a and all3inputSHA/AA64 checks.
- Target render-dde00ed-58624-16 Limited USER/session1 reaches successful
  adapter/device/resource-group construction and RT0 binding. First nonempty
  Mesa submission loses device(entries3/references6/fence1) and readback
  does not return before the30sprocess deadline. No pixel acceptance.
- Owned task removed. Original failed compressed evidence archive45a34009
  downloaded/verified. Driver/desktop retained; fresh active binding0002,
  PnP0/running58624oem17/exactSYSd48e118a. Readiness/epoch/timeout/reset
  registry values match snapshot14; do not infer exact refusal cause yet.
- Probe84eb920 adds per-clear/readback and raw KMT render diagnostics, and
  preserves seeded replacement buffers on failed KMT thunks. Production UMD
  remains dde00ed. Exact51Git inputs compiled natively; collect/verify its
  receipt before the diagnostic target run. Offline follow-up dispatched.
- Guessed umd_log.h was absent; no logging source was changed by that read.

### Diagnostic target17 and residency audit

- Probe84eb920 built natively in0.658s(overall1.918s),51Git/source/archive
  inputs and AA64 EXE1eb27975 independently verified. Offline37468023505 all4PASS.
- Target17 first preclipped/empty-noop clears S_OK; first readback calls KMT
  with484bytes/6allocations/6patches. Raw statusc0000001/HRESULTd0000001,
  replacement capacities65536/1024/1024. Deadline30.093s, no pixels accepted.
- Both16/17 failed evidence verifiers pass8input hashes/3wrapper hashes,
  exact Limited token/source pins and driver/desktop continuity.17archive4d0bf275.
- Post17 readiness retains native render failure stage0 and unchanged reset,
  epoch and timeout records; fence trace/guest allocation counts advanced.
  Read-only DxgKrnl/System event scan yields no events for this period.
- Source audit finds the target harness omits WDDM2paging queue and residency
  ownership that the direct Mesa KMT path implements. Next probe isolates this
  gap with unchanged production UMD/KMD. Causation remains unproven.
- Recovery guessed absent nested AGENTS, viogpuwddm/viogpudo paths, workflow
  filename and helper-directory source receipt. Correct locations discovered
  with rg; read failures changed no source or evidence.

### Corrected raw-KMT residency and verified hardware clear/readback18/19

- Probe-only aaa70c9 adds WDDM2 paging queue, unique allocation residency,
  pending paging waits and matching evictions. Native ARM64 compilation and
  enumeration verify51 declared/measured/Git source files, archive and EXE.
  Offline37472553344 is all4SUCCESS; production dde00ed is unchanged.
- Targets18/19 independently PASS192 actual pixels/checksumffefa655, intact
  padding, two RT subresources and3 successful nonempty KMT submissions,
  diagnostics1/0. Context1/1, allocation7/7, lock6/6, residency6/6, wrong0.
  Elapsed1.2268632/1.166842s; Limited USER/session1 token verified.
- Eight exact payloads/native ARM64 and three wrapper hashes verify per run.
  Archives SHA683421a34386bc0678cc0d90e1d4a0e64048006e3736618d44ea78922aedb466
  andf2c9d3a77c4a8b2eb6fd95de7c207534b6048be7c6b12353b1ea4dd1b95f6826.
  Fresh SYSsha/PnP0/binding0002/signed58624/oem17 and DWM1552/Explorer4184
  retained; failure/reset/epoch/timeout/admission deltas empty.
- Both owned Limited tasks removed; owned32MB bounded DxgKrnl trace18
  stopped and ETL/archive/script hashes retained. Failed16/17 not overwritten.
- Full goal remains active. Next actual typed declarations/state/UMstream/
  nonindexed draw and pixel proof; no caps, production export or paired pin
  widening. Presentation/reset and ordinary DX8-DX11 still required.

### Typed D3D9 draw source and native fixture

- Owned bounded declarations, explicit public state mapping plus scene capture,
  viewport/zrange/scissor and UMstream0/nonindexed draw implemented. Snapshot
  selected vertices on DDI caller before first callback; native binding survives
  public UP stream reset. Checked primitive/upload/pointer ranges and typed
  handle/lifetime ownership; worker flush/release before retirement.
- Native MSVC ARM64 controlled fixture02 PASS89329, compile4.3458338s,
  run0.07881s. Probe source also compiles /W4/WX. First01 compile failed wrong
  guessed DDI slot; actual SDK DeleteVertexShaderDecl fixed and failure retained.
- Three semantic controls compile and correctly reject: vertex reread16172/768,
  missing declaration identity22001/840, missing flush19651/822. All52 declared/
  measured/source-archive hashes, evidence archives, native PE and installed
  driver/desktop continuity independently verified. CPU fixture does not render.
- --draw adds192 quad/scissor/partial-color-write pixels plus original192 clear
  pixels; independently computed draw checksum53a03d45. Full production CI and
  hardware draw proof next; ordinary admission/caps/paired pins unchanged.

### Committed draw source and architecture CI

Sourcec874d55af7928c1ebcce3d0a4ec6c6311580f3a5 pushed. All52 native fixture
inputs match committed Git bytes. Full renderer37477931517 and offline
37477937872 dispatched on this exact source; offline all4SUCCESS, full identity
and shader jobs SUCCESS, renderer architectures still running. API37477846431
and package37477846467 also linked. Source/CI JSON retained under draw artifacts.
Draw harness mode supports the strict384-pixel oracle and retains independent
Limited task/scripts/source/payload/teardown verification. Android SSH8022 and
ADB5555 healthy; root observes same VM10130/6656MiB, no VM/config/image change.

### Exact draw production gates and candidate staging

Full37477931517 all6SUCCESS and offline37477937872 all4SUCCESS; API
37477846431/package37477846467 SUCCESS. Downloaded all four full artifacts;
independent verifier passes three PE architectures, imports/exports closed,
42429 adapter/49 backend/45 reject/89329 device checks and14 actual ARM64
execution hashes. Exact ARM64artifact11421036350 is sourcec874d55; candidate
UMDsha f4061b32d91fc136ce975ed9af8a706cd6d3f3037482a86b9483010c98ce80b8,
probesha31767d9126c95c4f7f599f85d3f69963128412ada40dbd47eda429ec6e58741a.
Archiveb4d31a30754e6f16fec89c7b49a4f3faea7467d1590773d4eb2662a71150b038
and all three staged payloads independently verify in the guest. No registry
or installation. Fresh Limited USER/session1 draw20 prepared with identical
UMD/probe source, matchedMesa8443 and three verified script hashes. Actual
quad/scissor/colour-mask run next; no target draw acceptance yet.

### Verified hardware D3D9 draw20/21 and next shader work

- Sourcec874d55 full37477931517 all6SUCCESS/offline37477937872 all4SUCCESS;
  API37477846431/package37477846467 SUCCESS. Downloaded3architectures and
  14 actual native ARM64 execution hashes independently verified.
- Exact c874 UMD/probe with matchedMesa8443 and installed58624 pass target20/21,
  diagnostics1/0. Both independently verify384pixels (192clearffefa655,
  192draw53a03d45), intact padded readback, nonzero vertex-start and three
  quad/scissor/colour-mask stages. Six nonempty KMT submits succeed; context1/1,
  allocation10/10, lock9/9, residency10/10 and wrong-thread0. Elapsed3.7381376s
  and2.9324823s; eight payloads/three scripts/nativeARM64/Limited USER session1.
- Evidence archives22b0e6fcb1a61c60e8c294bf4a1364b1288ae6201f498faf5c64402f276e375f
  and92c86b386e833d37c120234037b9a875ed0559a7accbb48b749457d9832a913e
  independently verified. Both owned tasks removed. Fresh exact signed58624/
  oem17/SYSsha/PnP0/binding0002 and DWM1552/Explorer4184 retained;54 native
  render-failure/reset/epoch/timeout/admission values unchanged after both runs.
- Record offscreen draw acceptance only. Next typed programmable shader and
  constant ownership; textures/buffers, presentation/reset and ordinary DX8-DX11
  system-runtime acceptance remain open. Full goal active, no caps/pins widened.

### Typed D3D9 programmable shader worktree

- Added owned VS/PS create/bind/delete and six float/int/bool constant DDIs.
  Caller snapshots precede first callbacks. Bounded shader entry keeps byte
  count through the existing SM1-3 translator. Stage/device/type-safe handles,
  postcreate identity, retryable deletion/flush and worker release enforced.
- Native ARM64 MSVC fixture02 PASS107385, compile6.5373586s/run0.0744307s.
  Four compiled mutations caught intended contracts: shader reread2440/1044,
  constants reread3603/940, missing identity9303/1084, missing flush7451/1069.
  Independent56source/archive/after-run/nativePE/driver/desktop verification
  passed. Earlier01 PASS107297 retained; it lacks the optional build-base pin,
  but all56 byte hashes independently link.02 explicitly pins ba2d147 base.
- --shader target oracle includes previous384pixels plus192 actual SM1/2/3
  pixels. SM3 uses float/int/bool in both stages; independent checksum1384c5a5.
  Commit exact source, complete architecture gates and run in Limited USER.
  Ordinary admission, caps and paired pins remain closed; full goal active.

### Exact shader source, CI and target harness preparation

- Committed/pushed8c4e097feb6e29dfaaff5b3c5e6041dac770f295. All56 positive02
  fixture inputs independently match Git bytes. Offline37486999026 all4PASS;
  API37486881619PASS. Full37486995183 identity/shader/x64/x86PASS, ARM64
  renderer/runtime still pending; package37486881554 linked separately.
- Added strict shader mode to the Limited USER harness and independent pixel
  verifier:576pixels, nine readbacks, six SM1-3 creates, six F/I/B uploads,
  three independent checksums, balanced callbacks/residency and exact payloads.
  All3 transferred script hashes/native Windows syntax verify. Inline parser
  check failed Windows command-shell quoting; transferred script fixes it.
- Fresh before22 signed58624/oem17/binding0002/PnP0/SYSsha still verifies.
  All58 selected failure/reset/epoch/timeout/admission values match old21.
  Explorer2488 replaced4184 before any new GPU test; DWM1552 remains. Retain
  this explicit fresh baseline and check continuity within upcoming runs.
  Guest absolute clock differs from host UTC; use within-guest lifetime and
  monotonic run durations. No clock/VM/registry/package changes performed.
- Two guessed helper/roadmap paths and one workspace-relative SDK read were
  wrong; discovered the existing files and corrected their workspace roots.

### Shader production gates and failed target22

- Full37486995183 all6SUCCESS, offline37486999026 all4SUCCESS, API37486881619
  and package37486881554SUCCESS. All4 downloaded full artifacts independently
  verify3architecture PE/import/export contracts,107385device checks perarch,
  42429adapter/49backend/45reject checks and14actual nativeARM64 execution
  hashes. Source8c4e097 ARMartifact11424158285 retained. Exact minimal candidate
  archiveade4a8520cfcabced6e43a3eccf5c15aee99bc807af589db7a803eb41fbf908b
  and3staged payload hashes/nativePE/source receipt verify.
- Target22 Limited USER/session1/source8c4e097/Mesa8443/58624 fails SM1stage7
  firstpixel:actualff17293b clear vs expectedff204060. Previous384clear/draw
  pixels PASS; both24byteVS/20bytePS createS_OK and7nonempty KMT submits succeed.
  Residency10/10/remaining0 recorded; no balanced-context/total-shader-pixel
  acceptance claimed. Owned task removedexit1. Archive24ec8d89ae202daf8405b14c203bcad6c00422d6882e8bb4b9d03f4228dcb6d0
  immutable; fresh driver/desktop/58sensitive values unchanged through22.
- Microsoft vs_1_1 input contract requires DCL, and actual DXVK BindInputLayout
  uses that semantic signature. Original probe forgotdcl_positionv0; input
  cannot bind. Add3token declaration toprobeonly (VS36bytes), preserve shader
  pixel/checksum/geometry oracle and production8c4e097 DLL. Compile/verify new
  pinned probe before fresh run; actual programmable acceptance still pending.
- Explorer old4184 ApplicationError1000 before22 faults installedviogpud3d.dll
  c0000005/RVA10db80/module timestamp6ac2defb. CI-fdfd legacyDLL timestamp
  6ac4a23c/image21000 does not match; no source-cause inference. Retain event,
  inspect actual installed binary for separate stability diagnosis.

### Verified SM1-3 target checkpoint and texture continuation

- Probe-only b3e83c8 corrects the required VS1 input DCL. Offline37491825113
  all4/API37491604110/package37491604172SUCCESS. Native ARM64 MSVC compile
  0.7457505s, all51 source/archive/after-run hashes, native PE/imports, six CLI
  negatives and exact KMT identity verify. Production DLL remains8c4e097.
- Target23/24 independently PASS576pixels diagnostics1/0:192clearffefa655,
  192draw53a03d45,192shader1384c5a5; nine successful nonempty KMT submissions,
  ctx1/1alloc12/12lock11/11residency12/12remaining0wrong0. Elapsed4.0513128s /
  4.3172908s. All8payloads3scripts/nativeARM64/LimitedUSERsession1 verify.
  Owned tasks removedexit0. Evidence archives28c9f39bd0820b1903fb1dbe2a695a06f001dd4455a37fadcb45f7bae5a9a547
  anddbe2cc26de78c10f2f7d4ead6f1e0730d21ea308ed559ca2b6c72691700ce334.
  Fresh exact signed58624/oem17/binding0002/PnP0/SYSsha/DWM1552/Explorer2488
  and all58 sensitive failure/reset/epoch/timeout/admission fields unchanged.
- Installed Explorer fault binary9d72bfee matches signed58624 Mesa D3D10 Zink
  sourceeaad15fa/image11833344/timestamp6ac2defb/viogpud3d_zink.pdb identity.
  Keep separate from legacyshim and uninstalledDXVKcandidate; matchingPDB/dump
  needed for exact cause. Guest/host absolute clocks differ; use guest-relative
  ordering and Stopwatch timings. No VM/image/package/clock changes.
- Continue coherent owned2D mip chains, explicit native sampler/state mapping,
  all-common-level TexBlt and worker teardown with caller metadata snapshots.
  Then GPU texture pixel oracle. Full DX8-DX11 goal active, no Finish/caps/pins.
- Resume read used a nonexistent src/dxvk_native_runtime path and briefly read
  tests from workspace instead of pinned repo. Discover src/umd and use exact
  project cwd; neither error affected edits, binaries or acceptance evidence.

### Texture source CI compile repair

- Source4beab5c is pushed. Offline37497502780 compiles the production device
  and controlled fixture on all three architectures, then rejects the new
  hardware probe because Windows defines `small` as `char`. Rename only that
  local to `smallDestination`, retaining all geometry/pixel expectations.
- Full37497509867 identity/shaderCPU pass; backend/native execution pending.
  No texture fixture or GPU acceptance is claimed. The existing Windows
  desktop continues its user-owned ISO transfer; diagnose SSH separately.

### Verified texture checkpoint and target EWDK preparation

- Source7874196 full37498655517 all6/offline37498596397 all4/API37498554766/
  package37498554868SUCCESS. All retained3architecture artifacts and14actual
  ARM64 execution hashes independently verify134906device checks perarch,
  42429adapter/49backend/45reject checks and closed production admission.
- Native ARM64 positive worktree02PASS134906, compile5.8650081s/run0.0783895s;
  four compiled controls reject upload-reread6909/1303, first-mip-only6907/1302,
  no-identity21136/1376 and no-unbind4216/209. All56 source/archive/after-run/
  mutation/Git/nativePE/driver/desktop links independently verify.
- Target texture25/26 independently PASS1088pixels diagnostics1/0:clear192
  ffefa655/draw19253a03d45/shader1921384c5a5/texture512d3afb9c5;17nonempty
  KMTsubmits,ctx1/1alloc13/13lock12/12residency13/13remaining0wrong0. Durations
  4.2857411s/6.5034715s. All8payloads3scripts/nativeARM64/LimitedUSERsession1
  verify; owned tasks removedexit0. Archives0d6c57ad8bc2cb2f3adae0f3a9e883da5bc8b57bdda5c6d9b27f792e36f7db69
  and0074886a0025fd89434b5668f0e14e547bc02c215b45f49418680ed3b9e9b3e3.
  Fresh exactsigned58624/oem17/binding0002/PnP0/SYSsha/DWM1552/Explorer6464
  and58selected failure/reset/epoch/timeout/admission fields unchanged.
- Explorer6464 predates these runs; prior2488 also faulted in installedMesa
  D3D10UMD c0000005/RVA10db80/timestamp6ac2defb. Preserve separate stability
  diagnosis; do not claim historical desktop continuity or artifact-free UI.
- WindowsSSH22 recovers with curve25519-sha256/publickey; existing root socat
  forwards to guestLLA22. Packet capture/sshd accepted-key records confirmpath.
  No service restart/VM/image/memory/clock/install/registry changes performed.
  AndroidSSH8022 andADB5555 remainreachable. Initial Android file discovery
  needed rootADB because the Termux app UID lacks storage access; root search
  finds no full EWDK in scopedDownload/1DMP/data-local-tmp locations.
- EWDK guestISO17912889344bytes stable, descriptor19785189376bytes; transfer
  stopped and no scp/sftp/clientssh processes. Complete-source path requested
  asynchronously. Read-only completed-image mount helper and automatically
  inventoried EWDK production KMD runner are prepared, not yet executed.
  Native controlled checks use the separate portableMSVC14.44/26100toolchain.
- Continue typedvertex/index buffers and indexed/resource-stream drawing;
  ordinary DX8-DX11 admission/presentation/reset remainopen. Fullgoalactive.

### Native buffer development and actual EWDK compiler recovery

- User confirms EWDK mounts; verify actual required reads/build, not descriptor
  size alone. D: read-only/MSVC14.50.35717/WDK28000 are present. Actual native
  and x64 compiler builds failC1510 with unreadable clui.dll. Build subagent
  traces UDF extents18283870208/18287994880 beyond physical17912889344bytes,
  matches official ISO raw ranges, recovers19missing files beside67readable
  originals in an owned compiler copy. Fresh C1b9 KMD build is running; original
  ISO/mount/VM/installed driver are preserved. Independent verification pending.
- Root implements typed worker-owned vertex/index buffers, dynamic usage,
  byte-range locks, multiple streams and indexed drawing in053e39f. Linear
  reserved metadata and index FVF are ignored; SYSTEMMEM buffers stay outside
  this slice. Resource creation snapshots metadata before callback and rechecks
  identity before publication; retryable failure keeps output/ownership intact.
- Controlled preflight01 has164072PASS on native ARM64, separate uncommitted
  four-file provenance. Committed worktree01 probe preflight catches /W4 /WX
  local shadow warnings; frozen failed receipt retained. 6ca8770 renames only
  probe locals. Final56Git inputs and four one-contract mutations are frozen
  asworktree02; native execution and full/offline CI are running.
- GPU oracle adds6cases stages18-23: separate streams3/7, byte binding and
  declaration offsets, missing final stride padding, both index formats,
  negative/positive BaseVertexIndex, nonzero StartIndex and dynamic
  NoOverwrite-range/whole-Discard updates. Each draw follows a fresh clear
  so a no-op cannot inherit the preceding result. Expected384buffer pixels
  checksum621cd685, total1472including existing texture/shader/draw/clear.
  Actual GPU execution remains pending; ordinary DX8-DX11 admission staysopen.

### Verified EWDK build, buffer controls and consolidated CI

- Requested build subagent independently verifies EWDK C1b9 production KMD:
  all9steps9.1133401s,118Git/tree/archive/after-build sources,86compiler files
  (19exact recovered/67mounted originals),55matching official metadata ranges.
  ARM64 Native410624byteSYSsha b773c84e8df806293a87a6e27c019c4e0995f51cbc660c871b826ce60947ae9e;
  onlyntoskrnl/HAL imports. Root independently rechecks local SYSsha/PE/CFG/NX/
  ASLR. Signed58624/oem17/DWM/Explorer retained; unsigned and uninstalled.
  Reusableownedcompiler C:\Users\Public\EwdkNativeCompiler-01\bin; EwdkRootD:,
  CompilerHostarm64, originalkit28000headers/libs. Exact handoff/evidence under
  artifacts/dxvk-ewdk-target-build-20261007. No full-ISO hash claim.
- Exact6ca8770 native positive164072PASS compile6.8425305s/run0.1118603s;
  executable622fab39331e6ca31ecab921ad68fcffa48a15ea0aab3edc6faefe85dbe66ce2.
  Four compiled one-contract controls independently caught at expectedchecks
  16153/20142/21059/16978 forrange/unbind/identity/index-reread. All56Git/archive/
  after-run inputs, nativePE and installed-driver/desktop links independently
  verify. Offline37508904695 all4SUCCESS before consolidation.
- User requests disabling redundant CI. cd4f5e2 changes only6workflow files:
  one automatic nativeUMD workflow includes unique Linux encoder sanitizer/
  negative controls inidentity. LegacyAPI/package/WDDM/source-transfer workflows
  disabled inGitHub; targeted offline/runtime controlsmanual. Redundant active
  runs canceled; latestpush creates exactlyonerun37509470022. Root verifies
  YAML/triggers/threearchitectures/nativeARM64 job plus56source matches6ca.
  Subagent continues independent consolidated-build verification; root owns
  actualGPUbuffer execution. Production source/caps/exports remain unchanged.
- GPU runners3local/nativeparse/hash links verify; actualGPU buffer execution
  remains pending compiledARM64candidate. Fullgoal staysactive.

### Verified buffer target acceptance and next depth/stencil slice

- Independent build verifier passes consolidatedcd4f5e2 run37509470022 all6jobs,
  all5GitHub artifactZIP digests, three native PE architectures,164072device
  checks perarchitecture and14actual native ARM64 execution hashes.
- Actual buffer27/28 diagnostics1/0 each independently verify1472pixels:
  clear192ffefa655,draw19253a03d45,shader1921384c5a5,texture512d3afb9c5,
  buffer384621cd685. Durations8.316949s/10.8154619s;24nonempty submits,
  ctx1/1alloc15/15lock14/14residency15/15remaining0wrong-thread0.
  Eight payload hashes/nativeARM64 and three frozen scripts match; Limited
  USER/session1 and tasks removedexit0. Fresh before/after readiness verifies
  signed58624/oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464 and58selected
  fields unchanged through both bounded runs. Longer-term Explorer stability
  and ordinary runtime activation remain open. Evidence underbuffer27/28;
  archiveSHA41d0e786d1e83bd89ce59cc58e19b4d0c26c5ed3c192acec962473893d37d0ad /
  d7fa6e392568707c1bc59f95af36dbbee0a22229799bb837568381e4b30bd0ce.
- Next implement D16/D24S8 native resource ownership, SetDepthStencil, independent
  TARGET/ZBUFFER/STENCIL clearing and actual depth/stencil pixel acceptance.
  Preclipped clear must avoid a second viewport/scissor clip; computed clear
  must use the actual intersection, including empty/outside scissor rectangles.
  No production caps/exports or paired pins are widened at this checkpoint.

### Native depth/stencil implementation and preflight

- Add worker-owned D16/D24S8 default-pool depth surfaces and native
  SetDepthStencil binding. Reject color/texture/buffer aliases, unsupported
  pool/multisample/lock/copy contracts and undersized depth attachments at draw.
  Destroy/close unbind before retirement; retryable failure retains ownership.
- Native Clear snapshots caller rectangles after device serialization and
  independently dispatches TARGET/ZBUFFER/STENCIL. Validate requested aspects,
  finite depth0..1 and stencil0..255; ignored fields stay ignored. Private
  ClearNative preserves preclipped rectangles and zero-count no-op, while
  computed clears clip against actual viewport/scissor intersections. Fix
  empty/disjoint clipping and image-extent bounds in shared renderer logic.
- Frozen uncommitted preflight03 compiles native ARM64 probe and actual DDI
  fixture under/W4/WX,182260checksPASS,compile6.903271s,run0.1019123s.
  All56current source hashes match frozen inputs before commit. Preflight02
  had178063checks before adding independent depth-size draw coverage. Earlier01
  compiled178063checks but used an incorrectSourceCommit field; preserve it as
  preflight only and exclude it from committed-source acceptance. Actual full
  renderer CI, committed fixture/semantic controls and GPU depth remain pending.
- Fourteen new GPU stages24..37 are implemented:896depth pixels, independently
  calculated checksumeac96ea5,total2368including prior1472. CoverD16less/gequal/
  ZWRITE, D24S8 independent aspects/write masks, preclipped and computed clear,
  empty rectangles, disjoint scissor and combined color/depth/stencil. Every
  case initializes distinguishable color/depth/stencil before rendering; no
  production caps/exports/registration or package pins change.

### Independently verified committed depth contracts and runner preparation

- Exact source2d28554271e2326c0baa7bfc578adb04d9e98e2c native ARM64 positive
 182260checks compiles6.8519516s/runs0.1222363s. EXEsha
 b63169c0c10cf00daa88a86e977e957e7e0462c4c5d2c5c4bb941c63ab702cb7.
 All56Git/source-archive/after-run inputs, nativeAA64/static runtime, signed
 58624/oem17 and desktop metadata independently verify asworktree01.
- Four single-mutation controls compile and fail their intended assertions:
 no-unbind5329line326; no-type-check4159line515; wrong-clear-flags2823line524;
 clear-reread3167line859. Each fixture exit1 and probe/compiler exit0 verifies.
 Evidenceartifacts/dxvk-native-d3d9-depth-20261007/native-fixtures-verification-01.txt.
- Native PowerShell parser reports zero errors for three frozen depth GPU
 runners; guest SHA256 hashes match depth-runner-scripts-01. GPU execution
 remains pending the actual ARM64 renderer/backend artifact. Single automatic
 consolidatedCI37515057679 runs; Linux/CPU checks pass, backend builds pending.

### Next mandatory D3D9 gaps audited from local Microsoft docs

- SetTransform/MultiplyTransform contain inline TransformType/Matrix, while
 SetMaterial contains inline Diffuse/Ambient/Specular/Emissive/Power. Native
 SetLight uses DATA/ENABLE/DISABLE bit flags and a separate optional pointed
 D3DDDI_LIGHT payload, which must be snapshotted after device serialization.
 These remain absent from the typed table; general DX8/DX9 fixed-function
 acceptance cannot be inferred from transformed diffuse quad rendering.
- Native BufferBlt specifies destination byteOffset and source byteRange;
 SYSTEMMEM/managed buffer creation and notify/upload ownership remain absent.
 Audit this next resource gate before advertising buffer/copy capabilities.
- Production source remains pinned2d285542 during CI; no later source slice,
 ordinary runtime activation, registration or package pin update is claimed.


### Accepted depth/stencil checkpoint and next fixed-function work

- Exact2d285542 consolidatedCI37515057679 all6SUCCESS independently verifies
  all5rawZIPdigests, three PE architectures/182260device checks and14native
  ARM64execution hashes. Root refreshes live CI after the GPU acceptance.
- Exact targetdepth29/30 diagnostics1/0 independently pass2368pixels each,
  including896depth/stencil pixels eac96ea5. Durations14.5854506s/14.7961268s;
 38nonempty rendercallbacks,ctx1/1alloc14/14lock13/13residency14/14remaining0
 wrong-thread0. All8payloads3frozen scripts/nativeARM64/LimitedUSERsession1
 verify; both ownedtasks removedexit0. Fresh readiness retains signed58624/
 oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464/starttimes and58selected
 fields with zero delta. GPU archiveSHA c2e1f6f00d07d28909885a57cb27c28cd680b423306153d69c9184b9a01d3177 /
 ef88465062eaa84cd63502641880b4b4c17a16076e1210f0acb39c6283058220.
 Evidence artifacts/dxvk-native-d3d9-depth-20261007/depth-29 anddepth-30.
- Preparation initially caught a copied expected-hash mismatch before starting
 depth29; compare against the actual retained receipt and frozen hashes, then
 start only after every preparation check passes. No failed GPU run occurred.
- Correct the earlier SetLight documentation audit: local docs call DataType
 bitwise flags, but actual target26100d3dumddi.h defines ENABLE=0,DISABLE=1,
 DATA=2. Follow the exact typed enum and validate unsupported values; audit
 CreateLight/DestroyLight lifetime along with separate optional light snapshots.
- Next missing typed callbacks: SetTransform/MultiplyTransform, SetMaterial,
 CreateLight/SetLight/DestroyLight. Add untransformed geometry and actual GPU
 lighting acceptance. SYSTEMMEM/managedBufferBlt, other resources, queries,
 presentation/reset and ordinary system-runtime DX8-DX11 remain mandatory.
 No production caps/exports/registration/installation or paired pins change.


### Typed fixed-function transform/material/light implementation

- Add typed SetTransform/MultiplyTransform for VIEW/PROJECTION, texture0..7
 and world0..255, plus copied SetMaterial. The renderer reuses DXVK's existing
 fixed-function matrices and lighting instead of translating them a second time.
- CreateLight maps arbitrary sparse runtime indices to reusable compact private
 slots. Native ENABLE=0/DISABLE=1/DATA=2 follows the actual target SDK enum.
 Point/spot/directional DATA snapshots copy after device serialization and
 before callback pumping; DATA preserves enabled state. Unsupported enums,
 unowned/stale indices and more than8active lights reject. Exact-success-only
 enable/retire state, retry/exception creation rollback, recreation and enabled
 close cleanup are covered by native controlled fixtures.
- User-requested build verifier independently passes frozen preflight02:
 native ARM64 /W4/WX probe and202248device checks, compile7.1772515s/run0.1041418s.
 All56source/archive/after-run hashes match the exact5worktree inputs; KERNEL32
 only/static runtime, ARM64 PE/COFF, signed58624/oem17/SYSsha and desktop retain.
 This is uncommitted-source preflight; full renderer CI and committed controls
 remain pending. Earlierpreflight01 retained separately without final GPU probe.
- Implement21actual GPU stages38..58:1344new fixed-function pixels checksum
 b3416e11, total3712including prior2368. Independent CPU matrix/lighting math
 derives expectations before GPU execution. Cover world/view/projection and
 noncommutative multiplication, emissive material, directional normal response,
 enable/disable/DATA preservation, sparse destroy/reuse, point range and spot
 cones. Leave one enabled owned light for actual DestroyDevice worker cleanup.
 No caps/exports/registration/installation or paired pins change.

### Fixed-function committed-source and actual GPU acceptance

- Exact source e86e0c50a55d05e6ef78e06fcc106e1e7e4adf49 committed positive
 worktree01 passes202248checks; independent verifier matches56Git/archive/
 after-run/nativePE/static imports and signed58624/desktop continuity. Five
 compiled controls catch lost multiplication, light DATA reread, sparse slot
 misuse, enabled retirement and S_FALSE private publication. Last two control
 harnesses preserve all assertions and use exact CHECK failure _Exit instead of
 static teardown; production fixture unchanged. Earlier retirement01 compiler
 failure and publish01 assertion/teardown timeout are retained and excluded.
- User-requested build verifier passes complete single CI37520111683 all6jobs,
 five raw ZIP digests, ARM64/x64/x86202248checks,14actual native ARM64 execution
 hashes,56preflight02source links and Linux sanitizer/semantic controls.
 ARM64artifact11440226149 DLL3b3863a5/probecfbb7c79 retained; root live-refresh
 all6SUCCESS after target execution confirms the exact source link.
- Freshtargetfixed31/32 diagnostics1/0 independently verify3712pixels including
 1344fixed-function b3416e11, durations13.1856983s/20.6108307s. Each59nonempty
 submits,ctx1/1alloc14/14lock13/13residency14/14remaining0wrong-thread0;
 all8payloads3scripts/nativeARM64/LimitedUSER1 verify. Owned tasks removedexit0.
 Freshsigned58624/oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464 and58selected
 readiness fields retain. Full proof artifacts/dxvk-native-d3d9-fixed-function-20261007;
 checkpoint docs/native-d3d9-fixed-function-20261007.md. No production cap/export/
 registration/installation or paired pins widened; ordinary DX8-DX11 remains open.
- Next local Microsoft BufBlt audit: hDst/hSrc are vertex or index resources,
 Offset is destination bytes, SrcRange carries source offset/size. SYSTEMMEM
 buffers are currently rejected; native DXVK common buffers already implement
 SYSTEMMEM staging, dynamic draw uploads and mapping sequence synchronization.
 Reuse that renderer with typed byte ranges and borrowed-data snapshots, plus
 actual GPU copy/draw/readback acceptance. Existing copyBufferRegion handles
 same-buffer overlap through a temporary allocation.

## 2026-10-07 SYSTEMMEM and BufBlt implementation in progress
- Implemented borrowed/owned SYSTEMMEM VB/IB, pre-callback creation/unlock/copy
  snapshots, typed BufBlt ranges and overlap-safe native DXVK copy/readback.
  Native-only Vulkan usage now explicitly covers both copy directions and
  writable mapping destinations, including dedicated allocator fallback.
- First controlled native preflight passed 216868 checks. Preflight02 retained
  MSVC C2975 probe compile failure; corrected using an outer VertexPayload
  type alias. Fresh preflight03/04 are with the independent build verifier.
  Final preflight04 freezes57inputs/eightchanges against82b80b9, archive
  c63727e767b9068fe7313ccba2991f840eae2d94fc11dea4828fd3e9139d9e0c.
- New --buffer-transfer probe independently expects512pixels/49865e45 and
  1056bytes/5bd18a08 (4224totalpixels retaining all previous gates). Frozen
  wrappers pass native PowerShell parsing; actual GPU/committed native
  controls and full renderer CI remain pending. No ordinary runtime PASS.

### Exact committed SYSTEMMEM source verification

- Source c8e7ff5 implements the renderer and native contracts; 0d585e2 aligns
  only the GPU probe declaration/bindings for dynamic SYSTEMMEM repacking.
  Raw copy offsets, payloads and independent byte/pixel expectations retain.
  Superseded CI37526611880 was canceled before GPU execution and its complete
  x86/ARM64/Linux ZIP evidence is retained separately, with canceled jobs
  explicitly distinguished from compiler failures.
- Build subagent independently verifies aligned native preflight05: strict
  ARM64 probe compile and216868fixture checks,57source/after-run hashes,
  installed signed58624/oem17/SYSsha and DWM1552/Explorer6464 continuity.
  Root and subagent separately link all57inputs to final0d585e2; preflight05
  and committedworktree03 archives are byte identical (3345ded70...).
- Fresh committedworktree03 passes216868checks. Five separately compiled
  controls03 fail the intended creation/copy/unlock snapshot, destination
  bounds and NotifyOnly assertions. Independent root verifier checks all57
  source/archive/after-run/Git links, nativePE/COFF, exact control mutations,
  KERNEL32-only positive static imports and unchanged desktop/driver metadata.
  Six original compiled probe objects are copied into supplemental evidence.
- Push0d585e2 triggers exactly one automatic run37530530983. Full renderer CI
  and fresh GPU33/34 remain pending; no ordinary runtime acceptance is claimed.

### Accepted SYSTEMMEM and actual buffer-transfer GPU checkpoint

- Final0d585e2 single automatic CI37530530983 all6SUCCESS. Build subagent
  independently verifies five raw ZIP/API digests, ARM64/x64/x86216868checks,
  14actual native ARM64 execution hashes,57source links and Linux sanitizer/
  semantic gates; retained original logs prove full private renderer compilation.
  ARM64artifact11444158161 ZIP1a09e3a0/DLL18146f8a/probeba66af56 retained.
- Freshbuffer-transfer33/34 diagnostics1/0 independently pass4224pixels,
  including512new49865e45, and1056bytes5bd18a08. Durations19.0761805s/
  18.0587911s;78nonempty renders,ctx1/1alloc15/15lock14/14residency15/15
  remaining0wrong0. All8payloads3frozen scripts/nativeARM64/LimitedUSER1 verify;
  ownedtasks removedexit0. Freshsigned58624/oem17/binding0002/PnP0/SYSsha,
  DWM1552/Explorer6464/starttimes and58readiness fields remain unchanged.
  Root live CI refresh after both GPU runs confirms all6SUCCESS and verifies
  five frozen local acceptance helpers remained unchanged through execution.
- Checkpoint docs/native-d3d9-buffer-transfer-20261007.md; evidence under
  artifacts/dxvk-native-d3d9-buffer-transfer-20261007. Full ordinary runtime
  acceptance and long-term installed desktop stability remain open.
- While renderer CI built, added next typed SetClipPlane DDI using local MS
  homogeneous half-space docs. It snapshots inline4coefficients before callbacks
  and rejects Index>=6 before public-core index clamping. Native uncommitted
  preflight01 strictcompile/220595checks and57inputs/4changes independently
  verify with signedSYS/desktop continuity. Newprobe stages67..76 cover all
  xyzw coefficients, enabled updates, sparseplane5 and plane intersections.
  Exact rational CPUoracle expects640newpixels/ac340bd5,4864total, with no sample
  on a clipping edge. Frozen preflight02 includes the probe; after both buffer
  runs the build subagent begins its native verification. Committed clip
  controls/fullCI/GPU remain pending; no caps/exports/package pins widened.
- Clip preflight02 strict probe compilation fails MSVC C2220/C4456 because its
  local vertices hides the earlier draw vertices. The build verifier retains
  and verifies the exact failure,57source/after-run hashes and signedSYS/desktop
  continuity; fixture compile/run was not reached. Root renames clipVertices
  and freezes freshpreflight03 cb2c99ee30d29b046e1d30843fdb6bdd03f8c5cc911aed35286f9040d8929f5e;
  independent target build is pending. Positivefixture/oracle behavior retains.

### Committed clip-plane native contracts and candidate CI

- Corrected clip preflight03 strictly compiles the probe and passes220595 native
  checks; build verifier independently links all57 frozen/archive/after-run inputs
  to48efb7d. Committed worktree01 has the identical cb2c99ee archive and independently
  verifies220595 checks, native ARM64 PE/COFF and retained signedSYS/desktop.
- Four committed compiled controls01 fail their intended invalid-index or
  coefficient/index snapshot assertions. Independent receipts retain exact source
  mutations, all57 input links, native PE/COFF, original probe objects and fixture
  assertion logs. No assertion is weakened; failing harness exits use _Exit.
- Single automatic CI37533397261 builds exact48efb7d. Source/Linux/sanitizer gates
  pass independently. ARM64 artifact11445755987 ZIP2b2a9e13, DLLd40485e9 and
  probeab77d09c are verified ready for the controlled GPU probe; other CI jobs
  remain pending. Added clip mode to the bounded interactive wrappers and exact
  rational CPU acceptance helper. Native parsing and GPU35/36 remain open.

### Clip CI passes; first hardware pixel failure retained

- Exact48efb7d single automatic37533397261 all6SUCCESS. Independent build verifier
  checks five ZIP/API digests,57Git/archive source links, three PE architectures/
  220595checks,14actual ARM64 execution hashes/PASS outputs/empty stderr, Linux
  sanitizers/semantic gates and actual private core/backend compile and DLL link.
- Frozen three clip wrappers native PowerShell parse/hash PASS. Exact CI candidate
  staged without installation/registration. Targetclip35 diagnostics1 fails the
  first new stage67 pixel with clip mask0: x0/y0 actualff091725 vsff739a4c.
  Duration26.3768348s; all eight prior readback summaries pass. Independent failed
  evidence verifier links all8payloads3scripts/nativeARM64/LimitedUSER1 and checks
  signedSYS/desktop/58readiness fields unchanged,15residency refs/15evictions/0left,
  no device-loss log. Archive2dea31a5430c7e05972cd9e2f336bd249fbc3651c9f3ccc7e53c08583c529930
  and original failure receipts remain retained; clip36 was not launched.
- Local diagnostic-only25a9edd adds complete clip pixel logging without changing
  geometry, coefficients, expected pixels or checksums. The build subagent owns
  fresh native strict compile/link against exact48efb7d candidate and57committed
  frozen source inputs. No diagnostic CI push or GPU acceptance is claimed.
- Diagnostic25a9edd native linked probe independently verifies57source inputs,
  strict /W4 /WX /MT, AA64 PE, UMD/GDI/KERNEL32-only imports and six invalidCLI
  checks. Probe e0a54fe6 and b1bb0014 evidence receipt retained; exact candidate,
  signedSYS and desktop unchanged. Targetclip-diagnostic01 reproduces the first
  stage67 failure and retains all64pixels: every pixel is backgroundff091725.
  Duration20.8718953s; eight prior slices pass, exact8payloads3scripts/native
  LimitedUSER1 and58readiness fields independently verify. Failure receipt retained.
- Local diagnosticd79b3bb tests one inherited state reset, NORMALIZENORMALS=0,
  before unlit clip draws without a NORMAL declaration. Geometry, clip coefficients,
  pixel logging and CPUoracle stay unchanged. Frozen57 archivea326bde1 is with the
  build verifier for a fresh linked probe; target test and diagnosis still pending.
- Diagnostic02 build verifier and root reviewed re-execution link all57 committed
  inputs, strict ARM64 compile/link, originalexe3506c13b/staticimports, sixCLI and
  read-only checks, evidence46171ae3, exactCI48ef candidate/SYS/desktop. GPU run
  reproduces64backgroundpixels atstage67 in22.7688432s; all eight prior slices pass.
  Failurearchive7a6c49ff and independent8payload/3script/58readiness receipts retained.
  All eight frozen acceptance helpers remain unchanged. Normalization reset has no effect.
- Localdiagnostic03 d230cb0 removes the ineffective reset and forces clip enable1
  then disable0 before the identical draw. Frozen57 archivecfd9dea5 is delegated
  for fresh strict native build/link and source/continuity verification. GPU pending;
  no production fix, clip acceptance, ordinary-runtime admission or CI push claimed.
- Diagnostic03 native build/root review verify57sources, originalexe9be5d14a and
  evidencef87da22f with strict ARM64/staticimports/CLI/continuity. Target run records
  enable1/disable0 and still64backgroundpixels in20.6867262s. Eight prior slices and
  8payloads/3scripts/58readiness fields independently verify; archived50365e3 retained.
  Live CI refresh confirms exact48efb7d all6SUCCESS and four redundant workflows disabled.
- Diagnostic04 e980efa8 removes the ineffective refresh and uses the known passing
  FLOAT3 POSITION/NORMAL/COLOR stride28 layout with normals0,0,1 and lighting0.
  Positions/colors/planes/640pixel oracle remain identical. Frozen57 archive1d032587
  is delegated for native build verification; GPU pending, no accepted fix claimed.
- Diagnostic04 strict native/root verification links all57 inputs, originalexe36bc48bb
  and evidence587b4b88, unchanged candidate/SYS/desktop. Target run PASS in27.5857693s:
  640clip/ac340bd5,4864totalpixels,1056bytes/5bd18a08,88renders, ctx1/1alloc15/15lock14/14,
  residency15/15remaining0wrong0. Every640rawpixel matches the unchanged rational oracle;
  exact8payloads/3scripts/LimitedUSER1, driver/desktop/58readiness and eight frozen helpers
  verify. Archivead5ca979 retained. This diagnostic layout has normals/stride28/color24;
  it does not prove which difference fixes the original compact-layout failure.
- Diagnostic05 eade97ce removes only NORMAL declaration/count3->2 while preserving all
  vertexbytes/stride28/color24/state/oracles. Frozen57 archivef983b81d, strict ARM64 probe
  73efdd3f and evidencea36c52bf independently verify; root reviewed verifier logic stays
  identical apart from pins. All target build processes ended before fresh GPU05 starts.

- Diagnostic05 GPU PASS in31.2372503s:640clip/ac340bd5,4864total+1056bytes,88renders,
  balancedctx/alloc/lock/residency/wrongthread0, retainedSYS/desktop/58readiness. All640
  pixels and eight unchanged helpers verify; removingNORMAL alone has no effect.
- Diagnostic06 95ce11cd retainsstride28/noNORMAL but restorescoloroffset12, with
  unusedpadding aftercolor. Frozen57 archivee2beeb66 is delegated for native build.
  Positions/colors/state/plane/640pixel oracle stay identical; no production fix claimed.

- Diagnostic06 originalexe2ab80439/native evidence4520e041 verify57sources/strictARM64
  and continuity. GPU run reproduces all64clearpixels in25.1943667s atcoloroffset12,
  even withstride28. Independent failure/8payload/3script/58readiness and frozenhelpers
  checks pass; prior eight slices retained. Stride alone does not explain the failure.
- Diagnostic07 25da07dc inserts one unused zeroDWORD beforecolor (offset16), retaining
  FLOAT3 POSITION/stride28/noNORMAL/state/geometry/oracles. Frozen57 archived754f966
  is delegated for fresh native build verification; GPU pending.

- Diagnostic07 nativeexe94e13af9/evidence0f0d9d3e verifies57sources; target
  all64pixels3f800000 atstage67 (23.6047045s) match unusedfloat1 atoffset24,
  differing from expectedff739a4c. Fresh failure-only verifier retains original
  logs/archivec763da2c and confirms8payloads/3scripts/58readiness/SYS/desktop.
- Vertex-input equality final divisor loop overwrites earlier inequality.
  Production helper preserves it with &&eq; 342 actual helper checks cover all
  metadata/binding/attribute/divisor fields and forced colliding cache keys.
  Clang/GCC sanitizer positives and reverted-fix negative controls pass, each
  with342checks. Existing identity69/runtimeidentity180/backend49/shader147
  sanitizer gates pass. Exact compact probe restored
  from25a9edd. Source proof verifies helper differs only by that short circuit;
  new CI/native build/compact GPU acceptance remain pending.

- ae61dded3d2ba9b93327abf1c9ea46bb981d7717 committed/pushed once; only
  consolidatedCI37541475016 triggers. Linux identity/shader jobs pass; three
  architecture core builds remain active. Native comparator positive342 and
  exact guard-removal negative exit1 independently link70primary+15Vulkan
  dependency inputs (8864cdc8), archive de684fb7, source/Git/build/after-run.
  Original positive70ea6e45/negative d31f257b/staticKERNEL32/AA64PE+COFF verify,
  zero warnings, installedSYS/desktop/candidate retained. Root reviewed rerun
  preserves agent receipts and also verifies exact compiler flags/paths,
  unique positive/negative cases and DWM1552/Explorer6464. Evidence bc5eaa4d.
- While ae61dde builds, local uncommitted typed query slice implements explicit
  six-type mapping, owned query tokens, Begin/End translation, completion-only
  S_FALSE, owned pending output and full EVENT BOOL, post-result epoch checks
  and worker cleanup. New fixture covers snapshots/reentry/concurrency, guarded
  output/cached results, strict failures/retry, stale/wrong-device handles and
  reset before publication. Separate57input preflight01 archive76b27cb9 records
  exactly4changedinputs; native strict compile/execution is delegated. No query
  CI push or GPU acceptance yet. Preserve ae61dde candidate acceptance first.

- ae61dde consolidatedCI37541475016 all6SUCCESS independently verifies five
  raw ZIPs, three architectures/typed220595,15 native ARM64 executions and342
  comparator positive/exact-reverted-negative checks. Root reviewed fresh reruns
  and ZIP-member proofs retain the agent's originals. Exact ARM candidateDLL
  41c0c950/probeb9c2dc84 staged with verified receipt; no installed driver change.
- Original compact stride16/color12 probe passes clip37diagnostics1 (33.4559408s)
  andclip38diagnostics0 (28.9399387s): all640/ac340bd5,4864totalpixels,+1056bytes,
  88renders, ctx1/1alloc15/15lock14/14residency15/15remaining0wrong0. Root
  independently recomputes all640rawpixels from exact rational half-spaces for
  both runs. Eight frozen helpers/8payloads/3scripts/LimitedUSER1/58readiness,
  installedSYS and DWM1552/Explorer6464 verify unchanged. GPUarchivesd8edab34
  and72407194, finalroot-clip-checkpoint-verified-01.json and acceptance doc retained.
  Vertex-input equality fix resolves the original compact-layout GPU failure.
- Uncommitted query preflight01 strict native ARM64 fixture passes272881;
  root reviews/reruns with exact count/fourchanges/compiler/desktop pins.
  CreateQuery now reads only input QueryType, leaving output hQuery unread.
  New--queries probe includes192 independently logged pixels and actual
  occlusion64/16/0, guarded unissued/begun pending, EVENT fullBOOL/cached/reuse,
  ordered timestamp pair, disjoint/frequency and owned stale-token cleanup.
  Fresh preflight02 frozen57inputs/archive0e9aa70a has exactly5changedfiles,
  delegated for native fixture/probe compile verification; queryGPU/CI pending.

- Query preflight02 strict nativeARM64 fixture/probe compile passes272881,
  compile7.6697069s/run0.121894s, zeroWarnings. All57frozen/build/after-run
  inputs,52Gitbase+5changed, exactPE/COFF/staticKERNEL32 and installedSYS/
  desktop/acceptedcandidate verify. Evidence8c2a77ed, fixture30bda184 and
  probeCOFF709d485d retained. Six fresh one-expression controls are frozen
  from that archive and delegated; originalpositive remains immutable.
  Independent query CPU oracle derives192pixels/57872e55 and occlusion64/16/0
  before any GPU run. New query wrapper/verifier copies preserve all accepted
  clip helpers and add raw completion/cached/pending/frequency/timestamp checks.

- Query0a605643a78306a101350ec88fca51cd0974cbc2 committed/pushed once;
  only consolidatedCI37544531323 triggers. Committed57 archive is byte
  identical to preflight02. All six strict native controls compile0/noWarnings
  then intendedexit1; root reviewed/reran originalproofs preserving them.
  Expectedcheck/line: flags3651/2618,pendingstatus3153/2596,pendingoutput3982/2627,
  callerpointer5477/2639,eventBOOL11433/2639,postepoch36384/2691. Original36COFFs,
  sixPEs/allsource+archive/after-run and candidate/compiler/SYS/desktop verify.
  Rootcontrol01 strengthened receipt attempted a missing failure-result.sources
  field; failure retained, fresh02 uses actual wrapper-built and collected hashes
  and independently relinks all positive57 directly to committed0a60564.
- Separate query verifier selfcheck02 passes a synthetic parser positive and
  four rejected pixel/EVENT-width/timestamp-order/pending-status records.
  No GPU execution is implied. Earlier selfcheck01 missing relative cliporacle
  is retained; fresh helper pins the unchanged canonical rational cliporacle.
  Seven local query acceptance helpers/oracles are frozen before GPU, preserving
  all eight old clip helpers and all earlier GPU receipts unchanged.

- Query exact0a60564 CI37544531323 all6SUCCESS. Build verifier independently
  joins five rawZIPs, actualquery/core/backendcompilelogs, threearchitectures
  comparator342/typed272881 and15nativeARM64 execution hashes. Fresh committed
  CPU build passes272881, strict probe compile,72primary+15dependency hashes;
  root reviews/reruns strengthened proofs preserving all originals. Three frozen
  wrappers native parse/hash match, generic paths now query mode. Root verifies
  originalARMZIP11450142911/23802847/1c552b8f and every archived member, native
  PEs and closedOpenAdapter; stages exactD/P/STATUS candidate process-local.
  Query39 diagnostics1 launched; acceptance remains pending. Initial root CI
  read guessed sunfarms repo and got404; corrected to actual checkout gh context
  sunflower2333 without any workflow dispatch or remote mutation.

- Query39/40 independently PASS diagnostics1/0:5056pixels+1056bytes, nine
  completions/6handles, occlusion64/16/0, EVENTfullBOOL/reuse, orderedtimestamps,
  DISJOINTfalse/frequency19200000. Durations35.2096953/34.2518279s,ctx1/1,
  alloc19/19,lock18/18,render96,residency19/19/wrong0. ExactSYS/oem17/desktop
  and58readinessfields retain; bothownedtasks removedsuccessfully. Root recomputes
 192query+640clip rationalrawpixels and joinsimmutableproofs in root-query-
  checkpoint-verified-02.json. Acceptance docs/native-d3d9-queries-20261007.md.
  Root CI03 verifies all5rawZIPmembers and15actualnativebinary/stdout/stderr
  links. Review01 UTF8BOM and02 all-active-workflows assumption were corrected
  in fresh03 without touching originals:4unuseddisabled,2manual-only retained.
  Initial checkpoint01 rawCRLF prevented regex matches; fresh02 normalizes text
  as original strictverifier, all832 rationalrawpixels perrun PASS. No production
  issue or failedGPUtest is hidden by these retained local reviewer corrections.

- Presentation local implementation publishes an owned completed readback through
  the real RuntimeMemory typed9 allocation/callback path and separately created
  GDI context. Native preflight01 archive03d74ae8 independently verifies57inputs,
  seven original AA64COFFs (six fixture units plus probe), zero warnings and the
  original272881checks; query candidate/SYS/desktop continuity retain. Root adds
  behavioral source02: six distinct pixels in3x2 A8/X8 targets, copied callback
  and argument snapshots, owned readback despite renderer mutation, reuse,
  malformed/nonpresentable resources, exact callback statuses, balanced partial
  acquisition, reentry, reset/adapter-close and failed-deallocation retries.
  Unexpected successful deallocation retires ownership while returning E_FAIL
  so retry cannot double-free. Frozen57input preflight02 archive233efc63 is sent
  to the build subagent. Actual typed display probe development continues; no
  presentation GPU result or ordinary-runtime admission claimed.

- Native presentation behavior02 compiles strictly but actual fixture exits1 at
  check2153/line2914. Root identifies omitted PresentCb in the adapter's callback
  snapshot and adds its one field. Fresh immutable03 e5242c24 compiles strictly
  and passes466831checks; unchanged02/03 oracle and54common inputs are linked.
  All57after-run/source/archive hashes, six real fixture TUs+probe/sevenAA64COFFs,
  actualallocation symbols/static imports and querycandidate/SYS/desktop retain.
  Exact original failed02 and successful03 evidence remain separate. Root fixes
  CI fixture linking to include its already-built umd_allocation.obj and adds
  user32 only to the target probe. Owned64x64window/screen capture, real KMT
  Present and GDI context/resource handling are strict-compiled, unexecuted GPU.
  Fresh presentation wrappers/verifier preserve all query checks and add a16384
  screen-RGB oracle73b9e1c5; six compiled one-mutation controls are delegated.
  Local planning patch with an empty final hunk failed atomically and was reapplied
  without it; guessed verifier/workflow filenames were read-only misses.

- Root independently joins original positive03's57frozen/Git/after-run inputs,
  all20native-evidence archive members and seven actualAA64COFFs, six strict
  compiler TUs, nativePE and actual466831stdout; root-present-preflight-verified-
  02.json PASS. Original reviewer01 assumed a different stdout summary spelling;
  fresh02 checks the actual exact native summary without changing source/proof.
  Screen oracle parser self-check01 passes a synthetic full16384pixel record and
  rejects six damaged variants; explicitly no GPU evidence. Frozen query proof,
  wrappers and candidate remain unchanged. Production architecture CI is next.

- Presentation061ee8f4523b0b9732f7c02349bad9e3e84d40f0 committed/pushed once;
  only consolidatedCI37549764498 starts. Identity/shader and all three production
  backends pass; nativeARM64 runtime job is still running. Six semantic controls
  compile strictly then fail at the intended assertion: callback2153/2914,
  argument2153/2914,row4007/345,kernel4006/343,postepoch176610/3074,
  failedrelease143764/3057. Root independently joins all six archives, unchanged
  55positive inputs/control, 42originalAA64COFFs, actual fixture outputs and
  candidate/compiler/SYS/desktop continuity without changing original proofs.
  root-present-controls-verified-01.json links all57positive inputs to061ee8f.
  Fresh committed87-input CPU rebuild passes466831 with six actual fixture TUs,
  seven originalAA64COFFs and strict probe compile, no warnings: archive355c51ff,
  evidence5b36e7b9,exe6f2861eb,compile8.8227984s/run0.1154459s. Root independently
  verifies72Git+15pinnedVulkan inputs and all archive/build/after-run bytes in
  root-present-committed-source-verified-02.json. Root review01 incorrectly
  requested a wrapper built_sources field; build-result.sources contains the
  actual87 build hashes and fresh02 uses them, preserving original evidence.

- Three active presentation wrappers01 native parse/hash PASS; their frozen
  probe65s/result77s limits exposed the inherited scheduled-task55s cap before
  any GPU run. Old parser receipt01 also carried stale query deadline summary;
  separate agent receipt02 records all actual old limits without modifying01.
  Root freezes wrappers02 with present-only scheduled cap75s, leaving previous
  mode55s caps intact. All three generic wrappers are native-parsed/hash-joined
  after staging; previous query wrappers copied into the owned staging backup.
  Fresh local-present-verifiers-03 freezes corrected wrapper paths and unchanged
  screen/query/clip/transfer oracles. Earlier helper freeze01 failed on guessed
  clip-oracle.py; complete02 uses the actual clip-plane-oracle.py and is retained.
  No actual presentation GPU run or ordinary runtime admission is claimed yet.

- Exact061ee8f CI37549764498 all6SUCCESS and five original ZIPs verify;
  root-present-ci-verified-01.json independently rejoins every ZIP member,
  15native execution binary/stdout/stderr hashes and delegated subproof digests.
  All architectures compile the actual embedded core/backend and allocation
  fixture linkage, typed466831/comparator342. Exact ARMZIP11452103360 is
  23820458bytes/a909b9fe; DLL6021120/f08c8885,probe229376/489178f0,
  STATUS5056/7a411806. Process-local candidate staged, no installation.

- Actualpresent41 diagnostics1 FAIL at first D3DKMTPresent: rawc01e0342,
  HRESULTd01e0342 (STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE). All prior5056pixels
  and1056bytes/queries pass internally. Owned source allocation/resource,
  separateGDIcontext and paging/residency succeed before rejection; no captured
  screen pixels or presentation PASS. Source/context release succeed, residency
  20references/20evictions/remaining0; owned task removedexit1, original evidence
  archive9b0c2af1/12members retained. SignedSYS/desktop and58readiness fields
  remain unchanged. Microsoft local source-owner docs identify shared ownership
  registration as a missing raw-runtime harness step; causality is unproven.
  Probe-only diagnostic adds windowHDC/source/LUID lookup, shared owner acquire
  and device-scoped release, preserving every pixel oracle and all UMD bytes.
  No exclusive/display-mode operation is added. A separately identified native
  executable build is delegated before another GPU attempt. Original CI ZIP
  contains no import LIB; diagnostic derivation from the exact committed DEF
  will be verified against the actual CI DLL exports and recorded explicitly.

## 2026-10-07 shared source ownership diagnostic
Native probe8d9e196 is warning-free; root-present-diagnostic-probe-ready-01.json
independently joins57Git/archive/build/after-run sources, originalAA64COFF and
PE, exact source-derived import library and SDK libraries/headers. No native
fixture rerun claimed; unchanged466831 fixture evidence remains source linked.
Present42 actual HDC lookup returns nativeLUID6e6c/source0, closing its extra
adapter successfully. Shared acquisition returnsc01e0342 before allocation or
present. No release was required because acquisition failed. All prior GPU
slices pass, residency19/19remaining0, signedSYS/DWM/Explorer and58readiness
fields retained. Failure archiveb22c15177e2e226199e8ed7d203bb4c883d86f01c2e889b44f1e466a61013ae7
retains12originalmembers. Root failure review recomputes832query/clip pixels,
checks8inputs/3scripts/token and task removal; presentation_accepted=false.
Emulated owner probe-only diagnostic is prepared next: SDK defines type4 as
no real primary ownership. It may expose whether legacy raw KMT presentation
can coexist with DWM; this is a hypothesis, not successful redirection evidence.
UMD061ee8f, all frame colors and strict screen oracles remain byte-identical.
Full061ee8f CI37549764498 is independently complete, all six jobs, five ZIPs,
three actual architecture links and15actual native executions. No duplicate
automatic run was triggered by the shared probe-only [skip ci] commit.

## 2026-10-07 emulated ownership diagnostic
Frozen57input sourced22863e nativeARM64 probe compiles warning-free, six invalid
CLI and read-only adapter checks pass. Root independently rejoins original
COFF/PE/libraries/SDK headers and raw official User32 NuGet ZIP member; native
fixture466831 remains byte-linked and was not rerun. Root reviewer02/03/04 stale
helper/archive/reused-CI paths remain as failed evidence; fresh05 resolves actual
receipt paths and passes without modifying original agent evidence.
Targetpresent43 acquires EMULATED type4 source0 on actualLUID6e6c successfully,
but first raw legacy primary blit still returnsc01e0342. Emulated owner releases
status0; sourceallocation/GDIcontext deallocate/destroy0, residency20/20remaining0.
No screen capture or presentation PASS. SignedSYS/DWM/Explorer and58readiness
fields retain. Raw12memberarchiveacc123311b702f54bbfcc78ddedabc239ab9e73ce890fc326c7590b0fe1adeef.
Root failure proof checks8payloads/3scripts/token/task removal and832rational
earlier query/clip pixels. Source ownership alone does not fix this raw legacy
primary path. Next isolated probe-only diagnostic sets documented
PresentToBitmap (0x4000), with exact source/window/rectangles and unchanged
16384screen oracle; no exclusive owner, mode change or production admission.
Actual ordinary runtime still requires caps/export negotiation, primary/opened
resource ownership and runtime-managed window redirection; this raw harness
provides its own callbacks and must not be mistaken for that admission.

## 2026-10-07 bitmap presentation rejection and system runtime diagnostic
Bitmap probeaf1ae8a compiles warning-free on nativeARM64 in2.4950676seconds,
six invalid CLI/read-only enumeration checks pass. Root review06 joins all57
Git/archive/build/after-run sources, originalCOFF/PE/LIB, originalSDK headers
and raw official User32 NuGet member, reused originalCI inputs and continuity.
Native32memberevidencec56d0090acf299b74c4a106944331a61d93775f46886478376b9341ad5547206.
Targetpresent44 acquires/releases EMULATED source0/nativeLUID6e6c status0,
then exact PresentToBitmap flag40c1 returnsc000000d/invalid parameters on first
submission. No screen pixels or presentation acceptance.20residency refs and
evictions balance; sourceallocation/GDIcontext close0, signedSYS/desktop and
58readiness fields unchanged. Root failure proof verifies8payloads/3scripts,
LimitedUSER/token/task cleanup and832raw rational prior query/clip pixels.
Original12memberGPUevidence0889cdc973e291624450b9cafa3b121f919f7b4b6f55898d5d1161cf24b8e5b1.
Do not accumulate speculative raw KMT present flags or claim DWM redirection
from a private callback harness. Next implementation is a separate read-only
OpenAdapter frontend forwarding only adapter/caps queries to exactUMD061ee8f.
It always blocks CreateDevice; original caps/counts remain zero. A separate
probe uses genuine System32 d3d9.dll for Direct3DCreate9/Ex and enumeration,
plus two null-input frontend guard checks. Native build/guard/baseline come
before any temporary legacy-driver binding. No production export/caps, D3D10
registration or renderer changes; no ordinary rendering acceptance claimed.

## 2026-10-07 explicit read-only HAL negotiation
The system D3D9 diagnostic now queries GetDeviceCaps(HAL) for every enumerated
D3D9/Ex adapter. This ensures an actual caps request is measured instead of
assuming adapter enumeration alone loads the registered UMD. No CreateDevice,
drawing or presentation call was added. The original 78e66f6 60-input packet
is retained as source-audit evidence; a fresh exact committed packet will be
used for the first native build. Microsoft local INF documentation maps the
three UserModeDriverName entries to DX9, DX10 and DX11 in order; the SDK exposes
separate KMTUMDVERSION queries. Root is preparing a read-only active-name check
before any temporary legacy-only binding. No registry write has occurred.

## 2026-10-07 session1 runtime negotiation preparation
Root native build review01 PASS (60inputs/42members/2COFF/2PE/officialSDK).
Names01 and baseline01 independently verify under original LimitedUSER session1.
Bind01 changes only DX9 registration, but observed KMT lookup retains old path
and no candidate frontend calls occur. Original registration is restored.
Root33member failure trace remains verified, with34registration/58safety fields,
signedSYS and originaldesktop retained. No rendering or ordinary admission.
A process-local DX9 name selector is added to the diagnostic probe to obtain
actual Microsoft OpenAdapter/GetCaps callbacks despite cached target naming.
Only an exact original-package DX9 response changes; original function, other
queries/versions and normal baseline are preserved. Native strict build and
null-guard verification are delegated before session1 execution. No production
UMD code, caps, normal exports or workflow configuration change.

## 2026-10-07 exact process-local selector packet and target build
Committed/pushed diagnostic5214397 with skip-ci; latest automatic production
run remains37549764498SUCCESS. New immutable60input packet03 SHA05f24f70,
59unchanged inputs versus a84 and only probe.cpp changed. Build subagent reports
strict native ARM64 two-unit build zero warnings, six invalidCLI64, original
null guards and SSHbaseline count0/Ex8876086a. Root review pending original42
member archive b736777a. Fresh scripts04/worker02 read registry only, invoke
selector exclusively inside the owned probe and retain LimitedUSER/session1
and task/physical-name continuity. Collector02 explicitly checks selector task
removal. Next root original build review, native parser/ABI validation and actual
session1 callback trace. No production caps/device/resource or GPU claim.

## 2026-10-07 actual system callback trace and typed caps implementation
Root native build review02 PASS. Native parser/ABI/hash joins for scripts04
PASS. Selector01 executes genuine System32 D3D9 under original LimitedUSER
session1, observes two successful OpenAdapter/18caps and actual ExLUID6e6c;
zero caps/empty formats still reject HAL. Root32member archive736d3fc7 proof03
passes no registry writes/selector restoration/task cleanup/SYS/desktop/34+58
fields. Typed caps implementation and meaningful adapter fixture bounds/snapshot
checks are ready for native target CPU verification, followed by one automatic
consolidated CI. No new UMD has been installed or ordinary rendering claimed.

## 2026-10-07 committed caps native verification and single CI
Build subagent completes native478eca2 adapter42308 and device466831 checks,
strict3+6CPP compile/links with zero warnings and original9COFF/2PE. Original
33member archiveaedcf040 and88input packet e3d216f0 retain. Root fresh archive
review02 independently rejoins73Git+15pinnedVulkan, exact logs/output counts,
SDK/WDK headers and five original static libraries, native toolchain and
signedSYS/oldcandidate/systemD3D9/desktop. Root reviewer01 used the wrong helper
receipt key; the failed original remains and reviewer02 uses actual name fields.
Production478eca2 is pushed once, creating only consolidatedCI37561240247.
Updated read-only frontend/probe permissions and path select an owned fresh
CapsCandidate-478eca2. CreateDevice remains blocked. Native diagnostic build,
all six CI jobs/raw artifacts and another actual Microsoft caps trace are next.

## 2026-10-07 native caps diagnostic and device-contract preparation
Exact765a7e0 standalone native frontend/probe builds without warnings; six
invalid CLI cases, two null guards and env-absent SSH count0/Ex8876086a baseline
pass. Source60, original evidence collection and root review remain separate
from uninstalled production478 and its in-progress soleCI37561240247.
The next diagnostic source adds a blocked CreateDevice/Ex contract mode after
VirtIO1af4/1050, exactly one adapter and SM2 HAL caps preflight. It owns a hidden
16x16 window, records actual runtime flags/legacy buffer metadata and callback
presence, and never forwards device creation or any device callback to core.
API attempts are explicitly counted (two on complete trace); no drawing or
presentation is called. Native compile/guards and genuine caps trace must pass
before this device-contract run; ordinary hardware acceptance remains open.

## 2026-10-07 completed caps CI and actual runtime rejection
Full37561240247 all six jobs SUCCESS. Root raw ZIP/native provenance review01
passes all three builds and15 actual executions,42308/466831. Native diagnostic
765/c065 independent original build reviews pass. Fresh uninstalled478 candidate
SHAee680cb0 is staged from the verified ARM64 CI original. Selector-caps-01
archive481c992d/root review01 observes two actual OpenAdapter and20successful
GetCaps; formatcount4/querycount6 and exact80/24byte lists now work, but both
Microsoft9/Ex HAL caps still return8876086a. ZeroCreateDevice. Before/after
registry34/critical58, original SYS/desktop/two candidate hashes and task cleanup
verify. Device-contractc065 remains unexecuted due to its failing caps preflight.

## 2026-10-07 matched Microsoft HAL validator and dynamic texture gap
Microsoft symbol-server d3d9.pdb at image GUID312958D5-15B1-B907-952F-1B73B103FDD1
maps exact section headers/ARM64 public addresses; public stripped PDB age3
differs from image CodeView age1 and is recorded, not asserted equal. Native
IsD3DHALSupported RVA1226f0 takes nonzeroDevCaps2 to122808 and rejects missing
Caps2 bit29 at12280c. Production478 hasSTREAMOFFSET1/Caps2zero; native dynamic
texture creation/lockDiscard is absent. Implement the actual feature and retain
unsupported cube/volume/MSAA/autogen limits. Separate diagnostic frontend adds
only Caps2.DYNAMICTEXTURES to the exact478 profile, logs before/after, uses a
distinct permission and always blocks core device creation. This measures the
single-bit cause and genuine blocked runtime contract, not GPU acceptance.

## 2026-10-07 dynamic2D implementation and targeted verification preparation
Native resource descriptions capture Dynamic independently of mutable callback
metadata; renderer texture creation receives D3DUSAGE_DYNAMIC. Default-pool
dynamic2D mip surfaces are lockable. Discard requires an unmapped whole top level
and cannot invalidate another mapped mip; invalid surface flags/ReadOnly/partial
discard/lower-level discard reject without backend calls. Native Caps2 now
declares the implemented feature for pending verification. New meaningful
fixtures cover2formats/4pools, usage snapshots, data retention, lock/unlock
failures, retry and teardown. New target --dynamic-textures mode adds768 raw
pixels for3mips/2updates/A8-X8 while retaining all old5056pixels+1056bytes.
No native fixture, GPU or ordinary acceptance is claimed before actual results.
Separate diagnostic777 strict native build/guards/SSH baseline passes original
49member root review01; its exact478 candidate/SYS/desktop retain. Session1
single-bit blocked contract trace is next while production CPU verification runs.

## 2026-10-07 first dynamic verification failures retained
Actual777 single-bit DeviceContract trace still rejects HAL with8876086a;
preflight stops before both API attempts (actual0/core0), and selector/task/
registration/SYS/desktop restoration verifies. Dynamic textures are required
on the inspected branch but are insufficient for admission. Root's prepared
successful-contract reviewer rejects the0attempt trace as intended; retain it
as a failed preflight and independently review the complete failure evidence.
Native531 adapter fixture passes42309. Full device fixture compile rejects
the new test's misspelled D3DDDIPOOL type; official SDK defines D3DDDI_POOL.
Fix that test type in a fresh committed packet. Original failed build/source/
logs remain separate; no device execution or productionCI was attempted.

2026-10-07 03:14 UTC: Independent review02 of actual single-bit runtime
trace device-contract-dynamic-01 verifies original32member archive536bf095,
40successful GetCaps callbacks/eight exact Caps2 deltas/four genuine adapter
opens, but HAL9/Ex remain8876086a. VirtIO preflight accepted0; API/core device
calls0. Registry34/critical58/SYS/desktop/candidates/task cleanup retain.
This disproves sufficiency of the dynamic bit; full goal remains active.
Read-only matching-PDB disassembly identifies legacy PrimitiveMisc0x2000:
FetchDirectDrawData RVA19b7cc maps it into public fog/specular-alpha0x10000,
IsD3DHALSupported RVA122854 requires it forSM2, and FillInCaps RVA16571c clears
it from public output. New isolated frontend retains dynamic diagnostic and
adds only legacy0x2000 to exact478 profile (28ef0 ->2aef0). No production caps
change/device callback/GPU admission. Fixedb21 native fixtures compile warning
free; device test fails at379107 because SYSTEMMEM binding wrongly expectsS_OK.
Keep originals; correct fixture to verify rejected binding makes no backend call.

2026-10-07 03:17 UTC: Correct dynamic fixture's SYSTEMMEM binding oracle.
The actual DDI intentionally rejects sampling a SYSTEMMEM transfer resource.
Now assert E_INVALIDARG and unchanged backend bind count for SYSTEMMEM, and
assert exactly one bind plus one destroy-time unbind for video/native pools.
Do not change production SetTexture semantics. Originalb21 failure379107 and
zero-warning compile evidence are retained; fresh88-input packet03/native run
required before production push.

2026-10-07 03:28 UTC: Fresh native0dd packet03 passes42309 adapter and
521942 device checks, zero strict compiler warnings. Root independently
reviews original35member archive66cf59ef and all88 Git/archive/native inputs,
actual9COFF/2ARM64 PE outputs, raw SDK/CRT provenance and candidate/SYS/desktop
continuity; root-native-dynamic-fixtures-verified-01.json records acceptance.
Single production push starts consolidated37566610477 at source0dd6f6f.
New isolated dynamic GPU scripts parse on native ARM64 PowerShell and their
originals/hash/deadline gates pass root review; GPU768pixels remains unexecuted.
Separate hardware-probe strict native compile rejects two newly introduced
C4456 shadowing warnings. Retain original25member archivebe9f9b42; rename only
the two sampler loop variables and require a fresh native probe compile before
a corrective production push. No GPU, registration or ordinary admission PASS.

2026-10-07 03:36 UTC: Corrected8d7 hardware probe strict native ARM64 build
passes with zero warnings and remains unexecuted. Root original27member
review90e2c092 joins88 frozen Git/Vulkan inputs, derived DEF/import symbols,
original478 CI ZIP exports, SDK/CRT and SYS/desktop continuity. Native fixtures
actually compiled0dd; independent equivalence proves their87 relevant inputs
unchanged in8d7. Push exact8d7 once; prior0dd CI is superseded by concurrency.
Isolatede229 diagnostic actual HAL9/Ex caps both S_OK and preflight accepts
VirtIO1af4/1050. Two blocked device callbacks report flags3 and1, core0. Static
CRT buffering interleaves callback and API-result lines; worker anchors count
only1 result despite2 attempts, so prepared strict reviewer rejects complete
contract acceptance. Retain original runtime archive and fix only standalone
frontend trace emission to one synchronous WriteFile per formatted record.
Fresh native diagnostic build/guards and clean session1 trace are required.

2026-10-07: Replacement single consolidated CI37567440170 builds exact8d7;
superseded0dd37566610477 is cancelled by existing branch concurrency. Isolated
eaa7 synchronous-trace frontend passes fresh native2CPP/8CLI/nullguard/SSH
baseline, root original49member d1ceccb2 review02 and all60 Git/SDK/continuity
joins. Fresh blocked session1 trace02 is running, without core creation.
Implement documented AllowMultithreading/AllowFlipBatching permissions at
both adapter and device validation boundaries (supported mask3, reserved bits
still reject). Existing worker remains serialized and Present synchronous.
New fixtures exercise0/1/2/3, every reserved bit, no callback/backend on invalid
inputs and mutation of caller flags after snapshot. Fresh native CPU source
and architecture/GPU validation are pending; do not claim ordinary admission.

2026-10-07: Root independently verifies original8d CI37567440170: all6jobs,
5rawZIPs/3PEarchitectures/15actualnativeARM64 executions. Exact8d candidate and
isolated native-parsed GPU scripts are staged without installation. Dynamic45
passes5824pixels+1056bytes in46.3889s. Its first host review omitted the earlier
buffer-transfer oracle; new family02 adds only byte-identical preexisting oracle
and preserves all original7 verifier files. Original45 evidence then passes.
Dynamic46 times out65.9019s during progressing clip74 and remains failed; before/
after driver58fields/desktop retain. Fresh native-parsed script family02 extends
only dynamic process105s/task115s/poll117s. Original host native-schema review
used path instead of name and is corrected using unchanged original parser rows.
Dynamic47 diagnostics0 passes5824pixels+1056bytes in72.2869s. Both accepted runs
verify768dynamicpixels/bfeeeac5, all prior gates, ctx1/1alloc19/19lock18/18,
render108/residency19/19/wrong0, exact LimitedUSER and owned task removal.
Root dynamic checkpoint01 joins all originals and explicitly rejects failed46.

Clean blocked system-runtime legacy-fog02 now root PASS: 8genuine opens,
60caps S_OK, two real API/device callbacks flags3/1 and zero core creations;
registry34/driver58/SYS/System32/desktop/tasks restore. Exactc8f native CPU
permission fixtures root PASS43067/535562, zero warnings/all88 inputs and
original35members/9COFF/2PE. Push exactc8f starts only consolidated37569563644.
Separate889acc3 diagnostic preserves blocked478 mode and allows an explicitly
permitted exactc8f process-local lifecycle trace. Real HAL9/Ex offscreen creation/
clear/readback/release must all pass for success; no presentation/production
admission claim. Freeze60 exactGit inputs53bb0e7b; delegate native compile/CLI/
dual-permission nullguards/ordinarySSH baseline only. Actual root GPU runtime
run awaits native review and independently verified c8f CI candidate.


## Genuine runtime lifecycle01 and DDI version mismatch

Exact889 frontend/c8f CI37569563644 enters actual System32 CreateDevice for
HAL9/9Ex, returns two private S_OK devices and immediately receives two
DestroyDevice calls before any resource/state callback. Both public API calls
return8876086a. Original42-member archive SHA2e40fa9f916456059a15d2c4f3b6a3ba5e09d3a69a4dfc7b03ac2ec99ffed485
is retained; registry34/critical58/signedSYS/DWM1552/Explorer6464 and selector
restoration pass. Frozen512-pixel lifecycle oracle has no positive GPU result.

Matching public PDB locates ValidateUMDeviceFuncs at RVA0668d8 and
IsDDIVersioningInvalid at RVA062378. The latter rejects a non-null old function
or a null new function. For driver version>=4002 the runtime requires this
old/new pairing for BufBlt/TexBlt/VolBlt/CreateResource, and for version>6000
requires AcquireResource and ReleaseResource. Native9 currently reports the SDK
default11000 while implementing the original entrypoints. Official local
d3dukmdt.h defines Vista000c and original d3dumddi.h DEVICEFUNCS has99 pointers
through Rename. New local fix negotiates000c and bounds both outer/inner table
publication to that prefix; native guard-page/canary fixtures remain required.
No production OpenAdapter export, registration change or GPU acceptance.


## User requested parallel DX8/DX10/DX11 replacement

DX10 and DX11 agents now own isolated categorized worktrees and coordinate
typed table/creation boundaries. Existing build verifier finishes frozen c7
CPU evidence before taking DX8; root keeps actual DX9 lifecycle, native original
proofs and shared integration. No independent CI pushes or target concurrency.
Root lifecycle01 original42-member restoration/source/payload review passes as
root-system-runtime-device-lifecycle-01-verified-01.json SHA7153de91fcd769323a030a3fea2e7490a39fbb3a13b8dc30f3cf548e2f080085.
Both genuine APIs remain rejected, with zero accepted render pixels. New
standalone frontend/probe pins c7 and reads only the99-pointer Vista table;
compile/guard verification and actual runtime rerun remain required.


Matching raw System32/public PDB GUID/sections/native table validation proof
now verifies as system-runtime-symbols/native-device-function-validation-verified-01.json
SHA39a5aca29bb616dd5167de504cfdd13b3c43fe57b3fe8e7b495f5de82bf1624e.
Image CodeView age1 and stripped public PDB age3 are explicitly distinct.
Agent reports exactc7 strict native43067/538869, both protected table paths
executed, zero compiler diagnostics and exactc8/478/061/SYS/desktop continuity;
original archive collection and root independent verification remain pending.


2026-10-07 continuation: Root originalc7 CPU review02 now PASS43067/538869,
35raw members/88 source inputs/9COFF/2PE/zero warnings and exact guard marker.
Review01 rejected a stale stdout assertion; original error and review01 are
preserved, unchanged native archive re-reviewed correctly in02. Supplemental
rawc8 candidate continuity proof01 passes all four original files.
Standalonec0ea native Vista runtime build rootPASS60 Git/53raw members/2COFF/
2PE/10CLI/3nullguards/zero warnings; candidate absent during guards, GPUfalse.
Six-script Vista lifecycle family01 stages and natively parses five executable
scripts, root proof SHA5b514339a3b4687de9d282549707714dde0f95c0fff5686baac321664d1ec0f1.
Prepared stage/run helpers retain independent512-pixel oracle and deadlines;
actual device-lifecycle-02 awaits full c7 CI37573265151 success and verification.
DX8 read-only original target inventory proof01 verifies System32 d3d8 absent,
SysWOW64 d3d8/d3d9 I386 and d3d8 imports d3d8thk. No runtime load or installation.
DX8 agent receives raw imports/exports/CodeView inventory; DX10 and DX11 remain
isolated, coordinate typed ABI and shared declarations, with no target overlap.


2026-10-07: Exactc7 consolidated37573265151 rootPASS6jobs/5rawZIPs/3PEarch/
15actualnativeARM64. CI proof SHA29352289cf1eece8060c73c760c0a13523330d96319a672f729d2166c821bdfc.
Exact CI ARM64 core SHA141ee74feb1d1b51e44bb0673769b848a695efe45136f5fc0ff1b1c05f66aae8
is process-local staged, no installation/registration. Actualc0ea/c7 Vista
lifecycle02 moves past ValidateUMDeviceFuncs and requests two16x16 X8 back buffers,
flags1081 (RT/NotLockable/DiscardRenderTarget), pool3,mips0; CreateResource mask
rejects1000/E_INVALIDARG. Both API calls80070057/object0. All58critical/34registry/
SYS/desktop/names/selector restored and both created devices destroyed. Original
42member5c8740b37566a32c5ff30c4533b8dd1a2b918cc37539aa72874bf65e88ac2ed2
reviewPASS20d7a2349b9eba40e2ee50a29711aabce9258745a5644b3d0c4127c6465a83e5,
evidence/restoration only, no rendering. Independent512-pixel oracle unchanged.
DX10b490ed1 and DX11bdc963c integrate as85ca8eb/0d4ff1d with typed10.0/10.1/11
selection, live runtime callbacks and resource/UAV/compute paths. Higher production
feature gates stay closed. Root fixes four ctor callback lifetimes, exact direct
factory interface/builds and native-entry six-failure live callback mutation;
MSVC native execution remains pending. GeneralSM5graphics/SO/tessellation/class
linkage and runtime activation are still incomplete. DX8 originalI386 runtime
calls OpenAdapter Interface8 and CAPS12; its isolated bounded projection is pending.


2026-10-07: Implement target-only DiscardRenderTarget1000 hint to accept observed
runtime flags1081 without changing preservation, primary/shared ownership or
feature advertisement. Add actual-shape four A8/X8/lockability controls with
1024 readback pixels, failure/retry, invalid-output/no-backend and caller-mutation
checks. Local strict official x64/x86 COFF01 first implementationx86 exposes
preexisting signed count_if comparison; original failure retained, bounded count
converted tosize_t. Fresh implementation02 passes both architectures; unchanged
fixture01 objects both pass. Native MSVC and actual runtime still pending.
Integrated callback fixes/ports source8d7a2ea is frozen for exclusive port_dx10
native CPU verification. New consolidated build/package/execution lists include
DX11 and compute fixtures, making17 future native ARM64 cases; no second workflow
or push dispatched. Root will send new exact discard source packet next.


2026-10-07: Actual native adapter8d7 attempt01 /W4 /WX /MT fails fixture C4127
at two constant offsetof CHECKs; three production adapter/query/contract AA64
objects compile cleanly. Agent original failed tar84fa1254e0610376494e0dc4c1d72d017381fc91e28837ed6782347383fc7284
retains source12/SDK21/libs5/RSP/logs and SYS/System32/desktop checks; root review
pending. Convert only these compile-time ABI bounds to static_assert, preserving
runtime guard-page semantics. Native rerun and discard-source packet pending.


2026-10-07: Freeze exact4e91632 discard/native9 source89inputs SHA
c27811a6f2cd25c3524064fa38757c30a07ced3ae4588c36418adadd8fe162c2.
Native verifier owns new static_assert adapter retry and then target9 fixtures.
Prepare separate frontend/probe permission device-lifecycle-4e91632 and fixed
DxvkD3D9DiscardCandidate-4e91632 path; table99/512-pixel oracle/deadlines and
blocked caps mode stay unchanged. No native build/run or CI push yet.


2026-10-07 continuation: Root independent original native4e adapter reviewPASS
93976/12Git/52members/4COFF/1PE. Root native9 discard reviewPASS43067/554085,
89Git/Vulkan/141members/9COFF/2PE and both Vista99 protected writes, proof
SHA82740dd5389fde3e6f040390ee1c84efd9ef6e86e143649d141eff300e26518c.
Standalone68d original53-member review02PASS60Git/2COFF/2PE/10CLI/3guards/
zero warnings; proofSHA8f71d2b8d2ea5a02aa55759df232ce2186e5acb4a07bd7cf2c65b8def86259f6.
Review01 missing reused d3d9 SDK receipt is retained;02 rejoins originalpackage.
DX8b61 integrates5cff890; root verifies13inputs/15COFF/9PE and integrated
Linux543caps/4235imports. Earlier4237includes originalMicrosoftmappedimage.
SM5metadata134 integrates550d336; root GCC ASan/UBSan actual71checksPASS.
DX8identity/backend and DX11realshaderstages remain isolated active work.
FreshGitHubquery confirms sole latestc7 run37573265151success; no newCIpushed.
Prepared4e candidate/17fixture collector/runtime03 scripts remain unexecuted.

WARP4e attempt01 stops on missingd3d11.lib;02 retains malformed helper/I and
pinned dependency compilerdiagnostics. Fresh03 splits unchanged pinned dxbc
at repositoryW3 policy from strictW4/WX UMD/fixtures, retaining all warnings.
It reaches productionumd_ddi.cpp and exposes15C4127 constantStage comparisons
in templated SRV/sampler/CBbinders. Root8db3d7a changes exactlythose15to
if constexpr, no other implementation edits. Agent preserves03 and re-freezes
exact8db for nativeWARP/related lifetime regressions. No4eCI/runtime dispatch;
newcandidate/frontend pins and fresh native9 provenance remain required.


2026-10-07 continuation: affe7d4 native ARM64 nine-control original archive
3881b1d6b9a1d52bf3c86f81a7c1ca4e94097b8ef8aa2ffa0d83f749e56dd78b
collected; root independent review pending. Production/fixtures strictW4/WX
zero warnings; pinned dxbc W3 retains61 dependency warnings. WARP retains old
DWM1552/Explorer6464 before/after. Native9affe attempt01 stops before compilation
on strict Get-Process Explorer enumeration. Separate read-only snapshot02 shows
currentDWM1864(start1791378029900)/Explorer4464(start1791352985381), USERconsole1
Active, same SYSd48e/oem17/58624/PnPStarted/service4. Cause of post-WARP desktop
identity transition unresolved; no shell restoration justified or attempted.
Root owns target; capture fresh stability/boot snapshot then freeze new desktop
baseline for subsequent CPU/runtime controls, preserving all old proofs.
Standalone2851 and native9affe source archives frozen separately; no new push.
Local search shell unmatched wildcard is a read-only lookup error, no inputs changed.


Root snapshots02/03 join current desktop/SYS/PnP/session; System41/6008 records
unexpected restart, cause unresolved. New baseline01 frozen, no recovery. Port_dx10
owns native9affe02 and standalone2851 CPU. Root WARP review01/02 stdout-parser
errors retained;03 explicitly matches actual runtime GPU PASS852 and rotation1428
forms, original native packet unchanged. Front packet preparation assertion used
a nonexistent user32-copy anchor; unexecuted partial four receipts preserved,
completed via actual librarySource anchor. New2851 build copies originald3d9 SDK
receipt into native evidence; source60/coreaffe/oracle/deadlines unchanged.


2026-10-07: Root affe WARP original review03 PASS, proof94c37c2ff9f6f6b8b3764c40944a5b4a0eeef0bcfc08f732e09840aa3034220a.
Source71=47Git+24pinned dxbc,202original members,25COFF/9PE/22 native stages;
checks308/35/2637/10834/1428/6404/1629/1144/852. Strict UMD/fixturewarnings0;
unchanged pinned dependencywarnings61 retained. Historical SYS/desktop unchanged
during WARP. Fresh baseline03 independentlyjoins newDWM1864/Explorer4464; System
41/6008 unexpected restart, no current shell absence/recovery. Exactaffe production
object pushed once; livequery confirms sole consolidatedCI37580729947inprogress.
No frontend2851 or newer agent slices pushed. Root originalreview01/02 parse faults
and unexecuted frontend anchor-preparation error retained separately. Source and
execution archives unchanged; native9affe/frontend2851 CPU verifier owns target.


Root integrates functional DX11 1f asbf4f779 and DX8 498 as7c2c8ef locally; only
Meson shared_library conflict, retained both shader11 andd3d8_compat source units.
Integrated root SM5 ASan/UBSan93 and legacy2800/actual pinned SM1 converter47PASS.
No newer push. Prepared affe runtime family preserves512-pixel oracle/100000ms/
115s deadlines; replaced historical hardcoded desktop IDs with frozen current
PID+starttime checks, original c7 helper family retained byte-for-byte.


Root fresh affe native9 original142-member review01PASS43067/554085, proofSHA
1eab24277147ff0fb22f18de7571634aff755dcba45395f8eede1806c0526c86. Front2851
original54-member review01PASS60Git/2COFF/2PE/10CLI/3guards, proofSHAcd941b7341ed159cd2c08226ff27984c0245cb2dbc0c0609eaf7d2aff5dc5d3e.
Native-parsed fresh6scriptfamily staged with no API/registry calls; unchanged
512oracle/100000ms/115s deadline. Soleaffe CI37580729947 all6jobs success; raw
5ZIP/source/PE/17nativeexecution collection running. Target root-owned after
CPU verifier release. New root7c2 portableSM5/legacy/SM1 passed93/2800/47;
DX8 original302source/311validation root review passes202Git+92pinned+8SDK/
33COFF+6PE. Future build/package/native control lists now include allthree new
SM5/legacy/SM1 fixtures,20native cases; no additional CI push or dispatch.


Before actual runtime dispatch, root found helperfamily01 start-time guard used
100ns ticks reconstructed from JSON Unix milliseconds. That could reject a valid
unchanged process due to lost submillisecond precision. Family01 was parsed but
unexecuted; preserved. Freshfamily02 checks PID and DateTimeOffset Unix milliseconds
exactly matching original baseline precision. No fixture/source/oracle/deadline
changes; actual runtime remains pending candidate artifact verification.

Fresh helper02 preparation first draft changed historical original template path
to nonexistent02; failed before output. Original draft retained, restored old01
source path, new02 output/guard precision remains. No target execution occurred.


Root soleaffe CI original collector PASS five ZIP/threearchitecture/17native,
proof31f9105d. Candidate3files staged core952118bc/probe4d0aa194/STATUS20cda9b2,
rawCI ARM64 ZIPd8e5ba44. Actual lifecycle03 helper02 stops in baseline/restored
KMT preflight before any API/probe call due historicalLUID6e6c vs currentec6b.
Original20member archivea48c940f preserved; collector removes bothownedtasks.
Native queries all S_OK, Source0, three identical original UMD names, close/release
balanced, exactLimited USERsession1. Result incorrectly retains historicalCIrun
37573265151 and restoration_failure from repeated same preflight; no restoration
success inferred from that result. Separate after-readinessfa901e50 joins exact
SYS/desktop/PnP and58critical fields; original34registration unchanged. Root
independent reviewdc47a7b1 scopes verification to failedpreflight andzeroAPIcalls.
Fresh helper03 archivea33d5c9e corrects LUID guard andactualCI37580729947 metadata
only, joins originalfailureproof before execution, keeps512oracle/deadlines.
Originalhelper01/02 andallfailedarchivebytes unchanged; nativeparse/lifecycle04next.
Root read-only lookup unmatchedglob and absentselector trace lookuperrors retained;
no production input changes. Fresh x86 packet7c2 frozen bybuildverifier, target
root-exclusive while actualgenuine lifecycle continues. No newCIpush.


2026-10-07 continuation: Genuine family03/lifecycle04 completed. Original42-member
archive03bd63362010a975402a8dc7578564c3919a168b63b3c5dfed635a41cfbb6704
and independent reviewb15d5cfc68bceae8cc06b2793ed51ca514ff6c9fbfa0622eac84c1e02c8c65ed
join Limited USER/currentec6b, two accepted1081 backbuffers, two rejected
SYSTEMMEM65536-byte VB02080004 creations, public9/9Ex80070057 and zero API pixels.
34registration/58critical fields, desktop/SYS/owned tasks restore. Secondary
first-flush88760868/Mesa-4 remains unresolved; no evidence of successful submit.

Root merges DX10formatbcbde4f asc2c4d14 and DX11query/IAffb418f asf4bf37f cleanly.
New root MightDrawFromLocked implementation snapshots borrowed bytes before
callbacks and forces owned draw-time SYSTEMMEM backing. Independent DX11 review
found two issues corrected: require Lock's own0x100 rather than creation-only
flag, and explicitly FlushBuffer after DirtyRange so mixed/unused streams queue
updated bytes before every draw. Locked state/count stays held through draws.
Reviewer also found upstream UploadPerDrawData subtracts vboOffset instead of
srcOffset in the partial-tail branch, permitting logical over-read for60-byte
VB/first1/count3/stride16/extent12; root owns bounded planner repair/control.
Agent owns isolated actual renderer repeated mixed/unused/16/32-index controls.

Local failed controls retained: integratedportable01 missing shared DXGI header
after private4029PASS; portable02 input41PASS then Clang dependency unused-private
reserved field under strict flags. Fresh03 uses original GCC SM5 command without
suppression and adds portableMSAA. LocalCOFF01 retained missing build-policy
flags/ARM64 Clang builtin-vs-official intrinsics mismatch;02 device/fixture4PASS
but baseline winternlOBJ_CASE_INSENSITIVE conflict in both backends. Fresh03 uses
Windows-only harness prelude and unchanged project warning policy, no source-wide
suppression. Actual ARM64 native execution remains mandatory.

Build agent exclusively owns target after root lifecycle04. Official EWDK14.50
Hostarm64/x86 owned tree575inputs/17recovered originals/61Microsoft ranges READY;
source ISO/mount unchanged. Exact7c2 CPU packet now running. User authorizes
passwordless Administrator. Root continues local development; no new CI push.


Root vertex-copy production helper and independent scalar-byte oracle pass
4050438 checks under GCC+Clang ASan/UBSan. LocalCOFF04 passes8UMD/backend/fixture
objects, but core warns on three preexisting unused captures. Removed those
captures; fresh05 strictly compiles both production cores with zero diagnostics.
Root local proof joins8+2 objects, exact input bytes and SPDX-only provenance.
Portablequery/IA/format03 completes SM5351/MSAA269, joining4029/41 prior originals.
Consolidated Linux script passes old controls plus planner/private/MSAA; original
malformed-token diagnostic retained. Future Meson/build/package/native lists
now cover25 ARM64 cases. Existing sole CI37580729947 refreshed all6SUCCESS.
Build verifier first x86 attempt tar-list fails PS5.1 nullExitCode before any
compile; original retained, explicit ProcessStartInfo runner/freshroot next.
LocalCOFF03 Windows-only prelude requires NTSTATUS for DDI units; fresh04 uses
original winternl for those units and Windows-only for private core/backend.
Read-only guessed native result/workflow/tools paths failed, no inputs modified.
New native-device and actual renderer controls remain mandatory; no new push.


Root follow-on bounds review identifies aggregate uint32 UP size can wrap across
streams/IB. Checked append now rejects overflow before allocation and keeps the
ordinary path; IB source/count products use uint64 clipping. Fresh portable02
GCC+Clang ASan/UBSan4050444 checks each; freshCOFF06 four core/copy objects PASS
zero diagnostics. First native packet closure incorrectly looked for conditional
Vulkan submodule header in root Git, stopped locally before archive/transfer;
failed empty01 retained, submodule-aware frozen-source preparation follows.
Agent probe6054985 adds18 real mixed/unused/perdraw repeated locked-buffer stages
98-115, expected1152newpixels/6d390bc5. Root review of callback414a flags borrowed
D3D9 table rereads after CreateDevice; agent asked to retain owned originals to
preserve production snapshot behavior before integration/native execution.


Protected teardown control added for a still-locked flagged borrowed VB after
PAGE_NOACCESS: DestroyDevice must discard the held lock without reading/uploading
borrowed bytes. Both local x64/x86 strict fixture COFF compile; native execution
pending. Native x867c2 caps544/API2800/adapter45802 pass, device7919 assertion fails.
Source audit identifies missing RT setup, not a capture switch (root initial
capture inference corrected to agent). Agent37e306e adds unbound rejection then
owned8x8 RT/bounded20-byte-stride60-byte FVF forwarding and exact cleanup.
Diagnostic4285518 corrects414 borrowed-table lifetime risk with owned22-slot
originals and protected/retired-table controls. Integrate both final slices and
freeze fresh root CPU source; original failed/native/local archives retained.


Integrated final probe605 as1124a52, protected teardown ase8120ad, DX8RT fixture
37e306e as36696a1 and diagnostics414/428 asc762aab/e7e1711. Original snapshots
retained; final forwarding uses owned22-slot original callback prefix throughout
lifetime. Callback CPU marker calls25/Vista22/function99/admission0; added to
sole consolidated build/package/ARM64 lists, now26 cases. Native execution not
yet claimed. Frozen f4 WARP88-input packet sources remain unchanged; fresh PS5.1
explicit-handle/raw-stream helper is prepared byDX11 before target handoff.
Native locked-buffer source preparer02 now resolves pinned submodule Git origins
for conditional Vulkan includes, and uses explicit ProcessStartInfo/raw streams.
First failed empty root archive-preparation01 preserved separately.


Root native ARM64 CPU4 packetb75-02 locally joins328 exactGit/submodule inputs,
sourcearchive4bd8b8a8fcadc7f24bff90177fbd1ed8ff8de3bc3a7d118dffee2115a86c650d;
no target parse/build/run yet. Actual EWDK x86b75 firstparty strict passes
544/2800/45802/584971, with observed unbound80070057/noDraw/0bytes and bound
S_OK/oneDraw/60bytes/stride20. Pinned dxbc vendor build first failed blanketW4/WX
(empty-macro/unused/shadow diagnostics); documented Meson vendor policyW3 and
conforming preprocessor now selected in a fresh helper, originalsource unchanged.
Root consolidated bounded fixture runner now rejects a missing ExitCode instead
of treating it as success, and drains stdout after wait. ARM64 runner already
compares explicit nonzero exit. Source/current driver goal remains active.


## 2026-10-07 integrated DX8/DX10/DX11 and bounded native runners

Root independently joins probe605 and callback428 original source/COFF evidence
with integrated sources. Fresh root-agent-integrations-02 verifies six strict
x64/x86 COFFs, the18-stage1152pixel6d390bc5 locked-buffer oracle, unchanged
callback caps/permissions/runtime probe and all nine original f4 packet files.
Attempt01 failed an assertion before saving a proof; it remains unaccepted.
Latest live affe CI37580729947 remains all6PASS; newer root source is unpushed.

SM4.1 agentc9c08fa is cleanly integrated as c9634b5. Logical10.0 rejects4.1
before the broader backend; actual new portable568 and typed10/10.1 fixture18
WARP draws/4608pixels are retained in the sole CI (28 native ARM64 cases).
Native new shader rendering is pending. Integrated ClangASan portable checks
are running under consolidated-linux-02 with raw logs and before/after source
hashes. Earlier source-path searches guessed .github/scripts and a zsh glob;
no files changed, and actual build/ARM64/Linux scripts are under scripts/.

Build agent reports exact b75 x86 six positives544/2800/45802/584971/52/4237,
34I386COFFs/9PEs, two intended semantic negatives and seven malformed CLI exits64.
Twenty first-party/W4WX compiles have zero warnings. Fourteen pinned vendor
units use repositoryW3/conforming preprocessing, retaining C4244/C4146 raw
warnings. Original collection/independent review and explicit target release
are pending; no hardware/runtime acceptance is implied.

Root freezes fresh CPU helper03 from unchanged b75 packet02/328inputs/four
fixtures. It reuses the tested f4 raw-process component and independent
failure-preserving finalization, and adds conforming preprocessing. Original
helper02 had unbounded kill/reap/pipe-drain calls and could lose its build
receipt if a final continuity capture threw;02 remains unexecuted and retained.
All fixture definitions, source/archive/compiler/SDK/library/candidate/desktop
pins remain unchanged. Target ownership stays verifier until explicit release,
then DX11 f4 WARP15, then root ARM64 CPU4 and the actual renderer/runtime gates.


Integrated consolidated-linux-02 completes PASS in31.73s: ClangASan/UBSan
SM4.1 568, existing shader147, boundedcopy4050444, privatechildren4029,
MSAA269, identity69/runtimeidentity180/backend49 and vertex342+negative.
Before/after integrated input hashes retain. Raw expected malformed-parser
stderr is preserved, with no sanitizer findings. Agent reports final frozen
c9 eight x64/x86 COFFs zero diagnostic bytes; root original review follows.
One combined production push starts the sole CI while native target execution
and original review continue; production admission and installed package stay
closed/unchanged. Later hardware testing requires successful exact CI artifacts.


## 2026-10-07 current published regression and native original review

Published b6cda5f sole CI37593963656 reproduces d3d11-device line54 failure in
both x64 and x86; ARM64 production, identity/shader CPU pass, native runtime
skipped. Raw failed-jobs log811269bytes SHAfca4eee4 preserved under
artifacts/dxvk-native-d3d9-locked-draw-20261007/ci-b6cda5f-failure-01.
Frozen f4 native WARP15 fails first fixture c0000409; original archive636bdf36
joins88 sources and11 retention flags with no finalization error. Fresh agent
diagnostic02 disproves the 32-slot layout hypothesis: 10.1/11 original/rebuilt
public WARP layouts pass. First compute shader callback16 returns80070057 after
169 checks; exact original/rebuilt comparison is next, no production fix yet.
DX11 retains exclusive target CPU; root issues no remote commands.

Root independent x86 original review03 accepts399 files/398 collection entries,
139 sources/133 Git origins/six supplementals,575 official compiler/header/lib
before/after joins,69 explicit commands,34I386COFF/nine I386PE, six positive
markers, two semantic negatives and seven CLI exits64. Observed FVF unbound
80070057/draw0/bytes0 then boundS_OK/draw1/60bytes/stride20. Original50,543,748-byte
archive61c38914 and root-native-x86-original-verified-03.json are preserved.
Root review01 guessed Name rather than actual lowercase desktop name/pid/start;
02 incorrectly demanded vendor-only Zc preprocessing in all first-party response
files. Both failed locally before writing acceptance;03 reads actual policy and
passes. Exploratory receipt reads used absent build-result.json and dict rather
than collection list; corrected to originals, no target changes.

Fresh CPU4 source/helper05 pins b6cda5f329inputs/13units including missing
umd_d3d8_compat.cpp in adapter/device. Original helper02/03 unexecuted; helper04
preparation correctly failed locally because its seed328packet omitted compat.
Failed04 metadata retained.05 prepared-input receipts pass, native parse/build/run
remain pending explicit DX11 release. New raw locked pixel print compiles strict
x64/x86 objects with machine8664/14c, zero stderr, resultPASS4.389s. No further
production push until the compute regression correction is validated.


Root follow-up source read corrects two resumed assumptions: existing605 probe
already prints all1152 D3D9_LOCKED_BUFFER_PIXEL raw values and final LOCKED_BUFFER
CLI marker. Redundant two-line D3D9_LOCKED_PIXEL trial is removed; its local strict
COFF originals remain as an unneeded trial, final production source unchanged.
SM4.1 root original review01 used an uninitialized worktree submodule (Git fell
back to parent); use actual initialized pinned root dxbc Git repository.02 then
used a guessed fixture filename;03 derives exact sm41 test names from original
manifest. Failed empty01/02 directories remain unaccepted. A runner read used the
checkout-relative workspace helper path; corrected to absolute workspace path.


SM4.1 independent root original review03 passes384 files/383 collection joins,
79 exact original Git/dependency inputs, raw GCC and ClangASan/UBSan568/147/35,
eight strict x64/x86 COFFs/zero diagnostics,365 compiler read inputs and28
original Microsoft SDK/header/library joins from four original packages. Root
supplemental integrated-source-original-joins-01 independently joins all four
actual COFF source paths (including tests/umd-d3d10-shaders.cpp) and shader header.
Native ARM64 SM4.1/WARP rendering is pending target handoff. A follow-up rg used
workspace rather than checkout tests; corrected through original COFF paths.

Fresh locked-runner-scripts-01 derives only new mode admission from frozen dynamic
runners, preserves ordinary1472 pixel gates and adds18stages/1152pixels/6d390bc5.
Three scheduled scripts retain LimitedUSER checks and process-local payloads.
Owned original raw-process componentd8cf replaces nullable Start-Process exit/
unbounded waits. Locked process65s, kill5s, pipes20s, task120s/result122s bound
failure cleanup; old frozen runners unchanged. Native parser/execution pending;
no hardware acceptance or target command by root while DX11 owns CPU.


Fresh locked verifier preserves prior1472 typed pixel/callback/readiness gates and
independently requires all1152 original pixel values in exact18-stage row-major
order, recomputesFNV6d390bc5, checks every locked resource/create/range/unlock/
retirement and bound raw child exit/pipe bytes. It requires pinned fresh desktop
baseline9eb3c586 and explicit actual LUID; no historical6e6c fallback. Python
parses; native PS parser/GPU execution remain pending. New unexecuted orchestrator
requires all six exact-source CI jobs PASS and local candidate bytes equal actual
original ARM64 ZIP, and guards independent cleanup/readiness/archive finalization.

DX11 diagnostic03 actual public WARP accepts both original compiled compute and
legacy rebuilt computeS_OK; generic decodeShader11 rejects before modern backend.
Original diagnostic archive023b5508 retained. Root source review of agent's fresh
repair worktree sees a22-line stage/operand/mask/duplicate validation addition for
four compute system input registers, excluded fromISGN; native/portable controls
are in progress. Old dx11-port worktree is frozen; use actual dx11-warp-repair
worktree from git worktree list. A reference artifact search guessed a worktree
artifact directory; actual original evidence stays in workspaceartifacts.


Root integrates10c831c compute input correction as555c83d, then exact malformed
binding preservation3f64065 as14f0a69. Root integrated ClangASan/UBSan646 checks
PASS16.48s, before/after input hashes unchanged. Independent original local review
joins two646 raw GCC/Clang runs, six actual strict x64/x86 COFF objects, zero logs,
and exact three integrated source files. Agent local receipts lack before/after
source hashes; this limitation remains explicit, fresh native Git packet provides
those actual inputs. Root direct diagnostic03 archive review independently joins
171members,88 retained sources,19 outputs,11 retention flags, realc0000409 exit,
original/legacy identical compute tokens and actual ThreadId00020012 declaration.

Corrected native3f first attempt still fails decodeShader11 on the new all-four-ID
shader after original and legacy public WARP succeed. Failure archivef25b2d20
retained; root holds CI. Actual next diagnostic confirms SV_GroupIndex uses
zero-component operand00024000, while other three input declarations use xyz
vector masks. Agent collects the original DXBC and will add only the dedicated
scalar zero-component form/effective-x overlap validation plus exact114-word
native FXC portable replay. No successful corrected native WARP15 yet; target
exclusive ownership remains DX11. Existing CI37593963656 remains failed exactly
b6 (fresh read confirms all six job states); no additional workflow dispatch.

Frozen GPU runner packet includes six original scripts/oracle/verifier inputs,
manifest33cceff1 and target prep helper48a8a0f6. Preparation/verification operates
only in new CUsersPublicDxvkLockedRunners-01; native parser/readback still pending.

Root resumed current096e024: all-four-ID compute96-thread native oracle and39
malformed typed creations now pass. Later failure is four-stream GS/SO, after
packed mixed float/uint/sint IA and earlier graphics controls advance. Original
and rebuilt public plain GS bothS_OK; agent owns target CPU while comparing the
exact stream-output conversion. Fresh GitHub37593963656 still fails x64/x86 at
b6; identity/shaderCPU/ARM64backend pass, ARM64runtime skipped. No new push.

Resumed read errors: first workspace discovery used paths relative to checkout
instead of workspace; DX8 artifacts are workspace-owned rather than in its
worktree; shader11/DDI backend were guessed under src/umd. Correct with absolute
workspace paths and rg --files before source reads. First plan patch mismatched
wrapped SM4.1 sentence; atomic patch rejected without changes, corrected above.


Root resumes current096e024 with merged669 local sanitizer checks and actual
corrected compute96-thread/39malformed native controls passing. Later original
four-stream GS/SO creation remains E_INVALIDARG; packed mixed-type VS and original/
rebuilt public plain GS succeed. Local Microsoft stream declaration ABI confirms
stream/slot/register/xyzw mask fields. Declaration ordering remains a hypothesis
until direct original/rebuilt public GS/SO comparison, not an accepted root cause.

Fresh GPU orchestrator02 guards owned task cleanup after any remote preparation
attempt, including lost SSH response following registration. Original unexecuted
01 stays unchanged. Revision962a8194 and source receipt locked-orchestrator-source-
02.json retain exact diff and Python parse; no native execution or acceptance.
Empty findings apply_patch was rejected after successful helper02 preparation;
no plan change occurred until this explicit append. Root target access stays idle
until DX11 release. All6 CI states refreshed at exactb6, no new push.

Native81fa root original failure review02 verifies every163archive member,
88exact Git source/before/after/collection joins,19AA64outputs and11retention
flags. Three corrected shader/native fixture sources join current096e024 exactly;
all remaining source differences are recorded. Actual failure remains caller576/
2016checks/153callbacks/80070057, realc0000409 exit, bounded original pipes.
First ad-hoc review prep duplicated subprojects in the manifest's already full
submodule path; saved review01 then guessed standard COFF rather than actual
MSVC bigobj header. Corrected02 reads actual bigobj machine at offset6. Both
local validation failures preceded acceptance and imply no native state change.
A resume read also guessed nonexistent scripts/check-ci-state.py; actual GitHub
state is read directly through gh. No target call or production push by root.

Actual DX11 native GS/SO diagnostic03 isolates declaration ordering. Original
FXC and rebuilt public GS/SO both reject0,1,2,3,0gap withE_INVALIDARG/null and
accept0,0gap,1,2,3 withS_OK/non-null, unchanged slots/strides32/16/24/16 and
RasterizedStreamUINT_MAX. Single-stream raw/grouped controls pass. This confirms
an API conversion boundary rather than lost output signatures. Agent collects
originals then stable-groups public declarations byStream, preserving stream-local
element/gap order, exact byte/query oracles and malformed control behavior.
Root original81fa archive actually has163files/SHAe4632a68, correcting the earlier
plan typo171. Its three root-source differences are umd_ddi.cpp and SM4.1
umd_shader.cpp/.h; repaired compute sources join exactly, full-root native
acceptance remains a separate gate. No root target calls or production push.

Root freezes CPU4 native preflight helper6de68da8 and execution orchestrator
868831df, with original source/helper05 eight inputs checked locally. Native
preflight will pin prepared manifest32ade9cb and parse both original PowerShell
helpers on native ARM64 before build. Orchestrator preserves each raw SSH/SCP
command/stream and independently finalizes/joins the original archive after any
build attempt. Neither helper is executed; target still belongs toDX11.
DX8 agent local04 now preserves seven before/after source hashes, four strict
I386COFFs/three PEs, soleOpenAdapter export, GCC/Clang306positives and eight
semantic controls. Native CPU/build and genuine SysWOW64 runtime remain pending;
agent freezes final packet after updating legacy manual entrypoints for the new
separate modern-DDI guard unit. Core pins come from a future original CI DLL.

Root reviews/integrates exacte6bf08d six-line stable SO grouping asd153e818d61a.
Fresh root merged ClangASan/UBSan945 PASS17.44s/compile0/run0,13declared input
before/after hashes retained, result root-stream-output-d153e81-01. Nativeexacte6
strict build, GS/SO construction and all13 malformed pointer/private-byte/canary
controls pass. Later NativeQuery creation124 returnsE_INVALIDARG before the
four-stream draw; no full byte/query/pixel acceptance. Agent retains original
failure and compares exact public Query/Predicate/QI behavior for stream-specific
booleans before correction. Target ownership staysDX11, sole CI remains held.

DX8 freezes e99391a28-input packet (14harnessGit/11uncompiled b75references/three
licensed headers), local04positive306 and legacy18COFF/9PE compile gates. Root
finds its new native Run() still has unbounded Kill/WaitForExit and serial40s pipe
drains. Agent replaces only new helper with exact already-tested raw-process
component in a follow-up/fresh packet; e993 originals retained, no integration or
target execution yet. Root resumed Meson read guessed tests/meson.build; actual
root meson.build/src/umd/meson.build contain no DX8 runtime-probe target, confirmed.

Root independent DX8 local04 review02 passes seven exacte993 Git/source-before/
after joins,37original commands, four strictI386COFFs, three independently parsed
I386PEs, soleOpenAdapter frontend export, raw GCC/Clang306sanitizer positives and
eight exact semantic negatives. Imports remain the original system/CRT libraries.
Review01 guessed a positive command name rather than matching its actual
sanitizer compiler argv; rejected locally before proof write,02 selects the
original command structure. Native helper bounded-process follow-up pending;
e993 frontend code remains unintegrated until that fresh packet is ready.
Target scheduling: after DX11 current query correction attempt/collection,
explicitly release to rootCPU4, then frozen DX10SM4.1, then return to DX11 if its
remaining WARP15 stages need more work. No overlapping target calls or CI push.


Root integrates bounded DX8 followup77bbe79 asf897bc2 and per-stream overflow
query correction634d781 as2c1b0e2. Public WARP diagnostic shows CreateQuery succeeds
for9/11/13/15 while CreatePredicate rejects all four; ordinary Query owners are
kept for native typed predication. Exact634d native strict build and query/draw/
readback advance to later GraphicsShader decode assertion558, stage not captured;
lastHRESULTS_OK/checks2700/callbacks186. Original archive309694cb/88sources/21SDK/
2APIlibs/19AA64outputs/6ownedstages/11retentions are joined before explicit target
release. Root starts frozen CPU4b6-05 in orchestrator session5099; DX10SM4.1 follows
only after original finalization. DX11 prepares next code/stage diagnostic locally.
Merged root production/query/native fixture strict x64/x86 check is running
locally using retained07 compiler arguments and original before/after sources.

DX8 final handoff02/9b1804ee joins fresh77bbe29-input packet8c3a43b1 and independent
frozen-packet-cross-verification03/937952c3:880 compiled tool/header/library inputs,
four strictI386COFFs/threePEs. Nine compiled first-party inputs remain byte-identical
e993 to77; GCC/Clang306 and eight intended semantic controls retained. Native
PowerShell parse/EWDK CPU and genuine runtime execution remain pending; no core
DLL is required for CPU packet. Future CI core commit/run/path/hash stay null.
Build verifier now audits sole consolidated actual I386 private-loader config
locally, without target access, rebuild/relabel or CI dispatch.

Resume reads guessed nonexistent tests/meson.build and build-backend-umd.yml;
actual root meson.build/src/umd/meson.build and build-native-umd.yml are discovered
before follow-up. These failed local path reads caused no native state change.


Root native CPU4b6-05 completes in13.55s and raw archivee8c17473 is finalized.
Independent root-review02 verifies all409members/329Gitinputs/21SDK+five original
libraryZIPmembers/13AA64COFFs/fourPEs/eight actual process handles/exits/pipes and
11retentions. Adapter45802/device584971/copy4050444/callbacks243 pass. Ten unique
compiled units equal currentroot2c1b0e2; full-source differences umd_ddi.cpp and
umd_query.h remain explicit, so no full-root native acceptance is inferred.
First reviewer incorrectly assumed every positive line says PASS checks=;
callbacks original says verified checks=243 with25calls/22VistaCallbacks/99funcs.
Original failedreview01 and root-review-error-01 retained; corrected02 reads actual
marker, without rerunning native fixtures or altering their original bytes.
Merged query correction strict x86/x64 sixCOFFs pass with684root input before/
after hashes, raw zero stderr and original07 SDK/compiler flags. No target run.

DX8 independent root-frozen03-review02 passes all29source archive/Git joins,
880compiled before/after inputs, fourI386COFFs/threePEs, two older original ZIP
hash/CRC joins, actual PE imports/soleOpenAdapter export and nine unchanged root
compiled inputs. First reviewer guessed handoff key compiled_first_party_source_
joins; actual key compiled_input_links is inspected and used. Original failed01/
errorreceipt retained and no proof was written until corrected02. No native run.

DX10 frozen c9 actual strict first-party compilation/link,18AA64COFFs/twoPEs and
nativecontainer568 pass. Typed shader fixture rejects first expected S_OK at
expect30/E_INVALIDARG before any draw; no18draw/4608pixel acceptance. Agent preserves
167-member archive7301d4bd/three original FXC4.0 VS/GS/PS containers+token pairs/HLSL,
seven exited/drained owned children, all12retentions and completed collection/
transfer before explicit release. Target next belongs exclusively to DX11 for
its already frozen634d stage/code/class-linkage diagnostic; root and DX10 stay
local. After diagnostic finalization, root will run fresh77bbe DX8 CPU packet.

Fresh GitHub read: CI37593963656 remains exactb6 failure; identity/ARM64backend/
shaderCPU pass, x64/x86 backend fail, ARM64runtime skipped. Four unused workflows
remain manually disabled; offline/runtime-control are manual-only, consolidated
backend is sole production push trigger. Build verifier identifies all three
backend jobs omit VulkanLoader, so future production cores would search public
winevulkan/vulkan-1 despite existing module-local loader support. Narrow workflow/
config provenance fix will pin existing architecture-named private loaders.

## 2026-10-07 DX8 native success, hull repair and typed DX10 setup

DX8 exact7c1f545 native CPU PASS is retained as original127-member archive
9f7db177:28 owned stages all exited/drained,306 policy/14 CLI guards,4I386COFF/
3PE,29source/575compiler/8SDK/7library before/after joins and unchanged state.
No runtime/selector/core/hardware call is inferred. Actual EWDK bcrypt header is
shared/bcrypt.h; original77bbe precompile failure remains retained.

Root220b4c0 hull phase decoding passes merged1118 sanitizer checks. Exact14ebee
WARP15 native packet is root-owned after agent execution turn stopped under
automatic cybersecurity-content filtering without a result. Root directly runs
the prepared build/test, preserving original phases. Native corrected result
pending. DX10 ad13 proves all shaders create/bind; first Draw rejects missing
typed fixture state. Root merged fixture chain0256206/d536e58/3178900; immutable
production/HLSL/oracles retained. Native corrected e1bc suite pending.

Root integrates61e private-loader CI as a334e39; source/config local checks pass,
new CI native parse/execution pending. Separate bounded process repair assigned
to build verifier. No push/dispatch/installation yet. Workspace artifacts live
under /home/sunf/droidvm-repos/artifacts, not the checkout artifacts directory.
Two initial cat reads incorrectly used checkout-relative paths and failed; rg
confirmed actual original paths and handoff messages were explicitly corrected.
Microsoft local SetRenderTargets/SetViewports/ClearRenderTargetView docs were
read during fixture review; actual original SDK signatures remain authoritative.

## 2026-10-07 continuation: integrated modern source and exception fixtures

Root exact890 native attempt finalizes archivecebf778e49106fdf1036c72b7a2f686ef3d59718d374a5fbdde16cb678a40f27 (6644818bytes/290files):13 fixtures PASS, actual typed4147/callback430, query189/private4029/input41; final d3d10-formats strict compile fails oneC2220/sixC4702. All31 raw owned stages exited/drained and all11retention flags true. The original14 run had3832/367; do not transcribe poll-sensitive counts into890. Root release review errors01 (transcribed count) and02 (assumed homogeneous checks= format) remain preserved. Discovery command also initially used checkout-relative path for a workspace .planning helper; corrected through rg --files. Another exploratory host_commands[0] print assumed a list; actual DX10 schema is a mapping. No original guest evidence or source changed in these review corrections.

Root3c1ef05 changes only two fixture files: volatile runtime exception injection avoids optimizer proving callbacks noreturn; every CHECK site, caught exception, zero output and call-count oracle retained; production templates unchanged. Portable ClangASanUBSan269 and four strict optimized official-header x64/x86 COFF controls PASS; actual MSVC test pending. Root3868b01 integrates bounded SampleIndex fix a00, andc407197 integrates separate modern=true test export925e with guarded modern table/malformed creation controls. Public admission/capability bodies unchanged.

Native a00 completes9draws/2304 exact reference comparisons including SampleIndex256; next interpolation DrawE_INVALIDARG is captured. Agent original review227/79Git/21SDK/7libs/18COFF/2PE/7native stages/12retention PASS; archive d1b4e7f161619d89d5a28b1fb11abafebaa7cbd5047c937a29b78b471624bd93. Exact PS TEXCOORDreg0 conflicts with VS SV_Positionreg0/TEXCOORDreg1. Official D3D10 linkage requires register alignment; fixture-only position-first signature correction is next, not production fallback. All57/18/4608/3 oracles remain required. Root has exclusive target; c407 fresh packet88/15/archive8200a4614eb38648ec7a7b258d9d2120c5af0a18cca0f4c5ed1e861ecc6ab79f/manifest03d373966ef24af7902851e0f3ba5856ddcf9ead9104e4de6574f6784c7e0f46 is prepared and preflight running. bf6 full-script parse/wrapper smoke and c407 adapter native guards follow. No newCI/push/hardware acceptance.

## 2026-10-07 completed native WARP15 and bounded wrapper smoke

Exact c407197 full native WARP15 PASS. Original archiveb367f1dc987d7fec090d62ef3ce69afa729cd8a4db500d8fcb9121085768bf55 (7044979bytes/305files) and root-original-native-review-01.json join88 exact Git/archive/beforeafter inputs,34AA64COFF+15PE,34 exited/drained raw-process stages,21SDK+7libs against raw official packages, original helper/d8cf hashes, strict RSP flags and actual15 fixture outputs. Typed3977/callback396, native-entry2637/lifetime10834, SM51118, query189, private4029, input41, format3475/callback12/backend2 and MSAA269. Firstparty warnings0, unchanged pinned parser65. All11retention flags true, original source differences to root empty. Completed suite remains controlled WARP/CPU, no native hardware/ordinary runtime admission.

Native bf6 wrapper smoke PASS: full committedPS AST0errors, actual functionextent9af16c1856583de7b3d3d1569d9d794a42418b073cf6b9ebc9e5088fa5b2c155 unchanged; real success0, nonzero7, concurrent1048579stdout+1048587stderr and fixed30-second kill/reap30.0061365s controls all accepted. Original archive3435c36d04391c54fe14ed5038f271efe6b38c0a526b1df53ec5d3756041fa04 (17663116bytes/47files),46 indexed original joins,9source/575compiler/3SDK/4library beforeafter plus SYS/service/desktop identical. NativeI386COFF/PE/PDB; three compile/link/inspect+four wrapper cases+tarcollection=8 actual retained OS children all exited/drained (intentional timeout exit-1 expected). All five host transports complete0; root release proof finalized. No production core built/loaded, GPU runs0.

Root integrates fixture4a6 as85ae592; synthetic compatible-input control bug5/16 was then caught in agent original review and preserved. Correctiveb358 integrated111585a changes actual offsets5/15, asserts literal16 and compares all46 words; frozen763/147/35 and COFF4 PASS. HLSL2119/SHA7d658b unchanged after4a; all production/pixel/reference/57/18/4608/3 bodies unchanged. Isolated b358 packet differs from merged root at six sources including umd_ddi.cpp and umd_shader11.cpp; agent is preparing exact d2f native packet instead of relabeling isolated objects. Rootd2f0e0348b5ee13e7f6d140f00aba8dca036e491 integrates one CI adapter invocation via unchanged bounded runner9af/d8cf, keeping other26 calls, architecture/stdout/zeroexit policy. Five-sourcec9 actual nativeAST audit remains next. c407 adapter guard02 has13 source joins/8localCOFF, actual nativePS5.1parsePASS and nativebuild underway. Root retains exclusive target; no push/CI yet. Small exploratory filename/CWD/schema errors are discovery/reviewer failures only and never overwrite original receipts.

## 2026-10-07 native adapter guard and CI AST finalized; merged SM4.1 handoff

Modern c407 guard02 PASS94,208. Original archiveb3f3fd58d46756167c82b5e609679b76bbe9a4908ffb9c7a16367a4705c31c66 (995242bytes/62files), three actual OS children exited/drained0, fourAA64COFF+onePE, zero first-party warnings, source13 Git/archive/current-root/beforeafter/collection joins and all11 retention pairs verified. Seven local transport receipts finalized0. Root release is finalized; build verifier independently reviews official inputs locally. No core loaded or GPU/runtime admission.

Native PS5.1 c9 adapter AST audit PASS: full before/after parse0errors, bounded26-to27/direct adapter1-to0, byte-identical function9af/C#d8cf, outputadapter-test.txt/exit0/30000ms/architecture condition retained. Five source inputs beforeafter unchanged; upload/audit/transfer original raw receipts exit0. Root final release28274e0f joins transferred proof. The broad CI script was only parsed; its build and GPU actions were not executed by the audit.

DX10 merged d2f packet prepared3d753bf0/source732868ee/builder7b19b1ee, original local proof69a32b61 with GCC+Clang ASan/UBSan763/147/35 PASS. Root independently rejoins all79 inputs to current-root Git/archive bytes, including the changed DDI and shader11 compilation units. Explicit target handoff6f4fd6d4 gives DX10 exclusive ownership for native57/18/4608/3; all peer target calls stop until original collection/transfer and release. Root corrected a submodule-bool schema discovery after an exploratory TypeError; no originals changed or new target attempt.

Fresh GitHub API snapshot root-ci-current-state-d2f0e03-01 confirms latest sole automatic CI37593963656 failedb6, no fresh published candidate; four unused workflows disabled_manually, offline/runtime controls active but manual-only. DX11 local real-KMT compute probe performs384-word/1536-byte structured UAV readback with typed11 guards; source/strict/native link reviews continue before integration. No push or hardware/ordinary-runtime acceptance.

## 2026-10-07 complete merged-source SM4.1 accepted

Exact d2f native763 compiler/7483 typed checks,57 original shader pairs/HLSL7d658b,18 draws/4608 reference-equal pixels/3 expected malformed create callbacks PASS. Original archive5c2a71a26762c5202ee4cc6e9952e03b8860fff91d679b47268beb5da27c1fc3 (3371615bytes/275files). Agent original prooff96b6e1e and root-sm41-merged-d2f0e03-review-01.json independently rejoin all79 current Git/source inputs, actual7 retained/exited/drained native stages,18AA64COFF+2PE,57 exact DXBC/token chunks, all18 scene rows and preserved final markers. First-party warnings0/parser65 retained; full SDK21+libs7/raw package28 joined by agent. All12retention true, six local transport operations finalized0; release41d59f explicitly returns target to root with no pending target operation.

Both full CPU/WARP publication gates pass. Root bcedab5 changes only the stale ARM64 runtime-case marker568 to actual763; fixture inputs and all draw/pixel/negative oracles unchanged. DX11 compute-probe review caught an overrestrictive locked-allocation residency guard: matched Mesa keeps command/global BOs mapped through Render, while the proven D3D9 transport permits owned mapped references. Agent corrects/finalizes separate test probe/CI package slice before one consolidated push. Final native script parsing/probe compile-link and actual fresh CI artifacts remain pending; no hardware/ordinary-runtime success inferred.


## 2026-10-07 corrected compute source and final script audit

Root integrates eb3/23d as7b32aaf/da0a4ed; seven independently reviewed inputs match current committed Git. Corrected probe supplies DXGI runtime revision9 (Version0x00020009), preserves actual optimized D3DCompile/HLSL/DXBC/SHEX and384-word readback, and permits owned mapped BO residency. Independent read-only source proofca2795e1 verifies four original x64/x86 COFFs, typed10/11 callback signatures, guarded ownership/cleanup and separate GCC/ClangASanUBSan12482-check oracles with12288 bit rejects each. These are CPU/source checks; actual core/hardware pins remain null.

Final actual WindowsPowerShell5.1.26100.1591 audit PASS full before/after script parse0errors, original27 bounded invocations preserved and one compute-oracle call added, function9af16c/C#d8cf unchanged, ARM64 table28 and strict SM4.1 marker763 joined to original native stdout. Native proof7cc2b00a and releaseedd4a073 finalize all three host transports0; both current root scripts still match tested55a23df/cd3914 bytes. Four unchanged locked GPU runner scripts also parse0errors on nativePS5.1, proof3d6d68af/releasecf1d68d7; no script main, scheduled GPU task or core invocation occurred.

Root independently joins frozen23d packet19 exact Git/archive/current-root source inputs and13 prepared files. Native attempt01 passes three nativePS parses and originald8cf Add-Type, then LIB exits1146 because unparenthesized concatenations split each /DEF and /OUT scalar into prefix/path/quote response rows. Original archive3a42a11f (58files), failure reviewd62d5fbc and release0670d171 preserve the actual child retained handle/exit/drained pipes, source19 and13 retention pairs; all seven host transports finalized. No probe compilation or hardware occurred. Fresh helper02 parenthesizes all response-array scalar concatenations and asserts exact cardinality/paths; builder e7659b95 and preparedad99c2e6 retain identical19-source archive04e350c5, manifestaf1920f1, strict flags,12482-check oracle and eight60-second native children. Root review PASS and explicit e33ea960 authorization hand exclusive target CPU to DX11 for02; root and peers stay local until collection/transfer/release.

Fresh GitHub API snapshot root-ci-current-state-da0a4ed-01 confirms publishedb6 remains latest failed37593963656, precedingaffe37580729947 succeeds. Four unused workflows remain disabled_manually; sole automatic native UMD workflow and manual-only controls retain. Fresh-CI collector now checks exact six job names/five original ZIPs, all three generated private-loader configurations/source11, actual28 source invocation receipts and original DLL CodeView GUID+Age against the PDB. Actual historical three architecture pairs and swapped-original PDB rejection pass; Optional raw-CR ARM64 Hash/Path robustness and duplicate-row checks added; the original actual read_text path already normalized CRLF and parsed17 historical rows. Independent collector review4b8cb612 PASS; no fresh candidate has been accepted or hardware run.


## 2026-10-07 native compute probe publication gate complete

Exact23d native helper02 PASS and explicit release0a39d778 finalize all seven host transports0 and all eight native retained/exited/drained children0. Original archive4f495a3eb926da4bf2dcf055d3bd043542d72742ab48234737499fb51cf48c10 has1130775bytes/89files. Agent original review4ee76335 joins19Git/source/beforeafter/collected inputs,21SDK/7libraries/4tools against original Microsoft packages and all13 retention flags. Root independent413315b7 reopens all89 originals, current19 inputs, actual raw process exits/pipe lengths,2AA64COFF+2PE, required modernadapter/KMT/D3DCompile imports and CPUoracle12482 output. Strict /W4 /WX /MT /O1 /std:c++17 /EHsc /Zc:preprocessor retained; actual optimized compute workload remains present. Probe/core/GPU unexecuted, ordinary runtime admission and future hardware pins remain unset.

Root-publication-native-gates-da0a4ed-01.json verifies current88 WARP compilation inputs unchanged and78/79 SM4.1 source inputs unchanged; the sole difference is Meson metadata for the new separate probe, with all compiled SM4.1 units/oracles unchanged. Full native WARP15 and SM4.1 57program/18draw/4608pixel/3negative proofs remain accepted. Both final CI scripts still match nativePS5.1-tested55a23df/cd3914. Root commits this checkpoint and publishes one consolidated candidate; actual successful six-job CI, original five archives/private loader joins and target GPU/runtime acceptance follow. Target CPU is released to root; DX11 prepares the USER compute GPU runner locally and DX10 finalizes isolated typed10 probe locally.


## 2026-10-07 single consolidated candidate published

Root commits checkpoint930d7f7552d1a087ab12e6a2a3756e9556e2398a and pushes the existing work/native-dxgi-rotation-20261005 branch, actual push0 with preserved raw receipt. No manual workflow dispatch. Fresh GitHub exact-commit list confirms exactly one push-triggered native UMD run37630945784 (2026-10-07T13:44:41Z); shader-cpu succeeds, other backend/identity/runtime jobs pending. Four unused workflows remain disabled. Actual successful CI/current artifact/private loader/hardware pins are still pending; root retains exclusive target CPU.

DX10 isolated6b4cad7 followsf88 with named atomic SDK version constants because the SDK macro does not parenthesize its argument. Original local proof53c3c9b3/archive5c11e6bb retain15 Git inputs, original four optimized x64/x86 COFFs,4141 sanitizer oracle checks and unchanged peer transport33bec601/common implementation/completed57/18/4608 suite. Independent review continues. Root catches scalar-array response-file grouping in its prepared helper02 before any target execution; owner prepares helper03 with explicit scalar grouping/path cardinality checks. DX11 prepares interactive USER compute runner and independent384-word raw validator locally; both peers keep future core/run/hash pins unset until successful original CI artifacts are verified.

###930d7f7 CI failure and corrective work

CI37630945784 was the sole automatic push run. Identity/shader-cpu pass;
ARM64/x86 raw workflow blob identity rejects before Meson. x64 builds core
and passes26 owned native fixtures, then D3D9 backend rejection process
exits0xc0000005 with retained handle/exit/raw pipe drain. ARM64 runtime is
skipped. Original API state, all3 job logs, contracts ZIP13da0c3b and failed
x64 ZIP55fe9cf7 (47179bytes/92members) are retained under central
root-consolidated-930d7f7-01. Root original review9ba39f68 joins original
API source/run/digest/size/CRC plus all27 process receipts and raw streams.
No successful candidate or new hardware pin follows this failed run.

Root fixes null LibraryLoader::sym before LibraryFn member initializers,
retaining strict module-local private loader/no fallback policy. Actual
GCC/Clang sanitizer original-source negative controls each reproduce the
null call; fixed originals pass16checks. Full native identity PASS under
root-ci-repair-loader-02. Initial attempt01 retained19 pre-existing Unix
Win32 compatibility unused-parameter errors; only this diagnostic is
suppressed for the host regression, Windows warning policies unchanged.

Independent Git2.43/non-racy and official Git-for-Windows2.55 source
confirm stat-clean checkout-index returns before testing force; initial
racy control rewritesLF and remains separately retained. Explicit raw
ls-tree/cat-file helper bypasses this cache and conversion/archive attrs;
source12 includes this helper and existing byte/hash checks remain strict.
Independent raw-source helper proofc1923023 PASS:835 actual root regular
blobs plus synthetic export-subst, three text/auto/export-attribute controls,
12strictGitchecks/fivegitlinks retained/wrongCI-SHA-cwd/mutation rejection.
One corrected consolidated push follows; genuinehardware pins stay null.

Root reviewed DX10 helper03 source15/archive and9 prepared inputs,
root handoff9b02834e. Before any target call DX10 found undefined service
variables under StrictMode; root held the known-defective invocation and
requests fresh04/local audit rather than a knowingly failed native run.
DX10 still exclusively reserves CPU; DX11 GPU runner remains local-only.

Corrective42cf765c63e300b8ec058676e4243408a89741d6 is committed/pushedactual0.
Exactly one automaticCI37635552675 starts; no manual dispatch. Allfour
duplicate workflows verifieddisabled_manually. Seven committedGitinputs
join independentcanonical proofc1923023; currentnativecompiled joins are
WARP88,SM41 78/79onlymesonmetadata,compute18/19onlybuildPSmetadata.
No successful artifact/core/hardware pins are set yet.

### Target05 native CPU progress

DX10 exact04preflightfailure finalized: original40files/861973bytes
archive302a2574, threeownedcollector sidecars, source15/SDK23/libs7/tools4,
all8 hosttransportsactualexit/rawcapture and oneownedtar exit0/drain.
Release9d46c8f0 returns target; no compiler/fixture/COFF/PE/core/GPU.
Root verifies originalarchive digest/size/count. Source-order implications
from04emptystdout remain distinct from observed final preflight PASS.
Fresh05helper adds explicit System.ServiceProcess assembly load and actual
resolvedtype/fullname/location/file evidence; source15/flags/RSP/oracle4141/
CLI5unchanged. Rootdirect05handoffb4b733a8 hashes9prepared inputs/narrow
04-to05diff. Native05NoProfilePS5.1.26100.1591 ARM64 PASS fouroriginalprefix
markers plusJSON: parse2/errors0, RSP16/8/16/3/flags14, rawd8cf Add-Type,
resolvedServiceController assembly133960/SHA2b349085/actualRunning4.
SSH actual0/drain and priorSYS/binding/DWM1864/Explorer4464 preserved.
That strict native compiler attempt now fails with five C4310 warnings from
unmasked full64 D3D10 supported-version constants narrowed to UINT, promoted
to C2220 by unchanged /WX. No COFF/PE/oracle/CLI/core/GPU was reached.
Original57-file archivec1b09fa3/893682bytes, proof58c27a2a and releasef9ba9e4b
are finalized: extract/compiler/collector handles exited0/2/0 and drained,
eight host transports finalized, source15/SDK23/libs7/tools4 and13retention
flags joined, SYS/service/binding/DWM1864/Explorer4464 unchanged. Root reopens
original archive/digests/count; source05ffdc2 adds explicit low32 masking
locally without changing full64 matching or warning flags.

Fresh42CI completes identity/shader and all three backend builds PASS.
ARM64 runtime fails after typed D3D11 fixture exit0: original stdout includes
the expanded mandatory SM5 graphics/queries/packed IA/streams/tessellation/
classes phrase absent from the old harness regex. Exception also appends
intentional malformed-operand stderr as context; stderr is not the rejection
predicate. Root corrects the precise full success line, retaining real exit
and checks; independent original matcher/all28-case audit is ongoing. All5
original API ZIPs and six raw job logs are being preserved separately from
the unchanged six-success-job candidate gate. DX11 owns a short CPU-only
native helper ParseFile6/Add-Type slot. No new core/hardware acceptance.

Independent marker reviewa754d516 joins original42 producer/decoder/Git
inputs and candidate scriptfa43550a. Four actual-marker/LF/CRLF/multiline
positives and18 invalid-line/feature/count/exit controls PASS in the shared
Python regex syntax; native PS execution remains future CI. Exact hull
malformed18-case source deliberately logs extended operand kind0 while
requiring E_INVALIDARG, untouched storage/one callback and unchanged bound
shader. All28 corrected patterns match original successful x64 and x86
raw job output (56 matches), proofd3ad8439, with each actual gh transport
exit0 and raw bytes/hash rejoined. Only the old D3D11 expression is stale.
Root prepares one precise marker-fix push; fixture/decoder/d8cf/core source,
all other27 expressions and exit/deadline controls remain unchanged.
Original5 retention01 kept two complete tiny ZIPs but three large ZIPs timed
out at120s and remain excluded. Fresh02 first retains all6 rawjoblogs then
retries only those3 readonly downloads with300s transport bounds; tiny
originals are byte-copied with original API/exit0 receipts, never relabeled.

### Corrected marker revision and native06

Root commit/push e7811754fc0c9640f57d0bb36843ebb2c35d0017 actual0 changes
one ARM runtime marker plus three plans. Exactly one automatic push run
37640080344 starts; identity/shader PASS, backend3 running. No manual dispatch
or duplicate workflow activation. Original42 retention02 completes actual0,
receipt185f39c4; all5 API ZIP size/digest/CRC and six raw logs retained.
Independent failed-backend proof8599d2e8 PASS source835 per arch/12 exact
configuration files beforeafter/generated5/private3/three DLL-PDB pairs,
all28 corrected x64/x86 artifact markers and56 handled/exited/drained bounded
receipts. Partial native ARM has eight output pairs, seven accepted original
binary hashes; later20 unrun. Whole CI42/candidate/hardware remains failure.

DX11 frozen helper02 actual native ARM64 PS5.1.26100.1591 ParseFile6/errors0
and unchanged d8cf Add-Type PASS. Archivea3504520/70016bytes/23files,
review1f25602c and release628cd196 retain15source/10prepared joins and three
host transports exited0/closed. No helper body/core/GPU/task/workload ran.
Root handed CPU to DX10 after frozen06 direct proof623f066b joins9prepared
files/source15/Git/common10, source05ffdc2-only mask correction and4COFF/
4141GCC-Clang/2positive4negative original controls. Local442-file archive
08694e15 and native05 failure originals remain separate. Actual06 native
preflight passes parsers2/RSP14-16-8-16-3/d8cf/live serviceRunning4;
strict ARM64 compile/link/oracle4141/CLI5 once is running. No hardware pins.

### ARM64 actual timeout, bounded runner migration and native07

Marker-corrected e781 CI37640080344 finishes five jobs SUCCESS and actual
ARM64 runtime failure. TypedSM5 fixture exits0 and marker passes; shader
fixture20 times out30s after19 accepted cases. Original ZIP379a9e72 has41
members/20 output pairs/19 accepted hashes. Shader stdout retains21FXC
programs/five completed scenes; stderr ends at SetViewports checkpoint for
ps_lod. Independentc4ba9a0e proves exact original raw bytes and123 normalized
stderr prefix lines/26 stdout prefix lines match accepted native d2f
7483checks/18draws/4608pixels in0.2271555s. All boundary source bytes match;
no concrete RuntimeService race, checkpoint return or backend entry/cause
is established. Do not extend the functional deadline or accept this run.

Root changes only ARM64 harness and its JSON upload: exact d8cf raw owned
handle/concurrent BaseStream drains, per-case original receipt before gates,
unchanged28case expressions/30s/exit0/PE/source/hash/CWD. Native PS5.1 AST
and unchanged C# Add-Type PASS with original19-file archive4322f363,
root9b313596; extract/audit/tar children each retained/exited/drained0 and
all3 host transports actual0/finalized. Main harness/fixtures/GPU unrun in
that audit. Local independent source/mock controls cbe55ab8 retain1positive/
11negative contract cases; source/current-root script/workflow byte joined.
All production, decoder, fixture and raw C# source remain unchanged.

DX10 native06 source05ff actual strict compile/link2COFF2PE and CLI5 PASS,
then whole import stdout filename d3d10 caused helper false positive before
CPU oracle. Full partial originals finalized/released. Frozen07 parser
reads exact DLL module lines in one unique import/summary section; ignores
filename header, rejects forbidden/unknown/empty/ambiguous imports and
requires only KERNEL32.dll. Actual native2parse/2positive14negative PASS;
strict nativeARM64 build warnings/errors0 and actual4141 CPU oracle/CLI5
PASS. Archivea72f56c9/1244494bytes/98members, agent811332a4/root2c4518cd
rejoin all98original byte rows/source15Git;14owned native stages/8host
transports finalized, all14retention true, SYS/service/binding/DWM/Explorer
unchanged. Release560c9fed returns exclusive target CPU to root. Hardware
10_0/10_1 helpers are now local preparation, with all real core/CI/LUIDpins
unset and a pending original BCrypt dependency.

Five-System32 original copies gdi32/kernel32/d3dcompiler47/advapi32/cfgmgr32
PASS nativeAA64/PE32+, compiler4b68 unchanged, beforeafter exact. Archive
1206cc2b/4931940bytes/15files, agent577ea178/root0dfdc7be; three host
transports and ownedtar exit0/drained, release83a7bd. Matched Mesa8443 ICD
original11408122116 and loader11408838487 genuine API ZIPs retained, but
new unsigned loader3001217 does not equal historical signed51d9. Historical
parent archives11358925550/11357811721 now return actual NotFound. Direct
Mesa3e50 artifact11348022580 remains available: ZIPebc42f59/28299564,
unsigned loader3665eeb3 matches only signed-loader diagnostic prefix after
checksum/security normalization. Original bytes never transformed or
relabelled. Peer prepares fresh truthful process-local Mesa8443 runtime03
with genuine unsigned3001, separate actual loader/ICD runs and mandatory
new native validation; frozen47 helper02 and mixed receipts remain intact.

Current e781 retention01 all6logs/tiny2ZIPs complete; three large original
API downloads reach24428544/22937600/25444352 bytes at300s, incomplete and
excluded. Fresh readonly retention02 has450s host transport bounds and
reuses only exact complete tiny originals with API digest and actualexit0
receipts. Native functional deadlines remain30s. Whole six-job acceptance,
current GPU candidate, public runtime admission and hardware remain false.


### Published ARM raw capture revision and exact CI executable replay

Root committed/pushed793b7ca9c9861021480972c38d362a0d80313285 actual0,
exactly5files (two CI harness/upload files and three plans). One automatic
push run37644615059; no manual dispatch; four duplicate workflows remain
disabled_manually. Fresh identity/shader CPU SUCCESS; backend3/native pending.
Bound strict collector03 cb6b0ba0 and independent success verifier54fc92c7
pin exact793/run37644615059, preserve835Git/12config/generated5/private3/
PE-PDB3/56x64-x86 child markers and add87-member ARM runtime originals with
28actual retained/exited/drained children and exact d8cf/compiled receipt.
Offline2f6a549a controls pass29ARM/10metadata/3CLI rejects; actual CI not yet
accepted. Historical collector02/e781 verifier remain immutable.

Prior e781 complete retention02 actual0 retains all5 genuine API archives
and6raw job logs, receiptd17e188d. Independent failed-run revieweebedad8
rejoins canonical835 per arch/config12/generated5/private3/PE-PDB3,
108PE machines,56x64-x86 owned children/markers and ARM19accepted hashes/
20output pairs. All whole-CI/current-candidate/hardware acceptance false.
ARM cross shader actualMSVC14.51.36231 HostX64/ARM64 uses /O2 /MT /W3;
older accepted native used14.44 /O1 /MT /W4 /WX. CRT policy is not different.

Root runs EXACT original e781 crosscompiled shader EXE831488bytes SHA9f258095
under nativeSystem32 ARM64PS5.1/unchanged d8cf/30000ms, original actual
exit0/exited/pipesdrained in0.2437476s:7483checks/57FXC/18draw/4608pixels/
3expected callbacks PASS. Original126-file archive32a1d3ba/425820bytes and
root proof6a558e64 rejoin114 DXBC/token files plus HLSL to accepted native
d2f originals. SYSd48/ServiceRunning/DWM1864/Explorer4464 and five System32
library metadata remain exactly unchanged. Two native children (fixture,
tar) and3host transports exit0/finalized. Old CI timeout cause remains
unproven; no CI-success/current-core/real-hardware acceptance is inferred.
Root first offline regex review omitted CRLF end handling, exit1 preserved;
corrected review02 only, no original mutation or second native invocation.

Read-only original System32 BCrypt copy collected in that same packet:
264616bytes/AA64, SHA791feefce0d157b0751a0d81f1a717df775d63d29d930b6b0bfa7d5b5c183182,
exact native before/after path/version/hash and tar byte joins. DX10 hardware
helper can now bind this genuine dependency while future core/CI/LUID pins
stay null. Root retains targetCPU; DX10/11 local helpers/provenance work only.


### ARM runtime checkout byte correction after793 CI failure

Exact793b7ca9/run37644615059 completes with five successful jobs and ARM
runtime112879776058 failure before any fixture starts. Original API runtime
ZIP11493618960/1509bytes/SHA748400ab3acc46f8a855d3e857dfcdb0c9a2fc7d2329de357809eaf74b28619c
contains only the retained raw runner. Actual original4798bytes/SHA46299450
is exactly Git4697bytes/d8cf with101 LF expanded to CRLF. Script31cf line20
correctly rejects that byte mismatch before runner receipt/Add-Type/loop;
zero fixture child executions, not a new shader timeout. Root byte proof
root-raw-runner-checkout-failure-verified-01.json and independent agent review
join raw archive/log/Git bytes. Exact checkout configuration causing newline
conversion is not retained; no production or timeout-causality claim follows.

The runtime job now reuses unchanged scripts/restore-native-ci-source.py,
checks its actual exit, and copies its original835-row canonical receipt into
candidate/arm64-native-canonical-source.json before fixtures. Existing upload
patterns retain that JSON inside the same artifact root; accepted runtime
shape increases87 to88 original members. Original ARM script31cf/C#d8cf,
28 ordered markers, PE/source/exit0/30s/CWD gates and all compiled sources
remain unchanged. The actual runner image20260924.168.1 official software
manifest includes Python3.13.15; no extra dependency action is introduced.
All5 failed ZIPs/6 job logs are being retained under root-consolidated-arm-raw-01/
failure-originals-02 using450s read-only host downloads; runtime deadlines
are unchanged. Frozen successful collector03/verifier54fc remain unexecuted;
a fresh88-member collector/verifier will bind the next exact source/run.
Hardware core/CI/source/hash/LUID pins remain null. Root owns target CPU;
DX10/11 finish local USER runner packets. Administrator management is allowed,
and graphics acceptance continues in the established limited USER desktop.


### Published canonical ARM runtime checkout correction

Root committed/pushed d7e5c7d46b8ce889e993bfab66a3b78b076c49d1 with
actual commit/push0 and clean worktree at publication. Exact four files are
workflow plus three authoritative plans; code delta is only six runtime-job
lines. Root narrow review and independent be21e650 pin workflow86deaa0d and
unchanged31cf/d8cf/1c7 sources,835 regular Git inputs,28fixtures/30s gates.
Exact raw REST list confirms one push event/run37648387721 for the new source
and sole build-native-umd workflow; no manual dispatch. Current run is pending;
no CI-success/core/hardware admission. Fresh88-member originals tooling binds
this source/run while frozen793 tools are preserved. Failed793 originals
retention continues separately. Root owns target; DX10/11 local USER packets.

### 2026-10-08: successful d7e5 exact CI and native USER runner readiness

Current d7e5/source CI37648387721 completes all6SUCCESS; root collector04 actual0
retains all5 original API ZIPs/six logs/three architecture canonical835/config12/
generated5/private3/CodeView-PDB identities and56 nonARM+28 ARM owned fixtures.
Runtime88 includes exact835-row canonical source receipt. Independent a780 ACTUAL
PID2347144 exit0 in3.146s, empty stderr; prooffcc99f9a and actual-process00e08c82
join all474 unchanged original CI files. Root collectorproof08d3fccd/admission217ee6e9
and fresh ARM64 candidatearchive50224855 authorize original candidate preparation
only; GPU and ordinary admission remain false. One push workflow remains enabled.
DX10 nativePS8 original archivea369745f/111 members: source15/common10, native5/
host7 exit0/drained, pending-readyfalse rejection, d8cf Add-Type and serviceRunning4
PASS; installed SYSd48/411040 and DWM1864/Explorer4464 remained unchanged. Root
original rejoin804357a2 PASS. DX11 native helper03 Parse11/Add-Type2 original
archivec11a6069/39 members/source21: native4/host3 actual0/drained, review1ae91827
and release9ebd27ef return target to root. Neither parser executes core or GPU.
A real ready-path incompatibility exists in frozen DX10 inputs schema versus
template/reader. Agent preserves frozen originals and narrowly corrects that helper
string in a fresh packet, without production changes or a needless new CI push.
Root stages fresh read-only limited USER/session1 identity capture with native30s
child/KMT desktop names/owned adapter/DC cleanup/SYS/service/desktop snapshots.
Runtime03 binding and actual process-local private module-load check are next;
DX8 ordinary runtime preparation continues locally. Hardware pins await originals.

### Current USER identity gate finalized

Fresh native limited USER read-only KMT identity PASS: original19-file archive,
six input bytes/sourcebeforeafter, native child PID9444/handle2312 exit0/drained
in1.069s and native tar collector8736 exit0. Five owned SSH/SCP phases exit0.
Actual USER SID1000/session1/non-elevated/elevation3/integrity8192 selects
LUIDec6b000000000000/source0, three original524-byte UMD-name queries with
zero open/query/close statuses and released desktopDC. Actual SYSd48/411040,
oem17/58624/PnPStarted/serviceRunning and Explorer4464 are unchanged. Limited
USER cannot read DWMStartTime (originalnull retained); separate management-after
original confirms DWM1864 and its prior exact start time unchanged. Root proof
eed7b7ce joins raw originals/selected identity18d461fc. Root owns target; DX11
locally binds all47 roles for the upcoming separately scoped module-load attempt.
Native ScheduledTasks omits optional default RunLevel in exported XML despite
explicit Limited and actual token3. Narrow follow-on offline readers accept that
valid default, reject explicitHighest/empty/duplicate, retain real token guards.
Official Microsoft task schema/security-context docs corroborate optional/default
low privileges; frozen readers and native helpers remain preserved. Root's first
offline comparisons assumed literal XML/default and observable limited-USER DWM
starttime; only review adapters changed, not native inputs or results.

### Runtime03 CPU-load originals fully bound for the first live attempt

Agent binds accepted d7/CI37648387721/artifact11495898065 with actual identity
18d461fc/LUIDec6b/source0 and all47 original roles. Readyfalse preview manifest
9227bc7b preserves unchanged native helpers; source19/config12/PE/PDB/original
archives/genuine unsigned3001 loader/1b0c ICD/5843 z reader PASS. Root invokes
the reader offline using only an ephemeral ready copy, rejoins all47 bytes and
19 raw Git blobs, proof42aa22f6 authorizes a fresh ready CPU manifest. No module
or GPU has run yet. Agent freezes d8cf-owned Prepare/Start/Result/cleanup and
collection wrapper before the separate root exclusive CPU-load handoff.
Narrow offline reader1c9c847e differs from frozen8bead only in optional default
RunLevel handling; wrapper9a912050 imports unchanged501/c239. Original controls
8e87a7ee: full original13125 self-test plus32 targeted checks pass, highest/empty/
duplicate and actual token mismatches still reject; original PT1M task remains
rejected by the compute PT2M deadline. All nativePS/C# inputs remain exact.
DX10 corrected schema/reader fresh preparation binds67/72 actualroles; fresh
160-byte privatecapability reply and runtimeCPUload originals remain pending.
A source-only owned30s read-only capability capture helper is being frozen;
it may not invent generation/WDDM/software/render fields from old data.
DX8 fresh isolatedd7 checkout prepares a separately gated I386 API8 device
forwarding frontend and real SysWOW64 D3D8 hardware workload. Strict I386
frontend local compile passes; actual WOW KMT slot/native build/device/render
readback and presentation are pending. Old readonly API8/CAPS12 remain preserved.

### 2026-10-08: native runtime helper corrections and continued porting

CPU05 retains genuine failed Prepare/diagnostic originals and release700f0183.
Actual nativePS5.1 wrapped genuine12-row CI JSON in an extra array; source data
remains correct. Fresh CPU06 fixes only explicit object-array casts/scalar guards
and early Prepare error output, preserving all47 original payloads/ready0f3/currentd7.
Root direct reviewae2ed2f7 rejoins25 refs/14 tar members/seven unchanged helpers.
Actual native source/config preflight child8380 exits0/drained and sees12/12rows.
Prepare/Start pass; limitedUSER12176/session1/token3/8192 fails in controller Run
before module probe because typed string Authorization collides with local JSON
variable. Original51-file archive89e89c53/review54564b56 and releasec150fd97 retain
failure/9native children/8host transports and removed task. No module/GPU acceptance.
DX11 prepares narrow fresh07 rename locally.

DX8 actual EWDK parser passes; native build stops before compilation on copied
libcpmtd.lib original pin. Read-only comparison3ff02bbf retains both genuine
18,463,504-byte originals: exactly one flipped bit at offset10770273, owned018f4c11
versus mounted14.50.35717 original48108e70. Other574 tool rows match original575
and before/after are stable. Failure46-file archive87203502 is preserved, no copy
or retry yet, explicit releaseb6bb221e. Agent prepares one original-library repair
and fresh02 build locally. User permits passwordless Administrator for management.

Root null-GS SO worktree reference/codes/dxvk-umd-null-gs-stream-output-20261008
implements bounded D3D10 passthrough via real prior VS DXBC and exact rebinding,
with three72-word raw-bit draws/canaries/missing-output rejection added to fixture.
Strict originalMS SDK/WDK/MSVC-header x64/x86 production+fixture4COFFs pass actual0
with zero diagnostics in local-strict-coff-02 (joins unchanged production01).
Native WARP/readback and DX11 null-GS remain pending; ordinary masks stay closed.
DX10 isolated Texture3D source port reports six original-header COFFs pass.

Root private160 query-only directreview38fea902 rejoins57 raw prepared members/
56source pairs/38 native payload members/actual100 local controls. It uses fresh
separate180s InteractiveLimited USER task and three read-only adapter queries
(4/160/4bytes), with no core/device/context/resources/GPU. Authorizationb6fc9d1f
hands target exclusively to DX10 after CPU06release; actual native queries pending.
Other agents and root remain target-idle during this owned attempt.

### CPU07 acceptance and merged SO/volume source, 2026-10-08

CPU07 limited USER private module load now PASS: root direct4740d980 rejoins
all119 original members/archivebc47fc02, actual PID11724/handle2324/exit0/drained
22.85s, six guarded HRESULTs, four owned module loads/reverse releases/absence,
all10 native children finalized and exact task removed. Explicit releasea1b5bc1c
returns target. Original180s SCP-9/50MBpartial preserved; separate complete
original transfer byte-joins native archive but interrupted host exit is unknown.
Zero VK/KMT/D3D/GPU counters describe explicit probe scope, not DllMain instrumentation.
No hardware acceptance or GPU-ready manifest follows from CPU loading.

Root reviewed frozen EWDK single-library recoveryf8ee4420 and authorizesa72350e3
after CPU07release. Actual repair575 PASS: original-readyc633 unchanged,
drifted018f backup and same-version mounted/staged48108e retained/rejoined.
Fresh unchanged33-source DX8 CPU02 native build fails probe line428 C3493
(color lambda capture), after genuine frontend I386 COFF/DLL success. Original
81-file/23582715-byte archive7d2ba781 and all raw failure receipts preserved.
Agent finalizes release then fixes exact compiler issue locally; repair is not repeated.

DX10 private16001 failed actual native PS5.1 DSC parse before Initialize/task/query;
original58-file archive56745a85/review63dd22f0/release4f6f5e94 preserved. Fresh02
Get-Private160Configuration and owned PreInitialize native AST/Add-Type gate are
root-reviewed073258eb:53 local refs/38 payload/48 fullpacket/37 master rows,
only3 changed control helpers/134 actual local checks exit0. Actual native prefix
and4/160/4 queries remain pending; target goes DX10 next after EWDK release.

Null-GS SO production commit21cdb35 now supports actual prior legacyVS and SM5
VS/DS bytecode with atomic native linking, genuine cache rebinding, retirement
owner and strict nonnullS_OK. Final current originalMS x86/x64 eight COFFs PASS
local-strict-coff-05. Fixtures add72-word raw/canary controls and1024-word public
DS comparison; native run pending. Texture3D agentcommit56d44ba adds bounded
10/10.1/11 resource/view/Map/XYZ transfer/mip behavior, local385547 policy and
six originalMS COFFs PASS, native27cases/9138voxels/945pixels still unrun.

Both commits cleanly integrate in isolatedworktree
reference/codes/dxvk-umd-so-volume-integration-20261008 HEADd821fc0; main/current
acceptedCI remain exactd7/all6success, no newpush. Root freezes combined93 rawGit
inputs and4 nativefixtures under artifacts/dxvk-so-volume-integration-20261008/
native-warp-d821fc0-01, descriptorae941b02/sourcearchivec02bd21d. Actual native
AST/build/30s fixture/309volume originals/readback are pending after private query.
Public admission masks remain unchanged. Real384 compute words/dual1792graphics/
ordinaryDX8-DX11 rendering and Present/DWM still required before completion.

### Actual adapter query, combined native failure and next target owner

ROOT private16002 directreviewbfb541eb hashes all134archive files/133collector rows/37sources+master/17native+15host exits and18raw queries/canaries. Genuine LimitedUSER7580/session1/8192 opens/closes ownedadapter/DC, queries4/160/4 pass and report LUIDec6b/source0/gen2/caps0/WDDM2200/rendertrue/softwarefalse. Fullnativeattempt remains failed Result1/Finalize1: liveNative telemetry44management/36limited changed, while SYS/binding/package/registration/service/desktop equal. No originals rewritten, query-only acceptance does not imply hardware.

Actuald821 combinednative packet passes nativePS5.1 AST/Add-Type,15production/dependency units and4ARM64fixture builds. LegacySO1252/SM5nullGS4193/domain1024 andvolume-policy385547 pass actualownedexit0. Texture3D profile0 initial/XYZ/dynamic/W-slice cases pass, but scopedmip readback6/mip3 exactword comparison fails; originalownedexit-1073741819 retained. First17savedrecords independently calculate1875matchingwords; failingword was not saved by originalCHECK-before-write. Agent prepares GPU-only scoped3D normalization and fail-first original retention, expected27/9138/945 unchanged.

All245archive originalsaf6b6e8c and93Git source originals join root00e716dd. All13native/9host children finalized, source/tool/21SDK/7libraries/5system/candidates/SYS/binding/desktop unchanged, no task created. ExactCIvolume additions(twofixtures/309originalretention) parse nativePS5.1 errors0, sourcecopies unchanged; noCItest execution or push. Rootreleased63ee07a toEWDK withfresh03authorization7e8a5187. Corrected171 probe nowcompiles, latercallback CPP strictMSVC fails; originals collecting, no repairreplay/validruntime/GPU. Root/DX10/DX11 target-idle.

DX11 locallybinds actualprivate160/rootproof plusCPU07/currentd7/Mesa47members to readyfalsemanifest52078756; real384words remains nexthardwaregate. GitHub run37648387721 exactd7 all6success andfourduplicateworkflows disabled verifiedlive. OrdinaryDX8-DX11 rendering/Present/DWM acceptance remains pending.

### GPU08 exclusive handoff and reviewed corrected source packets

ROOT ready GPU282df90a changes only readyfalse→true in boundmanifest52078756.
Rootf62421ef independently joins47 actualCPU07 payloadfiles/19rawGit/16genuine
ZIPmembers and5ARM64PEs; CPU07/rootprivate-query scope stays distinct from
actualGPU output. Frozen d312 hostrecipe/native8/helpers are reviewedc3796bb1
and authorized09f12ba9 against latestDX8CPU03releasebd943617/rootreviewe4e9b08c.
Native GPU08 prefix actualexit0 now parses7/helpers47/config12/12 and verifies
source/inputs unchanged; originalnativeowner PID11268/child9828/handle2532/exit0
drains1.172s. Separate LimitedUSER Prepare/Start/Result/Collect remain owned
byDX11; ROOT and peers target-idle.

DX8CPU03 failed actualC2872 ambiguouslog in callback fixture after4I386COFF/
3PE builds;107files/archive500b88c5 and13native+collector finalized, lastSCP255
retained and onlymissing-sidecars recovered by freshactual0. Roote4e9b08c
independently joins originals/closure/failure and SYS/stateunchanged. Exactfive
callbackTrace renamecommit6ae2f61 passesfocusedlocal609originalinputs/I386
compile-link; frozenCPU04 ec0967ba/archive73058ed0 rootprepared6eecd850 joins
33/30Git/3licensed and4unchangednativehelpers; actualCPU04 pending,no repairreplay.
PhasedgenuineI386 names/12CAPS/448offscreen/64screen preparationb21f724 remains
readyfalse pending actualnativeCPU04 outputpins; no targetcalls/installation.

Volume terminalmip proposedcorrection27df15d integrates2489b0f abovefa66393
CI changes; production normalized3D scratchchain and fail-first original
retention add108public observations withoutchanging27/9138/945 or309volume
originals. BothmergedcommonDDI originalMSx86/x64COFFs localstrict02pass0/
zerodiagnostics; rootprepared47f07bfe freezes93Git/newarchiveb1fb20a3/
descriptor68018da4. Actualcorrectednative02 pending afterGPUrelease; no newpush.


## 2026-10-08 actual real GPU08 compute accepted; corrected volume02 owns target

GPU08 originals archive93c1d6f8 (85,340,690 bytes/126files) is fully joined. Root
`review-gpu08-originals-root.py` independently checks all384 rawuints against
workgroup arithmetic and original384 trace lines; payload47 and Git19 match.
Actual limitedUSER/session1 controller11724 owns nativeprobe12144/handle2440,
exit0/drained24.363s. Callback tuple[108,1,1,6,6,5,5,1,24,6,6] is balanced, all
wrong-thread/cookie/error/leak/paging counters0. Native9/host9 finalizedactual0;
task absent. Root actual proof and release review are in
`artifacts/dxvk-native-dx10-dx11-20261007/guest-compute-gpu-d7e5c7d-08/`.
Ordinary-runtime admission and presentation remainfalse. Previous whole-registry
private16002 attempt remainsfailed; accepted rawquery5add/private rootbfb stays
query-only evidence. Fresh native02 descriptor68018da4/source2489b0f is authorized
against GPUrelease2a53b6b4; ROOT preflight running. No driver/registry/VM changes.


ROOT native02 preflight stopped at a PS parser missing newline `)if`; hostupload0/
preflight1 bothfinalized, no script body/source upload/build. Fresh native03 fixes
one LF in preflight, keeps source93/all8 payloads/build/parser/collector/d8cf exact,
and uses new original receipts. NativeARM64 PS5.1 preflight andAST2/Add-Type PASS;
actual strict-MSVC four-fixture build nowrunning. MainCI37648387721 liveall6PASS;
four redundant workflows remain disabled. Targetowner ROOT; peers local-only.


2026-10-08 ROOT actual corrected2489 native03 closed and integrated: archive
4,356,406 SHA d5fc7539/611files,93 rawGitinputs, native12+collector1 andhost7 exit0,
all11retentionpairs equal. FirstpartyW4WXwarnings0; pinneddependencyW3 retains65.
SO1252, typed3733/callback280/nullGS72+domain1024, policy385547, Texture3D12025/
27cases/9138voxels/945sampled; independent production309 wordsPASS, public108
36miprecords mismatchcounts30x0+3x1+3x8. SDKsupplement directraw21headers/2private
libs, all21header+7library receipt valuesmatch; five sharedlibs hashes native.
Main source ff5d77824 bytejoins all firstparty native compiled inputs. Added
volume fixtures to existing single matchingCI, allold28markers/deadlines/d8cf
unchanged. Target release1b1e4942/EWDKauth0129b236; CPU04 actualbuild running,
575 tool originals must remainexact/no repair replay. Ordinary runtime/Present
notaccepted. Consolidated push next; no newcore/probe may inherit d7 acceptance.


2026-10-08 source618ca58aae85c427eb63a4e79ac4a4d43b33043e pushed actual0;
exactly one automaticNativeUMD run37681929181. Identity/shaderCPU currentlyPASS;
threebackend jobs running, no newcandidate admission. Currentoldcore/d7KMT384
remain scopedhistorical. NativeDX8CPU04 actualPASS/root directreview saved:
archive988bdb3e/31,167,409/181files, source33/Git30/licensed3/575toolbeforeafter,
SDK10/libs9,5I386COFF/4PE,329policy/62callbacks+11forwarded/30invalidCLI,
48nativebuild+collector1/host7actual0 allfinalized. Original release1547258b
returns targetROOT; no core/validKMT/factory/GPU. Phasedc2ab manifestcad2 remains
readyfalse, actualprobe9009/front699b joined. FreshnativeAST5/AddType andeach
limitedUSER names→HALenum→448offscreen→64Present runtime phase stillpending.
DX10 USER05 frozeneb5e70/72roles/prefixqueued; DX11 peerpreparesD3D9locked2624
with genuine currentd7 originals. No additionalpush/manualCI/driver/VMchanges.


2026-10-08 continuation: user confirms passwordless Administrator available for
management/builds; GPU checks remain limited USER/session1. All six jobs of
sole automatic618/CI37681929181 now PASS. Fresh ROOT collector06 corrects only
ARM member summary88→511, pins unchanged independent volume reader Git SHA
b0c463995f012c5a9c5cf78cf52b233a8641a8170998fc65f2265f08cca5fb4d,
and starts original collection under host PID2544062/deadline1200s. Frozen05
not modified. DX8 parser-only handoff5319f9c0 is now exclusively owned by EWDK
agent; AST5/Add-Type only, no modules/KMT/GPU, original collector then release.
ROOT independently rejoins DX10 USER05 prepared descriptor dfd16e6a: 20files,
15prefix originals/152frozen archive/151collection/70known72roles/9helpers/
15probeGit+10coreGit; proof root-native-user05-prefix-prepared-direct-review-01
PASS, readyfalse. Names/HAL/offscreen448/Present64, DX10 real1792 and D3D9
real2624 remain pending; no driver or VM changes.


2026-10-08 ROOT CI618 originals admission: actual collector06 PID2544062 exit0,
five genuine originalZIPs/1721members, allsixjobsPASS/source853blobs+5gitlinks,
four exact canonical receipts/three DLL-PDB/config/private-loader chains,
ARM30/x64+x86 60 owned fixture children all0/drained. Independent complete
309production volume files/9138voxels/945sampled +108public files per archPASS.
New ARMcore6262784/a4053e44, x64 5918720/0f68ebcc, x86 5500928/d9536b79; these
are candidate-only and do not inherit d7 hardware acceptance. ROOT direct
review script review-ci618-originals-root.py and originalproofs retained.
DX8 parser actualPASS archive4ae29d77/20files/AST5/AddType; native2/host5 all0,
ROOT directreviewa594fc8c/release6340c17c accepted. DX10 USER05 nativeprefix
actualPASS archive41fb6692/46files/AST8/AddType/ServiceAssembly Running4/
pendingreadyfalse rejection; native7/host8 all0, protectedstatic34/rawSYS/
desktop retained, ROOT directoriginalreview saved, release891d243c accepted.
ROOT names-only admission23d6d374 changes onlyready/native_phase_parse/pending
from frozenDX8cad2; EWDK exclusiveauth995b506a now executes one I386 names
query and collection. No core/factory/GPU/installation permitted. D3D9 frozen
prepareddd53595b independently joins63files/32Git/helperTAR9/inputTAR15 and
acceptedGPU08shared47 exactly; unchanged probe/oracle gives native2624 total/
1152raw lockedpixels and checksums6d390bc5(word)/41e03dc5(byte). ActualD3D9
retry stillpending, readyfalse; target remainsEWDK names-only until release.


2026-10-08 actual I386 names01 FAILED beforeKMT: limited USER/session1 probe
2404/handle2256 exits1/drained0.0447392s, original196-byte stdout contains
USER gate then system-GDI32 guard failure. No adapter/namequery/core/factory.
TAR61c4793c/1961912bytes/38files preserved; ROOT direct failure/continuity joins
all source/input/systemDLL hashes, static34 and desktop; task removed, native2
closed/host7 finalized(result-command1)/outer1, explicitrelease53355f1d accepted.
Microsoft IsWow64Process documents FALSE for32bit apps on64bit ARM; exact
process-machine APIs are required. EWDK peer prepares freshprobe/frontend
IsWow64Process2/GetSystemWow64Directory2 fix and diagnostics locally, without
weakening system-path/I386 guards or changing oldfailure. No enumerate admission.
ROOT integrates fiveexistingDX8 commits into isolated branch
work/dx8-trunk-integration-20261008/head eeebfcf: source29/30 exactlyjoin native
CPU04; onlymeson.build differs by alreadyaccepted newSO/volume registrations.
Mainc903b4c recordsCI618/helperproof locally; no secondpush yet.
D3D9 exclusivehandoff b89230b6/rootreadymanifest508b97c8 authorizes one frozen
d7 typedlockedbuffer gate after release53355f1d. Nativeprefix parse7/config12/
hash63 and Prepare bothactualPASS; current GPU run/collection stillpending.
All ROOT/peer target calls suspended until DX11 explicitrelease. DX10 peer
continues concrete1792 USERpacket71roles, root72 pending.


2026-10-08 ROOT accepted actual D3D9 locked-buffer gate and released target:
raw archive13801813/85936635bytes/143files/199members,63roles/32Git,1152 independently
replayed pixels plus1472 unchanged native asserted pixels; word6d390bc5 and
byte41e03dc5,18stages; callback ownership/residency clean and static34 unchanged.
Actual probe7012/handle2288 exited0/drained2.076s, USERsession1/elevation3/8192.
Ten native and nine host receipts independently joined original streams and
actual exits, task removed, pending0, releaseee3c23b6 accepted. Ordinary runtime
and presentation remain untested; source618 does not inherit d7 GPU proof.
ROOT reviewer source review-d3d9-locked-originals-root.py retains reproducibility.
FrozenDX8CPU05 packet21898/manifest4c155/source5c420e4 directly rejoined33inputs
(30Git+3licensed) and four unchanged acceptedCPU04 helpers. Exclusiveauthc1c1c6f6
now runs one native EWDK build/collection; strict575tool before/after admission,
no repair replay. Fresh names05 remains separate and unexecuted.
ROOT DX10 USER05 review2f406791 independently joins71 original/stagedroles,
25Git/39archive members/80TARfiles and9unchangedhelpers against acceptednative
prefix4c42cd7e. New72-role sealer may prepare the actual1792pixel gate; no
hardware authorization yet. Latest618 local runtime candidate64roles is ready
for fresh CPU prerequisite preparation; newactualcorea405 proof is required.


2026-10-08 native CPU05 process-machine repair verified by ROOT: original181-file
TARc48fbc5d/31169398bytes,source5c420e4/33inputs/30rawGit,exactreadyc633 and575
compiler before/after,10SDK/9libs,5I386COFF/4PE strict W4WX zero warnings,
policy329/callback62+11forwarded/30CLI expected64. Actual48build children plus
collector4412/handle2560 exit0/drained2.071s; sevenhosts/outer exit0/rawfinalized.
ROOT proof042786e0 and separate release review accept release100c118d/pending0.
Newnativeprobe631808/2cef9cfa andfrontend440832/7b4e6b26 are current5c originals;
no validKMT/systemfactory/GPU executed. Isolatedtrunk68f83e6 additionallyintegrates
helper166f identity/trace validation; eight oldphasehelper bytes unchanged,
newnames05 admission stillpending. Target ROOT-idle before DX10 dualhardware
handoff. Live GitHub recheck confirmsCI618 stillsuccess and four redundant
workflowsdisabled; solematching automaticbuild remainsactive.


2026-10-08 trunk pushed once,3d39760 (618→3d39760 actualgitpush0): DX8 seven
native-tested commits merged5b51100 plus27controlfix274f429. DX10 threeprobe
commits integrated363d9cd/09b47d9/3d39760; actualtrunkprobe/oraclethreefiles
exactsource05ff andsharedtransport exacteb3a9d8, portableoracle4141/WerrorPASS.
An offlinejoin used05ff for separatetransport (pathabsent) and failedbefore
writingproof; correctedexplicitprovenance02 passesfourbytejoins. Production
SO/volume/CI618bytes unchanged. New CI on3d is pending; no hardware admission
for newsource. ROOT sealedDX10directreview joins38files/81TAR/72roles; manifest
5d9d198a adds onlyactualrootrole2f406791 to71 unchangedroles. Exclusiveauth
fb1acf22 toDX10d7USER05/10_0+10_1/1792pixels; management USERSSH knownworking,
limited USERsession1tasks unchanged. CPU05release100c ROOTaccepted; allROOT/
EWDK/DX11targetcalls remain suspendeduntil explicitDX10closure/release.

2026-10-08: Trunk3d39760 soleCI37689715890 now allsixSUCCESS; DX11 agent
collects fresh genuineZIP/canonical881+5/source/process/volume originals in
successful-original-ci-3d39760-01. No new workflow dispatch. ROOT separately
accepts DX10failed01 archive101/81sealed/native5/host12 and releasebc6a7b:
management Before/After exactaparttimestamp, protected10/static34 stable;
limitedUSER DWM1864 StartTime null versus management known is retained.
Fresh02 rootreview5280b214 joins82sealed originals/81unchanged old/72roles/
9helpers and separate actualmanagement401bef3d. Auth716e7f79 transfers sole
target to DX10 for unchanged1792pixel dualprofile gate; actualGPU stillpending.

EWDK native05 parser actualPASS:20fileTARdb35d28b, AST5/errors0/AddTyped8cf,
11before/copies/after equal, retained native2/host5 all0/drained. ROOT direct
originalreview accepts release229b3741; names05 fresh03 admission is local.
No systemD3D8 factory/validKMT/core/GPU follows from parser alone.

ROOT isolated cube worktree at reference/codes/dxvk-umd-dx10-cube-resource-20261008
adds legacy six-face resource/SRV/RTV/DSV translation with strict shape/ranges,
all-remaining SRV mips and scopedGenerateMips. New actual productionDDI
reference fixture checks sixface init/update/copy/clear/depth/scopedmips/
atomicfailures; raw168subresources/10380expectedtexels awaitnativeexecution.
CrossCOFF fixture has passed x64/x86/ARM64; nativeMSVC/WARP proof andintegration
remainpending. A Linux productionCOFF attempt hit an existing absentMinGW
D3D11_VS_INPUT_REGISTER_COUNT headerconstant; no productionheaderworkaround.
Publiccontractmask remainsclosed; graphicsacceptance requiresrealviogpu.


2026-10-08 ROOT accepts actual DX10fresh02 realKMT dual profiles: archive
ad75e5ed/184189818bytes/231files/344members, originaloracle807b3bdf,
agentfinalproof7c0a4083 and releasecebe6146 independently joined. ROOTreader
7134c6b3 actual2598164 exits0/drained4.47s; directprooffd8347aa recomputes
all7 literalRGBA images/1792pixels plus raw shader/reflection/token/transport,
source15/currentheaders10 perprofile and native9/host12 retained closures.
10_0 actual10680/handle2668 exits0/24.825s (1draw/512pixels);
10_1 actual13232/handle2588 exits0/8.885s (3draw/1280pixels), stderr6479 each
retained with unchanged renderer-error policy. Both interactive tasks are
limitedUSER/session1/elevatedfalse and removed. Context1/1 each; allocations
7/7 and8/8, locks6/6 and7/7, renders2/5, escapes30/42, residency7/7 and8/8;
wrongthreads/errors/leaks/pendingpaging0. Protected10/static34/SYS/desktop
unchanged;143 raw telemetry/frame differences retained. Oldd7/coreb0fc scope
only, ordinary-runtime/current3d admission closed. The first offline review
lacked local HLSL; actual2595661/2595663 exit1 logs remain, no native retry;
exact13-file closure03 enabled unchanged reader2596673 exit0.

Names05 authorization42bb1b24 (2083bytes) hands target exclusively to EWDK
using unchanged recipea88/descriptor25cc/admitted85e6. One limitedUSER I386
KMTnames query only, no PE/core/factory/GPU staging. ROOT/DX10/DX11 remote
calls suspended until original closure/release. DX10 starts separate typed10.1
cube-array resource slice based791; DX11 modern cube sentinel slice compiled
strict x64/x86 production+fixture. ROOT starts separate typed11 CubeARRAY
scopedGenerateMips source/fixture at reference/codes/dxvk-umd-dx11-cube-array-mips-20261008,
with common mippolicy hunk ownership coordinated. All new cube checkpoints
remain local/nativeproofpending and public CompleteResources staysclosed.


2026-10-08 ROOT accepts names05 failure and readonly DLL release: original
names archive390a9fd7/1961779bytes/38files and native11944/handle2640/exit1/
drained1.823s stdout232 contain only USERidentity and explicit-process-machine-
APIs win32=127. No KMT/machine/directory/factory/core/GPU stage reached. Native
collector11864/0 and sevenhosts closed, taskremoved; ROOTproof9fa594fa accepts
release90252e9d. Subsequent readonly export archive50418477/1462431bytes/16files
joins five frozenhelpers, source/copy/source-after threeDLLs, native12196/0
andtar7700/0/drained, eighthosts closed (two wrapper-only guards retained),
SYS/desktop/static34 unchanged. ROOT boundedPE/APIset reader directly confirms
SysWOW64Kernel32 lacks Directory2W/A, IsWow64Process2 forwards to contract;
KernelBase Directory2W executable20ca80/ordinal834 and IsWow64Process2
executable14a5f0. NativeAPIsetv6/count973 WOW64l1-1-3 routesKernelBase. Direct
proof1cfbc585/2963bytes accepts release772fe285. FirstROOT reader draft assumed
all exports arecode and rejected dataWow64Transition; draft retained and no
proof written, corrected only to requirecode for the requested exact APIs.
Target ROOT-owned idle; EWDK develops narrow same-API resolution under missing
Kernel32 export only, explicit actualI386/pointer4/canonical provider guards,
and fresh nativeCPU source/build packet. No legacy architecture fallback.

Cube slices now committed locally791006c/1f2ffde/7669c3a/b5b863b; ROOT prepares
one combined isolated candidate and all Meson/CI raw readbacks. Typed10.1
resource policy97387 GCC/Clang sanitizer controls and8strictCOFF pass; modern
SRV production/fixture4strictCOFF pass. ROOT CubeARRAYGenMips fixture expects
5cases/450subresources/30690texels/900files, strictx64/x86 production+fixture
PASS and ARM64fixture strictLLVM COFF PASS. NativeMSVC/reference/hardware are
pending; unchangedofficial ARM64intrin.h prefetch incompatibility is retained.
CurrentCI3d37689715890 stillsuccess; liveGitHub confirmsfourredundant workflows
remain disabled and solematchingautomaticbackend active. No newpush/dispatch.


2026-10-08: Combined cube candidate frozen8528d91357255fe8f31138d5438e7313e2367fec
in reference/codes/dxvk-umd-cube-integration-20261008. Four production slices
merged with only Meson insertion conflict (both targets retained). Added modern
SRV target; fivefixtures build/ship/run under unchanged d8cf/30s and independent
four raw oracles on eacharchitecture, ARM64cases35. Resource oracle2e0da92
independently checks180subresources/3540texels/540files; all cube originals total
2086files. Combinedproduction strictactual2623291/x64 and2623292/x86 exit0,
emptydiagnostics/inputsbeforeafterequal; proof5f4e028d/19745bytes. PythonAST/
bashsyntax/35CIregistrations pass; actualnativeMSVC/reference/PS5.1AST pending.
DX10 prepares common160-source/23COFF/8PE packet locally, with oldview/1D/3D
regressions and unchanged immutablevolume readerb0c46399 (public108 observations
retained separately). Frozenold791/1f/7669/b5 packets remain exact. No push.

DX8 provider repairdb0e183 committed/frozen; ROOTdirectpreparedreview6419035c
(2540bytes) joins sourcearchive047375c5/105543bytes/33inputs/30Git+3licensed,
manifestbf1083f1/13614,13pins,886actual localinputscopies/beforeafter/4I386COFF/
2PE/12commands0,16actualsource mockselector cases eachGCC/Clang. Native CPU06
may execute exact Directory2W from alreadyloadedKernelBase only whenKernel32
exporterror127, preserving IsWow64Process2/effectiveI386/pointer4/canonical
providerandowner guards. Originalreadonly release772 andROOTexportproof1cf
accepted. Exclusiveauth5250125b/1840 hands target to EWDK for one fresh native
build/read-only processAPI observation thencollection; expected51nativebuild
children+collector,575tool/SDK11/lib9/provider3 unchanged,32malformedCLI,
policy329/callbackforwarded11. No repair replay/KMT/factory/core/GPU/install/
registry/VM changes. ROOT/DX10/DX11 remote suspended until reviewed release.

ROOT local Microsoft docs/header review establishes separate remaining cube
array RTV/DSV follow-on: D3D11DDIARG_CREATEDEPTHSTENCILVIEW explicitly contains
TexCube and independently preserved read-onlyFlags; RTV/DSV FirstArraySlice/
ArraySize use individualface slices, so validranges may crosscubes. CommonRTV
andlegacyDSV currentlystrictsingle6; modernDSV currently noCUBEbranch. DX11
records isolatedfollow-on, preserving8528. LocalrealKMT SO/volume probe work
continues; expected legitimate error/pending callbacks must be retained and
exactly joined, not reset or suppressed. Default transportbalanced() remains0.

## CPU06 failure release and next native reference gate

ROOT independently accepted CPU06 archive4a153ecf/release831ea2c with proof
a1445895. Strict compilation succeeded; readonly provider guard failed before
KMT. All18 native/10 host owners closed and source/tool/SDK/provider/driver/
desktop state remained exact. Target returned to ROOT. EWDK remains local-only
for mapped-image diagnostics, DX11 for SO/volume probes; DX10 finishes the
combined8528 native packet (160 source inputs,23COFF/8PE,8 WARP references).
This packet also parses the two exact new CI scripts with native PS5.1 before
compilation. Ordinary runtime and real hardware gates remain open.


2026-10-08 current target ownership: EWDK exclusive native mapped-image CPU07.
ROOT prepared proof26c96adb joins15inputs (12Git+3licensed),721actual compiled
inputs,2I386COFF/1PE/emptydiagnostics,3unchanged guard extents and oldfrontend.
Latest combinedcube native01 failed only firstlink LNK1104 uuid.lib after16
AA64COFF; archive253files/d04087fb and releasea5628043 independently accepted
by ROOT775db22e. Freshcube02 keeps source8528 unchanged and adds originalSDK
uuid.lib only; packet remains local pending review. CPU07 authorizes one
readonly observation of4modules/7filehandles and4malformedCLI; no KMT/factory/
core/GPU/repair/install/registry/VM. ROOT/DX10/DX11 remote suspended until
originals and explicit EWDK release reviewed. ROOT cube KMT probe7a47d03 is
committed test-only; local strictx64/x86 and GCC/Clang159292checks/154752bits
PASS, independent reader1positive/7negative PASS. DX10 independently reviews
probe locally. DX11 f96 SO/volume probe nativeCPU packet prepared locally;
separate moderncube RTV/DSV implementation proceeds from frozen8528. No push.


## 2026-10-08 current continuation: cube02 owned native gate

ROOT CPU07 original review9bba9ea8 accepted archivee30e4914/75files and
release591a31a5. Actual strict2I386COFF/1PE,4CLI64,12native and7host+outer
closed,source15/compilerreceipt575/SDK13/libs9/statebeforeafter exact. Three
alreadyloaded I386 system modules report logicalSystem32 but BOTH logical and
Directory2W(I386) SysWOW64 handles resolve to exact mapped NT SyChpe32, same
fileIDs/headers/bytes/hash;7pairedcloses. ARM64 management SysWOW64 originals
remain distinct and are not relabeled. EWDK now LOCAL-only identity repair;
existing guards stayed byte-identical inCPU07 and no runtime was admitted.

ROOT cube02 prepared50b5e5d directly joined160Git+398package member originals,
including original UUID15080838/f0b2efac from unchanged SDKe0d6e5e0. All6
nativehelpers normalize to01 after only freshnames/pins/eighthlibcount; old
7libs/source/oracles/strictflags/static34 unchanged. Exclusiveauth3493b4ac
hands target to DX10 one native02 attempt,23AA64COFF/8PE/8WARP references/
29buildstages/2086cube rawfiles; original perchild exit/drain/state and explicit
release required. ROOT/EWDK/DX11 target calls suspended.

ROOT combined local branch work/dxvk-device-integration-20261008 at9241eca
contains exact8528 production+7a47cubeKMT+f96SO/volumeKMT. Merge conflicts only
Meson/build insertions; every target retained. ARM cases38 include new cube/
SO/volume portable oracles. Actual Meson parser/registration/source-byte joins
PASS proof02808784/1756bytes,14probe/shared/oracle files unchanged. Independent
DX10 review bdf5d045/28767bytes found no blocking defect; original x64/x86 COFFs,
GCC/Clang159292/154752 and753synthetic files independently joined. NativeARM64/
actual shader/core/token/KMT/hardware remain open. DX11 freshCPU02 adds same
UUID after all4original optimized COFFs independently prove DEFAULTLIB; old
CPU01 preserved, compiler provenance explicitly scoped4tools/22headers/8libs.
Local cube-array RTV/DSV follow-on remains separate,4strictCOFF passed.

GitHub live:37689715890 stillsuccess at3d397608; fourredundant workflows remain
disabled_manually and sole matching backend active. No push or dispatch yet.


## 2026-10-08 native cube03 failure, local integration, CPU03 handoff

Native cube03 original11 release libraries resolve all linking. The first
WARP fixture passes initialization/updates/faces but fails scoped-mips at
subresource4: original wordff040001 versus expectedff000000. Actual fixture
exit-1073741819 and wrapper1 retained, with failed word/metadata saved before
CHECK. ROOTefd74363 directly joins archive07584ebd/391files,160Git inputs,
16AA64COFF+1PE, six helper/two CI AST, all6 retention flags/static34/SYS/desktop,
12 native children and13 absent host owners. Explicit releaseb3c9278e accepted.
No automatic retry, source/helper changes or full-suite/hardware admission.
ROOT preparedbde0c018 had already joined401 package originals and exact03
helper normalizations. DX10 now owns local GenerateMips follow-on only.

Aggregate086fe36 now contains exact8528+7a47+f96+66b+5624. Merge preserves
all existing SO/volume/cube probes,27 phase controls and strengthened build/
prepare helpers; added sharedheader wiring only. Meson/build/shipping40 ARM
cases pass localda51f591. Typed10.1/11 cube-array target source5624 independently
reviewed against localMSDDI slice docs: final4 strictx86/x64COFF, rawoptimized
reopen,26 reader corruption controls and9 frozen source joins, ROOT937634af.
Native moderncube target references remain pending.

SharedDX8identity66b ROOT94aaa5ea joins889 compileinputs/16 original zero-exit
commands/5I386COFF3PE and91 controls eachGCC/Clang. CPU08 ROOT0630c8e3 joins21
source inputs18Git+3licensed bytes against original CPU07TAR and nested
licensed original source paths, four separately counted registration files,
frozenhelper Git bytes, hostAST/actual no-targetplan/allpins/manualbuildflow.
Expected4COFF3PE,91 predicates,4CLI and one three-module/six-file-close readonly
observation; no frontend/core/KMT/factory/GPU. Native08 remains queued.

ROOT authorizationdd72d77d gives DX11 sole target ownership for one frozen
CPU03 build-only attempt after acceptedcube03release. It binds prepared
99d9b777/ownedmanual1bbb7e5a; bounded upload/stage/parse/build/collect/transfer/
observe, exactfailedattempt collection and independent originals/release.
ROOT/DX10/EWDK target-idle until explicit accepted release. No push/dispatch;
lastpublished3d39760 soleCI37689715890 PASS/fourredundant disabled.

Current corrective source ec6209e98dc39c7b0ce28ca6030772e13059f89d was pushed
once by owned PID2941667/0. Sole NativeUMD37736830999 is terminal FAILURE:
shader/identity/ARM64-build PASS, x86/x64 FAIL, ARM-runtime SKIPPED. The two
DDI callback assertions no longer stop primary execution. x86 now reaches
existing allocation fixture check186/line100: expected LockCb-returned124,
but the refactored nonprimary balancing Unlock uses requested123. Original
1b code unlocked returned lock.hAllocation. Correct the production balancing
handle, preserve the assertion and reject publication on renamed output.
ROOT original job logs913806/31707d9a retained; all-success core tuple remains
unadmitted. Full fresh failure artifact collection is separate.

DX8 offscreen03 actually FAIL before any workload: public CreateDevice HAL
hardwareVP returns8876086a. ROOT37f91309 independently reopens all41 original
TAR members/f0aaa3be, exact canonical closed attempt, static34/SYS/package/
binding/service/PnP/desktop, seven payloads/four original system libraries,
three task XML snapshots and selector/protection/callback teardown. Native2,
host7 and outer2940169 are closed; explicit releaseb4d47e07 accepted. Target
is now idle ROOT-owned. Existing successful HALc5f is preserved separately;
448readback/64Present/default replacement remain unaccepted. Root and DX8
agent infer frontend unload/reload causes Core's strict preloaded-core guard
to reject its already-owned retained core; actual rejection stage was not
logged, so this cause is not yet proven. Frozen fbd7afd holds one separately
owned verified I386 frontend reference through runtime teardown and adds
precise stage/HRESULT logs while preserving all guards/caps/core bytes.
Local compile/model evidence only; focused new native proof is next.

DX11 remaining Texture2D SRV mip/array slice source f236fbc is reviewed against
original local Microsoft -1 semantics. Isolated ROOT a7369f3 keeps all7 exact
source files and adds3 CI hook files. ROOTf4cc7fec joins6 optimized x64/x86
COFFs and source originals; all48 old ARM cases/35 shipped fixtures retained,
two each added (50/37),1380 raw originals whitelisted,504 views/5184 native
and public words/63 negatives expected. Independent peer1847d9f6 reviewed
all hooks. Native PowerShell/WARP/readback is pending; no main source push
or runtime admission from local evidence. DX10 shared-primary remains local;
current allocation regression fix takes priority before next combined push.

Integrated source23bbc91 contains the exact returned-lock allocation fix,
DX11 remaining2D SRV7files plus CI3files (d62e6b6), nested failure-original
retention54abbda and exact frontend-lifetime4files d1b4d21. ROOTbbfc5a41
reopens453 e47 originals and accepts the single production change to balance
nonprimary Unlock with LockCb-returned lock.hAllocation; fixtures unchanged.
SRV hooks peer1847d9f6 and ROOTf4cc7fec retain all48 prior cases/35 fixtures,
adding2 each (50/37). Consolidated sole NativeUMD push is next; native results
remain pending. Failed ec originals810ef7e9 retain2 genuine ZIPs155 each,
92 closed native children; both logs expose allocation check186. Nested72
primary raw files are absent, so ec remains runner-proof scope only.

Target exclusivity transferred to DX11 for one frozen Hostarm64/x64 EWDK
C2 readonly04 diagnostic under ROOTddf72357, prepared review8caa9a4f.
ROOT verifies21pins/4packet members/17 genuine CPU03 selectedx64 pairs;
actual local prepared reader2967465 closes0/54checks. Scope34known+6candidate
observations/40before-after and at most3 evidence-only DLL copies. No compiler,
DLL execution, repair, PATH, registry or GPU changes. Native AST/diagnostic/
collection/release pending. Latest accepted target release is37f91309;
ROOT/DX8/DX10 target-idle until current attempt originals/release accepted.
DX8 focused fbd native03 preparation is local only, exact2COFF/2PE and reused
original guard; public device/pixels/Present/default VM replacement unaccepted.

Published combined3ed22aa soleNativeUMD37739924828 running after owned2967775
closed0/4.35s, rawstderr139/c2a987f8. ActualGit1011blobs/fivegitlinks.
ROOT full focusedfbdnative03 recipe review and independent original plan
reader2968455 closed0; preparedROOTe1a0c408. DX11 currently owns target for
readonly04; no fbd target authorization until its accepted release.

ROOT C2x64 readonly04 acceptanceb8c9eb9b independently reopens25 originals/
archive361292df and exact5native9host closed children/releasedc94f2dc.
Original786checks→ROOT811 only25 preserved-byte equalitychecks; all other
canonical fields exact. Three matching EWDK14.50 x64-output support originals
are retained separately from ARM counterparts; no compiler readiness claim.
ROOT granted one focused nativefbd lifetime03 under b01073d3 after release;
actual2COFF2PE/reused37bguard/17native7host+outer PASS announced by agent,
original120TAR087ad61f and canonical d7b4c66d/explicit208deb83 preserved.
ROOT direct replay is pending; no new HAL/publicdevice/pixel/Present acceptance.

Lifetime phase7d9cfd0 six exact files are locally integrated27545a3 (unpublished),
ROOTsourcec1c2655f joins six committed raw sources and174 protocol controls/
unchanged prior HAL canonical replay. New lifetime scope remains separate
from e3 CPU and37b setup; newfrontendowner must span adapter/device teardown.
Sole publishedCI3ed22aa/37739924828 is still running. CI50 collector exact
three scripts reviewed, ready only for actual terminal6/all6SUCCESS;
ROOT ec failedcanonicalreader2969886 closed0 reproduces310files/92native
and denies absent rawprimary pixels/currentcore. No extra CI triggered.

CI50 actualfailure: old viewcheck18/119 rejects valid Tex2D.ArraySizeUINT(-1)
with FirstSlice1/total2. Production now correctly resolves remaining1 under
local MicrosoftDDI semantics. New typed504views/5184words and independent
reader, primary24/768/72, and allocation controls pass earlier in actual
x86/x64 logs; no fresh allsuccesscore. DX11 prepares test-only correction
preserving positive sentinel and explicit finite-overrun/other negatives.
Original failurecollection9268files/9051a49b retained; independent whole
failure source/process/raw review pending. No successcollector execution.
ROOT nativefbd03 actual120archive/17native7hostouter/explicitrelease is now
independently accepted b75f199e; canonical replayer2983536 closed0. Newlifetime
phaseAST5 scope is authorized once0b36d6f7 under latestb75f, targetexclusiveDX8;
11inputs/new2scripts/main6exact source checked. Names/HAL/publicdevice/pixels/
Present stillpending. VM defaultMesa unchanged, ordinaryreplacement unaccepted.

ROOT failedCI50 independent replay2987192 closes0; canonical rawreview649624bd
reopens9268ZIP members/two1011Git+5links source receipts/96native/7collector/
22new boundedlocalreader children. Independent20 cube/Resolve/Blt/primary/SRV
readers and immutable volume reads pass. Primary48images/1536pixels and
SRV1380files/504views/5184words perx86/x64 are retained actualreference scope.
CurrentCI/core stillfailed; oldviewfailure onlyoriginaljoblogs, no ownerreceipt
invented. New lifetimeAST parser host2985573 closes0 after35.62s; actual
AST5/Add-Type original reader/release currently pending, targetexclusiveDX8.

ROOT independently reviewed and integrated the exact test-only7ce8692 as
c2ff33c472e3501ea6856b293bfd9d445f686883: documented remaining ordinary/MSAA SRV counts now
positive with exact finite results; all original normalizedGenerateMips/RTV
sentinel and finite-overrun/zero/first==total negatives retained. Original
GPU shader/pixel/resolve bodies unchanged. ROOTfrozen62a5412c reproducible:
two optimized officialSDK COFFs/249 selecteddependencies/10actualclosedlocal
children and436737GCC+Clang sanitizer controls each. No production/CI count
change. Combined with localphase27545, one consolidated fresh CI50 pushnext.

Fresh lifetime AST ROOT189076e8 independently joins20originals/2native5host
and explicit4941d8d6; actualready8d289 changes onlythreefields. ROOT-created
pending text mismatched namesdescriptor; names01 stoppedbeforeanytransport,
outer2992654 exit1/.115s. Originalfailure/auth/manifest preserved6d31041c;
ROOT306061f4 independently accepts zero-target closure. Corrected manifest
6942f0cc changes onlypending to exactfrozen single-list string. Newnames02
outer/readers changes onlyauth/manifest/localreceipt paths; unchangedhost13a
and never-used guestpaths remain. ROOT grantsf2fa7833 names02 once; actual
host2995479 live, names/KMT proof and release pending. No GPUfactory yet.

Published18452d7c020d8e176c288e18edd51a012c2a58ce consolidates exact6phase
andtest-onlySRVfixture45+/1-. Ownedpush2996474 closes0/4.30s; soleNativeUMD
37743643139 nowrunning, actualGit1012blobs/fivelinks, cases50/shipped37.
No manualdispatch/redundantworkflow/usablefreshcore admission yet.
Freshnames02 actualPASS ROOT666e5fe1 independently rejoins38raw/TARd952a4dc,
canonical11cfc14d undercorrected6942manifest,2native7host/taskremoved/
outer2995479+explicit9ec19c2b. Rootreplayer2997577 closes0. ROOT grants one
fresh heldfrontendHAL enumerate01 under98ce33fa and correctedenumouter;
targetexclusiveDX8. Publicdevice/offscreen448/Present64 remainpending.
Sharedprimary114cbd frozenlocal15files/2newfixture/90raw next52cases39shipped
is separatefromCI50 correction; compiler/originalsource archivepending.

### Heldfrontend HAL, first actual INDEX16 failure and CI50 green

Current checkpoint 2026-10-08: published18452d7/soleNativeUMD37743643139
completed all SIX jobs SUCCESS (50 ARM execution cases/37 shipped fixtures).
Exact fresh original collection/review is in progress; usable source/core tuple
admission awaits those originals. ROOT replays heldfrontend HAL ef1e8d exactly,
accepting41 raw originals/2 native+7 host closures and release with93fdbf8a.
Actual fbd offscreen01 now gets through two runtime opens/internal core creates;
public CreateDevice fails80070057 at first INDEX16 CreateResource format101,
flags02100044. Only later cleanup Render fails80004005 with DEVICE_LOST logs;
GPU loss is not established as the first cause. ROOT3004357 preserves exact
failed originals9f2deaa8/41 files and accepts closure-only release85c7c07b,
protected34/SYS/binding/service/PnP/desktop and input7/system4 unchanged.
No448pixel or64screen proof, ordinary replacement or default Mesa change.
DX8 next patch narrows MightDrawFromLocked handling to VB, preserving IB lock
and draw restrictions. Borrowed-primary114cbd manual source/local originals
review passes ROOT3005498/720a7245 (2608/89Git/340compiler/8COFF/26children);
isolated main union preparation adds52/39 without dropping SRV50/37.
CPU04 private-support9a2833 prepared-reader ROOT3006887 passes265 checks,
joining74refs/51members/73 selected copies/6 genuine corresponding DLL origins;
actual native parse/compiler/view producers remain pending target handoff.

Local review notes: early resumed path lookups used nonexistent producer/planning
paths; subsequent rg inventory selected actual project paths before edits. A
draft ROOT prefix replay contained an incomplete remembered hash expanded
incorrectly; the original SHA256 was read and corrected before invocation.
Both independent actual replays then passed with empty stderr. No target action
or source admission occurred from those draft/path errors.

## 2026-10-08 merged X8/UAV strict compilation and CI integration

Previous continuation is progress: CRT and UAV source integrated, actual
USER06 native prefix/explicit release independently accepted, and final72
inputs sealed. Current live API3101146 confirms a6 CI37753759333 terminal
failure;184 CI37743643139 remains the last successful core. No new core is
yet admitted. X8 and original constructor diagnostic are now integrated.
Fresh owned merged compiler3100472 exits0, six strictO2 official-SDK objects
and six COFF machine reopens pass,362 selected inputs unchanged. Harness
review preserves old52 cases and adds two. No target operation occurred.
Two further agents audit real DX10 data-only SO and DX11 binding hazards;
these are independent local worktrees, not future promises of acceptance.
Path lookup errors from wrong cwd/guessed template parent were corrected by
using actual absolute paths and rg file discovery; no source was affected.

Actual consolidated source32b7bbb068dcbfc77b5fac0be5d374c495afeda9 was
pushed by owned3102831/0 with actual start/reaping/raw closure. ROOT API
3103796/0 returns exactly one NativeUMD run37761389362, in_progress,
https://github.com/sunflower2333/dxvk/actions/runs/37761389362. No other
workflow was triggered and no manual dispatch was issued. FreshCI54 core
admission remains pending actual all-six-job success and original collection.

ROOT CI54 observer01 exited1 on a receipt schema KeyError: the two genuine
Git receipts use regular_stdout_closed/regular_stderr_closed. Observer02
uses those actual fields, preserves01, and passes without collector execution.
USER06 owner01 local delayed-copy error is preserved and corrected only by
fresh owner02 before its first real workload; no native workload was repeated.


Actual CI63 publication and target attempt (2026-10-08): ROOT3329923/0 pushed beb465c once; sole push Native UMD37777560861 confirmed by original3331326/0 and ROOT direct query open. Current63/50/73/189 collector preparation is frozen; execution/admission pending. ROOT CPU08 prepared originals3335761/0 passed716 checks/107inputs/52native members, source100f0ea and separatebd601 view/copy origins. One CPU-only ROOT authorization3336405/0 binds latest accepted CPU07release9954/admission665d. Upload3338963/0 and native stage3340068/0 pass; native parse/build/views pending. Target solely owned ROOT; no default Mesa replacement or new hardware admission.
