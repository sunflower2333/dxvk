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
