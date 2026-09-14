# 原生 VIOGPU UMD：离线开发第一批（float MRT）

## 基线与范围

本批从 DXVK `a98c19fbc8020ef7f7b47c79ccffe2fcb3c9ad74` 开始，开发分支为
`work/native-offline-contracts-20260915`。它不改变驱动父仓库固定的 DXVK/VKD3D
版本，不注册 UMD，不开启任何 feature level，不改动 VIOGPU KMD 或 Mesa。

本批落实的是此前计划里的多渲染目标（MRT）子项，并不是整个 DXVK/VKD3D 移植。
共享资源打开仍缺少向引擎导入同一 kernel backing 的路径；不能通过新建资源、
复制像素或编造 kernel handle 宣称 OpenResource 已实现。

## 已接入生产路径

`umd_shader.cpp` 的 PS DXBC 重建支持 1～8 个 float 颜色输出，保留每个输出的
寄存器、SV_Target 编号和 component mask；允许稀疏、非顺序输入签名，输出按
寄存器顺序规范化。拒绝重复寄存器、越界编号、空/非法 mask 和明确的整数类型。
本批不增加 depth/integer 输出、SM5/D3D11 完整 shader 支持，也不重写 shader 指令。

`umd_ddi.cpp::setRenderTargets` 支持八个颜色槽和空槽。先检查全部非空 RTV 的
device ownership、当前支持的格式、实际 mip/array/MSAA 尺寸、与 DSV 的尺寸
兼容性、同一资源的重叠子资源，再一次提交给 backend OMSetRenderTargets。
错误的最后一个目标不会清掉前面的绑定；ClearSlots 不是部分更新指令。
省略的尾部解除绑定，count=0/clear=0 仍清空颜色目标。

本批颜色格式仍限定 RGBA8_UNORM/BGRA8_UNORM；不是完整格式或资源类型支持。
现有每槽 blend enable/write mask 和 D3D10.0 共享 blend factors 可用于多目标。
原生句柄仍须满足现有 runtime/DDI 私有对象契约；本批没有实现任意野指针安全
检查。现有 `runtimeMissingD3D10Requirements()` 返回的缺口位不变。

## 可重复验证

```sh
# 子模块是基线已固定的版本，不跟随 master。
git submodule update --init --depth 1 subprojects/dxbc-spirv
bash scripts/test-umd-mrt.sh
CXX=clang++ bash scripts/test-umd-mrt.sh
bash scripts/test-umd-mrt.sh --negative-control
bash scripts/test-native-umd.sh
```

Linux 执行实际 policy、实际 DXBC 编码器和独立 parser；不是模拟编码器。
覆盖 644 次策略断言和 634 次 DXBC 字节断言，故意把语义编号折叠到零时必须在
预定的编号断言失败。数字是断言次数，不是相同数量的独立测试用例。
既有 identity/runtime-identity/shader-container 用例继续运行。

在已选择 MSVC 和配套 WDK 的 Developer PowerShell 中：

```powershell
./scripts/test-umd-mrt.ps1 -Architecture x64 -OutputDirectory artifacts/mrt
# 选择 x64_arm64 交叉编译环境后：
./scripts/test-umd-mrt.ps1 -Architecture arm64 -CompileOnly -OutputDirectory artifacts/mrt-arm64
```

Windows 使用真正 WDK 类型编译生产 DDI、资源/worker/callback 实现和 shader
代码。`tests/umd-mrt.cpp` 显式注入 WARP，仅在这个测试程序中替换 backend factory；
不会把 WARP 注入正式 UMD。通过实际 DDI 创建 texture/RTV/shader/state，Draw 后
用实际 DDI copy/map 检查全部像素。独立 OMGetRenderTargets 检查绑定是否保持。

五组 MRT 场景为八目标、稀疏目标、末端拒绝保持状态、缩减/全解绑，以及每槽
blend/write mask；三轮读回合计 6144 个像素。视图测试还检查 mip、数组切片
相交/不相交、深度尺寸与 MSAA。ARM64 构建在 x64 runner 上只检查编译/链接和
PE 架构；不能把它当作 ARM64 执行成功。

永久工作流只编译版本库中的真实文件，不在编译时应用补丁。一次性源文件
传输脚本在最终树中删除；预检记录只是审计历史，不能取代最终提交的 CI。

## 仍然未完成

OpenedResource/共享内核 backing 导入、Legacy callbacks 的实际语义、完整
texture/format/shader、primary/flip Present、真实 runtime threading 验收、
VIOGPU 上的 GPU 绘制/读回、reset/TDR，以及 VKD3D 后续 graphics/resource/fence
工作仍待完成。没有把任何候选 DLL 改为已启用，也没有更新父驱动包的生产 pin。
WARP 成功不等于 Microsoft runtime 已加载 viogpudxvk.dll，更不是手机 GPU 验收。

## 接口依据

- https://learn.microsoft.com/en-us/windows/win32/api/d3d10/nf-d3d10-id3d10device-omsetrendertargets
- https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargets
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3d10ddiarg_openresource
