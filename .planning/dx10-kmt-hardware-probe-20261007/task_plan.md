# Native DX10 hardware probe

Prepare a separate, locally verified real-KMT D3D10.0/10.1 shader and resource probe. Root owns all target access, installation, runtime/hardware admission and CI. No target calls or pushes in this slice. Preserve completed57-program/18-draw/4608-pixel/3-callback fixture exactly.

1. [complete] Coordinate stable shared KMT transport source with DX11; audit typed legacy adapter/device/DDI and actual KMT/runtime-cookie ownership.
2. [complete] Implement bounded named-export DX10.0/10.1 clear/VS/PS draw/staging-readback pixel oracle with exact selectedLUID and explicit object destruction.
3. [complete] Add relevant SM4.1 workload and malformed/CLI controls; preserve runtime callback table lifetimes and closed production gates.
4. [complete] Map ordinary Microsoft d3d10/d3d10_1 activation next gates from primary local docs.
5. [in progress] Run meaningful local portable controls and strict original-header COFF, freeze source/evidence/native build metadata, commit reviewable separate slice with attribution.
