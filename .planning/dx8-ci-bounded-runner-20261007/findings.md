# Findings

- The old CI function has two parameterless `WaitForExit()` calls; one follows
  `Kill()`. Both are removed by calling the already native-tested raw runner.
- The exact C# source SHA256 is d8cf5089bfe02483e8fc3014645ebe08a2683ad53b2a9052637eb46586e0ddad.
- All 26 fixture invocation lines and the sole workflow are byte-identical to
  the preceding 61e3d393 loader-config commit.
- The component keeps one OS process handle, reports observed exit status,
  bounds kill/reap to five seconds and drains both raw pipes in one 20-second
  wait. `CreateNew` protects prior output; reruns need fresh paths.
- Native 7c1f545 original verification: 29 source inputs, 575 compiler files,
  eight SDK headers, seven libraries, 28 process records, 306 policy checks,
  14 CLI guards, four strict zero-warning COFF compiles and three I386 PEs.
- Original native verification01 failed only on case-sensitive `BCRYPT.dll`;
  actual imports contain `bcrypt.dll`. The old attempt is retained. Fresh
  verification02 corrects case handling and passes all remaining gates.
- Native-original proof02 SHA256: 1600e886a2a6ac84ac22afe67b41ac4e6c70d3f18ece73b5c108446664aa4482.
- Local unchanged-C# controls all pass: exit0, exit7, dual pipes over 1MiB,
  timeout/kill/reap, missing executable, existing output and invalid deadline.
  Actual timeout child exit137 and both drained pipes were retained.
- Local component proof SHA256: 57b4975b7b6f4a0d5e35a4ddb3aa2b24ca47b46edc83fed97a7a6ed8473ac4ec.
- No Windows-native changed-wrapper or full-core/runtime proof is claimed.
