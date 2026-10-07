# Native D3D9 discard render targets, 2026-10-07

The genuine Microsoft System32 runtime now reaches the native resource callback
for both HAL9 and HAL9Ex. Original `device-lifecycle-02` requests a16x16 X8
windowed back buffer with flags `0x1081` (RenderTarget, NotLockable and
DiscardRenderTarget), local video-memory pool3 and mip count0. The earlier mask
rejects the documented discard hint and returns `E_INVALIDARG` before backend
surface creation. Both public devices therefore fail with `80070057`.

Accept DiscardRenderTarget only with the implemented color render-target
contract. Its permission to discard old contents allows the existing preserving
backend. The hint adds no primary, sharing, format, pool or presentation claim.
Non-target, depth, buffer, primary, shared and reserved-bit requests continue to
reject with untouched caller outputs. Resource metadata remains snapshotted
before callbacks. The original back-buffer rejection/restoration evidence is
under `artifacts/dxvk-native-d3d9-interface-20261007/`; its root review is
`root-system-runtime-device-lifecycle-02-verified-01.json`.

The fixture exercises the actual runtime shape for A8/X8 and lockable/nonlockable
surfaces. It checks failure/retry, caller mutation, binding, clear, copy, all256
readback pixels per combination and balanced resource lifetimes. These are
controlled backend tests. Exact `4e91632` now passes actual ARM64 MSVC
`/W4 /WX /MT`, with43067 adapter and554085 device checks and zero warnings.
Root independently verifies the original141-member evidence,74Git plus15
pinned Vulkan inputs,9COFF/2PE,21official headers/5libraries, both protected
Vista99-pointer publication paths and unchanged signedSYS/desktop/System32DLL.
Proof: `artifacts/dxvk-native-d3d9-discard-20261007/root-native-discard-target-fixtures-verified-01.json`.
Genuine-runtime GPU acceptance remains pending. Official-header x64/x86 COFF compilation
passes for both implementation and fixture. The first strict x86 implementation
compile exposed an existing signed light-count comparison; the bounded count
now converts to size_t. Original failed attempt and accepted fresh implementation
objects remain under `artifacts/dxvk-native-d3d9-discard-20261007/`.

The consolidated build script now builds and packages the new typed DX11 device
and compute-container fixtures for all three architectures, executes them on
x64/x86 and includes them in native ARM64 validation (17 cases total). Existing
production runtime/capability gates remain closed. Higher shader stages,
ordinary DX8-DX11 hardware rendering and visible presentation remain pending.

The separate `68d0d4c` frontend/probe pins only the fixed discard candidate
path and `device-lifecycle-4e91632` permission. Root native review02 verifies
60Git inputs, original53-member archive,2COFF/2PE,10malformedCLI/3nullguards
and zero warnings. It performs only the read-only SSH enumeration baseline;
the new candidate is absent and no core/device lifecycle is executed. The
review01 missing reused SDK receipt and its correction remain retained.

Contract: [Microsoft resource flags](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dukmdt/ns-d3dukmdt-_d3dddi_resourceflags).
The corresponding official SDK26100 header and Microsoft DDI source are local
workspace references used for this change.
