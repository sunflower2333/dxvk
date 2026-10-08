# Findings

Local ne-d3d10umddi-d3d10ddi_query.md requires occlusion counts even with no RTV bound and NULL-PS PSInvocations not incrementing. SetRenderTargets must atomically set all RTV/DSV bindings, including NULL handles. D3D10 PSSetShader accepts NULL to disable the stage. Existing native11 drawReady already requires only VS/topology/index state; common decoded preparation preserves the D10 final-raster-position boundary independently of cached shader state.

The independent semantic audit confirmed cleared viewports are legal and Microsoft functional-spec sections 4.1.3–4.1.6 permit absent output targets with or without PS. Output-free work must not be skipped because IA/VS, occlusion and SO remain observable. Different public/native WARP clipper/helper counts are allowed; enforce precise IA/NULL-PS/occlusion invariants and retain the other counts rather than requiring arbitrary equality.

An affine dyadic VS plane detects interpolation and coordinate mistakes more precisely than a constant depth. Explicit depth=.625 and discard-all/no-output/cleared-viewport clear preservation separate null-PS handling, dedicated depth output, lack of output declarations and geometry/query propagation. Native disabled stencil descriptors contain valid KEEP/ALWAYS values even though production already normalizes disabled faces.

The QUERY contract excludes discarded pixels from the OM/occlusion count. Its DSP lower bound proves positive PSInvocations for the surviving empty/depth cases, but does not prove a positive lower bound for unconditional discard. Retain that count observationally; exact clear preservation, zero occlusion, upstream IA/VS and removed=0 remain mandatory for discard. The reader includes a positive synthetic zero-discard-counter control to guard this distinction.

The c138 shader worktree and 69dba94 integer-RTV follow-up worktree remain frozen. This worktree is based on 69dba94 and contains only subsequent depth/query draw work.
