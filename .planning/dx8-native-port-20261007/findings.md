# DX8 findings

The original target SysWOW64 D3D8 DLL is I386; native System32 D3D8 is absent.
Genuine runtime imports d3d8thk and GDI32 KMT directly, not D3D9. Original
RVA12e42 writes Interface8, RVA12e73 calls resolved OpenAdapter. GETD3D8CAPS12
is the separate212-byte output. The original KMT import slot is RVA b11d4.

Microsoft public PDB GUID/sections/export agree but internal stripped age3
versus DLL CodeView age1; no age identity is claimed. Exact runtime machine
code determines Interface8 independently.

New caps projection preserves implemented limits, caps VS1.1/96 and PS1.4,
removes9-only flags, snapshots before publishing exact212 bytes and rejects
malformed sizes/tags without modifying output. Native typed9 SDK asserts and
legacy8 ABI assertions compile. Production admission remains closed.

Read-only/explicit offscreen system8 probe is implemented. Six stages exercise
clear, FVF, SM1.1 declaration/shader handles, dynamic2D texture locking,
CopyRects readback and reset; exact384-pixel oracle checksum a9dc9545. No
Windows PE or target operation was executed. Exact x86 core/Vulkan/Mesa
payloads and genuine interface8/caps/device/presentation gates remain pending.

Separate caps-only proposal patch adds typedcaps12 adapter/fixture/link inputs
without changing reserved files or widening Interface8. It applies to c0ea
with git apply --check; root owns actual integration onto newer production.

Second slice: const LegacyD3DApi now threads through adapter/core/backend;
Open/Create copy metadata and selected callbacks before identity queries.
CAPS12 is8-only, CAPS13 is9-only, unchanged production gates/export list.
API8 private parent is non-Ex then D3D8-compatible before device construction;
the old native parent always enabledEx, unlike DXVK8's public private-core
wrapper. API9 defaults remain unchanged.

Real pinned sm3 Converter successfully emits output IR for VS1.1/PS1.1/1.4
and sampling IR for distinct1.1 TEX/1.4 TEXLD. GCC/Clang sanitizer47PASS.
Converter expects validated input: direct malformed-header experiment caused
an assertion because its Parser bool indicates iteration, not ShaderInfo
validity. Originals retained; tests now verify the real header guard before
conversion. The production typed DDI already validates framing/version.

Typed legacy limits96VS/8PS constants,8stages, legacy declarations and
no integer/bool state are enforced. Full9 behavior/fixtures remain. CPU-only
tests and cross compilation cannot establish genuine System8 FVF/DDI calls
or actual private-parent flags on hardware. Exact x86 core/loader/Mesa and
genuine Interface8 negotiation remain prerequisites. Legacy fog bit is not
invented by the CAPS12 projection. Root owns newer9 resource/ABI integration.
