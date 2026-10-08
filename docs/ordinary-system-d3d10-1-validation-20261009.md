# Ordinary SYSTEM D3D10.1 validation

The existing private core accepts D3D10.1 interface0x000a0002 with runtime
build1 or later, flags0, and publishes the typed D3D10_1DDI_DEVICEFUNCS
table at logical feature level10_1. This change adds a validation frontend
and ordinary probe mode for that existing surface; it does not change
production masks, tables, exports, or default registration.

The default frontend remains exact D3D10.0. Build a separate frontend image
with VIOGPU_SYSTEM_VALIDATION_D10_1 defined, named
viogpudxvk_validate10_1.dll. It forwards only exact D3D10.1 OpenAdapter10
arguments to the existing private core and retains the same read-only entry
record and architecture-specific sibling core path. Existing CPU03 D3D10.0
frontend/probe originals remain accepted and immutable.

The probe's explicit10.1 operand resolves d3d10_1.dll from the actual SYSTEM
directory and calls D3D10CreateDevice1 with the selected hardware VIOGPU
adapter, requested D3D10_FEATURE_LEVEL_10_1, flags0, and D3D10_1_SDK_VERSION.
It checks the returned ID3D10Device1 feature level, exact frontend/interface,
core path, created-device adapter LUID/vendor/device, private160 identity and
effective KMT Version1 filename. It retains the existing512 literal pixels,
two Presents, device-removal checks, exact SYSTEM module paths, held process
and restoration protocol. It does not intercept filenames or preload the
frontend/core.

Shared lifecycle invocation uses Api10, Phaseoffscreen, and the new explicit
D10Profile10_1 selector. The existing native slot1,22 reviewed owner operands,
raw six-value backup, worker job, watchdog, restart and reverse recovery are
unchanged. The selector's nameoffscreen still uses a native probe containing
two Presents. Other APIs require the default10_0 selector. The D10.1 probe
manifest uses API101 and records exact profile10_1, factoryD3D10CreateDevice1,
featureLevel0xa100, entryInterface0x000a0002 and actual runtime Version.
The progress-runner literal reader accepts explicit API101 and retains the
existing exact runner7def540f,2048 literal bytes, zero stderr and process guards.
The exact Api10 worker branch selects TU_WDDM_DIAGNOSTICS0 for this strict
success mode; Api9/11 retain diagnostic1. No pixel/module/error guard or caller
controlled hash allowance is changed by quiet selection.

Native builds and ordinary remote rendering/recovery are pending. Source-only
table audit, local SDK COFF compilation, and accepted historical controlled
D10.1/WARP fixtures cannot admit ordinary D3D10.1 or default replacement.
