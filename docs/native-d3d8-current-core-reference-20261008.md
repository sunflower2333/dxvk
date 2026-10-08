# A separately admitted current core for the system D3D8 phases

The INDEX16 production correction needs a new genuine CI core. The existing
base CPU, owned ICD setup and frontend lifetime producers remain e3, 37b and
fbd respectively. Their original output bytes and admission records retain
their original source identities. A new core does not inherit old names, HAL,
offscreen or Present acceptance.

Historical manifests omit `current_core` and retain the exact de72 tuple.
New manifests add `current_core` with `accepted`, `source`, `ci_run`, `bytes`,
`sha256`, `machine` (I386 332), and `ROOT_reference` (an absolute host original
file pin containing `path`, `bytes`, `sha256`). The existing `core_source`,
`core_ci_run` and core file record must match that separate tuple. Candidate
paths derive from its actual source prefix and CI run, retaining the `-icd02`
suffix and the independently pinned unchanged loader/ICD/configuration bytes.

The ROOT reference has schema `system-d3d8-current-core-reference-v1` and
contains `verified`, `root_originals_direct_review`, `source_commit`, `ci_run`,
and `core` (`bytes`, `sha256`, `machine`, `original_ZIP_member`, whose value is
`viogpudxvk.dll`). It pins `ROOT_source_core_admission` using the existing
ROOT source/core admission shape: actual six completed successful jobs,
the exact source/run, full independent `ROOT_original_review`, original
collection and actual x86 core file. The original full ROOT proof must select
the same core and collection. Every referenced original is reopened and
checked against its byte length and SHA256.

The reference also pins `original_run_API`, `original_artifacts_API`,
`original_backend_ZIP` and `index_hint_source_original`, and records
`api_artifact_id` and `required_index_hint_source_sha256`. The decoder joins
the actual successful run and matching artifact API row to the ROOT original
collection. It reopens the unique ZIP core with CRC validation, matches its
bytes to the full ROOT proof, verifies unsigned I386 PE32 headers, and reads
the original native build configuration and canonical Git source record.
The configuration must select the private module loader and exact source/run;
the changed device source must match the canonical raw Git blob, length and
SHA256. ROOT separately reviews the concrete required source change before
issuing the pinned reference; an older CI lacking that source is insufficient.

The Windows runner checks the separately authorized manifest's current core
metadata against the actual core file pin. The independent archive decoder
performs the host original provenance joins. Previous-phase proof, runtime
arguments, output source/run and private payload paths all use that exact
joined current tuple. Caps, callback lifetime, selector restore, protected
state and pixel requirements stay active.

Controls use synthetic files and receipts, including the real archive caller
prefix and all three runtime dispatch modes. They reject mismatched ROOT,
API, ZIP, Git, configuration and PE records and reject old runtime traces for
a new core. These controls do not claim CI, native loader or GPU execution.
The concrete forthcoming CI reference, fresh staging, native AST parsing and
separate names/HAL/offscreen/Present originals remain necessary.
