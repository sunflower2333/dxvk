# Findings

The pinned consolidated workflow supplies no VulkanLoader argument. Its genuine
three-architecture production DLL builds therefore use public winevulkan/vulkan-1
search. The private Mesa staging source names the Khronos loader
viogpu_gl_loader_<arch>.dll; the ICD exports ICD hooks, not vkGetInstanceProcAddr.
The production private-loader branch already resolves only beside its own module,
rejects a same-name loaded module from another path, and has no public fallback.

The frozen77bbe packet builds four harness translation units, then executes CPU
policy, null/invalid caller guards and malformed CLI only. It needs no production
core and never executes a genuine D3D8 runtime mode or selector. The b75 references
are explicitly uncompiled; b75 native fixture evidence is not a production DLL.

Future read-only selection must use an unchanged I386 DLL from a successful
exact-source CI run. Its original raw ZIP API hash/CRC, build configuration,
source, DLL hash and machine must be joined before setting the process-local
path/hash/commit pins. Vulkan hardware acceptance additionally needs the matching
private Khronos loader, Mesa ICD/protocol, exact-LUID ownership and real pixels.

The sole automatic workflow remains build-native-umd. Five other workflows have
manual dispatch only; no automatic trigger or job count is added by this change.
The current CI fixture runner still contains an unbounded Kill/WaitForExit path;
the original30-minute workflow deadline bounds the job, not the process reap.
That independent issue is reported to root and left outside loader configuration.
