# Same-format X8 Blt follow-on

The actual x86/x64 CI run 37753759333 at source
`a6ca946d21572a23a49d7edc396179cccc1be88a` failed
`dxgi-open-primary-test` at line 281, with `887a0004`. The B8G8R8A8
case had already saved four exact readbacks and its padded allocation. The
next case uses wire format 2, which maps to `DXGI_FORMAT_B8G8R8X8_UNORM`.
The generic constructor assertion does not distinguish its opened, staging,
and source creations. Source inspection establishes that its independent
presentable source has no primary descriptor and the creation guard
deterministically rejects that format. The subsequent Blt policy would also
reject it. This is the bounded source explanation; the raw failure alone does
not identify which of the three constructors returned the error first.

The corrected path accepts linear X8 as its own format, including the
presentable resource's ordinary shape/usage/bind checks. Both renderer scratch
textures retain format 88. It does not reinterpret the resource as an alpha
format. Backend texture/view creation failures still propagate before a
destination command is recorded. X8 cross-format conversion and multisample
resolve remain explicitly unsupported until their pixels are separately
verified. Float and X8 sRGB remain unsupported.

The typed Blt fixture checks the exact X8 plan plus ten rejected
conversion/resolve/format/shape/flag plans, including unchanged output on
failure. The existing opened-primary fixture's only change is a failure-only
constructor diagnostic with format, opened/staging/presentable state and
HRESULT. All original success conditions and oracles remain byte-identical:
all
three formats, 36 images, 1,260 pixels, 90 original files, padded rows, borrowed
primary ownership, owned staging, callbacks and terminal teardown are still
required. The current 52 ARM fixtures and 39 shipped fixtures are unchanged.

The local Microsoft `DXGI_DDI_ARG_BLT` and `DXGI_DDI_ARG_BLT_FLAGS`
documents define the original source/destination subresource, destination
rectangle, rotation, and distinct Resolve/Convert/Stretch/Present flags. The
local SDK declares X8 as DXGI format 88. The embedded DXVK format table
maps X8 to the native B8G8R8A8 Vulkan storage format with alpha swizzle ONE;
the DXGI format remains X8 at this boundary. Those source facts do not replace
actual native renderer/pixel acceptance.

The original failed CI artifacts are retained. Optimized strict official-header
compilation and local scalar policy replay are separate controls; a fresh
successful native CI run is still required. Public runtime admission,
primary/display ownership and renderer residency gates remain closed.
