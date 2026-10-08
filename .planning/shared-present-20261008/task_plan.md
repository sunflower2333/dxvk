# Shared present allocation integration

Base32b7bbb068dcbfc77b5fac0be5d374c495afeda9. Goal: one actual allocation owner for legal shared+present resources and opened standard-primary DXGI Present, preserving runtime/KMD lifetime and exact pixel semantics. Ordinary runtime admission and default binding remain closed.

1. Complete: audit existing allocation/shared/present ownership and local Microsoft contracts.
2. Complete source, independent ownership review passed: implement legal common owner and transactional publication.
3. Complete local strict compile and synthetic-reader controls, native execution pending: extend typed native/public pixel and invalid lifetime tests, compile against official headers.
4. In progress: freeze source/evidence and hand off to ROOT for integration.

Errors: first inspection attempted nonexistent src/umd/umd_resource.h; Resource is private in umd_ddi.cpp. No target actions.
