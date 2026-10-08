# Findings

The opened-primary CI60 x86 log reports failure at line 703 with callback value
0x887b0002. That original log omits the case index. This task does not turn a
source branch inference into an observed native case identity.

The six-case loop expects DXGI_ERROR_UNSUPPORTED for every descriptor. Cases
0..4 carry no primary descriptor and are rejected by generic present/shared
shape checks with that exact value (0x887a0004). Case 5 sets pPrimaryDesc while
retaining MISC_SHARED. createResourceData invokes primaryResourcePlan before
those generic checks; primaryResourcePlan rejects nonzero MiscFlags with
DXGI_DDI_ERR_UNSUPPORTED (0x887b0002).

Local Microsoft D10 and D11 CreateResource documentation explicitly permits
DXGI_DDI_ERR_UNSUPPORTED through SetErrorCb. The local DXGI base-functions page
also identifies that result for BIND_PRESENT + nonnull pPrimaryDesc primary
fallback and its required rotation support. The source already uses that exact
DDI contract; changing production to the generic DXGI error would lose it.

The correction selects the exact expected HRESULT from the descriptor case.
It accepts no alternative or broader FAILED status. The first five statuses,
no new acquisition, zero internal backing and 128 unchanged private bytes
remain strict. Success pixel bodies, raw readers, markers, counts, production
source and admission gates remain byte exact.
