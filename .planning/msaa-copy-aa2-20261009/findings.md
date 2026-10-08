# Findings

The retained x86 original PID6288 reports DDI_HRESULT=80070057, API_HRESULT=00000000, step=texture-create, profile0, scene0 and check113. Its terminal process status is 0xc0000005; the original assertion alone does not establish the later crash location.

Static check accounting proves the first staging readback constructor: two Fixture constructions16; two source/destination constructors10; six layer clears78; read inspected1; resolved constructor5; staging private-size1; error callback1; failed ok1 =113. Public MSAA2 source/destination creation and the resolved sample1 constructor already passed. The global samples2/subresource2 labels retain the previous scene/clear state; the failing descriptor itself is sample1, STAGING, BindFlags0 and public CPU READ0x20000.

Texture copied desc.CPUAccessFlags into native MapFlags. Actual SDK D3D10DDIARG_CREATERESOURCE documents MapFlags as D3D10_DDI_CPU_ACCESS, READ=2, WRITE=1, MASK=3; production createResourceData correctly rejects 0x20000 before backend CreateTexture2D. The separate D3D10_DDI_MAP operation enum has different values and is not the create-resource field. Profile0 uses D3D10.1 with no DDI11 amortized callback handler.

Peer independently confirmed the exact source route and original stderr. The minimal correction translates public READ/WRITE to the native create-resource CPU bits. All production and fixture oracles remain intact.

Git blame attributes the wrong MapFlags assignment to the fixture's original d4611c9 introduction, before the later amortized processing work. Only that assignment changes in the correction; the entire remaining fixture and independent reader match aa2 bytes.

Originals: /home/sunf/droidvm-repos/artifacts/dxvk-trunk-integration-20261008/next-I386-selected-core-collection-20261009-01/selected-I386-failure-originals-01/selected-original-members/artifacts/. Collector proof12727/SHA71f0adaf56c7422b247306ec8cbc880f16a0aecfecbe3c554c3a72649495fb1e; one ZIP3353693/SHA6567be47af904c601f1118f357ba010274a07d2251b96fa768d2d12eef99a4ff.
