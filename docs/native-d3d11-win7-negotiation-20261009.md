# D3D11 Windows 7 DDI negotiation

The actual ordinary SYSTEM experiment at `55be-hardware-03` adopted the exact
validation frontend after the adapter restart. The frontend recorded successful
OpenAdapter, pipeline support and supported-version queries, followed by
CloseAdapter. It recorded neither CalcPrivateDeviceSize nor CreateDevice.
SYSTEM D3D11CreateDevice returning S_OK at logical FL10.0 therefore did not
establish that the selected DXVK frontend created the device. The unchanged
typed-device guard correctly rejected this run before any pixel oracle.

The core had advertised only `D3D11_0_DDI_SUPPORTED`, interface `0x000b000a`,
build 2. The original Microsoft SDK also defines a distinct Windows 7 revision,
`D3D11_0_7_DDI_SUPPORTED`, interface `0x000b000b`, build 2. Microsoft's
Initializing Communication with the Direct3D Version 11 DDI describes the
Vista and Windows 7 versions separately. Both use `D3D11DDI_DEVICEFUNCS` and
`D3D11DDI_CORELAYER_DEVICECALLBACKS`; the kernel callback prefix is shared.
`IS_DXGI1_1_BASE_FUNCTIONS` independently selects the DXGI base table using the
low runtime-version bits. Supporting the exact Windows 7 revision does not
select D3D11.1, GPU virtual-address submission or AllocationInfo2.

The adapter now recognizes and lists both exact typed11 revisions. The existing
typed11 creation path retains the original interface, version, flags, live core
callbacks, kernel callbacks and runtime identity. Production capability masks
remain closed; the dedicated validation adapter still admits only FL10.0, with
no optional threading or shader bits. Older build numbers and unrelated DDI
versions remain rejected. This repairs a concrete missing version contract;
whether it explains the Windows runtime's rejection of the earlier list must
be verified by a fresh ordinary SYSTEM run using the rebuilt core and frontend.

Frontend telemetry and the offscreen and Present probes now record and accept
the two exact typed11 revision identifiers. Their independent readers require
the exact two-entry supported list and successful typed11 CreateDevice with
live callbacks. They report the actual created interface. They retain the
512-pixel literal oracle, exact module ownership, hardware identity, process
lifetime and restoration requirements. Historical results retain their frozen
readers and are not relabeled as successes.

Local validation passed twelve optimized strict Microsoft SDK COFF compilations
for x86/x64, including the adapter and guarded fixture, frontend, forwarding
fixture and both ordinary probes. Synthetic offscreen and Present reader
controls rejected 79 and 113 corruptions. GCC and Clang ASan/UBSan exact-source
integer-policy replays each passed 1,310,988 checks; the original adapter body
failed the added Win7 admission control. These are local compilation and
synthetic policy/format results, not Windows device creation or GPU evidence.

The audit also identified an existing immediate-context requirement independent
of DDI revision: Microsoft's Changes from Direct3D 10 requires
`pfnPerformAmortizedProcessingCb` on the entering DDI thread after submission,
or at comparable frequency. That submission change is tracked separately.

Original local Microsoft material used:

- `windows-driver-docs/windows-driver-docs-pr/display/initializing-communication-with-the-direct3d-version-11-ddi.md`
- `windows-driver-docs/windows-driver-docs-pr/display/supporting-threading--command-lists--and-3-d-pipeline.md`
- `windows-driver-docs/windows-driver-docs-pr/display/changes-from-direct3d-10.md`
- SDK/WDK `10.0.26100.0/um/d3d10umddi.h`, version definitions and common tables.
