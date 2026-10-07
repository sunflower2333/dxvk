# Native adapter entry and device lifetime

The DLL now exports typed WDK `OpenAdapter10` and `OpenAdapter10_2` in addition
to explicitly named development helpers. `OpenAdapter10_2` ignores the initial
Interface/Version and returns the native adapter callbacks. The runtime then
queries versions and capabilities before choosing a CreateDevice interface.

This is still not an installable system UMD. The immutable production policy
in `umd_contract.cpp` reports the remaining D3D10 feature contract gaps, so
GetSupportedVersions returns zero supported interfaces and GetCaps reports no
pipeline level. Legacy OpenAdapter10 and native CreateDevice reject incomplete
interfaces. There is no environment variable, registry value or production
override that enables a partial feature level. The separate development helper
retains its bounded test purpose.

## Implemented runtime contract

- QueryAdapterInfo uses only the original opaque runtime adapter handle and
  its original callback. The existing 160-byte KMD reply is unchanged. LUID,
  nonzero ResetGeneration and capability bits are retained, while unknown or
  truncated identity trailers and nonzero reserved fields fail.
- Negotiation and private-size queries revalidate identity. CreateDevice
  checks before and after backend creation, so reset, LUID/capability drift,
  callback failure or adapter close prevents publishing the device table.
  Identity drift permanently retires that adapter instance.
- Adapter registry ownership pins in-flight calls across synchronous close.
  No registry lock spans a runtime callback. A recursive query reports
  WAS_STILL_DRAWING instead of recursing or deadlocking.
- Exact D3D10.0 typed tables and runtime-supplied private storage are used.
  D3D11 unions, unsupported threading flags and underspecified original
  callbacks are rejected. Only used kernel/core callback slots are copied; no
  current SDK table is blindly copied over an older runtime's allocation.
  DXGI requires retaining its runtime-owned callback table pointer because its
  callback addresses can change between UMD entries. That table stays live for
  the device, and Present reads its current callback slot.
- Successful device creation publishes local D3D and DXGI tables together
  after validation. Failure cleans the placement-created backend. A registry
  prevents duplicate creation in live/creating storage and protects
  destruction from reentrant DestroyContext and SetError callbacks. Existing
  devices retain immutable adapter identity independently of CloseAdapter.
- The real DLL continues to call the embedded DXVK backend with exact
  LUID/Turnip selection. It does not import public D3D11 device creation or
  change the original runtime kernel/DXGI callback handles.

## Validation boundary

`umd-native-entry.cpp` is built twice with the actual `umd_adapter.cpp`,
`umd_ddi.cpp`, runtime query and allocation source. One build links the real
closed production capability policy and confirms that it exposes no
incomplete interface. The other links a complete-contract fixture only in the
test executable so successful CreateDevice and DestroyDevice can be exercised.
Both tests use controlled QueryAdapterInfo replies. Their backend provider is
WARP; the shipped DLL has neither replacement.

The successful fixture checks allocation, synchronized readback/publication,
original runtime resource/device/core/DXGI handles, copied kernel/core table
lifetime and live DXGI table lifetime, pixel
bytes, generation change during creation, close during creation, duplicate
CreateDevice, nested queries, nested context destruction, cleanup and storage
reuse after failed creation. This proves those production code paths under a
controlled runtime/backend; it is not ordinary Microsoft runtime activation,
Vulkan/Turnip hardware proof or visible display acceptance.

The Windows CI builds actual WDK ARM64, x64 and x86 binaries. The x64 and x86
fixtures execute on the Windows runner; the separate test binaries are also
included in each architecture artifact for the target owner. ARM64 execution
requires the target. Linux ASan/UBSan checks the architecture-independent identity and
shader parsing paths. The build verifies exact real adapter exports and still
rejects public D3D11 creation imports from the production DLL.

## Remaining implementation sequence

1. Complete the chosen D3D10 feature contract before removing any admission
   gap. The mandatory hardware device table is populated, including bounded
   stream output/DrawAuto, synchronous predication, opened shared resources and
   identity text-filter handling. Those paths still need complete semantics
   and runtime/Turnip acceptance. Remaining requirements include resource
   dimensions, shader semantics, full multiple-render-target behavior and
   null-GS stream-output passthrough. ResetPrimitiveID and
   SetVertexPipelineOutput belong to D3D10PSGP rather than this hardware table.
2. Use the coordinated KMD/Mesa shared resource protocol for original runtime
   allocations, exact adapter/reset ownership, share/open, lifetime and
   synchronization. OpenResource failure must clean its own partially created
   objects because the runtime will not call DestroyResource after failure.
   Do not substitute DXVK's Wine metadata escape or an unowned Vulkan import.
3. Finish primary/backbuffer and full DXGI base operations, including mode,
   presentation and allocation ownership. Present and resource rotation now
   occupy two of the seven base callback slots. Retain the present windowed blit
   path as a bounded baseline; it is not primary/flip or zero-copy acceptance.
4. Implement an actual WDK D3D11 table with signature-specific wrappers and
   negotiated flags; do not copy D3D10 table bytes into the D3D11 union. Admit
   each pipeline level only with all mandatory shader/resource/query behavior.
   FL11_0 additionally needs compute, UAV, tessellation and other required
   functionality beyond the existing D3D10 subset.
5. Integrate separately selectable signed UMD registration and architecture
   selection. Use the parent's ordinary system-d3d11/dxgi test with exact
   VIOGPU LUID and loaded-module hashes, starting with explicit 10_0 and then
   the formal default 11_0 gate. Current closed policy should not be installed
   as an alleged working graphics upgrade.
6. Native D3D9 is a separate OpenAdapter/D3DDDI_DEVICEFUNCS adapter over the
   internal DXVK D3D9 backend and the same native allocation ownership model.
   Replacing an application's d3d9.dll is not native UMD implementation.

Official contracts reviewed in the workspace Microsoft DDI documentation:
OpenAdapter10_2/GetSupportedVersions, D3D10_2DDI_ADAPTERFUNCS/GetCaps,
D3D11DDI threading/shader/pipeline capabilities and QueryAdapterInfo. Actual
WDK types and three architecture compilation remain authoritative for ABI.

## Null-code stream output in development DDIs

The D3D10 and D3D11 development tables accept a bounded null-code geometry
stream-output object. The object retains the runtime output signature and SO
declarations. Draw links the actual prior-stage outputs and creates the backend
SO geometry object from the active vertex shader DXBC, or the active domain
shader DXBC in a D3D11 patch pipeline. The cache compares complete prior-stage
bytecode, so switching shaders cannot reuse another shader's stream program.
No shader instruction program is synthesized for a null-code object.

This slice supports plain register varyings and SV_Position, with at most 32
unique output registers. D3D11 null-code SO currently uses stream 0, no rasterized
stream and a prior shader without class-interface slots; other combinations
report E_INVALIDARG. The ordinary runtime admission masks remain unchanged.

The stream-output fixture adds three vertex-shader rebind draws, 72 raw 32-bit words,
tail canaries and a missing-output rejection. The SM5 fixture adds the same
raw-bit checks with an original public D3D11 API comparison, and a 1024-word
domain-shader capture compared with original VS/HS/DS blobs. Original Microsoft
SDK/WDK/MSVC-header x86/x64 compilation passes; native fixture execution and
Turnip/system-runtime acceptance are separate pending gates.
