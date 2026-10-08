# Preserve distinct Present input and runtime allocation identities

The genuine native ARM64 ColorFill02 controlled fixture built successfully,
completed the ColorFill, ValidateDevice, D3D8 bridge and Vista 99-pointer
controls, then exited 1 at check 143242, line 3698. Its first Present callback
had already found the mock allocation, checked its mapping state and checked
the exact original pixel frame. The failed expression was:

```cpp
f->lastPresent.hSrcAllocation == 71 && uintptr_t(resource) != 71
```

The mock runtime initializes each fixture's first Present allocation handle
to 71. Production UMD resource owners use a process-wide monotonic opaque
token sequence starting at 1. The new ColorFill controls create additional
owners before the unchanged presentation controls. Those independent handle
domains may have the same numeric value; the old fixture did not arrange the
distinct values required for its negative forwarding oracle. The native
failure is consistent with the owner token reaching 71. The original output
does not separately print the two conjunct values, so this is a source-based
diagnosis of the preserved failure rather than a claimed extra observation.

The correction explicitly configures the next mock runtime allocation after
the resource exists: 72 when the resource token is 71, otherwise 71. A setup
check requires distinct values. The original after-Present ownership check
still requires the callback to receive exactly that configured kernel
allocation handle and to receive a value different from the UMD resource
token. Exact pixel checks, callback lifetime checks, later tests and their
order remain intact. No production source, admission policy or masks change.

The failed packet, seven ARM64 COFF files, one PE, complete stdout/stderr,
five child receipts, source/tool before-after records and collected archive
remain under:

`artifacts/d3d9-colorfill-native-20261009/guest-native-d9-colorfill-0c97035-02/`

The independent failure review verified 680 finite joins, including eight
recorded native lifetimes absent, six closed host transports, the failed ROOT
outer process closed, and unchanged KMD, registry projection, PnP and desktop.
It is not a whole-fixture PASS. No x64 build or fixture execution occurred.

This correction is source-only and has not been compiled or executed. A fresh
controlled native attempt must rebuild and run the complete ARM64 and x64
fixtures. It does not establish real renderer, GPU or ordinary runtime support.
