## MODIFIED Requirements

### Requirement: OpenMP scheduling policy SHALL be configurable per hotspot stage
The renderer SHALL provide configurable OpenMP scheduling policy and chunk-size for ALL performance-critical stages without changing rendering correctness. 配置范围 SHALL 扩展至 clip、bin counting、clear 和 post-process 阶段。

#### Scenario: Configure scheduling for raster hotspot
- **WHEN** the runtime or build-time OpenMP tuning configuration is provided for raster hotspot loops
- **THEN** the system SHALL apply the configured policy (`static`/`dynamic`/`guided`) and chunk-size
- **AND** the final color and depth outputs SHALL remain bitwise-consistent within the same execution mode constraints

#### Scenario: Configure scheduling for clip stage
- **WHEN** `OpenMPTuningOptions::clipSchedule` 和 `clipChunk` 被设置
- **THEN** Rasterizer clip 阶段的 `#pragma omp for` SHALL 使用该配置的调度策略和 chunk 大小
- **AND** 裁剪结果 SHALL 与任意调度策略下保持一致

#### Scenario: Configure scheduling for bin counting stage
- **WHEN** `OpenMPTuningOptions::binCountSchedule` 和 `binCountChunk` 被设置
- **THEN** Rasterizer bin counting 阶段（pass 1 新路径和旧路径）SHALL 使用该配置的调度策略
- **AND** bin 统计结果 SHALL 与基线一致

#### Scenario: Configure scheduling for clear operations
- **WHEN** `OpenMPTuningOptions::clearSchedule` 和 `clearChunk` 被设置
- **THEN** Framebuffer::Clear、Framebuffer::ClearLinear、DepthBuffer::Clear SHALL 使用该配置的调度策略
- **AND** 缓冲区清除结果 SHALL 正确

#### Scenario: Configure scheduling for post-process stage
- **WHEN** `OpenMPTuningOptions::postProcessSchedule` 和 `postProcessChunk` 被设置
- **THEN** FXAA 和 ResolveToSRGB SHALL 使用该配置的调度策略
- **AND** 后处理结果 SHALL 与基线一致

#### Scenario: Fallback to safe defaults
- **WHEN** no tuning configuration is provided or provided values are invalid
- **THEN** the system SHALL fallback to validated default scheduling parameters（ICX 下为 `guided`，其他编译器为 `dynamic`）
- **AND** rendering SHALL continue without failure

### Requirement: OpenMP hotspot metrics SHALL be observable
The renderer SHALL expose stage-level timing and workload metrics for OpenMP hotspots to support tuning and regression detection.

#### Scenario: Stage timing output
- **WHEN** a frame is rendered with profiling enabled
- **THEN** the system SHALL report timing for at least geometry build, bin construction, and tile raster stages
- **AND** reported units SHALL be milliseconds

#### Scenario: Per-thread timing output
- **WHEN** profiling 模式启用
- **THEN** 系统 SHALL 在 tile raster 阶段记录每个 OpenMP 线程的工作时间
- **AND** 系统 SHALL 输出 max/min 线程时间和负载不均衡比率（imbalance ratio）
- **AND** 计时 SHALL 使用 `omp_get_wtime()` 且不引入数据竞争

#### Scenario: Regression guard
- **WHEN** a benchmark run compares baseline and optimized builds
- **THEN** the system SHALL provide enough metrics to determine whether frame time regression exceeds a configured threshold

### Requirement: OpenMP tuning MUST preserve thread safety
All OpenMP tuning changes MUST preserve existing data-race safety guarantees in shading and binning paths.

#### Scenario: Tile-exclusive writes
- **WHEN** multiple OpenMP threads execute tile raster in parallel
- **THEN** each tile's color/depth write region SHALL be exclusively owned by one thread at a time
- **AND** no pixel-level mutex SHALL be required

#### Scenario: Global counters consistency
- **WHEN** per-thread statistics are merged into frame-level counters
- **THEN** the merge operation SHALL use thread-safe mechanisms (atomic or equivalent reduction)
- **AND** the final counters SHALL be deterministic for identical inputs and thread count

#### Scenario: Per-thread timing array safety
- **WHEN** per-thread 计时数组在并行区内被写入
- **THEN** 每个线程 SHALL 仅写入自身 tid 对应的数组元素
- **AND** 不同线程的写入 SHALL 不存在数据竞争

## ADDED Requirements

### Requirement: 所有硬编码 static 调度 SHALL 替换为可配置调度
管线中所有 `#pragma omp parallel for schedule(static)` 的硬编码用法 SHALL 替换为通过 `OpenMPTuningOptions` 配置的调度策略，或使用 `schedule(runtime)` 配合 `omp_set_schedule()`。

#### Scenario: Rasterizer clip 阶段调度替换
- **WHEN** Rasterizer::RasterizeTriangles 执行 clip 阶段的并行循环
- **THEN** 该循环 SHALL 使用 `OpenMPTuningOptions::clipSchedule` 和 `clipChunk` 指定的调度策略
- **AND** 不再硬编码 `schedule(static)`

#### Scenario: Framebuffer clear 调度替换
- **WHEN** Framebuffer::Clear 或 Framebuffer::ClearLinear 执行并行清除
- **THEN** 该循环 SHALL 使用 `OpenMPTuningOptions::clearSchedule` 和 `clearChunk` 指定的调度策略

#### Scenario: DepthBuffer clear 调度替换
- **WHEN** DepthBuffer::Clear 执行并行清除
- **THEN** 该循环 SHALL 使用配置的 clear 调度策略

#### Scenario: FXAA 和 ToneMap 调度替换
- **WHEN** Framebuffer::ApplyFXAA 或 Framebuffer::ResolveToSRGB 执行后处理
- **THEN** 该循环 SHALL 使用 `OpenMPTuningOptions::postProcessSchedule` 和 `postProcessChunk` 指定的调度策略
