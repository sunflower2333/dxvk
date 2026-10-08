# Typed cube-array render and depth views

The D3D10.1 and D3D11 tables translate cube RTV and DSV descriptors to
`TEXTURE2DARRAY` views over their actual face slices. Valid intervals may
start inside a cube or cross a six-face boundary. The base D3D10 table
retains its single-cube policy. Modern DSV creation also accepts the SDK's
`TexCube` union and preserves both independent read-only flags.

`umd_cube_target.h` stages the complete renderer descriptor before publishing
it. It checks cube shape, binding, sample count/quality, mip and finite face
bounds using subtraction after bounding the first face. A failed conversion
leaves the caller's output unchanged. Existing guarded view publication,
callback, owner and retirement handling remains in the production path.
Resource creation, SRV/mip translation and public capability gates are unchanged.

The local first-party pages
`d3d10umddi/ns-d3d10umddi-d3d10ddiarg_texcube_rendertargetview.md` and
`d3d10umddi/ns-d3d10umddi-d3d10ddiarg_texcube_depthstencilview.md` describe
`FirstArraySlice` and `ArraySize` in array slices. The original WDK26100
`D3D11DDIARG_CREATEDEPTHSTENCILVIEW` has `TexCube` alongside its separate
`Flags` member. These contracts use face slices rather than the cube counts
of a shader-resource view.

`umd-cube-array-targets.cpp` links actual production DDIs to an explicit WARP
factory. Exact typed 10.0/10.1/11 tables create independent native and public
reference resources. Twenty-four RTV/DSV views include intervals crossing
cube boundaries and all four modern DSV flag combinations. Thirty complete
readbacks compare 35,700 native words and another 35,700 public-reference
words against an independent absolute face/mip oracle. Both original native
and public descriptors are retained, along with every raw readback and oracle.

Forty-six deliberate failures cover zero/overflowing/outside face intervals,
invalid mip indices, unknown modern flags and base10 cube-array creation.
Each requires exactly one live runtime callback, guarded private bytes still
poisoned and the exact original target/depth binding. Callback-table updates
in place must be honored. The final complete resource readback checks failed
operations did not change data. Descriptor controls also cover malformed
resource geometry/binding and staged output on failure.

Local optimized x64/x86 builds of production DDIs and the fixture pass the
original SDK/WDK strict compiler checks. Reopened objects retain all typed
factories, target callbacks and the public WARP/readback paths. The reader's
26 focused corruption controls pass on a clearly labeled synthetic packet;
these are file-format/oracle checks. Actual native MSVC compilation and
public-reference fixture execution remain pending.

The fixture's fixed marker fields are:

```
typed cube-array targets verified checks=<actual> views=24 snapshots=30 words=35700 callbacks=46 public_reference=1 hardware_admission=0
```

`verify-cube-array-target-originals.py` reconstructs all bytes independently
and joins the 138 raw files to descriptor, callback and readback traces:

```
python tests/verify-cube-array-target-originals.py --originals fixture-output --stdout fixture.stdout.raw --output fresh-review.json
```

The reader checks observed bytes only. Native acceptance additionally needs
the original source/compiler/SDK/library/input hashes, strict build results
and bounded owned process exits/raw pipes. WARP reference acceptance is
separate from actual hardware execution and ordinary runtime/Present gates.
No public admission gap is cleared by this slice.
