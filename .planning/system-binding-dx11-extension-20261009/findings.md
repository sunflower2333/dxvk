# Findings

- Actual local Microsoft enabling-support-for-the-direct3d-version-11-ddi.md requires the third UserModeDriverName entry even when sharing a DLL.
- Actual initializing-communication-with-the-direct3d-version-11-ddi.md requires OpenAdapter10_2, pAdapterFuncs_2, explicit supported-version/caps exchange, runtime callbacks and D11 device table. D11 DDI does not imply hardware FL11 support.
- Actual retained official d3dkmthk.h guest-sm41-ad138ce-01 lines1824+ enum has DX9=0, then DX10=1, DX11=2. Existing helper's query Type1 uses Version as first DWORD and MAX_PATH UTF16 filename.
- ROOT states dedicated a103 implementation is MAIN6b40a0f. Peer port_dx11 owns the SYSTEM D11 FL10_0 probe/modern forwarder. It confirmed nine operands afterEXE: LUID/front/core/private-loader/ICD-DLL/ownedICD-JSON/rawdir/Local-event/hold-ms. Front OpenAdapter10_2 calls only VioGpuDxvkOpenAdapter11Fl10_0ForValidation. Native same-dir core viogpudxvk.dll; x64 same-dir viogpudxvk_x64.dll. Probe requests onlyFL10_0, UNKNOWN+SINGLETHREADED, and verifies effectiveKMT DX11 before factory. Final negative/held markers awaited before freeze.
- Same global lease/rescue namespace and raw6 restoration will extend selected slot2; no second registry owner.

- Peer draft actual held writer schema1/api11 has pid/event/timeout_ms/output/factoryCalled/factoryResult/pixelsPassed/stage/result; CreateFile share0 then FlushFileBuffers/CloseHandle. Check signed HRESULTs and exact fields/path/event. Failed checkpoint remains holdable for restoration; successful worker requires final zero exit and positive marker. Exact peer marker is SYSTEM_D3D11_HELD, not its earlier intermediate message.
- Dedicated D11 slice has presents=0. Literal factory/draw/readback plus typed negotiation are distinct from later Present/DWM/full FL11 acceptance.

- Actual peer source HEAD0151b8bff2331b32bb2551e95f43e695dd0c5f44 frozen eight files; normal9 operands, negative9, closed JSON and positive marker match this controller. Final finite handoff joins exact producer pins separately from local helper controls.

- Before final handoff, actual peer reader RUNNER requires originald8cf whereas inherited owner uses progress7def. D11 only now selects exactd8cf because closed held JSON supplies readiness. D9/D10 keep7def. Preserve0151 reader bytes and separately pin selected original source; no reader gate relaxation.

- Peer final82963f0b fixes only DEF LIBRARY/OUT conflict. Normal probe/header/frontend/reader contract stays byte-identical to0151; final handoff pins829 for the complete deployable source.
