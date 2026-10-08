# Findings

Old D3D10 container construction rejects InstanceID, FrontFace, PrimitiveID, Clip/CullDistance, integer targets and dedicated depth outputs. Existing ShaderCode11 decodes SM4/4.1 and writes their real signatures. D3D10 full signatures are runtime unions and can exceed actual shader use; the local Microsoft DDI documentation permits deriving actual use from declarations. Preserve the full union for clip/cull ordinals.

Base32b7bbb plus data-only SO52c6324 copied as d889c40. SO tests and active final-raster-producer position boundary must remain.

Typed regular creation and shared graphics preparation now use ShaderCode11 with an explicit D3D10 profile. GS SO converts legacy validated declarations and preserves full-union clip/cull ordinals. Selected direct clip/cull SO entries and null-code full unions with clip/cull are still rejected by the historical resolver. Ordinary D10 depth-only/null-PS draws remain rejected by drawReady's pre-existing PS/color-target prerequisite; no admission masks changed.

Other ordinary-path gaps found in base32 audit: general shared/present ownership and opened-primary integration (peer work), BC block-aware transfers (peer work), renderer-owned KMT resource allocation enumeration/residency priorities, broader resource/MSAA semantics and gamma/present/0-copy/HDR. Capability/version identities remain zero until the real ordinary runtime acceptance packet exists.
