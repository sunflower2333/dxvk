# Findings

Existing accepted bee7 controller supports native ARM64/x64 only and edits UserModeDriverName slot0 for API9/9ex, slot1 for API10 and slot2 for API11. API8 original runtime requires UserModeDriverNameWoW slot0 and direct I386 core. Six raw values and both SYSTEM registry views must remain recoverable.

Selected native controller cannot observe WoW KMT effective names. Coordinated original I386 probe now provides --system-names (argc7), no Direct3D/core load, session0-safe exactLUID KMT querying. Controller will retain original process/stdout/stderr and use original/forward/before-reverse/restored queries. Native names remain original all3; WoW slot0 is separately proved. Payload policy has API8/x86-only and exact32-bit ICD metadata branches; no actual CI tuple exists yet.

Collector inspected actual historical I386 loader/ICD imports: no private zlib dependency. Original145/derived148 ICD JSONs have no library_arch member. API8 policy preserves exact api_version/library_path metadata and proves32-bit machines from actual pinned core/loader/ICD PE bytes; native JSON contract still requires library_arch64. Probe independently pins both original and derivative JSONs. Focused peer found pre-reverse watchdog names children needed owner kill-on-close job; D8-Names now acquires existing OwnWorkerLifetime job before launching its retained child.
