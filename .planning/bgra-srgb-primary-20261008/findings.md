# Findings

Microsoft local display/extended-format-aware-requirements.md requires fully typed backbuffer casts, all BGRA/BGRX families and BGRA sRGB scanout. bgra-scan-out-support.md requires primary, SetDisplayMode, Present and stretch/rotation/resolve Blt. fully-typed-back-buffers-casting.md restricts typed reinterpretation to BIND_PRESENT. interpretation-of-x-channel.md requires sampling/blending X=1 but copies preserve physical X.

Backend private CreateTexture2D strips PRESENT; use typeless cache only for supported present families and retain original format in resource owner. Allocation v0 remains codes1/2/3; BGRA sRGB bytes use code1 without gamma conversion. BGRX sRGB optional copy has NO_SCANOUT, real scanout denied. Existing non-sRGB real BGRX primary remains supported.
