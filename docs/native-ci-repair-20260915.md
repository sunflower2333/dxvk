# Native UMD CI repair: private renderer level and view-scoped 1D mips

The missing MinGW action was already replaced earlier on this branch. The
remaining failures appeared only after enabling the full native UMD workflow
and the real Texture1D fixture. They are not dismissed as infrastructure errors.

## Root causes and implementation

Independent direct WARP controls at `049ba459aa7b6d0e97ec8d05abec197ca56e5e97`
compared the original FXC geometry shader and the reconstructed UMD bytecode.
Both fail CreateGeometryShaderWithStreamOutput(NO_RASTERIZED_STREAM) at FL10,
and both pass at FL11. The debug layer explicitly says that value needs FL11.
The embedded renderer now has an explicit minimum implementation level of 11_0;
it rejects an insufficient actual backend. Test-only native factories use the
same helper, while their incoming D3D10 requests and admission checks remain.
This does NOT publish D3D11 native DDI support or remove any capability gap.

The same controls verified initial Texture1D data and the actual SRV descriptor
before calling GenerateMips directly. At both levels WARP modified the selected
source mip and failed to update some selected slices/levels. The strict oracle
continues requiring the documented view scope, not the observed WARP bytes.

For 1D generation the production helper uses one GPU-only, single-slice scratch
chain whose mip zero is the requested source level. It copies the source on the
GPU, invokes full-chain GenerateMips, and copies ONLY generated levels back to
each selected original slice. Creation completes before recording copies. The
original backing, source mip and excluded ranges are unchanged; single-level
views are no-ops. The helper performs no CPU texel generation/readback, global
idle, shared-resource emulation or test-backend detection. Existing backend
hazard tracking and command references own asynchronous lifetimes.

This correctness path adds a GPU scratch allocation and GPU copies per call;
no throughput or latency improvement is claimed. Later optimization may remove
copies only with independent view-scope evidence. Non-1D forwarding remains.

## Regression and validation boundary

The original all-remaining array/mip scenario remains and now also checks its
initial data. Truncated views and single-level no-ops check all source, excluded
slice and omitted tail texels. Existing SO packed/split/sparse/append/query and
predicated DrawAuto cases remain unchanged. Failure artifacts retain logs,
without turning partially built output into a usable candidate.

Normal CI remains enabled. No result from a previous revision is a result for
this repair: current commit must pass its own full native builds (ARM64/x64/x86),
ARM64 runtime references, offline suite, ordinary builds and package workflow.
These are not physical VIOGPU execution, native system runtime activation,
shared/primary Present or completion of the DXVK/VKD3D port. Driver-parent pins,
KMD, Mesa ABI, registration and runtimeMissingD3D10Requirements are unchanged.

Public contracts:
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11device-creategeometryshaderwithstreamoutput
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-generatemips
- https://learn.microsoft.com/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion
