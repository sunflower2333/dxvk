# D3D10 shader integration

Goal: reuse the actual decoded SM4/4.1 shader backend on the typed D3D10/10.1 DDIs while preserving runtime admission, stage/version rules and data-only stream output.

- [complete] Production integration and typed/native controls; old SO cases preserved.
- [complete] Strict official-header compilation, portable policy execution and reader mutation controls.
- [complete] Freeze, commit and hand off bounded source/local evidence; CI native execution and ordinary hardware admission remain pending ROOT.
