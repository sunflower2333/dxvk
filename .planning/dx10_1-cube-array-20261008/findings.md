# Evidence

The original26100 d3d10umddi.h D3D10_1DDI_DEVICEFUNCS table uses
PFND3D10DDI_CREATERESOURCE, the same four-argument signature as base10.
The base CreateResource descriptor documentation specifies six cube faces;
the10.1 TexCube SRV explicitly bounds First2DArrayFace+6*NumCubes by
Resource.ArraySize. The separate callback therefore selects the resource
semantics without casting a table or inspecting the embedded renderer level.

OriginalSDK d3d10.h defines texture-array axis512 and cube edge8192. The
10.1 descriptor preserves caller face count, all initial face/mip subresources,
and logical mip extents while sharing existing resource retirement/publication.

Peers: root owns cube-array mipGenerationStatus in umd_view.h; DX11 owns
cubeArrayShaderView11Desc and createShaderView11 cube branch in its separate
worktree. This slice changes neither branch.
