# Identity GPU Blt

Actual x86/x64 CI run 37761389362 failed the opened-primary comparison
after the first X8 Present at profile 0 following source update 4, although
the typed Blt returned S_OK. The failed word was not recorded. A fourth-byte
change is a source hypothesis, not an observed cause. The separate
shared-present change records format, seed, coordinates and actual/expected
words before the unchanged failure assertion.

An identity Blt with the same actual source/destination format, no Resolve
or Stretch flag, and a destination rectangle equal to the whole selected
source mip now records a GPU CopySubresourceRegion directly. It avoids the
old sampled scratch/RTV draw for this case and preserves physical bytes.
Actual format equality keeps sRGB-to-UNORM conversion on the original shader
path, while same-format sRGB is copied without decoding. Rotation, scaling,
resolve and conversion retain their existing implementation and validation.

The copy is recorded in a fresh private deferred context, finished with
FALSE and executed with RestoreContextState TRUE. Existing resource/context
pins, device serialization, reservations and liveness checks still surround
the helper. Exact same texture object and same subresource is a legal no-op:
whole-source extents plus destination bounds force zero offset. Different
subresources of the same object use the legal GPU copy. The outer shared
dirty tracking, runtime submission, Present publication and retirement path
also runs for the no-op. Shared allocations in this UMD have separately
created backend textures; sharing the CPU allocation does not make those
texture objects the same GPU image.

The local Microsoft DXGI_DDI_ARG_BLT and flag documents define whole-source
copy, selected subresources, destination rectangle and separate
Resolve/Convert/Stretch/Present flags. The official SDK D3D11 context methods
and the embedded DXVK ExecuteCommandList implementation establish the fresh
command-list state and restored application bindings. DXVK SetPredication
itself remains a GPU stub, so these controls do not admit hardware predication.

The existing thirty Blt images, 1,536 pixels, shader-token oracle, three
padded Present backings and lifecycle/reentrant failure cases remain intact.
A separate typed D3D10.0/10.1/11.0 phase prepares 120 raw images and 15,624
exact 32-bit pixels across RGBA, same-format sRGB, BGRA and X8. It covers
offset copies, selected destination mip/array and untouched subresources,
exact self-copy no-op, unchanged rejection destinations, and an issued,
signaled non-hint WARP predicate that suppresses ordinary native copies
before and after Blt while the private Blt copies and retains application
viewport, scissor, rasterizer, topology, shaders, render target and predicate.
Predication is cleared before all staging observations. Native-helper
supplements copy source mip/array subresource 1 into subresource 3 of one GPU
image and save both exact images. This supplement does not broaden the DDI's
current one-mip/one-slice presentable-source contract. Every channel is
compared literally, including varying X8 fourth bytes; no alpha mask or
success-oracle relaxation is introduced.

The independent identity reader requires all 240 raw/metadata originals and
its exact phase marker. The existing native reader invokes it after its
unchanged legacy byte/program verification, so the current x86/x64/ARM
harnesses automatically require both phases without changing fixture counts.
Its positive synthetic packet and 35 rejection mutations verify the reader,
not GPU execution. Optimized official SDK x86/x64 source and fixture
compilation, four actual COFF reopens and dependency/source pins are separate
local controls. Actual native CI execution is still required to prove this
fix, and ordinary runtime/display/hardware gates remain closed.

A separate preexisting scope remains: shared refresh/publication uses
immediate CopyResource, which a suppressing native predicate can skip while
cache/dirty bookkeeping proceeds. These unshared predicate controls cover
the central Blt helper, not end-to-end shared Present predication.
