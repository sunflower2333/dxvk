# Native D3D11 stream-overflow query creation

After the stream-output declaration repair, the strict native graphics fixture
advanced past GS/SO creation and thirteen transactional rejection controls, then
failed creating native stream-zero overflow query type 13. The selected public
query type was `D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0` (9).

A fresh diagnostic retained the original failure and compared both public
creation APIs on the same WARP device and descriptors. `CreateQuery` accepted
all four stream overflow types 9, 11, 13, 15 with non-null objects.
`CreatePredicate` rejected all four with `E_INVALIDARG` and null objects.
Microsoft documents the latter API for the legacy occlusion and global SO
overflow predicate types. The pinned DXVK implementation applies the same
restriction. WARP happens to expose `ID3D11Predicate` from these query objects;
DXVK only exposes it for legacy predicate types, so that QI is not a portable
creation or ownership requirement.

The native factory now creates stream-specific BOOL results through
`CreateQuery`, including the four queries used to implement aggregate overflow.
The query metadata still marks them as native predicates. The existing native
predication path resolves real query results and suppresses commands when the
result matches the supplied predicate value; it owns `ID3D11Query` objects and
does not require public predicate QI. Legacy predicate creation remains selected
explicitly for the two supported `CreatePredicate` types.

The query fixture checks the API selection for all stream stats/overflow types
and both legacy predicates. The native graphics fixture adds stream-two
predication after a real overflow: a suppressed draw must produce a real
zero-primitive statistics interval. Ending or destroying the bound predicate
must report an error, preserve the query, and still return its true result.
The prior aggregate, overflow, byte, signature and graphics assertions remain.

All six changed production/query/graphics x64/x86 official-header COFF controls
pass. The host Clang check uses the SDK's `_CRT_USE_BUILTIN_OFFSETOF` switch;
native MSVC compilation uses the original SDK definitions. Actual full ARM64
WARP15 execution of this frozen correction is pending at source freeze.

Diagnostic originals and the independent join are retained under
`artifacts/dxvk-native-dx10-dx11-20261007/guest-warp-e6bf08d-diagnostic-01`.
The original archive SHA256 is
`a86e10ff512515a8cb6572b6677969f037c429101e4081643126b226ac3dc613`.
The join includes 88 source files, 21 headers, 19 ARM64 outputs, six completed
owned process stages, all eleven retention controls, and exact results for all
four public stream overflow descriptors. The preceding exact e6bf08d attempt
and host diagnostic setup failure remain separate originals.

This slice descends from the frozen f4 source packet. It does not claim native
acceptance of unrelated integration-trunk changes, ordinary runtime activation,
or hardware rendering. Production D3D11 admission stays closed. The existing
CPU/GPU synchronization in native predication remains a performance limitation.

Primary sources:

- [CreatePredicate](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createpredicate)
- Official SDK `d3d11.h` and WDK `d3d10umddi.h` in the retained header packet.
- Pinned DXVK `src/d3d11/d3d11_device.cpp` and `d3d11_query.cpp` public API and QI implementations.
