# DX8 original CI core and CPU packet

The consolidated backend matrix must explicitly select
`viogpu_gl_loader_<arch>.dll` when compiling the original DLL. The existing
private branch resolves beside its own module and has no public Vulkan fallback.
The matching private Khronos loader exports `vkGetInstanceProcAddr`; the Mesa ICD
exports different ICD entry points and cannot replace that loader.

The build retains `native-build-configuration.json`, source-before/after
snapshots, `vulkan_loader_config.h`, original Meson build options/machines,
compile commands and Ninja input. The receipt pins the original linked DLL/PDB
bytes, architecture, source commit, actual GitHub run/attempt and configuration
sources. CI requires exact unfiltered Git source bytes and its architecture's
private loader. Direct development builds retain the existing optional argument
and truthfully report modified configuration sources.

For a future DX8 core, independently verify all six successful jobs and the API
digest/size of the original x86 artifact ZIP, CRC/unique members, exact source and
its DLL/PDB hash. Join the receipt to the generated header, Meson option and
actual compile/link inputs. Verify I386 machine, typed development exports and
closed production legacy `OpenAdapter`; match the immutable Interface8/caps
source to the Git commit. Keep the unchanged DLL at
`C:\Users\Public\DxvkD3D8Candidate-<commit7>-<run>\viogpudxvk.dll`, then set the
frontend path/hash/full-commit pins to those actual values. No b75 fixture is a
production DLL. The original raw ZIP and separately sourced Khronos loader/Mesa
ICD remain separate provenance chains. No runtime admission or hardware result
is implied by configuration or packaging.

The frozen77bbe CPU packet needs no future core. It builds four harness CPPs,
four original I386 COFFs and three PEs. It executes policy306, frontend
null/invalid-caller guards and14 malformed CLI cases only. No genuine D3D8
enumeration, selector or device call runs. Eleven b75 core references are
explicitly uncompiled. The supplemental native parser checks the original two
PowerShell helper hashes/syntax and compiles the exact bounded raw-process C#
type without launching any process.

Deferred orchestration is in the workspace artifact directory
`artifacts/dxvk-native-d3d8-system-runtime-20261007/native-readonly-handoff-77bbe79-03/`:

```sh
export PWF_PLAN_ROOT=/home/sunf/droidvm-repos/dxvk-umd-ci
# Root executes only after exclusive CPU ownership is released to root.
bash artifacts/dxvk-native-d3d8-system-runtime-20261007/native-readonly-handoff-77bbe79-03/deferred-native-readonly-77bbe79-01.sh
```

This file contains exact staging, native parser, build and collection commands.
It uses fresh public directories, the original compiler receipt at
`C:\Users\Public\DxvkD3D8EwdkCompiler-01\compiler-ready.json` and the mounted
28000 kit. Compiler auto selection prefers official Hostarm64/x86 and explicitly
permits/labels official Hostx64/x86 emulation. Original failed outputs remain
collectable. The local handoff JSON pins all five original packet/helper files
and the supplemental parser before transfer; the prepared shell file has not
been executed by this agent. Root subsequently ran the original77bbe packet:
native parsing passed, then header inventory failed before compilation because
it requested `um\bcrypt.h` instead of the SDK's `shared` header. Root retains
that complete failed original archive; a separate immutable helper follow-up
will supply the corrected inventory. No original packet or receipt is changed.

Local configuration verification compiled the original production loader/query
code for I386, AMD64 and ARM64 into explicitly named loader fixtures, replacing
only logger output. Each private image contains its configured wide loader name
and omits both public fallback names. An empty-option control retains public
search. All912 recorded transitive/tool inputs remain unchanged;15 original
COFFs and3 private fixture PEs were independently inspected. Unmodified
`util_string.cpp` emits one unused-parameter diagnostic per build, retained in
the raw cross logs. This verifies local source/configuration compilation; it is
neither a full production core nor native Windows/GPU execution. The additional
CI PowerShell receipt code still needs native parsing/execution before acceptance.
