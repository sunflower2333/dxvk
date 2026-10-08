# Preserve the cast-copy native empty-box controls

CI61 at e7954d7 failed identically on x86 and x64 after producing native and
public originals for observations 0 through 47. The retained stderr reports
checks=9950, callbacks=14 and D3DDDIERR_DEVICEREMOVED (0x88760870). Both actual
process receipts record exit -1073741819; the manifest was still buffered.
The retained files do not contain a phase record. Source CHECK accounting
locates 9950 at staging creation immediately after the first reversed-X
public empty-box call in the 1D negative controls. This location is an
inference from the exact published source and originals, not a recorded trace.

The native UMD already returns before forwarding each of the six literal
equal/reversed-axis boxes. Those calls, the thirteen initial rejection
controls, all remaining cases, and the 320-observation/640-plane byte oracle
remain intact. Production validation and admission gates are unchanged.

Each independent public no-op reference now receives the bounded canonical
equal-X box {0,0,0,0,1,1}, repeated for all six native equal/reversed axes.
The non-X axes retain the ordinary 0..1 layout for buffers and 1D resources.
This avoids feeding unsigned reversed extents to WARP. It proves the public
equal-X no-op reference only; it does not claim that WARP accepts reversed
geometry or each non-X empty axis. Native equal/reversed semantics still require the actual DDI calls
and unchanged readback sentinels.

Failure diagnostics now include the concrete phase, DDI/API HRESULT, profile,
dimension, formats, operation, control, subresource and observation counters.
Each existing manifest row is flushed so a failed run retains completed rows.
Final manifest content and the independent literal originals reader are exact.

The local Microsoft PFND3D10DDI_RESOURCECOPYREGION documentation specifies the
native equal/reversed-axis no-op. The public [CopySubresourceRegion contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion)
also specifies empty-box no-op and warns that invalid coordinates can remove
the device. This fixture correction does not broaden any resource-copy
capability. A successful corrected Windows run remains required.
