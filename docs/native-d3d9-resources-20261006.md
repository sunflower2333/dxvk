# Typed D3D9 surfaces and GPU readback, 2026-10-06

Source `dde00ed3d255d24073d111c90dc91e69247bba93` adds the first resource
operations to the unregistered typed D3D9 device. Full backend CI37464752821
passes all six jobs and offline37464754567 passes all four. Target18/19 pass
actual GPU clear/readback after probe-only residency repair aaa70c9. Production `OpenAdapter`, positive
rendering caps and ordinary DX8–DX11 runtime acceptance remain closed.

`CreateResource` constructs an ordered group of nonshared A8R8G8B8/X8R8G8B8
plain or nonsampled render-target surfaces. It snapshots descriptors before
callbacks, saves the runtime resource cookie separately, unwinds partial
creation and checks adapter identity again before publishing a unique driver
token. `SetRenderTarget` selects RT0 and a group subresource. `Clear` preserves
the native difference between preclipped rectangles and `COMPUTERECTS`,
including the zero-rectangle no-op. `Blt` supports straight same-format copies,
overlap through a temporary target, system-to-video upload and actual GPU
readback to system memory.

Caller-owned system memory retains its pointer and pitch. `Lock`/`Unlock`
validate the `NotifyOnly` tag and CPU view lifetime. Resource destruction
flushes before retirement; retryable failures retain ownership. Device
teardown discards outstanding views without reading expired caller memory,
releases all renderer resources on the worker, then closes runtime backing.
All operations use the existing caller callback pump and reject nested or
concurrent access with `WASSTILLDRAWING`.

The native ARM64 controlled fixture passes 77,449 checks. Its renderer is a
substitute; the actual typed adapter/device/RuntimeGpu/callback dispatch run.
Three independently compiled source mutations fail at their intended behavior
assertions: post-create identity cancellation, `NotifyOnly` validation and
descriptor snapshot against callback mutation. All 52 fixture inputs, source
archives, native executable hashes and driver/desktop continuity are verified
in workspace `artifacts/dxvk-native-d3d9-resources-20261006/guest-*/verified.json`.
The fixture does not prove the production renderer or GPU pixels.

The new `<LUID> --render` target probe checks three readback stages, all 192
pixels, independently calculated checksum `ffefa655`, row padding and guards,
two render-target subresources, balanced lifetime and nonempty runtime render
callbacks. Run it with the exact full-CI ARM64 candidate and matched Mesa
`8443c71a5ab32b9d58b904fa51f4bf2f9089db8d` under the measured Limited
interactive user in the existing VM. Keep original failed receipts unchanged.

Target runs16/17 reach successful resource construction, RT0 binding and clear.
Probe-only source84eb920 traces the first readback submission:484command bytes,
six allocations and six patches; KMT returns `STATUS_UNSUCCESSFUL` (`c0000001`).
The30sprocess deadline stops the stalled readback. Both compressed receipts,
all eight binary/manifest inputs, three scripts and Limited USER/session1 token
are independently verified. Driver58624/oem17 and desktop processes are retained;
active-binding render-failure/reset/epoch/timeout diagnostics remain unchanged.
No pixel success or balanced teardown is claimed for these failed runs.

Probe-only `aaa70c9d481828806addc4ab570e55b6108293ec` supplies the missing
WDDM2 paging queue and balanced allocation-residency references before raw
KMT rendering. Pending paging fences are waited on before submission; each
reference is evicted before its allocation is destroyed. Its native ARM64
guest build verifies all51 source inputs and the executable independently;
offline CI37472553344 passes all four jobs. No production UMD, Mesa or KMD
change was needed to pass the workload.

Targets18/19 run the same production UMD/Mesa/KMD and corrected probe under
Limited `DROIDVM\USER`, session1, with diagnostics1/0 respectively. Both pass
all192 actual pixels, checksum `ffefa655`, pitch/padding guards and two target
subresources. Three nonempty KMT submissions return success. Context1/1,
allocations7/7, locks6/6 and residency6/6 balance; wrong-thread callbacks are0.
Measured process times are1.2268632s and1.166842s. Eight payload hashes, three
wrapper hashes and native ARM64 identity per receipt independently verify.
Fresh active SYS/PnP checks retain signed58624/oem17 and binding0002;
DWM1552/Explorer4184 persist and failure/reset/epoch/timeout/admission values
do not change. Owned tasks and the bounded target18 ETW session were removed.

Evidence is in workspace `artifacts/dxvk-native-d3d9-resources-20261006/`:
`render-18/verified.json`, `render-19/verified.json`, compressed receipts,
native probe source/build receipts and fresh readiness snapshots. Target18
archive SHA256 is `683421a34386bc0678cc0d90e1d4a0e64048006e3736618d44ea78922aedb466`;
target19 is `f2c9d3a77c4a8b2eb6fd95de7c207534b6048be7c6b12353b1ea4dd1b95f6826`.
This corrects the measured harness refusal in16/17; the older09 lifecycle
device-loss cause and long-term stability remain separate open questions.

Textures/buffers, shader and fixed-function draws, remaining state,
depth/stencil, sharing, queries, runtime-owned presentation/reset and ordinary
system runtime activation remain required. A clear/readback success alone
does not close the full DX8–DX11 goal.
