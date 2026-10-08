# Findings

- Frozen D9 probe739de05 has ten operands: API, phase, LUID, front, core, private loader, loaded ICD DLL, raw directory, Local hold event, hold milliseconds. Its ICD operand is a DLL; VK_DRIVER_FILES remains the separate JSON.
- Native DX9 is zero-based tuple index0; native D10 index1. Both retain the same global adapter lease and rescue task namespace, preventing concurrent old/new owners.
- Production legacy frontend3597/SHA36476e4cabd5faed5c1cbb81ed51ba17bbdb6f8a274eedb248d1ef3ae589acfe selects front-directory arm64 or x64 child core. D10 keeps its same-directory core contract.
- D9 closed held checkpoint is a sibling of rawdir, schema ordinary-system-d3d9-held-v1. Writer uses FileShare.None then FlushFileBuffers and close; retry opening until closed. Retain the process object and compare its PID to the final runner receipt.
- Mutation intent schema2 includes selected_native_slot. Full six-value restore requires matching owner/key/slot and remains under the original restoration mutex. No intent means cancellation without stale backup replay.
- Frozen648f source/preflight and D9 probe739 source remain immutable. Local helper/byte controls prove no Windows registry/process/GPU behavior.
