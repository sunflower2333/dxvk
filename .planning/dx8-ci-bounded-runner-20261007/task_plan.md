# Native CI fixture process fix

Base: `61e3d3937088f97d0d4c6b76bd17b8040328b72c`. Work is local; root owns
integration, CI pushes and target access.

- [x] Reopen exact successful 7c1f545 DX8 native original archive independently.
- [x] Audit the actual single workflow and `Invoke-BoundedFixture` waits.
- [x] Use unchanged native-tested d8cf5089 C# source and preserve fixture calls.
- [x] Retain original runner bytes and per-process exit/capture evidence.
- [x] Compile and execute meaningful local positive/failure/capture controls.
- [x] Commit separately from the loader configuration; retain exact local pins.
- [ ] Prepare the root-owned native parser/function smoke packet from committed bytes.

Native PowerShell parse/CI execution of the changed wrapper remains pending
the root-owned target slot or consolidated workflow. No target operation or
hardware acceptance is authorized for this local task.
