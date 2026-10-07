# Typed D3D11 cube probe on VIOGPU

`dxvk-umd-d3d11-cube-probe` exercises the development adapter export through
real KMT callbacks. It selects one explicitly supplied LUID and writes to a
fresh directory. The caller must provide the reviewed matching core and run
under the interactive limited USER session. The probe does not install a
driver or change public runtime admission.

Six cases cover padded immutable cube-array uploads, a boxed upload and copy
between individual faces, three scoped GenerateMips ranges, and a six-face
resource. Full readbacks contain 4,830 R32_FLOAT texels across all 234 face/mip
subresources. Seventeen compute dispatches point-sample every selected face
and relative mip: 120 semantic words plus 968 untouched buffer-tail words.
Integer float values make the expected bit patterns exact.

Every readback retains actual and expected words and original dimensions and
pitches. Shader compilation retains HLSL, DXBC, SHEX and reflected signature
fields. The independent Python reader recomputes the words from the fixed
case arithmetic, verifies the actual container/signatures, and rejects
missing, extra or inconsistent files. Its byte result is joined separately
with process ownership, token/session, core identity and driver continuity.

Local strict x64 and x86 compilation passed against official SDK/WDK headers.
The portable oracle passed GCC and Clang ASan/UBSan with 159,292 checks and
154,752 single-bit corruptions. A compiled C++ synthetic exporter also passed
the independent Python word reader; seven negative controls rejected altered
actual/expected words, non-selected face contamination, sample tails, wrong
metadata, short pitches and extra files. These are local reader controls.
Native ARM64 compilation, shader observations and real VIOGPU runs remain
pending.

The shared transport, graphics helper, callback policy and shader reader are
verbatim Git inputs from DX11 probe commit
`f96f512bc5ee1133a15e8eb86db8fedf3a2be89c`. The production base is combined cube
commit `8528d91357255fe8f31138d5438e7313e2367fec`. New Meson/CI hooks build and
ship the private probe and run only its portable arithmetic oracle. Public
runtime, registration, Present and DWM gates remain separate.
