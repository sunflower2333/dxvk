# Exact private CI payload for the single SYSTEM binding owner

This follow-on fixes the frozen5b35 owner's hardcoded `winevulkan.dll` and ICD
format1.0.0 assumptions. The actual `build-native-umd.yml` passes
`viogpu_gl_loader_${arch}.dll`. The core's configured private branch appends
that name to its own module directory, verifies the loaded full path, and has
no public loader fallback. The retained USER06 ICD is exactly220bytes,
SHA256 `bd78e20731b52e6d2ded5d1c2ff23eb44127d009a72aaa50ae46a55a57d84dce`,
with format1.0.1/API1.4.354/architecture64 and
`library_path` equal to `.\viogpu_gl_vk_arm64.dll`.

The existing controller now requires `-ApprovedPayload <absolute-json>` and
`-ApprovedPayloadSha256 <ROOT-reviewed-64hex>` only with
`-ApplyReviewedTuple`. Default read-only snapshot/query does not compile or
invoke the new metadata helper. The same owner supports9/9ex/10/11, the same
native slots0/1/2, original global lease/rescue namespace, durable mutation
intent and raw six-value restoration. Frozen648/ac77/5b35 packets stay intact.

The approved JSON has exactly these fields:

- `schema`: `system-runtime-approved-payload-v1`.
- `source_commit`, `ci_run`, `ci_run_attempt`, `ci_repository`, `arch`: actual
  admitted source/run/attempt/repository and arm64 or x64. NULL or pending
  values fail. The selected probe's PE machine must match.
- `ci_result`, `core`, `build_configuration`, `canonical_source`,
  `private_loader`, `icd_json`, `icd_library`: each exactly
  `{path,bytes,sha256}`, with a normalized absolute Windows drive path,
  positive byte count and lowercase64hex SHA256.
- `configuration_files`: exactly five `{member,file}` rows. `member` is
  `vulkan_loader_config.h`, `meson-build-options.json`,
  `meson-build-machines.json`, `native-compile-commands.json`, or
  `native-build.ninja.txt`; `file` is an exact file pin.
- `icd_metadata`: the entire parsed, explicitly approved ICD object. Extra or
  changed fields fail. Supported approved formats are1.0.0 and1.0.1, each
  requiring exact API version, `library_arch`64 and library path. The path is
  either normalized absolute or `.\<one-basename>` beside this JSON; bare
  relative paths, parent traversal and searches fail.
- `dependencies`: up to32 exact additional file pins. These are the reviewed
  payload's explicit DLL dependency closure, including any required zlib
  DLL. Every listed dependency must actually be loaded by the held probe;
  do not list unrelated tooling, configuration files or optional unused DLLs.

ROOT's explicitly approved tuple hash binds these payload selections; the
helper does not create or admit a tuple. Its CI result must be verified and
join `core_production_bindings[arch]` to the same source/run and successful
producer. Staged core/configuration/canonical source hashes and byte counts
must match that original binding. The build configuration's source/run/
attempt/repository, twelve source-before/after Git rows, generated header,
five configuration originals, Meson options/host architecture, linked DLL
hash and configured private basename must agree. The exact private loader
must be that basename beside the staged core, with its approved hash; every
ICD/dependency raw file is hashed before worker setup and checked again by
the worker. No C# metadata gate calls LoadLibrary, a core export, KMT,
registry mutation, a GPU or an ordinary factory.

Current published source is `0ccc3a2fb228afd9cb8a224c7aaa473177d56610`, sole
Native UMD run37814911005 attempt1. Its six jobs and actual core/configuration
originals remain pending at preparation time. No current approved tuple,
core hash or hardware acceptance is synthesized here. USER06 source18452 is
a historical metadata control, not a replacement for current CI originals.
The CI collector does not ship the private Vulkan loader/ICD payload: ROOT
must separately approve their actual current-core-compatible hashes and
dependency closure before this owner can apply a tuple.

After the real probe publishes its held checkpoint, the worker enumerates
only that retained child. It requires exact loaded front/core/private loader/
ICD/dependency paths, byte counts and hashes, and rejects public fallback or
WARP modules. This supplies the missing D10 loader/ICD census without changing
frozen8eeb's producer. D9's frozen739 and corrected D11 probe786 retain their
own in-process module checks. A census failure publishes a failure-held
checkpoint so the owner still restores and releases; final success requires
a successful census. The census's PID/start and positive retained handle
must join the original runner's PID/start and independent positive retained
handle. Numeric handle equality is not used. This closes the possibility of
combining a reused PID's census with the original drained runner receipt.

`held-module-census.json` and `approved-payload-verified.json` are outside all
probe raw directories. Ordinary pixel/readback readers remain unchanged;
ROOT must join census, exact original runner/held identity, raw restoration,
genuine before/after KMT names and real KMD GPU originals before hardware
admission. D10 retains two Presents, D9 has its selected offscreen or Present
phase, and D11 remains the bounded FL10_0/512pixel/zeroPresent slice. This does
not clear capability masks or change Mesa/default/DWM registration.

Local strict Mono controls use the real retained USER06 config/header/Meson/
220-byte ICD plus explicitly synthetic CI-result/payload/census rows. They
exercise matching metadata for all four API profiles and mismatched source,
run, hashes, private name, ICD metadata, dependencies and reused PID starts.
They do not claim a current-core, registry, loader or GPU execution.
`scripts/validate-system-runtime-approved-payload.ps1` is a separate Windows
PS5.1 AST/Add-Type/pure-control preflight. Pass `-HistoricalOutput` the retained
USER06 `output` directory and a fresh `-Output`; it parses the owner but never
executes it. Windows execution of this new follow-on remains pending ROOT.
