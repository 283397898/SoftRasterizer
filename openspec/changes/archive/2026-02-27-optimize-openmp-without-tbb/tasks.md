## 1. Baseline 与热点确认

- [x] 1.1 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 为 bin 统计与归约阶段补充阶段计时点（仅记录，不改逻辑）
- [x] 1.2 在 `SoftRenderer/src/Pipeline/OpaquePass.cpp` 补充并行构建阶段统计输出开关，形成基线日志

## 2. Rasterizer bin 归约去串行化

- [x] 2.1 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 将 Pass1 的 `critical` 合并改为分块并行归约实现
- [x] 2.2 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 保留旧归约路径的可切换回退分支（用于快速回滚）
- [x] 2.3 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 校验并修正新归约路径下 `binCounts/binOffsets/binTriIndices` 一致性断言

## 3. OpenMP 调度策略可配置化

- [x] 3.1 在 `SoftRenderer/include/Render/RendererConfig.h` 增加 OpenMP 调度与 chunk 的配置字段（内部可用）
- [x] 3.2 在 `SoftRenderer/src/Render/RendererConfig.cpp` 增加配置默认值与边界校验
- [x] 3.3 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 应用配置到 tile 光栅并行循环（`dynamic/guided/static`）
- [x] 3.4 在 `SoftRenderer/src/Pipeline/OpaquePass.cpp` 应用配置到 DrawItem 并行构建循环

## 4. 可观测性与回归守护

- [x] 4.1 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 输出分阶段耗时（build/bin/raster）与线程数信息
- [x] 4.2 在 `SoftRenderer/src/Renderer.cpp` 将 OpenMP 调优日志接入现有帧统计输出路径

## 5. 验证与构建

- [x] 5.1 在 `SoftRenderer/src/Pipeline/Rasterizer.cpp` 以固定输入执行渲染一致性自检（像素统计/深度统计不回退）
- [x] 5.2 更新 `openspec/changes/optimize-openmp-without-tbb/tasks.md` 勾选状态并记录性能对比结果
- [x] 5.3 执行 CMake Release 构建验证并确保 `MFCDemo` 可启动渲染（不引入 TBB 依赖）

### 性能对比记录（本次会话）

- 关键路径改动：
	- bin Pass1 归约从 `critical` 串行合并切换为分块并行归约；
	- 新增 OpenMP 调度可配置（`static/dynamic/guided + chunk`）；
	- 增加 Rasterizer/OpaquePass 分阶段计时日志与一致性校验。
- 构建与运行验证：
	- CMake 构建成功（SoftRenderer + MFCDemo 链接通过）；
	- `MFCDemo.exe` 启动并返回 `ExitCode=0`；
	- 未引入 TBB/oneTBB 依赖。