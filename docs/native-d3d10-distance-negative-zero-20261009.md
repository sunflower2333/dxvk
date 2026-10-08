# Preserve negative-zero bits in the original FXC distance SO fixture

The `c8cda1f` CI run 37835241647 failed the first D3D10 distance stream-output literal check in both x86 and x64 jobs. Their retained original SM4 VS token streams are identical and contain `mov o2.x, immediate32 0x00000000`: FXC folded the constant `asfloat(0x80000000)` to positive zero before either stream-output path ran. The preceding native/public equality CHECK passed; the old fixture aborted before retaining either role's SO buffer.

The fixture now constructs the bits with `asfloat(0x80000000u | (id & 0xfffffffcu))`. The only vertices drawn by this fixture have IDs 0, 1, and 2, so their exact value remains `0x80000000`; the shader input keeps the expression runtime-dependent. The literal negative-zero oracle is unchanged. New native FXC tokens and stream-output captures must establish the actual compiler/runtime outcome.

Both roles' 256-byte buffer and 48-byte query records are retained with CREATE_NEW before any literal or native/public equality check. A literal mismatch prints its model, GS selection, sparse selection, alternate VS selection, role, word offset, actual bits and expected bits. Successful CHECK counts, stream declarations, offsets, strides, shader rebinds, query checks and layouts remain unchanged.

The independent Python reader changes only its exact embedded HLSL SHA to `99f1c6067fd4aa7213b4cddfc39b8878409b6c3413f390fe2c82b2a1e13b2028`. Its existing 77-file closure, 16 scenes, 32 buffers, 32 query frames, 6 FXC programs, 2048 literal word observations and 192 query words remain unchanged.

Local validation compiled the changed fixture against the existing actual SDK/WDK headers to strict optimized i686 and x64 COFF with no compiler output, then reopened both objects using LLVM and checked the machine types. The original literal expected function, declaration/policy/draw/backend bodies, static CHECK count, and complete reader body after reversing only its source SHA were checked against parent `c8cda1f`. Retained CI source/token/process originals were read without fetching or running them again. Evidence and retained process/output records are under `artifacts/signbit-local-01/`.

This is fixture correction only. Production UMD, advertised capabilities, workflows, registry selection and VM default remain unchanged. No new HLSL compiler, Windows native runtime or target hardware execution has occurred in this local validation.
