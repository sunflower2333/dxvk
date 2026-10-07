# Genuine x86 Microsoft D3D8 enumeration frontend

This slice prepares an owned, process-local read-only frontend for the actual
target's I386 Microsoft SysWOW64 `d3d8.dll`. Production admission, registered
drivers and caps are unchanged. Native Windows build/execution and genuine
runtime trace are separate gates; local compilation is not hardware evidence.

The original Microsoft DLL SHA256 is
`65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8`.
Original bytes set Interface8 at RVA12e42 before OpenAdapter RVA12e73;
its GDI32 KMT import is a four-byte IAT slot at RVA b11d4. It imports
`d3d8thk.dll`, not Microsoft's D3D9 API. System32 ARM64 D3D8 is absent.
The official WDK defines GETD3D8CAPS as12. The integrated adapter projects
the exact212-byte D3DCAPS8 prefix, with bounded VS1.1/PS1.4 declarations and
96 vertex constants, and refuses API9 caps for an immutable API8 owner.
Original receipts are under workspace artifacts/dxvk-native-d3d8-runtime-20261007
and artifacts/dxvk-native-d3d8-port-20261007.

`tests/umd-d3d8-runtime-front.cpp` builds only as I386 and exports only
`OpenAdapter`. It requires `read-only-d3d8-interface8-v1`, Interface8 and a
return address inside the genuine system I386 D3D8 module before dereferencing
runtime callbacks or loading a core. The future core must be the unchanged
original I386 production DLL from a successful exact-source consolidated CI.
The b75 native x86 checkpoint built fixtures, not a full production DLL.
Its eleven uncompiled source references in this packet establish the API8
baseline; they do not establish an available b75 core binary.

At staging, independently join the actual CI run/source, raw artifact ZIP/API
digest/CRC, original DLL member SHA256, I386 architecture and original exports.
Copy that original DLL into
`C:\Users\Public\DxvkD3D8Candidate-<commit7>-<actual-CI-run>\viogpudxvk.dll`.
Supply its exact owned path, full SHA256 and full40-digit source commit to the
probe. Do not rebuild, patch, rename to a public graphics API or attribute an
older DLL to that new source. The frontend snapshots these process-local pins,
validates the owned path/source prefix and file I386 header, and verifies the
actual SHA256 using BCrypt before loading. Its read-only file handle denies
write/delete sharing through load and the process lifetime; resolved paths and
preloaded conflicting cores are rejected. Later pin mutation is rejected.
The log labels the source commit as the expected CI identity from the receipt,
not information derived from PE bytes.
It uses only the existing typed development adapter export, demands Vista
DDI000c, forwards all GetCaps arguments/results unchanged and logs all53 raw
CAPS12 words. It preserves owned original adapter callbacks until close and
always blocks CreateDevice. No diagnostic capability bits are inserted.

The genuine runtime probe dynamically loads the absolute system D3D8 image.
`--front-enumerate <owned-frontend> <exact-installed-WoW-driver-name> <core-path> <core-sha256> <core-source-commit>` replaces
only its original GDI32 name-query IAT slot. Original KMT status, private-query
arguments, unmatched names, versions and output bytes remain unchanged.
Expected and replacement names are owned immutable snapshots; callbacks pin
that owner across the original query. Restoration stops publication, restores
the exact slot/protection and keeps an original-query fallback for callbacks
already dispatched. Both normal and error paths attempt hook/permission
restoration; a failed hook restoration rejects the run. The permission and
three core-pin environment variables must all start absent and are removed
after restoration. No registry write occurs.

`--front-guard <owned-frontend>` is a CPU-only loader guard. It verifies disabled
and wrong permission, enabled null input, six rejected API interfaces with
poisoned callback/table addresses, malformed API8 and a non-system API8 caller,
with all arguments unchanged and no core or system runtime loaded. The guard
unit uses the actual modern SDK DDI headers separately from the legacy D3D8 API
headers. Modern Microsoft SDKs omit the latter; three exact licensed legacy
headers are retained from the original already-verified native x86 packet.

The prepared native build helper compiles four strict `/W4 /WX /MT` translation
units into four I386 COFFs, one frontend DLL and two executables. It executes
only the306-check policy fixture, the frontend guards and14 malformed CLI
cases. It never executes valid enumeration, selector, offscreen or presentation
modes. Native parser/build execution is deferred until exclusive target
ownership is granted. Every source/compiler/header/library before/after hash,
original response/command/log/object/PE and original target state is retained.
The collector includes failures rather than replacing their originals. The
native CPU packet does not require any production core to be present and does
not build one. The frontend uses the original official x86 `bcrypt.lib` for
hashing; its header/library and all other compiler/SDK inputs are retained.

`scripts/test-native-d3d8-runtime-policy.py --output <fresh-directory>` runs
GCC and Clang ASan/UBSan positives and eight exact semantic controls. Controls
ignore original KMT status, ignore name-query version, accept a different
source prefix, or accept a nonhex identity; each must compile and reach its
intended assertion. Windows API execution is outside this portable test.

The older DX8 local/packet/CPU entrypoints include the new guard unit and its
headers when linking the existing runtime probe. Their local cross gate passes
eighteen COFFs and nine executables across I386/AMD64/ARM64; that verifies
compilation, not target runtime availability. The genuine target D3D8 frontend
and future runtime acceptance remain I386-only.

## Matching private payload gate

The verified earlier affe x86 CI artifact is a real I386 embedded core, but
predates the integrated Interface8 bridge. It cannot satisfy the future core
identity. Read-only typed adapter opening/caps do not create a Vulkan device.
For the later hardware gate, inspect the actual future CI build command and
generated loader configuration against its exact committed source. The b75
loader reference selects `umd_vulkan_loader` at build time, with no runtime
environment override; its default public names do not match the existing
private Khronos basename. A build configured with
`-Dumd_vulkan_loader=viogpu_gl_loader_x86.dll` resolves that original loader
beside the core without public-name fallback. This packet does not rebuild an
artifact to change that setting or relabel a rebuilt binary as the original CI
DLL. Record any configuration mismatch as a distinct hardware prerequisite.
The original matching x86 Mesa payload provides:

| File | Architecture | Original SHA256 |
| --- | --- | --- |
| viogpu_gl_loader_x86.dll | I386 | d459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7 |
| viogpu_gl_vk_x86.dll | I386 | 2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c |

The loader exports `vkGetInstanceProcAddr`; the ICD exports the separate
`vk_icdGetInstanceProcAddr` and negotiation hooks. Preserve the private ICD JSON,
all original ZIP/API/member digests and the matched32-bit MMD callback ABI.
Nothing is copied to the target or renamed to a public API basename by this
slice. The packet emits a precise matched-payload requirement receipt with
future core/run/path/hash fields pending rather than invented values.

Actual acceptance still requires a matching core/raw artifact, exact WoW driver
name from a fresh kernel query, genuine Microsoft Interface8/CAPS12/HAL trace,
then real x86 FVF/SM1 pixels, reset and presentation on the exact-LUID Adreno
backend. The existing `--offscreen` probe remains a future rendering test and
is not part of this read-only/native CPU task. App-local DXVK D3D8 and WARP
results do not satisfy these gates.
