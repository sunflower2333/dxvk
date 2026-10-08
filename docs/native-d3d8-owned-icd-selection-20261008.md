# Original I386 ICD selection correction

The retained enum01 attempt genuinely entered the production de72 legacy device
constructor through Microsoft Direct3DCreate8. Its private loader was found, but
only the provider's VK_KHR_win32_surface appeared among enabled extensions.
Vulkan instance initialization failed; no HAL or rendering admission followed.
The original core log does not report VkResult, so its numeric value remains
unknown. This change does not infer it from D3DERR_NOTAVAILABLE.

The exact original x86 Mesa ZIP JSON is 145 bytes, SHA256
74d7d5d6ae9432cde2d802507ed59bbe4c2f2b95d01e7ac3c2b56e9691932c80.
Its library_path is the bare name viogpu_gl_vk_x86.dll. Exact Khronos loader
source 6a6878c614c8c6dbe81ee7a9f1176bdb52dc7dd7 (SDK 1.4.304.0) intentionally
leaves bare names for OS DLL search. A library_path containing a directory
separator is joined to the JSON directory. This source identity is recorded
inside the genuine loader ZIP. The current candidate folder differs from both
the probe's executable folder and its working directory. Loading the private
loader itself with LOAD_WITH_ALTERED_SEARCH_PATH does not configure later ICD
searches. Actual successful d7 compute and D3D9 runners create separately owned
private JSON with an explicitly relative .\viogpu_gl_vk_arm64.dll path.

The derivation script preserves the complete original JSON and replaces exactly
one original library_path string with .\viogpu_gl_vk_x86.dll. Its 148-byte result
has SHA256 f50169e3e0efc6dea34fe0ce109228c79ce1df817a5508fb13a759c71d780ff3.
All other JSON fields and original bytes remain unchanged. The new diagnostic
pins all four originals plus this separate derivative, then selects the derived
file for both VK_DRIVER_FILES and VK_ICD_FILENAMES. Both variables are removed
on scope exit as before. No DLL search directory, registry, API caps, callbacks,
public device permission or fallback is changed.

The explicit fresh candidate suffix -icd02 preserves the real production
source and CI run. Other suffixes, malformed runs, traversal and alternate
streams remain rejected. The frontend also validates this path, so both probe
and frontend require a new native setup build. Their original e3 CPU09/10
fixtures, native binaries and failure evidence are retained independently;
new modified binaries must not inherit those original hashes or source identity.
All three production/private module bytes remain the original de72/6a/844 tuple.

Local controls verify the exact reversible derivation, reject changed source
JSON and overwrite attempts, execute 509 policy checks under ASan/UBSan, and
strictly cross-compile/link I386 probe/frontend. The exact loader path function
also runs locally with platform/allocator stubs. These establish the path
semantics and setup correction, not a native VkResult or HAL success.

The ordinary legacy constructor already snapshots runtime callbacks, creates
RuntimeGpu, then runs D3D9Backend::create through RuntimeService. GpuBackend
creates DxvkInstance before the bridge context callback, so this observed
instance failure occurs before runtime GPU context initialization. Runtime
service registration and callback marshalling are preserved.

A future native packet must separately bind the narrow new probe/frontend build,
original CPU09/10 base proofs, fresh five-file staging and its derived provenance,
updated exact phase descriptor/source checks, and the actual native syntax gate
for any changed worker/helper. Existing 3e/native parser evidence applies only
to unchanged helper bytes. No current 0f/dbe manifest or enum01 original is
modified or admitted retrospectively. Genuine HAL enumeration, public device,
rendering, readback, Present and default VM rendering remain pending.
