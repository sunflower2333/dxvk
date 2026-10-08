# Legacy ValidateDevice forwarding

The Vista D3D8/D3D9 device table now supplies the original typed
`PFND3DDDI_VALIDATEDEVICE` callback. It calls the embedded DXVK
`D3D9DeviceEx::ValidateDevice` through `D3D9Backend`, forwards its HRESULT and
publishes its actual pass count. The bridge does not choose a pass count.
The existing renderer method currently reports one pass and checks device
loss; comprehensive validation of every fixed-function state combination is
outside this change.

The [Microsoft DDI contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dumddi/nc-d3dumddi-pfnd3dddi_validatedevice)
returns the pass count for the current blending state or an appropriate error.
The original 26100 WDK declares `NumPasses` as an output and the table slot as
`HRESULT APIENTRY (HANDLE, D3DDDIARG_VALIDATETEXTURESTAGESTATE*)`. The bridge
stages that output, rejects a zero-count or non-`S_OK` success, and checks the
original adapter identity after backend validation. It publishes only while
the retained device remains serialized and live. Unknown, retired or busy
device handles never read or write the pointed output.

Existing device ownership, HRESULT normalization, caller-thread callback
pumping and Vista table bounds remain in use. Backend exceptions become the
existing allocation or generic failure result. Device loss remains terminal;
reentrant or concurrent destruction of a busy device receives the existing
retry result. No capability flags, modern profile masks or runtime exports
are changed.

`tests/umd-d3d9-device.cpp` now exercises this exact typed table slot with the
existing native adapter/device/runtime callback implementation and controlled
renderer. It covers both legacy API identities, pass counts 1/3/17/UINT_MAX,
renderer errors and exceptions, malformed successful results, stale handles,
reentrant and concurrent calls, adapter retirement/reset, and protected output
memory after a pumped callback. The existing fixture target, pixel/lifetime
tests and final stdout pattern remain in place; CI will execute the additional
controls in the same target.

Local checks compile the device implementation and fixture as optimized x86
and x64 Windows COFF with the original SDK/WDK, and replay unchanged production
ownership/output bodies under GCC and Clang ASan/UBSan. The full renderer bridge
uses the existing repository renderer warning policy because its unchanged
renderer headers require that policy. These local checks establish source
and ABI compilation, not native Windows fixture execution or ordinary-runtime
hardware acceptance. Those remain pending the consolidated native build and
actual SYSTEM HAL device validation.
