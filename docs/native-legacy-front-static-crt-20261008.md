# Preserve native static CRT inputs in the ARM64X merge

The CPU05 build at `d225a2cae78804ef6fea10214d8039a99b591287` passed native
ARM64 compilation and linking, and ARM64EC compilation. Its flat ARM64X link
then reported LNK4294, a missing native load-config symbol, followed by LNK1218
because `/WX` treats that warning as an error. The failed originals are retained
under `artifacts/legacy-runtime-deploy-20261008/native-arm64x-front-d225a2c-05`;
ROOT accepted their collection and explicit release with SHA256 `fdd4dbf0…`.

The actual native full-path response contains the definition file, native
object and ARM64 `kernel32.lib`. It omits the static CRT libraries selected
through object `/DEFAULTLIB` directives. The later ARM64EC stage changes `LIB`
to its x64-compatible directories, so those implicit native inputs cannot be
reconstructed by reusing that search path. This is a candidate explanation for
the load-config failure; an actual corrected native build must establish it.

The producer now resolves `libcmt.lib`, `libvcruntime.lib`, `libucrt.lib` and
`oldnames.lib` in the original ARM64 `LIB` directory order. It requires each
selected file to come from an ARM64 directory, records its complete path, size
and hash, and passes it explicitly to the first native link. After linking it
requires those same four files in the actual full-path response before carrying
that response into the flat ARM64X argument array.

The frontend source, `/MT`, `/W4`, `/WX`, optimization, seven native tool stages,
deadlines, failure collection and restoration of the original library
environment are preserved. No load-config symbol is fabricated and no warning
is suppressed. This change neither loads a frontend/core nor changes driver
registration. Native parsing, corrected linking, hybrid headers/imports/exports
and separate view tests remain required before compiler attestation passes.
