## Why

当前渲染主路径已采用 OpenMP 并行，但在 `Rasterizer` 的 bin 统计归约阶段仍存在明显串行化点（`critical` 合并），线程数升高时扩展性下降。项目已完整围绕 OpenMP 构建并稳定运行，短期切换到 TBB 会引入额外迁移成本与验证风险，因此应优先在现有框架内消除热点瓶颈。

## What Changes

- 保持并行框架为 OpenMP，不引入 TBB/oneTBB 依赖。
- 重构 `Rasterizer` 的 tile bin 统计归约逻辑，移除热点 `critical` 合并路径，改为分阶段无锁/低锁归约。
- 优化高负载循环的调度策略（`static`/`dynamic`/`guided` 与 chunk 选择），减少调度开销并保持负载均衡。
- 为 OpenMP 并行参数引入可控配置（默认值保持现行为主），便于在不同 CPU 核心规模下调优。
- 增加针对关键阶段的基准与统计输出，验证优化收益并防止回归。

## Capabilities

### New Capabilities
- `openmp-tuning`: 提供 OpenMP 关键并行阶段的可配置调度与参数策略，并附带调优与验证基线。

### Modified Capabilities
- `tile-shading`: 调整 tile bin 构建与光栅阶段的并行实现要求，要求避免高竞争串行归约并提升多线程扩展性。

## Impact

- 主要影响代码：
  - `SoftRenderer/src/Pipeline/Rasterizer.cpp`
  - `SoftRenderer/src/Pipeline/OpaquePass.cpp`（仅在需要统一调度策略时）
  - `SoftRenderer/src/Pipeline/EnvironmentMap.cpp`（仅在需要统一调度策略时）
  - `SoftRenderer/src/Core/Framebuffer.cpp`（仅在需要统一调度策略时）
  - `SoftRenderer/CMakeLists.txt`（仅在需要增加可选编译开关时）
- 依赖与架构：
  - 不新增第三方并行库，继续使用 OpenMP。
  - 对外 API 与渲染结果保持兼容，变更集中在内部并行实现与性能可观测性。