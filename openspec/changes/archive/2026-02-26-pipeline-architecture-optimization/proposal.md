## Why

当前软光栅器架构存在四个关键瓶颈：
1. **内存膨胀** - `Triangle` 结构体为每个三角形复制 30+ 个材质索引字段，导致内存占用过高且 cache locality 差
2. **管线硬编码** - `RenderPipeline::Render()` 固定了渲染阶段顺序，无法支持 multi-pass 或自定义渲染流程
3. **性能瓶颈** - Fragment Shader 在主线程单线程执行，无法利用多核 CPU
4. **资源分散** - 纹理、网格等资源管理分散在多个组件中，缺乏统一的生命周期管理

这些问题限制了渲染器的扩展性和性能，需要系统性重构来解决。

## What Changes

### 数据布局优化
- **BREAKING** 重构 `Triangle` 结构体，移除冗余材质索引字段
- 引入 SOA (Structure of Arrays) 数据布局，将材质索引提取到独立的 `MaterialTable`
- 新增 `TriangleSOA` 结构体，仅保留几何数据 + 材质索引句柄

### 管线阶段可配置
- **BREAKING** 重构 `RenderPipeline`，将硬编码的渲染流程拆分为独立的 `RenderPass`
- 新增 `Pass` 抽象基类，支持自定义渲染阶段
- 新增 `PassBuilder` 用于配置和组装渲染管线
- 支持条件性 Pass 执行和 Pass 间依赖

### Fragment 并行化
- 重构 Rasterizer 的片元着色流程，支持 Tile 级并行
- 新增 `TileShadingTask` 结构，封装 Tile 着色任务
- 使用 OpenMP 并行化片元着色循环
- 优化 Framebuffer 写入，避免 false sharing

### 资源管理抽象
- 新增 `ResourcePool<T>` 模板类，提供统一的资源生命周期管理
- 新增 `TexturePool`, `MeshPool`, `MaterialPool` 特化
- 支持 LRU 驱逐策略和内存预算控制
- 提供资源句柄机制，避免原始指针

## Capabilities

### New Capabilities
- `material-table`: 材质索引表系统，提供 SOA 布局的材质数据访问，减少 Triangle 结构体冗余
- `pass-pipeline`: 可配置的渲染管线系统，支持 Pass 组装、依赖管理和条件执行
- `tile-shading`: Tile 级并行着色系统，利用 OpenMP 并行化片元着色
- `resource-pool`: 统一资源池管理系统，提供资源句柄、LRU 驱逐和内存预算控制

### Modified Capabilities
- 无现有 specs，无需修改

## Impact

### 受影响的文件
- `SoftRenderer/include/Pipeline/Triangle.h` - 重构为 SOA
- `SoftRenderer/src/Pipeline/Triangle.cpp` - 适配新结构
- `SoftRenderer/include/Pipeline/RenderPipeline.h` - Pass 抽象重构
- `SoftRenderer/src/Pipeline/RenderPipeline.cpp` - Pass 流程重写
- `SoftRenderer/include/Pipeline/Rasterizer.h` - Tile 并行接口
- `SoftRenderer/src/Pipeline/Rasterizer.cpp` - 并行着色实现
- `SoftRenderer/include/Scene/GPUScene.h` - 资源池集成
- `SoftRenderer/src/Scene/GPUScene.cpp` - 资源管理适配

### API 变更
- `Triangle` 结构体字段变更（移除直接材质索引）
- `RenderPipeline` 接口变更（新增 Pass 配置 API）
- 新增 `ResourcePool<T>` 模板类

### 性能预期
- 内存占用：预计减少 40-60%（取决于场景复杂度）
- 渲染帧率：预计提升 2-4x（多核 CPU）
- 扩展性：支持 multi-pass、shadow mapping、deferred rendering 等
