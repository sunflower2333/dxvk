# Findings

Existing common mipGenerationStatus accepts a singleCube but rejects modern
CubeARRAY views. The typed11 resource/SRV path already exposes real cube
arrays. Validate selected cubes through aligned First2DArrayFace/6 and finite
NumCubes against ArraySize/6; validate mip ranges independently. Resource
shape must be square, one-sample/quality0, have complete6-face groups and
Cube+GenerateMips flags, plus RT/SRV binds.

The fixture has three actual cubes/five mips per case. Uniform source mip1
on every face isolates filtering from scope errors; unique face/mip/index
sentinels on all other subresources reveal unintended whole-chain/cube writes.
Four valid selected scopes and one missing-auto-bit failure retain450 raw
word files+450 metadata files,30690 texels, before comparison.

Original officialSDK x64/x86 production+fixture COFF passes with empty
compiler diagnostics. ARM64 MSVC-target compilation fails in unchanged
Microsoft intrin.h: clang builtin __prefetch disagrees with its const void*
declaration. This toolchain incompatibility precedes our source. No header
shim or diagnostic suppression was added. Actual LLVM-mingw ARM64 fixture
compile passes with Wshadow/Werror and genuineWDK ABI. Native MSVC production
execution remains necessary.
