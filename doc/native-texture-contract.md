# Native Texture1D and Texture3D resource contract

The actual D3D10 CreateResource entry accepts non-shared Texture1D arrays and
Texture3D volumes. It validates dimensions and mip chains, creates the matching
embedded D3D11 resource, and retains the existing staged ownership transaction.
Failure removes its reservation and releases staged objects before SetError;
the runtime never needs to destroy a failed handle. DestroyResource retires
private storage before releasing retained owners.

Initial data preserves the runtime's subresource order, row pitch and slice
pitch. A volume's depth is inside each mip subresource, whereas a 1D array uses
arraySlice * MipLevels + mip. Native physical dimensions can contain format
padding; the embedded API creates its layout from the texel dimensions.

Existing generic Map/Unmap and CopyResource use the same backend owners.
CopySubresourceRegion and UpdateSubresource now account for volume depth and
destination Z, check bounds without unsigned overflow, and validate row/slice
pitches. Region/update retains its existing RGBA8/BGRA8 format restriction;
typed creation and generic map/copy are checked by the embedded API.

The controlled fixture links actual production DDI, allocation and runtime
owner sources with Microsoft's real WDK definitions. Only the embedded backend
factory is replaced by WARP. It checks every mip/array/depth texel with padded
initial data, region/update isolation, native map writes, invalid pitches and
ranges, duplicate/failed/foreign handles, immediate private-storage reuse and
error callback reentry. Existing tests still check closed native admission.

This is a bounded resource-storage implementation, not full graphics feature
admission. Texture1D/3D shader/target/depth views and mip generation remain
unimplemented. Dedicated hRTResource/native-token backing, shared/opened
resources, primary/flip Present, arbitrary nested device retirement and a
complete negotiated DDI remain separate gates. No native supported version or
feature level is added; no system registration or Turnip hardware proof is
claimed by controlled WARP execution.

Microsoft contracts reviewed:

- PFND3D10DDI_CREATERESOURCE: any creation error invalidates the handle and
  prevents the corresponding DestroyResource.
- D3D10DDIARG_CREATERESOURCE and D3D10DDI_MIPINFO: 1D arrays, 3D texel depth,
  initial subresource data, physical format padding and primary flags.
- Direct3D Version 10 Runtime and Driver Handles: runtime-managed private
  memory and strongly typed handle lifetimes.
- PFND3D10DDI_DESTROYDEVICE: runtime first destroys child objects, then frees
  the device's private storage; deferring backend destruction alone does not
  prove legal runtime callback lifetime after the outer DestroyDevice return.
