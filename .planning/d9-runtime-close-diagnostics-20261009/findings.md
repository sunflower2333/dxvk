# Findings

Actual selected-aa2a2da-bee7-hardware-02 stderr reaches RenderCb and reports
raw80004005, generation2/context10/queue30, references6/stream228/locked5.
The public probe stdout ends before a CreateDevice return record. The final
observer reports context10 complete/released but stopped devnode problem21.

Exact signed KMD58624 is c1b9ea69fa31037c5feeb1cfdb77500118b0ae37, SYSd48e118a.
Its wrapper ABI matches RuntimeGpu: 64-byte header, six32-byte refs and native
stream228 produce484command bytes with stream offset256. Accepted USER06 uses
shared RuntimeGpu callbacks through a test KMT transport that explicitly
makes allocations resident; it is not ordinary SYSTEM D9 runtime proof.

NativeRenderFailure fields are first per adapter and omit early KMD guards.
All-zero values do not prove the runtime refused before KMD. Destroy slot10
stage4095 means complete; observer status0/released1/retained0 establishes
native owner retirement. Neither fact establishes a locking cause.

Guard failure cleanup runs Device::close, backend drain, memory.close,
RuntimeGpu.close and service.close. New finite scalar close records preserve
the existing release order and HRESULT precedence. Finished records describe
control-flow boundaries, not successful release of every owner. Remaining
handles, locked owners and active callbacks are printed with the result.

The signed SYSd48e PE sections match the unsigned58624 build and both have the
same PDB GUIDDCF9D1D3-AFE6-41F0-9374-CF5A1DDE6429/age1. Exact source/PDB and
signed disassembly resolve holderRVA0x4f200 to BeginNativeContextInitialization
after WaitNativeContextLifecycle and failcaller0x16540/site35 to ResetDevice
entry. Holder/acquiretime are not cleared on mutex release; held-ms is the
historical maximum and timeout-stage copies the last destroy-stage. These
fields do not establish a current mutex owner or leaked lock. Context10
completion is recorded after releasing its native lifecycle mutex.
