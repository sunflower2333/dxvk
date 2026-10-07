# DX8 system-runtime findings

Original target SysWOW64 d3d8.dll is I386, imports d3d8thk and GDI32 KMT,
and sets Interface8 at original RVA12e42 before OpenAdapter RVA12e73.
Original KMT IAT slot is RVA b11d4, pointer width4. The DLL SHA is
65d8980c469e45d862c68ad046731fd85c19401d3436fd46c65c19db1182dad8.
Modern SDK D3DDDICAPS_GETD3D8CAPS is12; its ABI is the212-byte D3DCAPS8
prefix. The integrated typed adapter rejects Interface9 caps for an8 owner.

Existing DX8 runtime probe has a genuine bounded PE parser and offscreen
oracle, but its selector still chooses viogpu-d3d9-runtime-front.dll and the
old ARM64-only read-only permission. That frontend/core cannot serve I386.
The new frontend loads only a separately owned exact future I386 CI DLL,
forwarding the original typed CAPS12 output unchanged and always blocking
creation. It verifies full actual SHA256 and I386 before load, with file
write/delete sharing denied and immutable source/path/hash process pins.

Original affe x86 core/Mesa/Khronos payloads are independently available.
Affe predates completed8 identity, and its default public Vulkan loader names
do not match private viogpu_gl_loader_x86.dll. A matching integrated core
with verified loader configuration and original x86 ICD is required for
hardware. Read-only typed adapter/caps does not create a Vulkan device.
No artifact is staged and no hardware admission is established by this slice.

b75 built CPU fixtures rather than a production core DLL. The manifest keeps
its eleven core files as uncompiled reference inputs only. The next successful
consolidated I386 production artifact supplies the real commit/run/member hash;
the frontend is not hardcoded to an unavailable b75 DLL. CPU guards require
no candidate binary. Later hardware cannot silently rebuild or relabel it to
fix a loader configuration mismatch. b75's Vulkan loader has no environment
override; the Meson compile-time private name and actual generated config must
be independently verified for the future core.
