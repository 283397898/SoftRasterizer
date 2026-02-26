## Why

当前 SoftRasterizer 存在两个核心问题：

1. **性能瓶颈**：`Triangle` 和 `DrawItem` 结构存在大量冗余索引字段（30+ 个 int），每个三角形复制一次导致内存膨胀和 cache miss；Fragment 着色阶段为单线程执行，是当前最大热点。

2. **扩展性受限**：`RenderPipeline::Render()` 硬编码了固定的渲染流程（Prepare → Draw → Skybox → PostProcess），无法插入自定义 Pass、无法支持 Multi-pass 渲染、无法配置后处理链；资源管理分散，缺乏统一的 Texture/Mesh 池化机制。

现在优化可以在不破坏现有功能的前提下，显著提升性能并为未来扩展奠定基础。

## What Changes

### 数据布局重构
- **BREAKING**: 重构 `Triangle` 结构，采用 SOA (Structure of Arrays) 布局
- 将 30+ 个纹理索引字段压缩为固定数组 `textureIndices[6]`，配合 `TextureBinding` 枚举访问
- 引入 `TriangleBatch` 容器，将三角形数据按类型分离存储（顶点、属性、材质索引）
- 减少 `Triangle` 结构体大小约 60%，提升 cache locality

### Fragment 并行化
- 在 `Rasterizer` 中实现 Tile 级并行着色
- 每个 Tile (32x32) 独立处理，使用 OpenMP 并行
- 引入 `TileContext` 封装每个 Tile 的局部状态

### Pass 抽象系统
- 引入 `RenderPass` 抽象基类，定义 `Execute(PassContext&)` 接口
- 实现具体 Pass：`OpaquePass`、`TransparentPass`、`SkyboxPass`、`PostProcessPass`
- `RenderPipeline` 改为持有 `std::vector<std::unique_ptr<RenderPass>>`，支持动态配置执行顺序
- 引入 `PassBuilder` 用于配置和组装管线

### 资源管理池化
- 引入 `ResourcePool<T>` 模板类，统一管理 Texture、Mesh、Material 等资源
- 支持资源的引用计数和自动释放
- 实现 `TexturePool`、`MeshPool`、`MaterialPool` 特化

## Capabilities

### New Capabilities

- `soa-data-layout`: SOA 数据布局系统，优化 Triangle/DrawItem 内存结构，减少冗余字段，提升 cache 性能
- `tile-parallel-shading`: Tile 级并行着色系统，将 Fragment 着色并行化以提升性能
- `pass-abstraction`: 可配置渲染 Pass 系统，支持自定义 Pass 插入、顺序调整和 Multi-pass 渲染
- `resource-pool`: 统一资源池管理系统，支持引用计数和自动生命周期管理

### Modified Capabilities

- `render-pipeline`: 重构 `RenderPipeline` 从硬编码流程改为 Pass 组合模式，保持对外接口兼容

## Impact

### 核心模块变更
- `Pipeline/Rasterizer.h/cpp` - 添加 Tile 并行着色逻辑
- `Pipeline/FragmentShader.h/cpp` - 适配 SOA 数据输入
- `Scene/RenderQueue.h` - 重构 `DrawItem` 结构
- `Pipeline/Rasterizer.h` - 重构 `Triangle` 结构，新增 `TriangleBatch`
- `Render/RenderPipeline.h/cpp` - 改为 Pass 组合模式

### 新增模块
- `Pipeline/RenderPass.h` - Pass 抽象基类
- `Pipeline/OpaquePass.h/cpp` - 不透明几何渲染 Pass
- `Pipeline/TransparentPass.h/cpp` - 透明几何渲染 Pass
- `Pipeline/SkyboxPass.h/cpp` - 天空盒渲染 Pass
- `Pipeline/PostProcessPass.h/cpp` - 后处理 Pass
- `Pipeline/PassBuilder.h/cpp` - Pass 配置构建器
- `Core/ResourcePool.h` - 资源池模板
- `Core/TexturePool.h/cpp` - 纹理池
- `Core/MeshPool.h/cpp` - 网格池
- `Core/MaterialPool.h/cpp` - 材质池

### 兼容性
- `Renderer` 公共 API 保持不变
- 内部 `Triangle` 结构变更需要更新 `GeometryProcessor` 和 `Clipper`
- 现有场景数据流（Scene → RenderQueue）保持兼容
