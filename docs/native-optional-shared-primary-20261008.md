# Optional shared primary copy fallback

The created scanout-primary path owns two resource-associated allocations: an
80-byte v0 flags1 primary and a flags2 staging allocation. `OpenResource`
currently admits one allocation, borrows its pixel owner and, for a flags1
primary, creates a separate internal staging allocation. A real shared scanout
pair remains rejected. This packet does not introduce a new pair wire format,
share the creator's staging allocation, or change allocation destruction.

The local Microsoft open-resource page describes a classic
`D3DDDI_OPENALLOCATIONINFO` array and calls INFO2 reserved. The actual SDK header
has an INFO1/INFO2 union and labels INFO2 WDDM v2 ADVSCH. The two structures have
the same element-zero prefix and different element strides. The current bridge
has no explicit open-array-model record or resource-private pair discriminator.
Consequently a second entry cannot be decoded based only on a count or on the
older page's reserved-member wording. A future pair implementation needs an
explicit runtime ABI selection and strict pair metadata/ownership validation;
it must preserve cleanup inside a failed OpenResource because the runtime does
not call DestroyResource after failure.

There is a supported single-allocation case which the old primary policy
rejected: a plain shared RGBA8/BGRA8 buffer with `pPrimaryDesc` carrying
`PRIMARY_OPTIONAL`. Microsoft permits the driver to keep this resource from
becoming a primary by using copy presentation and reporting `NO_SCANOUT`.
`primaryPlan` already implements that choice. The change permits exactly this
miscellaneous-flag/format combination through `primaryResourcePlan`; all other
shape, mode, flag and present-binding checks remain in place. Creation then
uses the same existing SharedSurface owner as ordinary shared present buffers,
with one flags2 allocation and a PresentSurface alias. The primary descriptor
is written only during successful resource publication and is not retained.

The new typed fixture imports the existing callback harness without modifying
its old assertions or entry point. It executes D3D10.0, D3D10.1 and D3D11 tables
for BGRA8/RGBA8, including OPTIONAL with NONPREROTATED, actual initial data,
one-allocation open, opened writes, creator presentation, creator writes and
opened reads. It verifies NO_SCANOUT, rejects SetDisplayMode without calling
the kernel callback, and checks that an RTV retains the one allocation after
the creator's resource storage is retired. Destruction of the opened resource
never deallocates the creator's backing. Retired Present handles fail before
publication. Each image retains native staging, independent public D3D11 WARP
pixel readback, KMD callback backing, observed pitches/identity/driver flags and
the exact 80-byte allocation metadata. Public WARP is a pixel control, not an
ordinary DXGI shared-primary or real display-ownership proof.

Target: `dxvk-umd-dxgi-optional-shared-primary-test`.

Exact marker:

```
DXGI optional shared primary PASS checks=<count> profiles=3 formats=2 images=24 pixels=840 negatives=12 raw_files=120 hardware_admission=0
```

Run the independent reader with a fresh directory containing exactly the 120
original regular files and an output outside that directory:

```
python3 tests/verify-optional-shared-primary-originals.py --directory <originals> --output <outside.json>
```

The reader checks 2520 observed pixel words plus all metadata and file closure.
Actual Windows fixture execution remains CI/target work. Linux compiler and
synthetic reader controls do not establish runtime, scanout or hardware
acceptance. All discovery/admission gates and real-primary pair rejections stay
unchanged. Query generation, distance SO and regional copies are outside this
packet.

Contract sources read locally:

- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/dxgiddi/ns-dxgiddi-dxgi_ddi_primary_desc.md`, OPTIONAL and NO_SCANOUT.
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_createresource.md`, present and primary descriptor input/output.
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_openresource.md`, allocation array and resource identity.
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3d10umddi/nc-d3d10umddi-pfnd3d10ddi_openresource.md`, failed-open cleanup.
- `reference/codes/windows-driver-docs-ddi/wdk-ddi-src/content/d3dumddi/nc-d3dumddi-pfnd3dddi_allocatecb.md`, resource-associated allocation arrays and internal-device staging.
- Actual SDK 10.0.26100.0 `d3d10umddi.h:494` and `d3dukmdt.h:468`, INFO1/INFO2 layouts.
