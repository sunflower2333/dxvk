# D3D10.1 cube-array resource creation

Base: 791006c17a83cfe279138d078b2f3df26d034f4b, isolated worktree
reference/codes/dxvk-umd-dx10_1-cube-array-20261008.

- Complete: read official WDK table and D3D10/10.1 resource/SRV distinctions.
- Complete: distinct exact-ABI typed10.1 CreateResource entry, bounded
  complete-face shape and existing atomic resource publication.
- Complete: independent portable shape controls and actual typed resource/readback
  fixture; GCC+Clang sanitizer97387 checks each and eight strict original-header
  x64/x86 COFFs, including the unchanged base10 cube fixture.
- In progress: frozen original source/compiler proof and reviewable source commit.
- Pending: root review and separately authorized native execution.

Base10 remains strict ArraySize6. Live callbacks, CompleteResources admission,
cube SRV translation and GenerateMips remain unchanged. Root/peer own those
other cube hunks. Target ownership was explicitly released after oldd7
1792-pixel dual-profile original review; all work here is local.
