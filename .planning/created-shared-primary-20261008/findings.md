# Findings

The existing created scanout primary owns a flags1/flags2 resource-associated pair. OpenResource accepts one allocation; INFO1/INFO2 share element-zero fields but have different array strides in the actual SDK. The bridge has no explicit open-array model/per-resource pair discriminator. Existing negative controls pass count2 with element0 only. Preserve rejection; do not infer array stride from count or the local doc reserved-INFO2 wording.

MS DXGI primary-desc OPTIONAL explicitly permits preventing primary/flip use and requires NO_SCANOUT for copy presentation. An existing legal one-allocation subcase was blocked by unconditional MiscFlags rejection. Permit exactly plain SHARED+OPTIONAL+RGBA8/BGRA8 through the existing primary shape/mode policy. Native creation already aliases one SharedSurface allocation with PresentSurface; allocation/open/publication/cleanup code stays byte-for-byte unchanged.

New native fixture has 24 native/public/KMD pixel scenes, initial-data and bidirectional creator/opened publication, output DriverFlags, SetDisplayMode rejection, opened-resource destruction and retained RTV owner after creator-private retirement. Independent reader expects 120 originals and 2520 pixel observations. Original existing primary/opened fixtures and all admission/discovery code are unchanged.
