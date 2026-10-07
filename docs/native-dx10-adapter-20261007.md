# Native DX10/DX10.1/DX11 adapter selection

The adapter now selects exact `D3D10_0_DDI_INTERFACE_VERSION`,
`D3D10_1_DDI_INTERFACE_VERSION`, and `D3D11_0_DDI_INTERFACE_VERSION` contracts.
Each device table has its own typed local storage and output union member.
The shared selector checks the matching build minimum and accepts only the
implemented creation flags. DX11 selects 10_0, 10_1, or 11_0 pipeline levels
with an optional SINGLETHREADED flag. Extra-thread suppression, unknown flags,
FL9, extended DX10 interfaces, and newer DX11 interfaces remain rejected.

Legacy OpenAdapter10 binds the requested DX10.0 or DX10.1 interface.
OpenAdapter10_2 negotiates later through GetSupportedVersions and ignores the
initial interface/version fields. Production advertisement is separate from
development table exercise: DX10.1 retains its additional semantics gate, and
DX11 retains independent immediate, compute/UAV, tessellation, and shader
interface requirements. Existing DX10 production requirements remain closed.
Current SDK definitions or non-null table slots cannot admit a feature level.

DXGI 1.0 versus 1.1 follows the actual WDK
`IS_DXGI1_1_BASE_FUNCTIONS(Interface, Version)` macro. The lower runtime version
bits select this table independently of the D3D device ABI. Both DXGI layouts
use typed storage, including a short 1.0 table for a DX11 runtime revision that
requires it.

Creation snapshots the selected metadata, used historical kernel callbacks,
and original output destinations before identity queries.
It preserves the original runtime-owned DXGI and DX10/DX11 core callback table
pointers because their entries remain live. A successful backend has a typed
DestroyDevice cleanup guard until a second identity query and synchronized
output publication succeed. Close/reset, a post-creation query failure, or an
exception retires that backend without publishing either output table. Adapter
tokens are never reused, so a closed handle cannot identify a later adapter.
GetCaps and version-capacity ownership are also snapshotted before callbacks.

The native adapter fixture links the actual adapter/query/production policy
and substitutes only the backend. It exercises 144 creation transactions over
three device ABIs, two DXGI layouts, all admitted pipeline/threading flags,
failure and positive-status normalization, close/reset, and query exceptions.
Actual Windows guard pages bound device tables, DXGI tables, historical
creation input, and used callback prefixes. Callback mutation controls verify
original output, runtime handle, callback, version-capacity, and caps ownership.
It checks that development paths expose no production feature capabilities.

Local x64 and x86 Windows COFF code generation with official SDK/WDK26100
headers passes with `-Wall -Wextra -Werror` for the adapter, fixture, query, and
contract source. Baseline device and native-entry source syntax checks also pass.
The local ARM64 clang18 attempt fails in the MSVC intrinsic headers because
the compiler's `__prefetch` builtin disagrees with the official declaration;
it does not reach a successful ARM64 validation. Receipts are retained in the
worktree at `artifacts/dx10-adapter-validation-01/`. Actual native MSVC ARM64
fixture execution, consolidated CI, and ordinary Microsoft runtime/Turnip
activation remain required. This adapter commit does not replace the installed
Mesa UMD or claim hardware rendering acceptance.

The device factory is supplied by the coordinated typed DX10.1/DX11 device
port. Its existing `createAdapterDevice(identity, args)` boundary is preserved.
The isolated adapter fixture builds from `tests/umd-adapter.cpp`,
`src/umd/umd_adapter.cpp`, `src/umd/umd_runtime_query.cpp`, and
`src/umd/umd_contract.cpp`, with the corresponding source headers and official
SDK/WDK includes. Its guard pages require normal Windows kernel32 imports;
the fixture has no Vulkan or public D3D runtime dependency.

Primary contracts: [CreateDevice arguments](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_createdevice),
[modern version negotiation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10_2ddi_getsupportedversions),
[live DX10 core callbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddi_corelayer_devicecallbacks),
[live DX11 core callbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d11ddi_corelayer_devicecallbacks),
and [DXGI output unions](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dxgiddi/ns-dxgiddi-dxgi_ddi_base_args).
