# Native D3D11 compute system input reconstruction

The integrated f4bf37f typed fixture reached its first real compute shader,
then reported E_INVALIDARG. A fresh native ARM64 diagnostic compared the same
FXC bytecode through public WARP: the original container and legacy compute
reconstruction both returned S_OK. The newer SM5 declaration decoder rejected
the program before calling the backend. The captured input declaration used
operand 0x00020012 (DispatchThreadID, vector x mask, zero index dimensions).

The decoder now recognizes the four dedicated Compute input operands:
DispatchThreadID, GroupID, GroupThreadID and flattened GroupIndex. These use
plain dcl_input in SM5; the first three have xyz components and GroupIndex is
scalar. They are supplied by dispatch and have no graphics ISGN entries.
Their declarations and instructions remain byte-for-byte in SHEX. See the
[Microsoft declaration contract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dcl-input-vthread--sm5---asm-)
and the official SDK d3d11TokenizedProgramFormat.hpp operand definitions.

Validation requires the Compute stage, plain input declaration, no register
indices or extra system-value operands, no operand modifier, valid vector
xyz or scalar x component shapes, and no overlapping declarations. Disjoint
vector masks can be declared separately. Rejection preserves empty decoded
output; no backend operation or production capability is fabricated.

The portable SM5 fixture retains its existing351 checks and adds318 checks
for all xyz masks, zero-component/scalar/vector GroupIndex, all four inputs together, exact
reconstructed tokens/hash, empty graphics signatures and malformed
stage/opcode/index/mask/modifier/overlap declarations. GCC and Clang each pass
all669 checks under ASan and UBSan. All six affected x64/x86 COFF units also
pass strict official SDK compilation. The local Clang18 check uses the
existing MSVC44 version-guard compatibility option and canonical Windows
header prelude; actual MSVC /W4 /WX verification remains separate.

The native typed fixture retains the full graphics/query/IA/SO/tessellation/
class controls. Every ordinary Compute creation compares the original FXC,
legacy reconstruction and SM5 reconstruction through actual public WARP.
A new eight-group96-thread dispatch checks every component of all four IDs
against a scalar CPU oracle and the original FXC WARP result. Thirty-nine
malformed typed shader creations must report one live caller-thread callback,
leave private storage and canaries untouched, and preserve a usable existing
shader binding. The native repaired run is pending at source freeze.

The first correction attempt3f64065 compiled strictly but the all-four-ID
control exposed FXC's dedicated GroupIndex encoding: dcl_input 0x00024000
uses zero components, zero selection bits and zero indices. A fresh
diagnostic preserved and dumped the original accepted shader. The decoder
now admits that exact scalar form only for flattened GroupIndex and compares
its raw operand with the canonical token, rejecting selection bits, unused
index bits or extended modifiers. Its effective x bit is used solely to
reject overlapping scalar declarations. Vector zero-component forms remain
invalid. The portable fixture replays the captured114-word SHEX exactly,
including all four declarations and instructions.

The original FXC DXBC SHA256 is
f0520fada658f48ab64429fae886da393d6106aeee60a0543a9fcd32786beb95;
the SHEX payload SHA256 is
c508baceef58b65ad0e53910eaa01d1b2e156333fee9363c7d0eeecb3c9e7328.
The failed3f attempt and diagnostic originals remain in separate evidence
folders; their source and all11 retention checks independently join.

Original evidence is retained separately under
artifacts/dxvk-native-dx10-dx11-20261007/guest-warp-f4bf37f-02 and
its diagnostic-01/02/03 sibling attempts. Diagnostic03 archive SHA256 is
023b55081dbe245bf153752dfbfaa625454f2c78bef669ff4acbb4ff7e2698ff;
the independent join verifies88 source files, all11 retention checks, direct
original/legacy success and modern rejection before the backend call.
The original frozen packet is unchanged. WARP is a controlled backend
fixture; native DXVK/Turnip hardware and ordinary runtime admission remain
gated, and this slice changes no capability policy or driver registration.
