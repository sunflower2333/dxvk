# ARM64 standalone DXVK native-DDI probe checkpoint

Source: b47f9555631781974982e0e5b7eb9a822318e615.
CI: https://github.com/sunflower2333/dxvk/actions/runs/34597997951 — all jobs passed.
Local directory: `.planning/dxvk-umd/probe-b47f955-arm64/` in this checkout.

Copy these three files together into an isolated Windows test directory.
No registration, INF installation or system Mesa replacement is required.

| File | SHA256 |
| --- | --- |
| viogpudxvk.dll | 5e1b6e604dfae7a81f2b3eae2d2537aae4c84536e413616bd833d3e12d7add52 |
| dxvk-umd-backend-probe.exe | e6a5f76dfa63c4150e05cee458beb111e7e05990da1a1e87a116116580c687d5 |
| dxvk-umd-ddi-probe.exe | d87a7a408f60738549a9c2d9e7c82d6638978b5fc2b8bbab5e5b0c576701af64 |

Use the exact adapter's eight LUID bytes encoded as 16 hex characters in
memory order. The KMD must supply the versioned adapter-identity trailer;
the installed Vulkan ICD must report the same valid LUID and Turnip driver ID.
The probe depends on the existing Windows D3DCompiler_47 and Vulkan loader.

```text
dxvk-umd-backend-probe.exe <16 LUID hex characters>
dxvk-umd-ddi-probe.exe <same LUID> --native-copy
```

The backend probe verifies 4096 magenta pixels. The DDI probe verifies the
64-byte buffer transfer, indexed/additive/sampled drawing, bound scissor,
depth/stencil occlusion counts 0 / 4096 / 0, 4096 final red pixels and a
completed GPU event. `--native-copy` then uses real KMT allocation/context
and Lock/Unlock calls to publish and verify those red pixels in kernel
backing. Both commands must exit 0.

`KMT_ALLOCATION_PUBLICATION` is backing-allocation evidence. The harness
does not call display Present or activate the Microsoft D3D runtime.
This checkpoint has not run on the target device. Parent owns remote execution.

Paired package 60005 pins this exact child under driver parent
a27eb3e80f13c13593fb38573f23328769829b74, CI 34598876655 (pending).
Its older Mesa 4ace9df is unrelated to the standalone probe: preserve the
newer active desktop Mesa when preparing hardware testing.
