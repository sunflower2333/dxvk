# Native DX10 and DX10.1 format-query slice

The native format callbacks now publish a result only after an actual `S_OK`
backend query. A partial backend write, failed HRESULT, positive non-`S_OK`
status, or exception leaves runtime output at zero. Successful single-sample
queries return one quality level; count zero or greater than 32 returns zero
without entering the backend. Count one still queries the backend so an
invalid format retains its `E_INVALIDARG` error. Null outputs are rejected
before the backend is called. This follows the
[native multisample contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_checkmultisamplequalitylevels).

The five ordinary legacy format bits retain their existing mapping from the
renderer: shader sample, render target, blendable, multisample render target,
and multisample load. Blendable and multisample render target remain gated
by render-target support. Public D3D11 IA, buffer, UAV, display and video bits
are not reinterpreted as 10.0/10.1/11.0 DDI bits. A failed format query maps
`E_INVALIDARG` to the native format callback's `E_FAIL` contract; other failures
remain failures. See
[CheckFormatSupport](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_checkformatsupport).

XR_BIAS explicitly returns only `D3D10_DDI_FORMAT_SUPPORT_NOT_SUPPORTED`.
Microsoft defines this format's attributes as scan-out, CPU lock and cast,
with views of another format used for rendering. The current native primary
and allocation path accepts only RGBA8/BGRA8; the renderer's DISPLAY capability
does not implement paired KMD XR scan-out. This is a rejection of an
unimplemented format, not an HDR-support claim. See
[XR_BIAS requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/dxgi-format-r10g10b10-xr-bias-a2-unorm).

## Exact ABI and remaining completeness requirements

This audit used the original WDK26100 `d3d10umddi.h`, the SDK26100 `d3d10.h`
and `d3d10_1.h`, and the local Microsoft DDI sources under
`reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/`.
The existing general display-guide checkout was read in place at
`windows-driver-docs/windows-driver-docs-pr/display/`; no active checkout was
moved. The local table-audit receipt records the original header hash and
every field name.

| Area | Exact native contract and current source | Remaining requirement |
| --- | --- | --- |
| Mandatory table | `D3D10DDI_DEVICEFUNCS` has 101 ordinary slots; `D3D10_1DDI_DEVICEFUNCS` has 103. The fixture checks each member through its typed table. `ResetPrimitiveID` and `SetVertexPipelineOutput` are only `D3D10PSGP` rasterization slots. | Non-null callbacks establish the table shape; accepted workload semantics still require validation. |
| 10.1 table changes | SRV size/create, blend size/create and relocation have their actual 10.1 types. ResourceConvert and ResourceConvertRegion are the two added slots. | Conversion currently accepts identical formats only; full format conversion remains closed. |
| Shader | VS/PS/GS create/size/destroy/bind and GS stream-output callbacks are populated. The legacy container path requires version token `0x40` and limits opcodes through `eDclGlobalFlags`. | SM4.1 token `0x41`, LOD/gather/sample-position/sample-info and complete signature/system-value semantics need a separate coordinated shader slice. |
| Query/predication | Event, occlusion, timestamps, disjoint, SO statistics/overflow and pipeline statistics have explicit mappings. Legacy pipeline output is eight 64-bit counters, not D3D11's eleven. | The DX11 agent owns the independent query-child lifetime registry and native11 mapping slice; this format commit changes none of it. Full runtime/Turnip predicate/query proof is pending. |
| Input assembler | Create/destroy/bind layout, topology, vertex/index buffers and draw variants are populated. SDK10.0 has 16 vertex slots/registers; SDK10.1 has 32. | The coordinated DX11 slice owns logical-level bounds, typed IA-format checks, independent layout children and actual backend IA support. |
| Resources | Legacy creation covers buffers and ordinary 1D/2D textures. The typed10.1 cube SRV translator exists, but its legacy resource factory still has no cube/3D creation path. | Full cube/3D/resource-dimension semantics, block/packed/depth transfer contracts and shared/primary ownership remain admission requirements. |
| Format/MSAA | This commit stages query output, normalizes successful count1, bounds counts and rejects native XR_BIAS explicitly. | Standard and center multisample-pattern creation and complete mandatory format conformance still need actual workload proof. |
| Counters/text filtering | No device-dependent counters are advertised; optional counters report unsupported. Text filtering accepts the identity 1x1 request. | These optional capability choices must stay distinct from mandatory shader/resource functionality. |

`umd_contract.cpp` and the separate 10.1/11 requirements remain unchanged.
Runtime threading, complete resources/shaders/MRT, opened/shared resources,
primary/DXGI, predicates and SO still prevent production feature-level
admission. Live runtime-owned core callbacks, typed adapter negotiation and
device dispatch/retirement are preserved.

## Validation and native build requirements

`tests/umd-multisample-policy.cpp` executes 269 checks on Linux with GCC and
with Clang ASan/UBSan, both with `-Wall -Wextra -Werror`. It covers every count
0 through 40, UINT32_MAX, backend writes on failure, exceptions after writes,
and successful count1 queries whose backend reports zero or many qualities.

Official-header Clang Windows COFF generation passes for both x64 and x86
for the actual production `umd_ddi.cpp`, new `umd-d3d10-formats.cpp` and existing
`umd-view.cpp`, with six objects and zero diagnostics under
`-Wall -Wextra -Werror`. Unchanged pinned dxbc-spirv headers are system includes;
SDK/WDK/MSVC headers are original, with only the established case-insensitive
VFS overlay and Windows/winternl NTSTATUS prelude. These are local compiler
checks, not MSVC ARM64 execution or WARP runtime results. An initial syntax
preflight incorrectly used static_assert on a runtime policy function; it
was corrected to a runtime check before the recorded COFF builds.

The new Windows fixture invokes both exact native tables and Microsoft's CPU
WARP as a substituted private renderer. It covers the native error/status
transactions, nine real backend format queries, sample counts 0..33, output
canaries, every ordinary table slot, in-place core callback replacement, and
DestroyDevice reentry from an error callback. The runtime-owned core tables
remain Fixture members through destruction. Actual fixture execution is
pending root-coordinated CPU ownership; its build has no installed-UMD,
registry or genuine system D3D10 activation step.

Build the native fixture with the shader object list in
[the D3D11 source manifest](native-d3d11-ddi-20261007.md), the four DDI objects
listed there, `src/umd/umd_contract.cpp`, and
`tests/umd-d3d10-formats.cpp`. The two new format headers and table-check header
are included source inputs and add no separate production object. Link the
same official static CRT/Windows import libraries as the established native
WARP packet, including `d3d11.lib` and `d3dcompiler.lib`. Keep `/W4 /WX /MT
/std:c++17 /EHsc /Zc:preprocessor` for UMD and fixture objects and the pinned
dependency's repository warning policy. Root owns consolidated build/run-list
integration. The portable policy fixture needs only its own source and header.

Original local diagnostics, objects, commands, hashes and execution output
are retained under the isolated worktree's
`artifacts/dx10-format-local-20261007/`. The base source is exact
`7c2c8efb06dd4c6d6e57edc10950328cbdc3147d`.

## Genuine Microsoft-runtime activation still required

The Microsoft runtime must load the registered UMD and call `OpenAdapter10`
or `OpenAdapter10_2`, query the paired KMD, negotiate an implemented interface,
and create the device through its returned native adapter table. Core, kernel
and DXGI callback ownership then follows the selected typed DDI. See the local
guide and [version10 initialization](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/initializing-communication-with-the-direct3d-version-10-ddi).

The installed names are ordered D3D9/D3D10/D3D11; using the same binary in more
than one slot requires that binary to implement each corresponding interface.
Correct exports alone do not satisfy installation or native feature
requirements. Package registration, native/WOW/ARM64EC architecture dispatch,
the paired KMD's identity and the production capability gates must all agree.
See [ordered UMD registration](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/enabling-support-for-the-direct3d-version-11-ddi).

Acceptance therefore requires absolute System32 Microsoft d3d10.dll and
d3d10_1.dll provenance, exact target-adapter identity, actual runtime-driven
adapter/device DDIs, native10.0 and10.1 feature-level behavior, resource/draw/
query/readback and presentation lifetimes on that adapter, and orderly
teardown/reset. These are project acceptance criteria, not an inference from
CPU results. `tests/wddm-d3d10-smoke.cpp` explicitly expects matching DXVK and
Turnip beside the app and requests only FL10_0; it cannot establish this
System32/native10.1 replacement. This slice performs no target commands,
registration, installation, production push or CI dispatch.
