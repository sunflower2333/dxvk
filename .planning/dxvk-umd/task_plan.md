# DXVK native Windows UMD

## Goal
Embed DXVK translation in a real VIOGPU D3D9/10/11 UMD, loaded by the Windows
runtime, with correct hardware rendering, DX8 compatibility proven through
the system runtime, and full Display+Render integration.
ARM64, x64 and x86 are required; an app-local runtime is not the target.

## Next Step
Typed D3D9 surface/clear/readback source dde00ed is pushed. Native fixture
77449PASS and three independently compiled semantic controls caught intended
regressions. Full37464752821 all6PASS and offline37464754567 all4PASS; exact
three-architecture artifacts and14native execution hashes independently verify.
Probe-only aaa70c9 corrects missing WDDM2 paging queue/residency ownership.
Unchanged production dde00ed/Mesa8443/installed58624 now pass target18/19
with diagnostics1/0:192 actual pixels, checksumffefa655, intact padding,
three nonempty GPU submits, context1/1, allocations7/7, locks6/6,
residency6/6 and wrong-thread0. Eight payloads, three scripts, native ARM64,
Limited USER/session1 and driver/desktop continuity independently verify.
Failed16/17 receipts remain immutable. Probe offline37472553344 all4PASS.
Typed declaration/state/UMstream/nonindexed draw worktree is implemented.
Native fixture89329PASS and three compiled semantic mutations independently
verify. Sourcec874d55 full37477931517 all6PASS/offline37477937872 all4PASS;
three architecture artifacts and14 actual ARM64 execution hashes independently
verify. Target draw20/21 both independently PASS384 pixels, clearffefa655 /
draw53a03d45, six nonempty KMT submits and balanced ctx1/1 alloc10/10 lock9/9
residency10/10 wrong0, diagnostics1/0. Eight payloads, three scripts, native
ARM64 and Limited USER/session1 verify. Fresh signed58624/oem17/PnP0/SYSsha
and DWM1552/Explorer4184 continuity retain all54 sensitive readiness values.
Typed programmable VS/PS create/bind/delete and six constant uploads are
implemented with bounded caller snapshots and worker ownership. Native
fixture107385PASS and four compiled semantic controls independently verify
56 source/after-run/archive/nativePE/driver/desktop links. Next commit/build
all architecture gates, then verify the576pixel --shader oracle on the target.
Full shader CI and actual programmable pixels remain pending.
Textures/buffers, indexed/multistream drawing, presentation/reset and ordinary
DX8-DX11 remain open. Keep the full goal active; no caps, production exports
or paired binary pin widening until their mandatory contracts are ready.

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
