# Findings

ROOT reports forward PNP restart adopted the exact DX11 front and SYSTEM D3D11CreateDevice returned S_OK FL10_0, but front event trace has no typed CreateDevice. Need discriminate actual negotiation from frontend instrumentation.

Microsoft26100 header defines Win7 DDI11 revision0xb000b/build2, separately from reported0xb000a/build2. Existing core rejects Win7 entirely. Microsoft defines one shared typed11 table/corecallbacks; DXGI1.1 selection remains independently determined by SDKmacro. Added exactWin7 mapping/reporting and coherent front/probe/literalreader support; independentcontractaudit is checking scheduling before final freeze. Full original collection located in workspace-root artifacts; earlier failure was wrong relative directory.

Independentcontractauditconfirmed common150-entrytyped11device/40-entrycorecallbacktable, unchangedWin7kernelcallbackprefix andreserved_ ADVSCH_ fields (noAllocationInfo2/GPUVAchange). Narrative originals under /home/sunf/droidvm-repos/windows-driver-docs/windows-driver-docs-pr/display. ExactWin7fix is justified ABI extension; reasonoldruntimeclosedeviceisstillhypothesis. Existing pfnPerformAmortizedProcessingCb requirement missinginbothrevisions; ROOTinformedandoutside this versionnegotiationfix.
