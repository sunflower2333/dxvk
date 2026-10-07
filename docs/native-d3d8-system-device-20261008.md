# Genuine I386 D3D8 device diagnostic preparation

This slice prepares a process-local diagnostic for Microsoft's original
`C:\Windows\SysWOW64\d3d8.dll`. It keeps the existing read-only enumeration
permission and adds a separate `device-render-d3d8-interface8-v1` permission.
Production `OpenAdapter` admission, adapter caps, driver registration and the
DXVK8 application DLL path are unchanged.

The hardware inputs are the original I386 core from source
`d7e5c7d46b8ce889e993bfab66a3b78b076c49d1`, successful consolidated run
`37648387721`, artifact `11495648222`:

| Input | Original SHA256 |
|---|---|
| `viogpudxvk.dll` | `7be8cbb9850407ccc304528911a6fbd71b01971fbeb4a86550cc8dcc2f346a3f` |
| `viogpu_gl_loader_x86.dll` | `d459f2d09080865cc3d591b498c02d38305a26963b401152f8230dc60c5ad7e7` |
| `viogpu_gl_vk_x86.dll` | `2b549889816163433faabe6f2c1d2a61d6c106078d08e30031b74c0a66cd7f5c` |
| `freedreno_icd.json` | `74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80` |

The last three inputs retain Mesa source `8443c71a5ab32b9d58b904fa51f4bf2f9089db8d`,
run `37453381660`, artifact `11408567654`. The private loader is a separate
Khronos source revision recorded in its original `loader-source-commit.txt`.
Raw ZIP/API hashes, actual PE32/I386 headers and original security-directory
presence are retained separately. No ARM64 payload, old signed binary, new
signature or rebuilt fixture is substituted for these original bytes.

The owned core directory is
`C:\Users\Public\DxvkD3D8Candidate-d7e5c7d-37648387721`.
The probe denies writes to the core, loader, ICD and unchanged JSON while it
runs; checks SHA256/I386 before loading; checks actual loaded private module
paths after CreateDevice and before teardown; and restores its two process-local
Vulkan driver-selection variables. These checks do not make a native or hardware
acceptance claim until the original executed outputs are independently joined.

## Runtime contracts

The frontend admits only an actual I386 SysWOW64 D3D8 caller, Interface8 and
one immutable core path/hash/source identity. It forwards the original CAPS12
request and bytes without modifications. Its read-only adapter continues to
block CreateDevice even if a caller later sets the device permission. A device
adapter rejects a later permission or core-identity change.

Device mode forwards the genuine CreateDevice request, preserving Interface8,
runtime Version, flags and input/output addresses. It snapshots the actual
Vista 22-pointer callback prefix once, restores the borrowed table identity on
return, and publishes only the core's Vista 99-pointer function prefix. The
owned callback table survives the original DestroyDevice call, including cleanup
callbacks. Failed teardown retains ownership. Four resource/device/present
forwarders log exact HRESULTs; no fallback factory or success result is invented.

Callback diagnostics observe actual returned allocation/context handles and
paired lock/unlock/deallocate/destroy callbacks. A mapped BO is allowed during
Render and residency. The bounded diagnostic inventory supports 512 allocations
and 16 contexts; overflow, missing handles, rename, unreadable output or incorrect
pairing marks the trace incomplete without changing the real callback HRESULT.
The CPU callback fixture tests immutable borrowed-table snapshots, mapped
submission, failed release retaining ownership and stale-owner rejection.

## Prepared modes

The new modes require the original Limited USER SID, session1, elevation type3
and medium integrity8192. Hardware modes also open/close the selected monitor's
HDC adapter and require an independently supplied live LUID/source match, plus
actual hardware/non-software KMT adapter type. Root's latest identity records
LUID bytes `ec6b000000000000`, source0; it must be refreshed before execution.

* `--kmt-names <luid16> <source>` queries the actual I386 GDI32 legacy driver
  filename and retains all260 UTF16 words. It does not load a core or invoke a
  D3D factory. Native ARM64 KMT filenames are insufficient to establish this
  WoW input.
* `--front-enumerate <frontend> <exact-installed-WoW-name> <core-path> <sha256>
  <core-commit>` retains the prior read-only path. Genuine HAL CAPS12 admission
  must be established before a device/render attempt.
* `--front-offscreen <frontend> <exact-installed-WoW-name> <core-path> <sha256>
  <core-commit> <luid16> <source>` uses only genuine HAL hardware vertex
  processing. It runs the original clear/FVF/VS1.1/PS1.1/dynamic2D/reset sequence
  and adds a real PS1.4 draw, then compares all448 original readback pixels.
* `--front-present` takes the same arguments and additionally clears the real
  back buffer, calls Microsoft D3D8 Present to an owned8x8 window, and compares
  all64 captured desktop RGB pixels. Its bounded2.5s screen poll records final
  original pixels even on mismatch. A successful Present HRESULT alone cannot
  pass this gate.

The earlier unselected `--offscreen` mode remains available with its original
384-pixel marker. It does not select this frontend and cannot establish DXVK
system-runtime hardware acceptance.

## Native CPU packet and current limits

`prepare-native-d3d8-runtime-device.py` freezes33 inputs: 18 committed diagnostic
inputs, 12 unchanged production reference files from d7e5, and three verbatim
licensed legacy API headers from the accepted7c1f packet. The production DLL is
never rebuilt by the packet. The strict native build uses the retained official
EWDK compiler, original mounted SDK and unchanged bounded raw-process C#.
It builds five original I386 COFFs and four PE files, executes policy329, the
controlled callback fixture and null/non-system caller guards, and requires
30 malformed CLI cases to exit64. It never executes a valid runtime selector,
KMT-name, device, render or Present mode.

Local evidence currently establishes strict I386 cross compile/link/PE checks,
GCC+Clang ASan/UBSan policy329 and six intended widened-policy failures. Original
callback fixture execution, native MSVC warnings, the WoW filename, actual HAL
admission, Vulkan runtime initialization and offscreen/Present acceptance remain
pending. The original signed SYS, DWM/Explorer, registration, VM and dependencies
must remain intact across each separately owned future invocation. Use the
unchanged d8cf raw runner with a30s child limit and actual handle/exit/pipe receipts;
keep CPU, read-only and hardware attempts in fresh separate evidence roots.
