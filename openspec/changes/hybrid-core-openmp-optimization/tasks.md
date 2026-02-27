## 1. CMake 构建系统：添加 SR_INTEL_OMP 条件编译宏

- [x] 1.1 在 `SoftRenderer/CMakeLists.txt` 中检测 IntelLLVM 编译器并添加 `SR_INTEL_OMP` 编译定义：`target_compile_definitions(SoftRenderer PRIVATE $<$<CXX_COMPILER_ID:IntelLLVM>:SR_INTEL_OMP>)`

## 2. 扩展 OpenMPTuningOptions 配置结构

- [x] 2.1 在 `SoftRenderer/include/Pipeline/FrameContext.h` 的 `OpenMPTuningOptions` 中新增字段：`clipSchedule`/`clipChunk`、`binCountSchedule`/`binCountChunk`、`clearSchedule`/`clearChunk`、`postProcessSchedule`/`postProcessChunk`，使用 `#ifdef SR_INTEL_OMP` 条件编译选择默认值（ICX: Guided，其他: Dynamic）
- [x] 2.2 在 `SoftRenderer/src/Render/RendererConfig.cpp` 的 `Sanitize()` 中对新增的 4 个 chunk 字段做 `ClampChunk()` 校验
- [x] 2.3 在 `SoftRenderer/src/Renderer.cpp` 的 `LogFrameStats()` 中补充新增调度字段的日志输出

## 3. KMP 亲和性初始化（条件编译隔离）

- [x] 3.1 在 `SoftRenderer/src/Renderer.cpp` 的初始化路径中添加 `#ifdef SR_INTEL_OMP` 保护的 KMP 环境变量设置代码（`KMP_AFFINITY=granularity=core,balanced`、`KMP_HW_SUBSET=1t`），确保在首个 OpenMP 并行区之前执行

## 4. Rasterizer 调度策略替换

- [x] 4.1 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` clip 阶段（约 L326）将 `schedule(static)` 替换为 `omp_set_schedule()` + `schedule(runtime)`，使用 `ompCfg.clipSchedule` 和 `ompCfg.clipChunk`
- [x] 4.2 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` bin counting 新路径（约 L720）将 `schedule(static)` 替换为 `omp_set_schedule()` + `schedule(runtime)`，使用 `ompCfg.binCountSchedule` 和 `ompCfg.binCountChunk`
- [x] 4.3 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` bin counting 旧路径 / legacy（约 L682）同步替换调度策略
- [x] 4.4 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` bin fill（约 L763）和 bin sort（约 L788）将 `schedule(static)` 替换为 `schedule(guided)` 或 `schedule(runtime)`

## 5. Framebuffer / DepthBuffer 调度策略替换

- [x] 5.1 在 `SoftRenderer/src/Core/Framebuffer.cpp` 中修改 `Clear()`、`ClearLinear()` 的 `schedule(static)` 为 `schedule(guided, clearChunk)`；由于 clear 函数不持有 FrameContext，改为直接使用 `schedule(guided, 4096)` 硬编码 guided 策略（条件编译：`SR_INTEL_OMP` 下 guided，否则 dynamic）
- [x] 5.2 在 `SoftRenderer/src/Core/DepthBuffer.cpp` 中修改 `Clear()` 的 `schedule(static)` 为条件编译的 guided/dynamic
- [x] 5.3 在 `SoftRenderer/src/Core/Framebuffer.cpp` 中修改 `ApplyFXAA()` 和 `ResolveToSRGB()` 的调度策略为条件编译的 guided/dynamic

## 6. EnvironmentMap 调度策略替换

- [x] 6.1 在 `SoftRenderer/src/Pipeline/EnvironmentMap.cpp` 中将 `ComputeSH9()` 的 `schedule(static)` 替换为 `schedule(guided)`，`ComputePrefilteredSpecular()` mip0 的 `schedule(static)` 替换为 `schedule(guided)`（环境贴图为初始化阶段，使用 guided 硬编码即可）

## 7. Per-thread 耗时采样

- [x] 7.1 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` tile raster 并行区内，当 `ompCfg.enableProfiling` 时用 `omp_get_wtime()` 记录每个线程的工作时间，输出 max/min/imbalance 指标。受影响位置：tile raster `#pragma omp parallel` 块（约 L853-L1160）

## 8. 编译验证与构建测试

- [x] 8.1 使用 ICX 编译器执行 Release 构建，确认 `SR_INTEL_OMP` 宏生效、所有新增代码路径编译通过
- [ ] 8.2 使用 MSVC 编译器执行 Release 构建（若可用），确认非 `SR_INTEL_OMP` 路径编译通过、无警告
- [ ] 8.3 运行 MFCDemo 加载测试场景，启用 `enableProfiling`，验证 per-thread 时间输出和 imbalance 指标正常
