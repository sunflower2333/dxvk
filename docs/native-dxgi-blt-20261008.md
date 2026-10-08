# Typed DXGI Blt

The ordinary DXGI1.0 and DXGI1.1 tables now publish the exact WDK `pfnBlt`
callback. Native D3D10.0, 10.1 and 11.0 keep their own device tables, live core
callback pointers and existing closed production admission masks.

The callback validates the runtime's original PRESENT source binding and RT
destination binding, registry membership, subresource indices, flags, rotation
and destination rectangle before any transfer. Supported formats are RGBA8
UNORM/sRGB and BGRA8 UNORM. Copies from sRGB retain encoded channel values by
copying into compatible UNORM scratch. Float/gamma conversions, other display
formats, sRGB MSAA and multisampled destinations return unsupported.

GPU-only scratch resources normalize the selected source mip/slice. A UNORM
MSAA source resolves into scratch; an internal SM4 pass-through VS and level-zero
PS perform bilinear scaling and CCW 90/180/270-degree rotation. A fresh private
deferred context records the operation with no application predication, shaders
or stream-output bindings. `ExecuteCommandList(..., TRUE)` restores the original
immediate state. All owners exist before recording, and only the final GPU copy
touches the destination rectangle. The implementation never invokes a system
shader compiler or creates a public renderer; the shaders use the existing DXBC
container builder and embedded DXVK device.

Present Blt requires the destination's actual paired shared allocation. Partial
writes refresh untouched pixels first; successful writes dirty its cache, publish
the complete result using synchronized readback/Lock/Unlock, join actual runtime
submission and invalidate the handoff epoch. A failed upload or submission stays
a failure even if GPU pixels or a physical allocation were already modified.
No intermediate resolve or conversion writes into the visible destination.
Registry reservations, independent COM/shared owners and retirement checks
survive callback-driven destruction; nested Blt/Present/Resolve/rotation is busy.

Backbuffer creation additionally permits encoded RGBA sRGB and UNORM MSAA using
the existing linear presentation allocation. Ordinary Present resolves MSAA into
a GPU image before staging readback. The wire allocation remains a single-sample
linear image; this is a copy bridge, not scanout or zero copy. Primary creation
returns the documented `DXGI_DDI_ERR_UNSUPPORTED`, including for a guarded
`pPrimaryDesc` pointer, without constructing ordinary allocation metadata.

The new `dxvk-umd-dxgi-blt-test` uses production typed tables/resources/views and
substitutes only the embedded renderer with controlled WARP. It retains 30
readbacks / 1,536 exact pixels across all three interfaces, plus three published
48-pixel shared frames. Ten cases cover borders, every rotation, shrinking,
stretching, RGBA-to-BGRA conversion, encoded sRGB, simultaneous resolve/convert/
stretch/Present, and destination mip/array indexing. Other subresources remain
unchanged. State restoration, malformed atomic requests, guarded foreign keys,
unsupported primary/float creation, failed Lock and callback-driven retirement
are checked. The internal shader containers and all actual map pitches/bytes are
retained in 68 original files. The independent Python reader derives bilinear
pixel centers as rational numbers and checks literal RGBA/BGRA, guards/padding,
shader instructions and exact file/marker totals. It imports no fixture arrays
or production shader/geometry helpers.

The existing 45 ARM64 cases are unchanged; build/run/copy hooks add this real
46th case. Native WARP execution and real-KMT hardware acceptance of this new
slice remain pending. Local original-SDK COFF and portable shader/geometry
controls are compile/CPU evidence only.

Ordinary whole-profile admission still needs actual render-allocation residency
association, complete primary/display mode and gamma semantics, remaining
display conversions/shared variants, honored creation thread modes, and genuine
Microsoft-runtime acceptance. This slice changes none of those capability gates.

## Primary sources

- Local Microsoft WDK `dxgiddi/ns-dxgiddi-dxgi_ddi_base_functions.md`, `pfnBlt`:
  whole source subresource, at least bilinear quality, combined operations,
  CCW rotation fallback, encoded sRGB copy and presentation artifact rules.
  [Microsoft documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dxgiddi/ns-dxgiddi-dxgi_ddi_base_functions)
- Local `dxgiddi/ns-dxgiddi-dxgi_ddi_arg_blt.md` and
  `ns-dxgiddi-dxgi_ddi_arg_blt_flags.md`: exact rectangle/subresource/flag fields.
- Original SDK 10.0.26100.0 `um/dxgiddi.h` / `shared/winerror.h`, and original
  WDK `um/d3d10umddi.h`: typed signatures and distinct DDI unsupported HRESULT.
- Existing embedded `D3D11ImmediateContext::ExecuteCommandList` resets command
  list state and restores the immediate application's state when requested.
