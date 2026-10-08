# Findings

Local D3D10 SO declaration specifies register/mask, with no position-only semantic restriction. Local Stream Output Stage puts capture before rasterizer and permits NULL GS. Public CreateGeometryShaderWithStreamOutput accepts prior VS bytecode/output-signature for no-GS capture.

Current streamOutputDeclaration rejects clip/cull and ambiguous same-register rows even when masks have independent owners. Null-GS union rejects any shared register and lacks full-union clip/cull ordinals. Shared shader11 SO mapping also passes absolute physical register lane as public StartComponent. DXVK ir_pass_lower_io.cpp shifts that API component by semantic first component, so packed cull lanes are offset twice. Correct translation splits ownership and subtracts semantic first lane.
