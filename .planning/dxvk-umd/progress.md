# Progress

2026-09-11: Paired801fde5 CI34589164956 ALL PASS. Logs confirm60002,
both DXVK probes,viogpudxvk.dll,Mesa viogpud3d.dll,KMD sys and cat signed;
final droidvm-arm64-drivers artifact uploaded (11522740bytes). Existing INF
still selects Mesa. Added development OpenAdapter adapter/device lifecycle
and mocked CPU tests; real WDK signatures verified against Microsoft docs
and SDK header. Local identity69/trailer176/shader41 sanitizer tests pass.
New adapter test will run before expensive backend compilation in Windows
CI; its result is pending. No remote hardware operations performed.

2026-09-11: Accepted dedicated DXVK UMD ownership. Read workspace safety rules,
planning skill and current Direct3D audit; verified independent clean branch.
Initial inspection was followed by the implementation and CI results below.

2026-09-11: eaf3f078 implements embedded exact-LUID Turnip factory and real
WDK resource DDIs. CI34586456839 passed all3architectures and identity69
ASan/UBSan cases. Independent driver parent94a27498 pins the DXVK submodule
and builds/signs/packages it beside Mesa/KMD without changing the active Mesa
UserModeDriverName. Paired CI34586690632 has passed regression/Mesa/DXVK jobs;
its driver signing/packaging job is still running. No remote tests performed.

2026-09-11: Added restricted SM4VS/PS container bridge, shader ownership,
rasterizer/viewport/target/topology state and DrawDDIs. Updated probe to require
4096red pixels after drawing over a green clear. Local shader41 and identity69
sanitizer cases pass. Windows CI additionally compiles HLSL and independently
reflects the rebuilt containers with Microsoft's D3DCompiler. That new CI is
pending; this is not native runtime activation or general DXVK UMD support.

2026-09-11: a52df2b compiled across3architectures, but the real Windows shader
CPU test caught token re-encoding differences. Fixed by preserving the raw
runtime tokens; b76de1d fast D3DCompile/container/D3DReflect test passed.
Added proposed identity decoder+callback consumer in7a373289 (no KMD producer)
and176sanitizer decoder cases. a9c69c0 also fixes DDI/API DO_NOT_WAIT bit
translation. Final child CI34589123522 is running. Paired parent801fde5 pins
a9c69c0 and version60002; CI34589164956 is running. Initial Inf2Cat file-list
failure was repaired in vcxproj; main KMD implementation remains untouched.

2026-09-11: b76de1d CI34588836455 is now allPASS: Linux sanitizer cases,
Windows fast HLSL/container/reflection57checks and ARM64/x64/x86 full target
builds. x86/x64 also ran57shader checks and the mock runtime-adapter callback
consumer successfully. a9c69c0 differs by the nonblocking Map flag fix and
coverage documentation; its final CI remains running, as does parent801fde5.
Removed the two disposable /tmp reference-header/test-binary files after use.

2026-09-11: Final child a9c69c0 CI34589123522 allPASS on ARM64/x64/x86,
including57Windows real-HLSL checks and mocked runtime callback consumer.
All3architecture artifacts uploaded successfully. Parent801fde5 paired
CI34589164956 has passed regressions and Mesa compile/link and is now building
its exact pinned DXVK prior to the KMD/sign/catalog job. No hardware tests.
