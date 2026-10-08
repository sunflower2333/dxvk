# DXVK native Windows UMD

## Goal
Embed DXVK translation in a real VIOGPU D3D9/10/11 UMD, loaded by the Windows
runtime, with correct hardware rendering, DX8 compatibility proven through
the system runtime, and full Display+Render integration.
ARM64, x64 and x86 are required; an app-local runtime is not the target.

## Current step


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
Current local integrated source23bbc91 is prepared for one consolidated
NativeUMD run with 50 ARM cases/37 shipped fixtures; native execution pending.

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

Published main57b3131 native43-case CI37713183282 completedFAILURE: actual
x86/x64 residency assertion line335 owned.empty(), ARMcompilePASS and
ARMnativeexecutionSKIPPED. ROOTd4e38995 retains both genuine failure ZIPs,
86original members/24closed fixture children/22PASS+2FAIL; no rerun of57.
Standardentryde72dc2 CI37711793677 allsixSUCCESS; original recovery02
PASScd7bcad4 and ROOT72723054joins9380originals/123nativefixtures.
ROOTe05dcef3 admits three originalstandard-entry exports/PDB GUIDage.
Newx86ba60b53f remains unstaged; all hardware/default acceptanceopen.

Latest checkpoint: original I386 candidate staging PASS, ROOT independently
joins17 original files and accepts release19d2203c with ROOT346022f6. Four
original ZIP payloads at the owned d7 candidate directory remain distinct from
the new90 core. No module/factory/KMT/GPU/default change during staging.
Enumeration789c3d73 completed but HAL caps FAILED: genuine Direct3DCreate8
internally invokes CreateDevice before any CAPS12; the readonly frontend
blocked that callback8876086a/core_create_calls0. Original40-file TAR309c2bfb
and113180-byte stdout retained. ROOT exact frozen failure reader returns
byteidentical120c82a6 retention; ROOTd12d1109 accepts release702c0c9d with
native2/host7/outer/all9 local owners absent. Target ROOT idle. EWDK repairs
the bounded internal-device path locally, with real standard entry/new core,
explicit no-draw/Present boundary and HAL failure exit, not a caps override.

Standard legacy OpenAdapter source318d330 is integrated locally after ROOT
manual source review and direct local-original reviewe37e639b:8 Git files,
15 successful strict COFFs,5 preserved initial ARM failures,6 original LLVM
reopens and current303 used-header rejoin. This does not attest historical
whole-toolchain continuity or native execution. All40 existing ARM cases and
CI90 cube hooks are retained; new Interface8/9 public-entry fixture makes41.
SoleCI37709079286 on90fb093 completed all6 jobs SUCCESS. ROOT91411b62 accepts
five genuine ZIPs/9368 original files,966 Git blobs+5gitlinks/fourcanonical,
120 closed fixtures,18 independent cube raw reviews and3 volume reviews.
The separate6b/5624 target WARP references and CPU04 oracle-only build are
superseded for controlled reference scope; actual GPU/ordinary runtime remain
open. New standard-entry source de72dc2 pushed once, soleCI37711793677 running
(identity/shader/x86 PASS; other jobs pending at latest live check).
No default driver registration changed; genuine normal-runtime9/Present and
ARM64X compatible forwarding remain separate work. DX10 implements tracked
allocation residency/priority callbacks; primary/Blt/display mode/shared resolve
and embedded rendering-cache VidMm ownership remain mandatory public gaps.
Residency sourcecc878273+CI hooksd8689ac now merged locally: ROOTd80e5b49 joins
501 originals/72Git/338compiler inputs/10 strict O2COFF/14 closed children and
GCC+Clang ASan/UBSan102085 each. All41 prior ARM cases preserved, new2 make43;
one shipping-list conflict resolved by exact union. New43 native execution and
push are pending; unchanged contract bytes do not admit full renderer residency.

Prior names checkpoint: names08 genuine I386 KMT query PASS, ROOT independently
reopens38 original files with admission2bda9e1c and accepts releasec50a177c
with ROOTca61a0ff. Actual limitedUSER/session1 query ABI524/260 words/version0
returns the existing viogpud3d_x86.dll in installed viogpu DriverStore.
Parser08 native AST5/Add-Type PASS; original20-file TAR39f9ee34 and2 owned
native children/5host/outer release6aff3997 accepted ROOT051b2c65. The
admitted manifest07c46de5 changes only ready/native_phase_parse/pending.
Four redundant workflows remain disabled. Genuine system8 offscreen/Present
templates are local and require actual successful prior phase evidence.
DX11 implements genuine legacy OpenAdapter and documented FOGINFVF mapping;
DX10 audits ordinary-runtime10/11 admission gaps locally. CPU04 helper delta
85f9ac8c confirms only separate compile/link and fresh receipt identities, with
all25 sources/11libraries/oracles unchanged; owned/native review remainspending.

Prior integration checkpoint (2026-10-08): prioritize genuine VM DX8/DX9 enablement,
following the user's observation that the default runtime is not replaced.
EWDK exclusively owns target CPU08 under authorization16e1ff88. Actual native
parser/build/collection and all7 host transports have completed successfully.
ROOT direct original review945dba7b verifies107 TAR files,18 Git+3licensed
sources,4I386COFF+3PE,18 closed build children,91 predicate checks,4 malformed
CLI exits and3 physical system-module pairs/6 successful handle closures.
Compiler575/SDK13/libraries9 remain unchanged. This is CPU/identity evidence;
no factory/KMT/core/GPU call or installation. Explicit target release is being
finalized before the actual855 names, enumerate, offscreen and Present phases.

Source3d19c53 is now merged into the main working branch. It combines8528
cube production,7a47/f96 real-KMT probes,66b identity,5624 typed cube targets,
855 physical phase validation and6b15247 GPU-only scoped cube mip correction.
ROOT6b source/local proofd1b48152 rejoins160 frozen inputs,341 original compiler
inputs,6 strict x64/x86 COFF compiles,6 LLVM inspections and2 positive/4
negative reader executions; original336 files/7 readbacks/10380 words remain
unchanged. Modern target review937634af and prepared02ab84a5 remain valid;
separate native6b and5624 WARP references are pending. All40 ARM registrations
remain present. Merged73 phase controls pass, including the unchanged27 prefix.

SO/volume CPU03 failed after two passing native probe builds because the
combined oracle compile/link command misrouted linker flags (D9002/LNK1104
OLDNAMES). ROOTa7a6ce97 accepts original156-file archive191e5e8d and release
0175b8ff, retaining3COFF+2PE including the partial failed oracle object.
Corrected helper-only CPU04 is frozen locally, not target-authorized.

DX11 now works locally on the missing standard legacy OpenAdapter/runtime9
path and reversible deployment. Existing private DX9 GPU proof cannot admit
default binding. No source or proof guard is relaxed to label it installed.
Latest publishedCI37689715890 on3d397608 remains PASS. A single new push will
test the merged source; the four redundant workflows remain disabled. New CI
hooks independently verify the273 public cube observations and retain every
ARM cube original alongside the strict production oracle. Actual graphics
remain limitedUSER/session1. Ordinary-runtime DX8-DX11/Present/DWM stay open.

### Retained prior checkpoints

Current trunk3d39760 pushed once with integrated DX8 and typedDX10 probes;
soleautomaticCI37689715890 PASS allsix and ROOT direct original review PASS.
Five genuine ZIPs/1721 original files, source881 blobs+5gitlinks/four canonical
receipts/three DLL-PDB/config/private-loader chains, ARM30+x64/x86 60 owned
fixtures all0/drained. ROOT also independently recomputed each architecture's
309 production volume files (9138voxels/945sampled) and retained108 public
observations. Latest ARMcore6262784/40f679e5 is candidate-only/hardware-unrun;
previous618/a4053e44 and actuald7/b0fcbc3a scopes remain distinct.

Actual d7 viogpu DX11 compute GPU08 and D3D9 locked-buffer draw are ROOT-accepted.
D3D9 actual archive13801813 joins143 original files/199 TAR members,63 input
roles/32 raw Git inputs;2624 native pixel checks include1152 independently
replayed locked pixels/18 stages with word6d390bc5 and byte41e03dc5 checksums.
Context1/1,allocation15/15,lock14/14,render42,escape328,wrong_thread0,residents0;
static34/SYS/desktop retained,ten native+nine host children closed,taskremoved.
Explicit releaseee3c23b6 independently accepted. DX10 fresh02 dual-profile d7
USER05 is now ROOT-accepted. Names05 failed beforeKMT; failure/release90252e9d
and subsequent readonly-provider release772fe285 are ROOT-accepted. Target is
released to ROOT after nativeDX8CPU06, source db0e183, authorization5250125b.
ROOT direct failure reviewa1445895 accepts explicitrelease831ea2c: actual
5I386COFF/4PE compiled with zero diagnostics, then readonlyAPI observation
failed at canonical-I386-Kernel32-provider. All18 native/10host owners closed;
source33/compiler575/SDK11/lib9/provider3/SYS/desktop unchanged. Policy,
callback and32CLI guards remained unrun. EWDK develops a separate local-only
mapped-image identity observation; existing runtime guards stay exact. DX10
prepares one combined8528 native reference packet; no remote without handoff.
Failed01 stopped before any graphics task at the cross-privilege DWMStartTime
comparison. ROOT accepted retained101-file archive/releasebc6a7b and verified
same-attempt management continuity. Fresh02 separates the management baseline
from the unchanged limited USER worker guards:82sealedfiles/72roles/9helpers,
ROOT preparation5280b214, manifestfbe66424. Native Inputs/Before/Dual/After and
collection all exit0/drained. ROOT direct reviewfd8347aa independently joins
original184189818-byte archive/ad75e5ed (231files/344members), all1792 literal
RGBA pixels, exact shaders/source/retained handles/tasks and limitedUSER1.
10_0 draws1/pixels512 and10_1 draws3/pixels1280; balanced KMT contexts,
allocations,locks,residency, zeroerrors/leaks/wrongthreads/pendingpaging.
Both tasks removed, native9+host12 closed; SYS/desktop/static34 retained.
Releasecebe6146 accepted. First offline review lacked HLSL; both actual failed
local children remain preserved, unchanged reader passed from exact closure03.
Oldd7 hardware acceptance does not admit current3d or ordinary runtime.

DX8 CPU04 native EWDK PASS/root review: Git30+licensed3/575 tools unchanged,
5I386COFF/4PE/policy329/callback62+forwarded11/30 invalidCLI. AST5/AddType then
PASS. Actual I386 names01 FAILED at system-GDI32 path guard beforeKMT,
child2404 exit1/raw196, limitedUSER1, taskremoved/native2/host7 closed. Original
38-file archive61c4793c and failed release53355f1d remain unchanged. Official
IsWow64Process ARM ambiguity requires explicit process-machine APIs. Topic
repair5c420e4 changes onlyprobe/frontend paths+pre-guard diagnostics, retaining
strict I386/path guards; local strict4COFF/2PE passes. Fresh native CPU05
build/collection passed:181 originalfiles/c48fbc5d,33source,575tools/10SDK/9libs
unchanged,5COFF4PE/W4WXzero warnings/policy329/callback62+11/CLI30,
native49+host7 allclosed. ROOT proof042786e0/release100c118daccepted. Source5c
probe631808/2cef9cfa andfront440832/7b4e6b26 are actual native outputs. Fresh
names05 failed at explicit-process-machine-APIs/win32=127 beforeKMT; its
original38-file archive390a9fd7/1961779bytes, native2+host7 closure and removed
limitedUSER task are ROOT-accepted (directproof9fa594fa). No repair replay.
Native05 parser original20-file archive
db35d28b, AST5/errors0/Add-Type and native2/host5 all0 are ROOT-accepted with
released target229b3741. Names05 fresh03 packet18pins/11unchanged text inputs
passes ROOT prepared reviewc62ac53c. ROOT admitted manifest85e6f71d changes
onlyready/native_phase_parse/pending; names-only authorization42bb1b24 was
consumed exactly once for the failed I386 names query/cleanup/collection.
No PE/core/factory/GPU staging or enumeration follows from this authorization.
Seven DX8 commits are merged to trunk5b51100 plus27controlfix274f429;
latest SO/volume/CI30 bytes remainexact. Genuine system HAL enumeration,
448offscreen/64Present remainpending; no names/admission follows from failure.

Corrected SO/Texture3D2489 actual native ARM64 and source618 CI allarchitectures
PASS; source integrated, public terminal-mip mismatch observations retained.
Ordinary Microsoft-runtime DX8-DX11 activation, production contract admission,
registered driver replacement and Present/DWM remainopen. Goal staysactive.

Readonly original guest DLL collection50418477/16files identifies the precise
lookup: SysWOW64Kernel32 Directory2W/A absent; SysWOW64KernelBase exports the
same exactDirectory2W API as executable RVA20ca80/ordinal834. Kernel32
IsWow64Process2 forwards toWOW64APIcontract; nativeAPIsetv6/count973 routes
WOW64l1-1-3 toKernelBase. ROOT direct boundedPE proof1cfbc585 joins actual
native2/host8/static34/SYS/desktop and accepts release772fe285. No APIexecution,
KMT/factory/core/GPU follows from the readonly evidence. EWDK prepares the
narrow same-API provider repair db0e183 and immutable nativeCPU06 packet.
ROOT prepared proof6419035c independently joins33 source/30Git+3licensed,
13frozenrefs,886 actual localinputs/copies/beforeafter,4COFF/2PE/12commands0
and16 actual-selector mockcontrols eachGCC/Clang. CPU06 source/TAR047375c5
and manifestbf1083f1 remain frozen. Actual archive4a153ecf/118files retains
strict native5COFF4PE and failed readonlyAPI stdout3642. Kernel32 Process2
and exact KernelBase Directory2 resolve; actualI386/AA64 machine query passes.
Loaded I386 providers report logicalSystem32 paths and the canonical guard
fails. Physical mapped-file identity remains unproven; no guard weakening.
ROOT reviewa1445895 joins same-attempt postfailure hashes and release831ea2c;
planned policy329/callbackforwarded11/32CLI were unrun after actual16 children.
NoKMT/core/factory/GPU; failed originals preserved with no replay.

ROOT isolated cube checkpoint791006c (based on3d) implements legacy six-face
resource/SRV/RTV/DSV translation and scopedGenerateMips. Fixture COFF compiles
strictly for ARM64/x64/x86; native MSVC/WARP originals remain pending (168
subresources/10380texels/336rawfiles). EWDK agent prepares its frozen native
packet locally. DX11 checkpoint1f2ffde adds typed10.1/11 cube-SRV all-remaining
mips, preserving finiteNumCubes/public admission. DX10 checkpoint7669c3a adds
typed10.1 cube-array resource creation, preserving base10single6. ROOT
checkpointb5b863b adds CubeARRAY scopedGenerateMips and typed11 readbacks.
All four production/fixture checkpoints are committed locally and strict
x64/x86 original-header COFF checked; newROOT fixture also ARM64COFF passes.
NativeARM64 production MSVC/reference originals remain pending; LLVM ARM64
production fails in unchanged officialintrin.h __prefetch. ROOT prepares one
isolated combined candidate with all Meson/CI fixture/raw-readback collection,
then one native reference build and one production push/soleCI after acceptance.
Combined isolated candidate is now frozen8528d91 (fivecube Meson targets,
ARM64CI35fixtures/four fresh raw directories/independent oracles, oldd8cf/30s
unchanged). ROOT strictcombined production x64/x86 actual2623291/2623292 both0
with emptydiagnostics/proof5f4e028d. DX10 prepares common native packet locally:
160 exact inputs/23COFF/8PE, fivecube and oldview/Texture1D/Texture3D references,
2086 raw cube files expected; nativePS5.1 CI AST prerequisite. No push yet.

Published d7e5c7d46b8ce889e993bfab66a3b78b076c49d1 / automatic push
CI37648387721 now PASS all six jobs. Root collector04 exits0 and retains five
genuine API ZIPs, six job logs, three architecture/source/config/PDB chains,
56 owned x64/x86 plus28 owned ARM64 fixture runs, and88 ARM runtime originals.
Independent frozen a780 ACTUAL verifier also exits0 (proof fcc99f9a), with all
474 originals unchanged. Root admission217ee6e9 authorizes candidate preparation;
fresh four-file ARM64 original packet50224855 is prepared without transforms.
The raw-runner d8cf, canonical835 restoration1c7, markers and30s gates stayed exact.
Do not execute old frozen793 success collectors on this run or alter failed archives.
DX10 native PS8 preflight finalized: original111/source15/common10, five native
children/seven host transports exit0, observed SYS/service/desktop unchanged;
root original rejoin804357a2 PASS. DX11 helper03 native Parse11/Add-Type2 also
finalized: original39/source21/four native+three host exit0, proof1ae91827 and
release9ebd27ef. Both parsing gates are CPU evidence; hardware remains unrun.
Fresh limited USER/session1 identity now PASS (rootproofeed7b7ce/original18d461fc):
LUIDec6b/source0/three524-byte KMT UMD-name queries, adapter/DC cleanup,
child0/1.069s, nativecollector0, taskremoved andfivehost0. Management-after
confirms unchanged DWM1864 start time; limitedUSER original DWMStartTime isnull.
Fresh original core/probes50224855 are now actually staged on Windows; root
stageproof5ff88170 rejoins all four native files and retained owned tar process.
Runtime03 readyCPU0f3 keeps all47 preview payloads exact. Root proof2d691375
rejoins60 CPU05 tar members/nine unchanged native helpers and bounds the new
stage/controller/task-cleanup scripts. Exclusive CPU05 authorizationfd5acd14
has been handed to DX11. Native stage PASS; Prepare controller exits1 before
task registration/module load, and owned failure cleanup confirms task absent.
Preserve/collect this failure and diagnose the original input guard; no Start
or GPU action follows. DX10 follow-on03 binds69/72 originals with corrected
schema/default RunLevel, and separate private160 read-only task prep is local.
Next complete limited USER module-load feasibility from the actual input failure,
then DX11 actual384-word compute or DX10 10_0/10_1 pixels on real KMT. No driver
installation or ordinary runtime admission follows from CI alone. DX10 prepared
schema mismatch is being corrected locally in a fresh helper packet. DX8 agent
prepares the ordinary SysWOW64 D3D8 rendering gate from current x86 originals.

Published source793b7ca9c9861021480972c38d362a0d80313285 completed
exactly one automatic push CI37644615059: identity/shader-cpu and all three
backend builds PASS; ARM64 runtime fails before Add-Type or any fixture.
The retained original C# is exactly Git source with101 LF expanded to CRLF,
so the strict raw-byte hash guard correctly rejects the runtime checkout.
Root adds the unchanged canonical Git restoration to that runtime job and
retains its835-row receipt as arm64-native-canonical-source.json. Successful
runtime evidence must contain88 original members, including28 owned children.
Original31cf ARM harness/d8cf C#/markers/30s/fixtures/production stay unchanged.
Native AST/Add-Type and exact prior CI shader replay both PASS below.
Preserve failed793 originals; no usable core or hardware acceptance follows.
Previous candidate e7811754fc0c9640f57d0bb36843ebb2c35d0017 remains failed.
Exactly one automatic marker-corrected push CI37640080344 completed:
identity/shader-cpu and all three backend builds PASS; ARM64 runtime fails
at the twentieth fixture after nineteen accepted cases. The typed D3D11
marker now passes. Original D3D10 shader output ends at program21 ps_lod's
SetViewports checkpoint; checkpoint fprintf completion or backend entry is
not established. Independent original proofc4ba9a0e joins the failed ZIP
and byte-identical fast native0.227s source/output prefix; cause unresolved.
No manual dispatch or usable hardware candidate.
Previous42cf765 CI37635552675 completed: identity,
shader-cpu and all three backend builds PASS; ARM64 runtime stops after
the typed D3D11 fixture exits0 because its expanded SM5 success message
does not match the older harness expression. Original archives/logs are
being retained. Independent proofa754d516 confirms the precise marker fix;
proofd3ad8439 matches all28 corrected expressions against original successful
x64/x86 logs (56 matches), with only the old typed-D3D11 marker stale.
Prior930d7f7 CI37630945784 completed: identity/shader-cpu PASS,
all three backend jobs fail, ARM64 runtime skipped; no usable candidate.
Root repairs canonical checkout bytes and missing-private-loader rejection,
corrected42cf765 is published after independent835source/attributes guards
and actual16loader/sanitizer original-negative/fullidentity controls PASS.
Preceding b6 CI37593963656 fails the D3D11 fixture on x64/x86; ARM64
production and identity/shader CPU pass, ARM64 native runtime is skipped.
Keep failed originals. Complete native WARP15 and merged-source SM4.1
18draw/4608pixel suites now both PASS. Compute probe eb3/23d is integrated
as7b32aaf/da0a4ed, all7 independently reviewed inputs match current Git.
Final native PowerShell5.1 CI AST27-to28 and four locked GPU runner parses
PASS. Strict native ARM64 probe compile/link and CPUoracle12482 now PASS;
original89-file archive and agent/root reviews finalized, release0a39d778.
Candidate930d7f7 failed CI; original job logs/two original ZIPs are retained.
Verify corrected-source CI/original artifacts before setting any hardware pins.
Marker correctione781175 is committed/pushedactual0 and changes exactly one
runtime expression plus three plans. Native ARM64 CI must still complete
all28 cases; exit/deadline/oracles/fixture/decoder remain unchanged.
Independent proof8599d2e8 verifies all5 original42 ZIPs/three835row canonical
source receipts/source12 beforeafter/generated5/private3/DLL-PDB pairs,
56 raw artifact markers and56 retained/exited/drained bounded children.
Partial native ARM8 outputs/7accepted hashes remain distinctly failed run
evidence; later20 unrun. Whole candidate/hardware acceptance stays false.
DX10 native05 passes PS5.1 preflight but strict MSVC rejects five unmasked
full64-supported-version constant narrowings (C4310/C2220). Original57-file
archivec1b09fa3 and proof58c27a2a are finalized, all three owned stages and
eight host transports exit/drain, releasef9ba9e4b returns target to root.
Probe-only low32 correction05ffdc2 passes four strict official-header x64/x86
COFFs, GCC/Clang4141 and2positive/4negative narrowing controls. Root06 review
623f066b rejoins nine frozen prepared inputs/source15/Git/common10 and442-file
original archive. DX11 native ParseFile6/Add-Type now PASS, original23-file
archivea3504520/review1f25602c/release628cd196 finalized with3transports0.
DX10 native06 compiled/linked without warnings and passed CLI5, then a
whole-dumpbin filename regex wrongly rejected its KERNEL32-only CPU oracle.
Original06 partial evidence/release retained. Frozen07 corrects only the
bounded module parser/record and fresh paths: actual nativePS5.1 parses2,
original import positives2/rejection negatives14, strict ARM64 2BigObj COFF/
2PE with warnings/errors0, CPU4141 and CLI5 PASS. Original98-file archive
a72f56c9, agentproof811332a4/rootproof2c4518cd, all14 retained/exited/drained
native stages and8 transports finalized; release560c9fed returns target to
root. Core/GPU/ordinary-runtime/hardware execution remains pending.

Published793 revision migrates all28 ARM64 CI cases to unchanged native-tested
d8cf raw concurrent pipe runner, keeping exactly the same markers,30s,
exit0, PE/source checks, hash log and child working directory. Retains actual
per-case process JSON before any failure plus original C#/compile metadata;
workflow uploads those original JSONs as well as original text on failure.
Independent local proofcbe55ab8 and nativeARM64 PS5.1 AST/Add-Type original
proof9b313596 PASS (28 cases/one6argument Run/30000/no Start-Process).
Native19-file archive4322f363/three owned native stages/three transports0;
this is parser/compile evidence, not main ARM fixture execution or a timeout
cause/fix claim. Exact crosscompiled shader replay now PASS; fresh six-job CI
remains required after the runtime checkout correction. Root owns target CPU;
peer DX10/11 prepare local GPU runners.
Five actual System32 original payloads nativeAA64/PE32+ and15-file archive
1206cc2b rejoin rootproof0dfdc7be; native copies and beforeafter pass. DX10
also needs BCrypt original. GPU core/source/CI/hash pins stay null.

Root integrated compute096e024, stable stream-outputd153e81, per-stream
query2c1b0e2, hull fork/join220b4c0, returning query sentinels7db692d,
runtime exception-injection fixtures3c1ef05, observed SampleIndex3868b01
(agent a00ddb7), and separate modern development adapterc407197
(agent925e10e). Public OpenAdapter10_2/capability admission remains closed.
The modern export forwards the existing typed development path; its new
adapter guard controls now pass actual Windows execution: 94,208 checks,
three retained/exited/drained native children, four AA64 COFF plus one PE,
and all thirteen source inputs equal the combined checkout.

Exact14ebee2 WARP attempt completed10 fixtures then strict query C4702;
original259-member archivebc2cffa5 and root original review PASS retain.
Exact890224f completed13 fixtures then strict d3d10-formats C4702 from two
unconditional throwing fixture callbacks. Actual typed D3D11 checks4147/
callbacks430 are joined to this run's original stdout (polling changes counts
from the earlier3832/367). Query189/private4029/input41 PASS. Archivecebf778e
has6644818bytes/290files; all31 actual owned stages exit/raw-pipe drain,
collection/transfer and all11 retention flags are finalized. Root release
review errors01/count transcription and02/heterogeneous stdout formatting
are retained. Build agent independently reopens all originals locally.
Root fixture3c1ef05 preserves every CHECK, caught-exception/zero-output and
backend-call oracle; production templates unchanged. Local ASan/UBSan269
and four optimized original-SDK x64/x86 COFF controls PASS. Actual native MSVC now passes every fixture. Exact c407197 original archive
b367f1dc/7044979bytes/305files and root original review PASS:88 Git inputs,
34 AA64 COFF+15PE,34 exited/drained native stages,21SDK+7libs against raw
official packages, all firstparty warnings0/parser65, all11retention.
Typed3977/callback396, format3475 and multisample269 are actual this run.
Root frozen c407 source88 still byte-equals current d2f0e034 compiled inputs.

DX10 typed fixture repairs0256206/d536e58/3178900 progressed from omitted
render-target state to e1bc8draw/2048pixel comparisons. Original FXC33-word
SV_SampleIndex CONSTANT declaration was rejected; bounded3868b01 now fixes
that exact encoding and preserves uint scalar/generated linkage, stage/model/
mask/flag guards. Frozen GCC+ClangASanUBSan688/147/35 and original-header
COFF4 PASS. Exacta00 native strict18COFF/2PE/portable688 succeeds and the
SampleIndex4x draw passes256 reference-equal pixels. Nine draws/2304pixels
complete before ps_interpolation DrawE_INVALIDARG. All227 original archive
members/79Git inputs/21SDK+7libs/33FXC pairs/HLSL are retained. Release has
seven exited/drained native stages, six finalized host operations, no finalization
errors and all12 retention flags. Agent reviews official D3D10 register linkage:
VS TEXCOORD0 isreg1 but PS readsreg0, which conflicts with VS SV_Position.
Fixture85ae592 (agent4a6e355) adds unused SV_Position first and verifies actual
producer/consumer registers/masks/sample mode; production unchanged. Local
original review caught a synthetic word-index mistake5/16; preserved failed
review and4a715 packet. Follow-on111585a (agentb35845f) corrects declaration/use
words5/15, verifies unchanged literal16 and all46 copied words. Frozen
GCC+ClangASanUBSan763/147/35 and original-header COFF4 PASS. Agent validates
merged root and prepares a fresh exact d2f packet; isolated b358 has six source
differences including two native compilation units and cannot be relabeled
as the combined build.
57programs/18draws/4608pixels/3 expected negatives stay strict.
Merged d2f packet is frozen with79 source joins to current Git/archive bytes,
source SHA732868ee and builder7b19b1ee. GCC and Clang ASan/UBSan each
763/147/35 PASS. Root finalized guard and native CI AST audit, then explicitly
handed exclusive target CPU to DX10 for one complete native attempt.
Exact d2f native PASS763 compiler/7483 typed checks,57 programs/18 draws/
4608 pixels/3 expected callbacks; source79/SDK21/lib7/18AA64COFF+2PE,
firstparty0/parser65. Archive5c2a71a2 has3371615bytes/275files; agent and
root original reviews PASS. Seven children and six host transports finalized;
all12retention unchanged. Target is explicitly released back to root.

DX8 native7c1f545 read-only CPU/guards PASS using mounted official EWDK28000
Hostarm64/x86: archive9f7db177/127files,29sources/575compiler inputs,
8SDK/7libs/4I386COFF/3PE/28 exited-drained processes,306 policy checks and
14 CLI guards. Agent independent original review PASS; genuine SysWOW64
runtime enumeration and rendering still need fresh successful exact CI core.
CreateDevice remains blocked in this read-only slice.

Private loader configurationa334e39 (agent61e3d39) uses explicit architecture
viogpu_gl_loader names, with actual generated config and original DLL/PDB/
source pins retained. Original local15COFF/3PE/912-input review PASS.
Bounded CI runner1a516ef (agentbf6eedd) uses exact native-tested d8cf C#;
seven local process controls PASS. Native bf6 exact full-script AST/function
9af16c and wrapper execution PASS all four success0/nonzero7/large1048579+
1048587/actual30.006s timeout controls. Archive3435c36d/17663116bytes/47files:
source9/compiler575/SDK3/libs4 beforeafter retain, originalI386COFF/PE/PDB,
eight owned native children including tar collector all exit/drain (intentional
timeout is expected); collection/transfer finalized. Build agent independently
reopens originals; its independent bf6 review9858ea8b PASS. Exact c407
adapter guard02 is finalized: source13 joins, nativePS5.1parsePASS, strict
native3 stages/4COFF+1PE and94,208 checks PASS. Archiveb3f3fd58 has995242bytes/
62files; seven host transports exit0 and all11 retention pairs unchanged.
DX11 real-KMT compute/UAV probe and independent12482-check CPU oracle are
integrated. Review corrected mapped-BO residency and DXGI Version0x00020009;
fresh optimized official-SDK x64/x86 COFFs retain actual D3DCompile/HLSL/DXBC/
SHEX/384-word readback. Independent proofca2795e1 passes all7 source joins,
four original COFFs, GCC/Clang sanitizer controls and callback/cleanup audit.
Strict native ARM64 packet23d has19 Git/archive/current-root inputs and eight
bounded children. Attempt01 failed LIB /OUT response-file grouping before
probe compilation; original58-file failure and released transports retained.
Fresh helper02 explicitly groups scalar arguments and preserves strict flags.
Actual strict MSVC compile/link plus CPUoracle12482 PASS,2AA64COFF+2PE;
source19/SDK21/libs7/tools4, all13 retention pairs and eight native stages
originally joined; archive89/4f495a3e, root413315b7, agent4ee76335/release0a39d778.
All future successful core/source/run/hash fields remain null. Rootd2f0e03 (agentc9d005f) routes the actual adapter CI call through the
unchanged wrapper, preserving all original26 calls/stdout/architecture and
zero-exit contract. Fresh five-source native PS5.1 AST audit now passes full
before/after parse0errors,26-to27 calls, original function9af/C#d8cf and
unchanged architecture/output/zeroexit/30-second deadline. Three original
host transports exit0 and transferred native proof is finalized.
Four unused workflows stay disabled; all offline controls stay manual-only.

Root b6 CPU4 adapter45802/device584971/copy4050444/callback243 PASS,
original409files/329Git inputs/13COFF/4PE/21SDK/five raw official library
members/eight exited-drained native stages root review PASS. These CPU and
WARP results remain separate from native hardware/ordinary runtime acceptance.
Locked-buffer18-stage1152pixel oracle6d390bc5, GPU runner02 and fresh-core
ordinary9 harness generator remain prepared/unexecuted. Old affe genuine9
SYSTEMMEM VB rejection and cleanup flush88760868/Mesa submit-4 remain open.
Genuine DX8-DX11 rendering/reset/visible presentation and broader HDR/DWM/
VPU/perf/zero-copy gates remain open; the goal stays active.

Frozen installed oem17/58624/SYSd48e/serviceRunning/PnPStarted and baseline
DWM1864/Explorer4464/KMTec6b/source0 retain. Preserve actual sole-VM configuration;
no package/image/registration/shell/memory change follows CPU evidence.
Passwordless Administrator is available when management requires it;
interactive graphics acceptance uses the established USER desktop.

## Parallel replacement work (user requested, 2026-10-07)

- Root exclusively owns target after finalized native23d helper02 PASS and
  release0a39d778. DX11 prepares interactive USER compute probing locally. DX10 prepares isolated
  real-KMT typed10.0/10.1 graphics probing locally.
- Root has finalized c407 WARP15/modern guards, bf6 wrapper, c9 AST and final
 27-to28 PS5.1 AST PASS; it prepares one consolidated CI and original-artifact
 verification locally.
- DX11 core/hardware identity pins remain null; no core/probe GPU invocation
 follows native CPU evidence.
- Build verifier independently accepts bf6 and modern guard originals plus
 corrected23d compute source. It reviews the fresh-CI collector locally.
- Root integrates all source into this branch; one consolidated CI per push.

## Presentation and query checkpoint details (historical)

Owned typed D3D9 queries are accepted at exact source
`0a605643a78306a101350ec88fca51cd0974cbc2`. Single automatic consolidated
CI37544531323 passes six jobs, five raw ZIPs, three architecture builds and
15 actual native ARM64 executions. Native fixture272881 and six compiled
semantic controls verify snapshots, pending/completion output and reset/lifetime.
Fresh committed source/archive/build/after-run verifies72primary+15dependencies;
root independently reviews original native, CI/archive and PowerShell proofs.

Target query39/40 diagnostics1/0 each pass5056pixels+1056bytes, six handles/nine
completions, occlusion64/16/0, full EVENT/cached/reuse, ordered timestamps,
DISJOINTfalse and frequency19200000. Root independently recomputes192query
and640clip raw rational pixels per run; every earlier slice remains strict.
Callbacks ctx1/1alloc19/19lock18/18render96residency19/19wrong0. Exact signed
SYS58624/oem17, DWM1552/Explorer6464 and58readiness fields retain. Frozen seven
query helpers, three scripts and eight older clip helpers remain byte unchanged.
Acceptance docs/native-d3d9-queries-20261007.md and root-query-checkpoint-
verified-02.json. Original failed local review attempts remain separate evidence.
Four unused workflows stay disabled; offline/runtime controls are manual-only.

Verify typed D3D9 presentation. Local Microsoft Present/PresentCb docs and the
actual SDK require real kernel allocation handles and an owned created context.
Existing DXGI presentation ABI cannot be cast to typed9. Surfaces now own private
renderer backing plus a completed pixel publication into a real kernel source
allocation. Typed pfnPresent uses its copied native PresentCb and a separate
created GDI context. Verify exact builds and target screen pixels next.

Presentation implementation is now local: completed private renderer readback,
owned standard linear source allocation, a separate created GDI context and the
typed native PresentCb with real kernel handles. Preflight01 strictly compiles
six fixture translation units plus the separate probe (seven original COFFs),
including actual umd_allocation.cpp; all272881 old checks pass. Immutable57input
archive03d74ae8 and native proof guest-preflight-01 retain the source/build chain.
Behavior preflight02 archive233efc63 compiled but failed first positive check2153:
the adapter callback snapshot omitted PresentCb. Fresh preflight03 e5242c24 adds
that exact field and the separate screen probe; unchanged full fixture now passes
466831checks, zero warnings, all57inputs/sevenCOFFs/native continuity verified.
Exact committed061ee8f native87-input rebuild passes466831; root independently
joins72Git+15pinnedVulkan inputs and seven originalAA64COFFs. Six compiled
semantic controls pass at their intended assertions; root verifies all original
source/archive/build/after-run and native evidence without rewriting it. Single
automaticCI37549764498 all six jobs pass; root independently verifies all five
raw ZIPs, architecture core/backend logs and15nativeARM64 execution payloads.
Three native-parsed wrappers02 fix the inherited scheduled-task deadline to75s
for the65s presentation probe, with77s result polling. Generic wrappers are
hash-joined and prior query wrappers copied into the owned staging backup. Target
probe --present renders four64x64screen frames across A8/X8 with owned/reused
kernel allocation and GDI context; independent16384RGB oracle73b9e1c5 is frozen.
The existing5056pixels+1056bytes remain required. First actualpresent41 passes
those earlier gates then rawKMT rejects stage1 withc01e0342/source ownership in
use. Source/context and20residency references close, driver/desktop58fields
retain; no presentation GPU PASS. A separately built probe-only diagnostic adds
shared source ownership registration/release; all UMD and pixel oracle bytes
remain unchanged. Native probe8d9e196 strictly compiles without warnings and
root independently verifies57source links, originalCOFF/PE/imports/libraries.
Targetpresent42 matches nativeLUID6e6c/source0 but shared ownership itself is
rejected c01e0342 before any present submission. All earlier rendering remains
verified;19residency references/evictions close, signedSYS/desktop58fields retain.
No screen or ordinary-runtime acceptance. Next probe-only causal diagnostic
uses documented EMULATED type4 (no real primary ownership); no exclusive
request, mode change or altered UMD/fixture/screen oracle is introduced.
Actualpresent43 acquires/releases EMULATED successfully, but raw legacy primary
blit still rejects c01e0342.20residency owners close and58readiness fields retain.
Next isolated probe-only diagnostic uses documented PresentToBitmap; strict
visible screen acceptance remains required, and ordinary runtime stays open.
Targetpresent44 raw bitmap request returns c000000d, with balanced20owners,
unchanged signedSYS/desktop58fields and no screen pixels. Stop speculative
raw present flags. Build a separate read-only OpenAdapter frontend and genuine
Microsoft System32 D3D9 enumeration probe; keep caps zero and CreateDevice
blocked. Verify exact native build and baseline before temporary legacy-only
binding to observe the actual runtime callback contract.

Previous ae61dde compact clip37/38 acceptance remains unchanged. Production
caps/exports, registration and paired package pins stay closed. Complete remaining
resource/state/presentation/reset and ordinary Microsoft system-runtime DX8-DX11
hardware acceptance on all required architectures. Keep the full goal active.

## Earlier checkpoints
Exact source787419696fc828bdc0f1c9d7d5337135fce0c82f implements coherent
static2D mip chains, all-common-level TexBlt and explicit sampler/state mapping.
Native ARM64 fixture134906PASS and four compiled semantic controls independently
verify56source/archive/after-run/Git/nativePE/driver/desktop links. Full
37498655517 all6SUCCESS, offline37498596397 all4SUCCESS, API37498554766 and
package37498554868SUCCESS. Retained3architecture artifacts and14actual native
ARM64 execution hashes independently verify134906device checks perarchitecture.
Target texture25/26 independently verify1088pixels diagnostics1/0:192clear
ffefa655,192draw53a03d45,192shader1384c5a5,512texture d3afb9c5. Seventeen
nonempty submits, ctx1/1alloc13/13lock12/12residency13/13remaining0wrong0.
All8payloads3scripts/nativeARM64/LimitedUSERsession1 verify; tasks removedexit0.
Durations4.2857411s/6.5034715s. Fresh signed58624/oem17/binding0002/PnP0/SYSsha,
DWM1552/Explorer6464 and58 selected readiness fields remain unchanged through
both runs. Explorer6464 is a fresh baseline; prior2488 has a second recorded
installed Mesa D3D10 UMD fault c0000005/RVA10db80. MatchingPDB/dump causation
and long-term desktop stability remain open, separate from uninstalledDXVK.

Typed vertex/index buffers, byte-range locks, resource-bound streams and indexed
drawing are implemented in053e39f; the strict native probe shadow warning is
fixed in6ca877040690ff9510fa999ef0370e0fd9ff4536. Uncommitted-source native
preflight164072PASS is retained separately. Exact56Git/archive/after-run inputs,
native ARM64 positive164072PASS and four compiled semantic controls independently
verify asworktree02. Offline37508904695 all4SUCCESS before CI consolidation.
CI-only sourcecd4f5e22fddc3c30d910ec4fe798c8dc34bd3168 retains the identical56
inputs; single automatic fullrun37509470022 now includes unique Linux checks.
All6jobs37509470022SUCCESS; five raw artifact ZIP digests, three PE architectures,
164072device checks perarchitecture and14actual native ARM64 execution hashes
independently verify. Exact cd4f5e2 targetbuffer27/28 verify1472pixels each,
including384buffer621cd685, diagnostics1/0, durations8.316949s/10.8154619s.
Each has24nonempty submits,ctx1/1alloc15/15lock14/14residency15/15remaining0
wrong-thread0. All8payloads3scripts/LimitedUSERsession1 verify. Freshreadiness
keeps signed58624/oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464 and58fields
unchanged; both owned tasks removedexit0. Typeddepth/stencil source2d285542 now implements resources/binding/clear/lifetime.
Committed native182260checks and four semantic controls independently verify56
inputs. Single consolidatedCI37515057679 all6SUCCESS; fiveZIPdigests,
threePEarchitectures/182260checks and14nativeARM64execution hashes verify.
Exactdepth29/30 diagnostics1/0 pass2368pixels each, including896depth eac96ea5.
Durations14.5854506s/14.7961268s;38nonempty submits,ctx1/1alloc14/14lock13/13
residency14/14remaining0wrong-thread0. All8payloads3scripts/LimitedUSER1 verify;
freshsigned58624/oem17/binding0002/PnP0/SYSsha,DWM1552/Explorer6464 and58fields
retain; ownedtasks removedexit0. Next fixed-function transforms/material/light
contracts and untransformed geometry pixel acceptance, followed by resources and
presentation/reset, with ordinary system-runtime DX8-DX11 rendering acceptance.
Keep the full goal active; no Finish, production exports/caps or paired pins
until mandatory contracts and actual ordinary-runtime acceptance are complete.

Fixed-function source e86e0c50a55d05e6ef78e06fcc106e1e7e4adf49 now implements
typed SetTransform/MultiplyTransform/SetMaterial and sparse, device-owned lights.
Final native preflight02 and committed positive worktree01 pass202248 checks;
five compiled semantic controls independently verify their intended assertions. Failed
retirement-control01 compilation and publish-control01 assertion/teardown
timeout remain separate evidence. Fresh controls preserve the complete oracle
and change only its failure exit to _Exit to avoid static teardown after failure.
Single automatic CI37520111683 all6jobs SUCCESS; the build subagent verifies
fiveZIPdigests, all3architectures/202248checks and14nativeARM64execution hashes.
Freshfixed31/32 diagnostics1/0 independently pass3712pixels each, including1344
newfixed-function pixels b3416e11. Durations13.1856983s/20.6108307s,
59nonempty submits,ctx1/1alloc14/14lock13/13residency14/14remaining0wrong0.
All8payloads3scripts/LimitedUSER1 verify; freshsigned58624/oem17/SYSsha,
DWM1552/Explorer6464 and58fields retain. Ownedtasks removedexit0.
Next SYSTEMMEM/managed buffer transfer and remaining native contracts;
ordinary system-runtime DX8-DX11 rendering/presentation stays open.

User-provided EWDK copy C:\Users\USER\EWDK_br_release_svc_im_28000_260714-1640.iso
mounts read-only atD: and exposes nativeMSVC14.50.35717/WDK28000. Initial
native/x64 compiler attempts actually failC1510 because selectedclui.dll UDF
extents18283870208/18287994880 lie beyond the physical17912889344byte file.
The user-requested build subagent recovers missing compiler files from exact
official Microsoft ISO byte ranges into an owned copy, preserving the mount
and original ISO. Fresh C1b9 KMD build/independent output verification passes:
all9steps9.1133401s,118sourcefiles,86compilerfiles,19recovered and55official
ranges. ARM64 Native410624byteSYSsha b773c84e8df806293a87a6e27c019c4e0995f51cbc660c871b826ce60947ae9e,
kernel-only imports and desktop/signed58624/oem17 continuity verify. Unsigned,
uninstalled. Future compiler C:\Users\Public\EwdkNativeCompiler-01\bin,
EwdkRootD:/CompilerHostarm64, kit28000; fresh owned root required.
Existing portable nativeMSVC14.44/26100 remains
the separate provenance for controlled fixtures. WindowsSSH22 uses the existing
Androidroot socat bridgeguestforward; curve25519-sha256 negotiation recovers
access without service/VM/image changes. AndroidSSH8022/ADB5555 are reachable.

Matched Mesa8443c71 fixes the measured missing runtime bridge on pinned3e50dd4.
Full ARM64 CI37453384744 all3PASS; multiarchitecture37453381660 all4PASS.
Property support07 returns3152574d/1/88/1. Empty-lifecycle oracle271a83c accepts
balanced lifetime without requiring unused render callbacks; offline37455724595
all4PASS. Device08 lifecycle calls succeed;09Flush loses device; diagnostic10PASS
with context1/1, allocations3/3, locks3/3, wrong-thread0 and render0. Startup
deferral gap is fixed by f648a1a; guest fixture66410PASS and compiled disabling
control caught. Offline37456996101 all4PASS and full37457000846 all6PASS;
three architecture artifacts and14native ARM64 execution hashes verify.
Exact f648a1a ARM64 UMD/probe with matched Mesa pass target device11/12/13
under the measured Limited USER/session1 token, diagnostics1/0/0 respectively.
All stages S_OK; context1/1, allocations3/3, locks3/3, wrong-thread0, render0.
All eight inputs and three wrapper hashes per run independently verify.
Active PnP binding0002, signed58624/oem17 and exact SYS hash verify after runs;
DWM1552/Explorer4184 retained.09loss cause and long-term stability remain open.
Next typed rendering slice needs resource/state/clear/draw/readback and its own
nonempty-submit/pixel oracle. Ordinary DX8-DX11 admission stays open.
Lifecycle af9ecd5 full37419569213 all6PASS/offline37419571615 all4PASS;
probe bfd7e77 offline37421820113 all4PASS. Diagnostic KMD fdfd8f99
CI37428593406 all2PASS and active signed58624 readiness recovered on boot02.
Resource/state/clear/draw/readback and ordinary DX8-DX11 admission remain open.

## Validated D3D10 Checkpoint
Rotation4f59adf passed all source CI and 1281 checks in the existing guest.
Callback-table retention/fixture lifetime af84b74 passed offline37335070338
and full37335069804, including native ARM64 execution. The additional Present
ownership/cancellation and nested-call repair34ff484 is reviewed/pushed, with
local identity/runtime identity/shader sanitizer checks69/180/147PASS.
Offline37338141939 is all PASS, including actual x64/x86 execution and ARM64
compilation. Full37338144307 is also all six PASS, including native ARM64
execution. Both changed guest fixtures passed with continuous desktop processes.
Paired b6bf4c4f pins34ff and includes the VS18/WDK17 runner/contract fixes;
full checker and73package tests pass. Paired37338579561 is all SUCCESS,
including signed package and native/EC/x86 runtime load checks. Downloaded
archive/manifest verification passed64GPU/9installer files and matches the
uploaded joint receipt. Preserve binary source pin34ff in docs-only checkpoint.
Exact b6bf KMD built and verified inside the existing Windows guest with
portable native ARM64 MSVC19.44.35229/SDK-WDK26100.1:5.114s runner,
4.113s KMD plus0.648s VirtIO. Base setup51.224s/support library0.269s;
provisioning transfers/downloads are separate. Installed58623/Desktop retained.
Next implementation is typed native D3D9 identity/caps plus embedded backend,
and remaining full DXGI/resource/feature semantics. Mesa CI37312838033 remains
pinned. All eight admission gaps and DX8-DX11 hardware/runtime gates stay open.
Keep runtime admission closed until its remaining capabilities and hardware
acceptance are complete. Preserve the active Windows desktop and VM settings.

## 2026-10-05 continuation phases
- [complete] Recover current DXVK/KMD package state and choose the next source gap.
- [complete] Implement and test the selected native runtime ownership contract.
- [complete] Compile the actual production DDI on supported architectures.
- [complete] Repair Present ownership across retirement and nested rotation.
- [complete] Validate reviewed Present repair on all architectures and in guest.
- [complete] Record exact validation and update the paired package if justified.
- [complete] Audit and compile exact-source KMD in existing Windows VM.
- [complete] Verify guest build outputs and record reusable build checkpoint.

## 2026-10-06 native D3D9 adapter continuation
- [complete] Recover the clean checkpoint, roadmap and WDK adapter contract.
- [complete] Implement typed identity/caps/close bridge with pinned ownership.
- [complete] Add malformed reply, version, reentry and retirement fixture coverage.
- [complete] Wire architecture checks and validate on CI and the Windows guest.
- [complete] Record exact evidence and commit the reviewed implementation.

That adapter-only slice does not publish production OpenAdapter or rendering
caps. Its then-unimplemented CreateDevice fails without modifying runtime outputs. Reuse the
existing identity decoder and preserve the original opaque runtime handle.
The next rendering slice remains embedded D3D9 with exact runtime ownership.

## 2026-10-06 embedded D3D9 continuation
- [complete] Audit shared runtime identity/selection and offscreen construction.
- [complete] Implement the embedded renderer without public factory or implicit WSI.
- [complete] Validate negative controls, architecture builds and runtime ownership.
- [complete] Record source/evidence and the remaining typed device/rendering gates.

## Next typed D3D9 device continuation
- [complete] Implement device creation/destruction with raw runtime handles and callback dispatch.
- [in_progress] Add typed offscreen resource/state/clear/draw/readback ownership.
- [complete] Validate lifecycle failure/reentry/teardown on all architectures and bounded construction on the actual target.
- [pending] Implement runtime presentation/reset and prove ordinary DX8/DX9 acceptance.

## Target KMD readiness continuation
- [complete] Verify all lifecycle CI jobs and retained architecture artifacts.
- [complete] Repair hardware probe adapter selection and verify guest build.
- [complete] Resolve active PnP binding and capture current KMD reset/timeout provenance.
- [complete] Repair the measured Mesa support refusal and validate bounded construction.
- [pending] Establish earlier reset/device-loss causation and long-term stability.
- [complete] Finalize lifecycle evidence, roadmap and checkpoint documentation.

## 2026-10-06 readiness diagnosis and guest build
- [complete] Independently verify the separate bfd7e77 probe and both failed hardware receipts.
- [complete] Trace the exact installed d10f9ba6 standard-resource destruction admission.
- [complete] Add immutable refusal provenance and common-clock reset/timeout timing if existing diagnostics cannot identify the gate.
- [complete] Run focused semantic controls and compile/independently verify the diagnostic KMD in the existing Windows VM.
- [complete] Record verified lifecycle evidence and the next target acceptance step.
- [complete] Prepare and independently verify distinguishable signed58624 guest package,47 retained runtime files,50 catalog members and51 rollback files.
- [complete] Validate unchanged diagnostic production inputs and corrected host fixtures in replacement CI37428593406.
- [complete] Install reviewed diagnostic and collect fresh same-clock refusal records in the existing VM.
- [complete] Validate bounded backend construction after observed reset recovery and matched Mesa repair.

## 2026-10-06 interactive ICD selection
- [complete] Recover pinned source, evidence and both SSH health signals.
- [complete] Record SSH/interactive token and run unchanged probe with a Limited interactive task.
- [complete] Verify exact selected ICD and all58624hardware receipt payloads.
- [complete] Fix the measured missing Mesa bridge and validate the affected source.
- [complete] Record new hardware evidence and remaining native runtime gates.

## 2026-10-06 matched Mesa runtime continuation
- [complete] Measure missing private support on exact pinned Mesa in the target.
- [complete] Integrate callback-owned bridge and failed-create cleanup on pinned Mesa.
- [complete] Validate transport/negative controls and native ARM64 guest compilation.
- [complete] Verify full ARM64 CI artifact and positive target runtime-support reply.
- [complete] Complete all Mesa architecture CI and diagnose lifecycle submission oracle.
- [complete] Validate startup callback repair and exact rebuilt target candidate.
- [complete] Validate actual typed surface/clear/readback GPU work and record remaining runtime gates.

Historical startup binary source remains f648a1a in its immutable receipts.
Current resource binary source is dde00ed; probe source is aaa70c9. Hardware
resource evidence is workspace artifacts/dxvk-native-d3d9-resources-20261006/.
Startup evidence is workspace artifacts/dxvk-native-d3d9-startup-20261006/;
shared matched Mesa payload is retained under mesa-matched-runtime-01/runtime-support-07.
Next implementation must add typed resource handles and backend ownership,
render-target/state binding, clear/draw/readback, plus an independent nonempty
submission and pixel oracle before widening caps or enabling production activation.

### Readiness slice errors
| Error | Resolution |
| --- | --- |
| Guest-probe verifier first patch joined two shell lines | Split the lines before execution; probe verification passes. |
| Guessed standard-resource test directory absent | Reuse discovered synchronous-timeout fixture and add a focused readiness fixture. |
| Static response-classifier contract rejected the diagnostic call | Retain the response branch and update its exact contract to include capture. |
| New fixture extracted an extra callback typedef and lacked VOID | Narrow enum extraction and provide the OS peer type. |
| Reset-order negative control accidentally retained capture order | Move the deliberately injected reset before capture; the corrected control is detected. |
| Recovery read used workspace .planning/dxvk-umd instead of the pinned nested project | Read dxvk-umd-ci/.planning/dxvk-umd. |
| Recovery guessed absent src/viogpu, src/wddm and artifact directories | Discover actual source/evidence paths with rg --files. |
| CI status redirection and verifier compile used nested-project paths for workspace artifacts/scripts | Use the workspace root for retained artifacts and helper scripts; rerun both successfully. |
| Termux pgrep/pidof cannot see the existing crosvm process | Verify SSH with id and use root ADB pidof; one existing PID10130, no VM action. |

## Previous next-step checkpoint (historical)
GS7c0b9e3 CI34620365858 ALL5PASS. Both original/rebuilt GS16payloadwords
match including actual XOR/sample processing. Source closes native GS/create/bind and
GS CB/SRV/sampler slots, with real DXVK stage compilation and linkage.
Local147shader ASan/UBSan, Windows2257 WARP/SPIR-V and three architecture
production gates pass. Exact ARM64artifact10272751005 is materialized under
probe-7c0b9e3-arm64; source-checkpoint-7c0b9e3.md records hashes and limits.
Next finish native SO declaration/gap/stride and buffer-counter semantics,
then remaining sharing/mandatory-table and real OpenAdapter/version activation.
No new backend GPU run is requested; ordinary runtime/display acceptance is open.
6dfb092 replacement mip CI34619152294 ALL5PASS; 488 WARP view/mip checks
pass on x86/x64, shader887 checks pass, ARM64 production compiles successfully.
Current production adds native GS shader/create/bind and GS CB/SRV/sampler
bindings, with raw32 VS->GS transport and PS-selected GS output types. Validated
original/rebuilt VS->GS->PS WARP payloads plus final SPIR-V array interfaces.
Keep stream output, predication, ordinary activation and full feature levels
explicitly unfinished; no extra remote backend run requested.
68d6bb4 CI34617903693 API-only controls isolate the runner WARP no-op to
FirstArraySlice1. Default SRVs and an explicit slice0 SRV generate the expected
red lower mip; both automatic and explicit mip allocations reproduce slice1
failure without any native helper. Production c9e389d remains unchanged.
The strict WARP oracle now selects slice0/mips0-1 and checks all8subresources,
preserving neighboring-slice and outside-view mip isolation. Follow replacement
CI to completion, then continue actual native DDI/runtime activation work.
Nonzero-slice DXVK mip generation still needs actual GPU validation; no repeat
of an unchanged backend workload is requested. Existing frozen hardware
artifacts remain unchanged.

VKD3D engine61edc56 and paired97e7b009 are handed off to the independent
/root/vkd3d_native_runtime agent. No further VKD3D source/pin edits here.
Preserve its existing documentation changes and old Mesa4ace package; that
package must not replace parent's newer GL5f60c4d/Mesa668d598 desktop stack.

## Recent checkpoints (historical)
AddedfiveAPI-onlyWARPcontrols(default/selectedSRV,single/array,auto/explicit
mipcount)beforeexistingnativehelper/statework. Allresourcecreateschecked,
printsselectedlowerpixel; originalfull-imageassertionsstillgateCI. This
broaderisolationfollowsrepeatedempty-debugno-op, notnewbackendfeaturework.
4ba6a19debuglayerenabledbutnoqueuedmessages; lowerstilloldgreen. Nextcontrol
usesdocumentedCreateTexture2DMipLevels0/fullchainallocation, queriesactual4mips,
generatesstrict2mipSRVslice1andchecksevery8subresourceincludingoutsideview.
Productionunchanged; awaitcontrolCIbeforeconclusion. Ifstillfails, broaden
WARPAPIbaselineisolationratherthanrepeatidenticalqueries.
e98896btypedcontrolCI34616556328sameoldgreenFAIL, despiteformat28/misc1/
caps3fef3f3/range0,2,1,1. Typeless-onlyhypothesisdisproved. AddedD3D11debug
layerandInfoQueuebefore/afterGenerateMips;4ba6a19CI34617189692running. Keep
productionc9codeunchangedandallexactpixelsstrict. ef4b174ALL5PASS.
Diagnosis c40dc0dCI34615967837: selectedslice1/mip1expectedredff0000ff,
actualoldgreenff00ff00; priorbase/otherslicecorrect. Originaloraclebasewas
TYPELESS. SplitrequiredmiporacleontypedRGBA8withexplicitMIP_AUTOGENcapcheck,
preserveoriginaltypelessviewtestsandallexactpixelchecks. ControlCIpending;
do notclaimrootcauseuntiltypedcontrolruns. ef4b174ALL5PASS confirmed.
URGENT: c9e389dCI34615423153 x86/x64 compilePASS butWARPviewcheck391
pixelmismatch. Addedsubresource/coordinate/expected/actualdiagnostics;
localizebeforechangingproductionorclaimingMipvalidation. ARM64buildpending.
ef4b174 state-resetCI34615056755 running. GenMipsc9e389dCI34615423153running:
nativeAUTO_GEN_MIP_MAPflag, correctE_FAIL/E_INVALIDARG,
ownedSRVrange and realDXVKGPUmipblits. WARPchecks exactmips/arrayisolation;
no change to targetbackendworkload. Follow bothCI before sourcehandoff.
cf492c9 closes four actual native initialization slots; CI34613950771ALL5PASS.
Newnative state-reset continuation implements allD3D10topologysetters,
16viewportatomicreplacement/nullslots/zerocount andscissorunbinding.
ExistingWARPtestaddsactualstategetterchecks; implementationCIpending.
Architecturecontract reviewed with OpenCLagent and officialMicrosoft docs:
Arm64X sharednative/x64 path, separateWoW32bit, no inventedx64registrykey.
Parent'sordinaryruntimef6da604CI34613737738PASS is the next acceptance tool.
Continue native reset/topology/viewport correctness before negotiatedtable
activation; no extra backend hardwaretest requested.
b8c73ef CI34606380427 ALL5PASS; both ORIGINAL and REBUILT MicrosoftWARP
payloads match all16words, shader887checksPASS. Exact two ARM64 files/hashes
delivered to parent. Targetseven-dxvkb8-ddi-03PASS Exit0/249ms, all4096pixels,
buffer/depth-stencil/KMTpublicationPASS. DWM2088/Explorer5820retained.
Native2D SRV/RTV ranges, MSAA views/resolve and explicit format-cap translation
1a7cf3b CI34608704564 ALL5PASS. IndependentWARP246checks and shader887PASS.
Targetseven-dxvk1a-resources-04PASS Exit0/259ms; all640MSAApixels and
4096drawpixels, depth/stencil/copy/KMTpublicationPASS. LUID5805/ICD9AA5/
KMD58386, DWM1036/Explorer5640retained; parenthosttraceclosurepending.
VKD3D0447a76 CI34606533835 ALL4PASS. Exact standalone EXE/hash delivered;
targetseven-vkd3d044-ranges-03PASS Exit0/1039ms, all15360words. Paired72cef21
CI34607682895 ALL7PASS, artifact downloaded/all40manifestentriesPASS. Duplicate
identical push run34607682868 cancelled after verifying same source/workflow.
Both tests used OS LUID581E/ICD9AA5/KMD58386/hostVirgl28a. Host trace started
afterDXVK completed, so noGPU-trace claim forDXVK; VKD3D was trace-covered.
NextVKD3Dbc61de9 implements native root32constant callbacks, bounds,
64DWORDrootcost and copied caller lifetime. Local17workloads/17408wordsPASS;
CI34609416636 ALL4PASS. Targetseven-vkd3dbc-constants-05PASS Exit0/1167ms,
17workloads/17408words including2root32rounds. LUID581B/ICD9AA5/KMD58386,
host0d87/Virgl28a/TURNIPblit, DWM2140/Explorer5972retained. Hostclosurepending.
Paired97e7b00 pinsbc61de9; CI34610190479 ALL7PASS, finalartifact10268526395
downloaded and all40manifestentriesPASS; existingMesa4ace registration retained.
ResourceIsStagingBusy and read-after-write hazards implemented with real
backendtracking and bounded mapped-resource/actualhazard targetfixture.
970f14a CI34611474525 early ARM64 compile caught Microsoftmarkdown hazard
parameter order mismatch and mutable FLOAT clear contract. Actual WDK uses
device/view/resource; corrected both.5b0983d replacementCI34612034025ALL5PASS.
VKD3D61edc56 hardens UAVdata/counterregistry handling, CI34612036226ALL4PASS.
Both exactARM64artifacts downloading for parenthandoff. ParentWindowsis
stopped pending user's RAMconfigdecision; all remoteoperationsstaywithparent.
Parent prioritizes actual Microsoft runtime activation over further backend
cases. Audit complete minimum OpenAdapter10(_2)/OpenAdapter12 gaps and begin
production wiring. Predication was not edited; upstreamDXVKSetPredicationis
a stub, so forwarding it would not implement actual conditional rendering.
No childremoteactions; native runtime/displayPresent acceptance remains open.

## Earlier checkpoint sequence

37678fb fastCI34605897766 still fails originalWARP signedzero; emittedVS
disassembly explicitly containsmov o3.xyzw,l(NaN,0,Inf,-Inf), provingFXC
folded the selected zero/denormal ICB floats too. Source the signed-zero bits
from the runtime constant buffer instead, bind it toVS in both real andWARP
fixtures, then revalidate. Keep full negativezero/NaN/Inf checks intact.
e4a3654 fastCI34605452108 originalMicrosoftWARP failed exactlyone of16payload
words: rawFloats.y expected80000000(-0),actual00000000(+0). This establishes
an invalid constant-payload expectation independently ofDXVK. Generate that
signed zero from dynamicICB bits.x, retain strict checks and failure diagnostics,
then require both original/rebuilt WARP and next bounded target run.
URGENT: Main'sseven-dxvk9c55-ddi-02failed final4096/4096pixels atLUID582A,
exit7/238ms;creation/copy/depth-stencilPASS,event1/error0. Shell2028/5780
retained. Diagnose added linkage before further runtime DDIs. New candidate
adds separate UV/raw-float/immediate-array/fixed-integer failure bits and one
bounded payload-word draw. Independent WARP original/rebuilt-container checks
will distinguish invalid probe expectations from compiler/driver failure.
Do not request an identical9c55rerun. VKD3Dranged-copy edits are isolated WIP.
Paired60004/1ab0151/9eb4361 CI34596574202 ALL PASS artifact10263060497.
Depth b47f955 CI34597997951 ALL PASS and includes --native-copy/blend/scissor.
Standalone ARM64 files/hashes/usage delivered to main in hardware-probe-b47f955.md.
Paired60005/a27eb3e/b47f955 CI34598876655 ALL PASS, signed artifact10264495517.
Keep its older Mesa4ace separate from newer active desktop GPU fixes.
Typed native input layouts/vertex buffers7b30c07 CI34598775633 ALL PASS;
dynamic maps/error translation272a067 CI34599288562 ALL PASS.
Main ran standalone272a067 with LUID2A58000000000000: backend/DDI/native-copy
ALL PASS. Real KMT backing and GPU pixels verified; Microsoft runtime and
display Present remain unproven. No unchanged probe rerun needed.
Continue general SM4 VS/PS varying linkage with independently checked types.
Linkage8f7bce4 CI34601912517 compiledARM64 but x64/x86 SPIR-V checks failed.
Fixed ISGN read-use masks and added bounded immediate arrays in87697ea;
replacementCI34602941896 running. Local130shader checksPASS. Do not deliver
8f7bce4 merely because its ARM64 cross-build passed.
61bc32b fixes the fast-CI static-library filename through Meson introspection;
full replacementCI34603295333 found a fast-job /MD mismatch.9c55e92 fixes only
that CI runtime setting. LatestCI34603595256 ALL5PASS; the matched ARM64 set
is downloaded and hashed inhardware-probe-9c55e92.md. Production87697ea already passed all
three architecture jobs, including750real-HLSL/final-SPIR-V checks onx86.
Parent additionally delegated independent VKD3D engine/paired continuation:
engineb2c510d CI34603845851 CPU Vulkan/ARM64/x64PASS, x86 still running.
Paired743acf9 pins/documents SRV/null-range; CI34604382308 is running. Build/sign/package and
deliver target hashes after the DXVK three-file candidate. No remote operations.
Continue remaining mandatory resource/shader/primary/shared DDIs;
main explicitly asks not to stop at CPU OpenAdapter harness.

## Phases
- [complete] Implement internal exact-LUID DXVK backend factory (runtime proof pending).
- [in_progress] Implement native DDI resource/shader/draw/readback coverage.
- [in_progress] Build and validate with personal-fork GitHub CI on supported architectures.
- [complete] Coordinate real-device standalone backend/DDI/KMT-backing proof with main.
- [pending] Complete sharing, synchronization, presentation and full native acceptance.

## Constraints
Use the independent DXVK and paired driver checkouts; preserve unrelated
dirty work in gunyah-guest-drivers-windows and existing VKD3D docs/pins.
The user authorizes personal CI and remote testing. Keep the two SSH channels
separate, use only the existing registered VM, and preserve its configured
memory size without a task-specific cap. Complex remote work uses a script and scp; no
base64 commands. Backups are copy-only and partition changes are forbidden.
Preserve driver58623 and the active desktop; the older paired58522 baseline
is a candidate package and must not replace it. This thread selects
PWF_PLAN_ROOT=/home/sunf/droidvm-repos/dxvk-umd-ci/.planning/dxvk-umd explicitly.

## Errors
- Hardware harness header search first guessed WDK/um/d3dkmthk.h; the actual
  canonical header is in SDK/shared. Located it with rg --files and audited
  typed KMT render/escape/allocation fields before implementation.
- First guest typed-device compile accepted all production sources but rejected
  the fixture's mixed-type auto declarator. Split the two snapshot declarations;
  retain the first attempt logs before compiling the corrected source.
- Typed-device recovery read guessed umd_runtime_backend.h and umd_device files;
  those are absent. Locate runtime bridge/native implementation with rg --files
  before further source reads. No edits depended on the guesses.
- Embedded9b4168b full37412713033 compiled all three cores but failed DLL
  linking because the annotation singleton lived in excluded d3d9_main.cpp.
  Source2a4095c moves it into shared d3d9_annotation.cpp; full37415595130 ALL6PASS.
- Two combined manual patches failed on wrapped planning anchors without
  changing files; corrected exact anchors before applying the source repair.
- Embedded effb654 full37411400537/offline37411402396 failed Windows
  compilation because the newly independent query/identity header included
  d3dumddi before D3D9 types. Add canonical d3d9types before the DDI header;
  retain failed logs and validate the repaired exact source.
- Generated local support-staging cleanup using rm -rf was rejected before
  mutation by command policy. List the known generated .lib files and delete
  only those files, followed by empty directories; preserve all archives.
- Guest fixture runner fourth attempt put /link and its option on separate
  response-file lines; CL treated NODEFAULTLIB as a compiler option. Pass
  /link /NODEFAULTLIB:oldnames.lib after @compile.rsp on the actual command.
- Guest static-CRT fixture link also requested OLDNAMES.lib (legacy aliases).
  The retained portable packages do not provide it. For this CPU-only fixture,
  exclude that default library while leaving every unresolved symbol fatal;
  full CI retains its normal complete MSVC environment. Add the actual SDK
  synchronization/runtimeobject default import libraries discovered in CRT.
- Guest D3D9 second attempt compiled with fatal source warnings, then required
  SDK uuid.lib at link. Add official c/um/arm64/Uuid.Lib to the supplement.
- A read-only pragma search guessed d3d9.h under the local SDK tree; it is
  supplied by the WDK payload. No code or validation relied on that path.
- Guest D3D9 first compiler attempt /W4 /WX rejected WDK dxgiddi/d3d10umddi
  anonymous-union warnings C4201. Use external-header warning classification
  while retaining /W4 /WX for fixture/bridge sources; no production workaround.
- A remote-notes search used a .sh glob with no matches; those notes contain
  PowerShell scripts. Delegated remote worker uses verified existing paths.
- Paired artifact worker hit service503 after download reached91%; resume its
  existing transfer/session and verification instead of starting a duplicate.
- A follow-up SDK search used an unexpanded guessed docs/wdk glob and zsh
  rejected it. The delegated exact WDK audit continues from discovered paths;
  no source change or claim depended on that failed search.
- A workspace artifact read used a checkout-relative path; corrected to the
  exact absolute evidence paths. Read-only failure made no changes.
- A combined inventory notes patch used the wrong wrapped progress line and
  applied nothing; corrected the exact line and kept inventory evidence intact.
- A follow-up residency search assumed a Mesa Driver.cpp in the paired sparse
  tree; that file is absent locally. Use discovered source paths for follow-up.
- A source-service read guessed a .cpp file for the header-only helper; no
  edits depended on that failed read. Completed independent review is retained.
- Focused local native suite reached shader validation before finding the
  tracked DXBC submodule uninitialized. Initialize the exact gitlink; no source
  workaround or submodule source change is required.
- A status/log read ran at the non-git workspace root; it was read-only and
  made no changes. Use explicit checkout workdir or git -C for status commands.
- Paired37336134109 contract checker still expected one Windows ARM64 runner
  after the split. Agent updates job-specific runner/dependency/signed-load
  contracts and runs the full checker before replacement dispatch.
- A combined notes/source comment update matched the wrong wrapped findings
  line and applied nothing; corrected against the existing exact text.
- A resume documentation patch used split-line context absent from the current
  rotation document and made no changes; corrected at its verified audit link.
- Remote-runner read used the DXVK checkout rather than the workspace root,
  and a guessed paired verifier-script name was absent. Those read-only failures
  did not affect source or validation; use exact discovered paths thereafter.
- Resume search guessed two workflow filenames and a fixture-script filename;
  rg --files located build-native-umd.yml, native-offline-contracts.yml and
  test-umd-mrt.ps1. No edits or CI action depended on those guessed paths.
- Paired37318070394 failed on missing WDK Microsoft.DriverKit.Build.Tasks.18.0
  under MSBuild18.10. Keep matching VS17 cross-build and separate native checks;
  do not rename task binaries or force unsupported WDK task versions.
- Callback retention8dbcc93 CI exposed a constructor-local DXGI table in the
  rotation fixture, plus two other fixture lifetime assumptions. af84b74 keeps
  DXGI tables alive for the device and preserves in-place callback relocation.
- Paired CI37312228456 could not download expired Mesa artifacts. Exact-source
  rebuild37312838033 passed; d5e482c4 updates all consumer run-ID references.
- Follow-up search used two obsolete source/script filenames; discovered
  current umd_adapter.cpp and umd_contract.cpp before continuing.
- A documentation glob used an obsolete directory and zsh rejected no matches;
  use the already verified DX8/DX9 audit and explicit discovered paths.
- ARM64 validation artifact has per-fixture outputs and arm64-runtime.log,
  not the guessed arm64-runtime-results.json; use discovered evidence names.
- Guest receipt used Get-Content strings carrying filesystem metadata;
  preserve raw evidence, normalize locally, use File.ReadAllText in runner.
- Guest-evidence planning patch had mismatched context and made no changes;
  reread the exact text and applied the corrected patch.
- GS local run caught obsolete shader check56 expecting any undeclared PS
  signature entry to fail. Microsoft's documented union contract permits
  unused entries; replaced it with explicit unused-generic acceptance while
  keeping declared/missing/mismatched rejection tests. A combined docs patch
  had a stray context line and applied nothing; corrected separately.
- Two documentation path searches found no DDI markdown in the regular
  windows-driver-docs clone; a guessed reference/docs directory and guessed
  dxbc_compiler.cpp were absent. Use existing WDK compilation and discovered
  dxbc_io_map.cpp for typed GS contracts; no source was changed by searches.
- e98896b typed RGBA8 control failed the same old-green mip result despite
  correct queried format/flags/caps/view. Added D3D11 debug layer and InfoQueue;
  the typeless-only hypothesis is explicitly disproved. A documentation patch
  context mismatch applied no changes; verify exact text before retrying.
- c9e389d x64/x86 WARP view test both fail check391 at a pixel comparison.
  Added exact subresource/coordinate/value and stage diagnostics; production
  compile succeeded. gh run view --job --log refuses logs while sibling jobs
  remain active; GitHub actions/jobs/{id}/logs endpoint provides completed
  job logs immediately. Do not weaken pixel checks or advertise this source.
- Resume searches guessed obsolete docs/INF/shader paths and a zsh glob had
  no matches. Actual docs are workspace windows-driver-docs, INF is
  viogpu/viogpuwddm/viogpuwddm.inx, shader is umd_shader.cpp. Discover before
  reading further files; missing searches made no edits.
- 970f14a CI34611474525 actual WDK signature is device/view/resource for SRV
  hazards, contrary to local Microsoft markdown's parameter naming. Corrected
  production and test order; clear callback also requires mutable FLOAT[].
- VKD3D UAV fixture initially named an unverified zero-flag enum. Replaced it
  with an explicit cast to the actual field type before replacement CI.
- Hardware9c55shader path failed4096finalpixels despiteCI final-SPIR-V interface
  checks passing. Interface type agreement is insufficient runtime evidence;
  expose actual values and compare original/rebuilt bytecode onMicrosoftWARP.
- Fast61bc32b CI34603295333 found the actual library but mixed Meson's /MD
  runtime with plain cl's default static runtime (LNK2038). Pin both sides to
  /MD. Production Meson targets already use a matched runtime; no UMD changes.
- New fast87697ea shaderCI34602941896 compiled its compiler library but
  failed LNK1181 on a guessed dxbc_spv.lib filename. Resolve its exact static
  library through meson introspect --targets instead. Architecture jobs link
  the library through Meson already and are independent of this script error.
- Full8f7bce4 x64 SPIR-V check558 failed after148HLSL/reflection checks passed.
  Source review found rebuilt ISGN component masks omitted the high-byte
  read-use bits. dxbc IoMap masked typed inputs tozero then used its implicit
  float fallback. Set input read-use masks explicitly; output mask semantics
  remain unchanged. New CPU checks and earlier full-SPIR-V CI gate added.
- General linkage intentionally permits SV_Position at nonzero registers;
  old test35 still asserted that it must fail. Updated that obsolete case to
  reject register32 and added positive nonzero-position linkage coverage.
  Current local identity69/trailer176/shader102 sanitizer checksPASS.
- Resume inspection guessed native-umd.yml; actual workflow will be located
  with rg before reading. Paired60005 success was independently reverified.
- Native input-register test caught swapped registerIndex/streamIndex at
  shader check43; fixed exact constructor order. Local shader53PASS and
  Windows real-HLSL/reflection74PASS on x86; full target builds pending.
- b40ccb1 CI34595019708 passed217native allocation/present callback tests and
  compiled the production UMD, then caught const input arrays incompatible
  with WDK's mutable pSysMem. Made probe arrays mutable and moved production
  DDI/probe object compilation before the expensive backend build.
- New paired allocation ABI checker initially resolved shared/ at repository
  root instead of viogpu/. Corrected the parent path; temporary test outputs
  were automatically removed by TemporaryDirectory.
- Native allocation CI34594671352 compiled implementation but its fixture
  used DoNotWait, absent from D3DDDICB_LOCKFLAGS. Confirm exact SDK spelling
  before correcting the test. Upload/copy fix1f4595d CI34594094209 allPASS.
- Constant-buffer patch had a documentation-context typo; the atomic patch
  applied no files. Corrected the exact documentation line before reapplying.
- Upload/copy828b535 CI34593487334 found WDK D3D10_DDI_BOX uses signed LONG,
  while D3D11_BOX uses UINT. Reject negative coordinates before explicit
  conversion; no potentially wrapped signed box can reach the backend.
- Initial patch context for src/meson.build mismatched; no partial edits were
  applied. Corrected the context and applied successfully.
- Two guessed filenames were absent; used rg results to locate actual files.
- CI34585824018 failed: x64 MSVC target used invalid x64_x64 alias; x86/ARM64
  backend library compiled, but probe omitted dxbc-spirv include dependency.
  Fixed both before the next run. Identity sanitizer test passed69 cases.
- Submodule add with --depth=1 did not fetch its nondefault branch. Explicit
  depth1 branch fetch/checkout recovered it; completed .gitmodules separately.
- Shader container integrity regression found pinned dxbc-spirv buildContainer
  emits regular MD5 while validateHash requires the DXBC variant. The bridge
  now recomputes with the existing DXBC hash function without submodule edits.
- Unknown opcodes are accepted with empty layouts by the upstream parser;
  the SM4 bridge now validates opcodes, lengths and exact reconstructed tokens.
- Parent general clang-format workflow34586690304 failed on inherited source
  style; this integration changes no parent C/C++ implementation files.
- Parent34586690632: KMD compiled, but initial MSBuild Inf2Cat failed because
  three DXVK files were staged only later in the workflow. Added them to
  FilesToPackage in the independent vcxproj; no KMD source changes.
- a52df2b CI34587997416 compiled all3architectures; Windows CPU checks rejected
  re-encoded real shader tokens at check43. b76de1d preserves tokens verbatim;
  fast Windows D3DCompile/container/D3DReflect CI34588836455 shader-cpuPASS.
- Superseded child7a373289 CI34588590093 and parentc025e63 CI34588644705 were
  cancelled after replacement runs started, because they retain the already
  diagnosed real-shader reconstruction failure. Not an observation timeout.
- Producer header was initially resolved relative to checkout root; rg showed
  its actual location isviogpu/shared/viogpu_adapter_identity.h. Read-only
  review completed at the correct path; no producer files modified.
- Parent total contract checker retained baseline epoch-version and two-file
  package rules, incompatible with this branch's existing explicit60003 and
  five-file package. Narrow checker integration validates exact reserved
  source version and exact five files; full contract nowPASS. Check-only
  follow-up does not change7051011 binaries and avoids duplicate full CI.

## 2026-10-06 matched Mesa runtime bridge (completed checkpoint)
- [complete] Measure the private support reply without creating a device.
- [complete] Integrate existing runtime ownership and teardown into exact pinned Mesa.
- [complete] Validate semantic failures, architecture CI and downloaded payloads.
- [complete] Verify matched candidate on the existing guest and record remaining gates.

### Resume diagnosis errors
Broad root hidden-file discovery entered generated AOSP trees and was stopped;
subsequent reads use the known .planning/dxvk-umd-remote-20261005 script directory.
An unnecessary Windows CIM query had invalid shell quoting; no remote changes.
Exact Mesa3e50dd4 was absent from the integration worktree's object store; fetch
only that source SHA with depth1 before creating the isolated candidate.

## Typed D3D9 draw continuation
- [complete] Implement owned vertex declarations and supported state/UMstream/nonindexed draw.
- [complete] Verify caller snapshots, reset/retirement/reentry/ranges with native fixture and semantic controls.
- [pending] Build exact production source on all three architectures and verify target draw/readback pixels.
- [pending] Continue textures/buffers/shaders, presentation/reset and ordinary DX8-DX11 acceptance.

| Draw error | Resolution |
| --- | --- |
| First native compile used guessed DestroyVertexShaderDecl table name | Actual SDK names pfnDeleteVertexShaderDecl; corrected all references, retain worktree01 failure. |

## Typed D3D9 texture continuation
- [complete] Independently verify shader23/24 and checkpoint unchanged binary pins.
- [complete] Implement bounded coherent2D mip ownership, native sampler/state mapping and TexBlt.
- [in_progress] Validate creation/snapshots/reentry/retirement and semantic controls on native ARM64.
- [pending] Compile all architectures and verify actual GPU texture/mip pixels.
- [pending] Continue remaining resources/drawing/presentation and ordinary DX8-DX11 acceptance.

### Texture worktree and access interruption
Coherent static2D RGBA8/XRGB mip chains, typed PS/VS texture bindings,
explicit native sampler/TSS mapping, common-level TexBlt, cropped externalCPU
snapshots before callbacks and worker unbind/flush/release are implemented.
Native fixture covers partial creation, metadata/pixel mutation, mip matching,
retryable deletion, reset/reentry and foreign handles. GPU --texture oracle
adds512pixels/8sample cases, checksumd3afb9c5, to prior576pixels (1088total).
First native CPU fixture source56files/archivec21418ff transferred before
WindowsSSH22 became unavailable; no compile/run result received, so no PASS
claim. AndroidSSH8022/ADB remain healthy. Daemon and root verify only existing
VM10130,6656MiB/8CPUs, same active image. Screenshot shows live desktop and
user ISO transfer. Ports22/8122/2222 and directADB-forward22 time out; existing
forwards and guestMAC/IP neighbor records present. Diagnose access while
compiling via CI; no second VM, reboot, image or configured setting changes.
Local Microsoft DDI docs are in reference/codes/windows-driver-docs-ddi,
revision7515063cea4c9e98db6a92986c5b4ddb0463fd16; prefer these for contracts.

Offline37497502780 compiles typed production DDI on all three architectures,
then catches the Windows `small` macro collision in the new probe. Rename
that local variable; no sampler implementation or pixel expectation changes.
Full37497509867 still builds the original source. Replacement exact-source
gates and native fixture/semantic controls remain required.

## 2026-10-07 typed texture acceptance and EWDK continuation
- [complete] Implement coherent texture/mip/sampler and snapshot/lifetime contracts.
- [complete] Validate native134906check fixture and four compiled semantic controls.
- [complete] Verify full/offline/API/package CI and all exact architecture artifacts.
- [complete] Verify target1088pixels diagnostics1/0 and fresh driver/readiness continuity.
- [complete] Record immutable evidence and current ordinary-runtime limits.
- [complete] Independently verify actual EWDK KMD build and exact recovered compiler provenance.
- [complete] Audit and implement typed VB/IB/range locks/streams/indexed drawing against local DDI docs.
- [complete] Verify exact source6ca8770 native controls, consolidated cd4f5e2 architecture CI and buffer GPU pixels.
- [complete] Record independently verified buffer hardware and EWDK build checkpoints.
- [complete] Implement typed depth/stencil ownership and native clear semantics; exact targetdepth29/30 pixel/readiness proofs pass.
- [complete] Implement and independently verify fixed-function transforms/material/lights, native controls, CI and targetfixed31/32 pixels.
- [complete] Implement and independently verify SYSTEMMEM/BufBlt, native controls, consolidated CI and targetbuffer-transfer33/34 pixel/byte/readiness gates.
- [complete] Implement typed SetClipPlane and fix vertex-input comparison; native controls, exact CI and compact GPU clip37/38 pass.
- [complete] Implement owned typed queries with completion-only output, native controls and independent GPU acceptance.
- [pending] Complete remaining state/resource/presentation/reset and ordinary DX8-DX11 acceptance.

## Typed D3D9 presentation continuation
- [complete] Accept exact0a60564 query CPU/CI/GPU checkpoints and freeze evidence.
- [complete] Audit typed Present/PresentCb ABI and existing runtime allocation ownership.
- [complete] Implement owned presentable allocation/pixel publication and typed callback.
- [in_progress] Verify native callback/pixel/lifetime/reset/reentry controls and exact architecture builds.
- [pending] Verify actual target presentation and continue ordinary system-runtime admission.

## Dynamic textures and ordinary HAL admission, 2026-10-07
- [complete] Implement typed dynamic2D mips/discard and retain SYSTEMMEM transfer semantics.
- [complete] Independently verify exact0dd native42309/521942 fixtures and original toolchain/continuity.
- [complete] Correct strict native hardware-probe shadowing warnings; root27member8d7 build review passes.
- [complete] Verify all original artifacts/executions from replacement consolidated37567440170; obsolete37566610477 cancelled.
- [complete] Freeze independent768pixel oracle and parse isolated dynamic GPU scripts on native ARM64.
- [complete] Verify full5056+768pixels and1056bytes on hardware with diagnostics1/0 (dynamic45/47); failed46 retained.
- [complete] Verify Caps2-only blocked runtime diagnostic fails before any device API attempts.
- [complete] Build isolated legacy fog-bit diagnostic and independently verify both genuine blocked Microsoft device contracts.
- [complete] Implement documented device permissions and verify nativec8f43067/535562 fixtures with full original provenance.
- [in_progress] Verify device-permission sourcec8f on all architectures in sole consolidated37569563644.
- [in_progress] Verify exact889 lifecycle diagnostic native build/guards; then run actual Microsoft runtime resource/clear/readback contracts.
- [pending] Complete ordinary DX8-DX11 hardware rendering and visible presentation; broader GPU program remains active.

## CI correction after930d7f7

Actual x64 has26 exited/drained zero-exit fixtures before D3D9 backend
access violation0xc0000005. Typed DX10.1/D3D11, SM4.1 compiler763/18draw
fixture and compute oracle12482 all pass in that original failed build.
Root original review9ba39f68 joins API run/source/size/digest/CRC and all27
owned children; ARM64/x86 reject workflow rawblob before Meson.
Missing private loader leaves the LibraryFn constructor calling a null
resolver before DxvkInstance admission. Root guards LibraryLoader::sym;
GCC/Clang ASan/UBSan each reproduce the original930 crash and pass all16
new regression checks. Full native identity suite PASS. Unix shim unused
parameters are the only suppressed diagnostic; Windows flags unchanged.
Git checkout-index can return for a stat-clean CRLF file before applying
--force; independent racy/non-racy control reproduces both behaviors.
New CI helper restores every regular tracked root file from raw ls-tree/
cat-file Git objects, verifies object identity, bypasses filters/archive
attributes, preserves submodules and emits an original source receipt.
Existing rawblob checks stay strict; source configuration includes helper12.
All28 owned fixture calls and C#d8cf remain unchanged.
DX10 helper03 was held before target calls after finding stale undefined
ServiceController result fields; isolated helper04 is being prepared.
DX10 retains exclusive target CPU; actual04nativePSpreflight parsed2/RSP/
C#Add-Type before failing ServiceController assembly unavailable in
NoProfile host. No compiler/builder/fixture/core/GPU ran; original failure
collection/release is being finalized; isolated05explicitassembly pending.
DX11 independently prepares USER compute runner/384-word validator locally.

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
One strict native compiler/CPU4141+CLI5attempt running; target exclusivelyDX10.
Fresh42CI identity/shader PASS and all3canonicalrestore steps success;
backend builds stillrunning. No new core/hardware acceptance.

### Current owned work after native CPU06 failure (2026-10-08)

- [x] Preserve CPU05 source-array failure and all actual diagnostics/releases.
- [x] Verify fresh CPU06 native configuration with genuine12/12rows and all47inputs.
- [x] Collect CPU06 typed-variable failure before module load and release owned task.
- [x] Run fresh narrow CPU07; actual limited USER module load/query/unload verified.
- [x] Finalize private160 native02 readonly query; raw-query scope passes, full-registry task guard failure preserved.
- [ ] Rejoin actual private160 with source/currentCI/system/Mesa/USER and prepare
      GPU-only manifest; run384 actual compute words, then DX10 dual1792pixels.
- [x] Repair verified mounted EWDK library once; all575 match. Fresh02 actual C3493 failure preserved.
- [ ] Finish corrected DX8 native CPU build; fresh03 probe compiles but strict callback compilation fails, originals collecting.
- [ ] Run null-GS SO native WARP/readback, complete SM5 path and integrate tested
      source with DX10 Texture3D port in one push/one CI workflow.
- [ ] Complete DX8/9 ordinary device/render/Present and actual USER DWM gates.

Target currently /root/port_dx11-owned currentd7 GPU08 real384compute; root/DX10/EWDK target-idle.
Current pushed source d7/all6 originalCI success; new source work remains isolated.

### Actual combined native SO/volume regression (2026-10-08)

- [x] Root independently join actual private16002:134 original files,37 source rows,17native/15host processes,18 raw queries/canaries; gen2/caps0/WDDM2200/query LUIDec6b passes. Full-registry guard still failed, raw telemetry changes retained.
- [x] Run exactd821 native ARM64 four-fixture attempt; legacy SO1252, SM5 VS/DS SO4193 and volume-policy385547 pass.
- [ ] Correct Texture3D scoped3D mip failure: originalprofile0/readback6/mip3 onevoxel comparison failed; last17 saved records independently match1875words. Preserve original245-memberarchiveaf6b6e8c and failureexit-1073741819.
- [x] Parse exact updated twoCI scripts on native ARM64 PowerShell5.1.26100.1591; twoASTs/errors0, no builder execution.
- [ ] Run corrected native Texture3D all27cases/9138voxels/945pixels and retain309 originals before sourceintegration/push.
- [x] Root review boundGPU08 47actualfiles/19Git/16ZIPs/5ARM64PEs; ready282df90a/rootf62421ef, frozenrecipe d312/rootc3796bb1.
- [ ] Run actual384compute on currentd7 core; exclusiveauthorization09f12ba9, nativeprefix actualPASS, taskphases running.

Single matching automaticCI remains active; currentd7 run37648387721 all6success directly rechecked. No newpush; public admission and overallgoal remain open.


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
