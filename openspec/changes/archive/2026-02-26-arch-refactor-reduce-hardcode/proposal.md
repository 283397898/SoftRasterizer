## Why

项目当前存在大量硬编码（glTF 魔法数字、重复的工具函数、臃肿的结构体字段）和架构层面的抽象泄漏（Pass 系统空壳、Renderer 双路径重复、ResourcePool LRU O(n)）。这些问题不影响当前运行效率，但严重阻碍后续扩展——每次添加新纹理类型需改动 6+ 处字段、每次添加新 Pass 需绕过空壳直接写 RenderPipeline、每次新增材质属性需复制粘贴访问器。本次重构以"面向扩展"为核心目标，在不影响并行优化（OpenMP / SIMD）的前提下消除硬编码、减少重复、让架构真正支撑后续功能迭代。

## What Changes

- 提取跨模块重复代码为共享工具模块（Zlib 解压、纹理采样、Lerp 插值、PBR 数学工具）
- 引入 `TextureBinding` 结构体替代 DrawItem / GPUSceneDrawItem 中 24 个散落的纹理索引字段
- 将 glTF 魔法数字（WrapMode 33071/33648/10497、AlphaMode 0/1/2 等）替换为强类型枚举
- 让 Pass 系统的 `Execute()` 承载真实渲染逻辑，消除 RenderPipeline 中的 pass-through 绕行
- 统一 `Renderer::Render()` 两条路径的公共逻辑，消除帧清除/PassContext 构建/统计输出的重复
- 修复 ResourcePool LRU 为 O(1) 实现，补全代际句柄验证
- 模板化 MaterialTable 访问器，消除 20+ 个结构相同的 getter

## Capabilities

### New Capabilities
- `shared-utils`: 提取共享工具模块——Compression（Zlib/Huffman）、TextureSampler（采样/WrapCoord）、MathUtils（Lerp/Clamp）、PBRUtils（GeometrySmith/FresnelSchlick）
- `texture-binding`: 引入 TextureBinding 结构体与 TextureSlot 枚举，统一纹理绑定描述，瘦身 DrawItem 系列结构体
- `gltf-enums`: 将 glTF 规范中的魔法数字替换为强类型枚举（WrapMode、FilterMode、AlphaMode、ComponentType 等）

### Modified Capabilities
- `pass-pipeline`: Pass 的 Execute() 承载真实渲染逻辑，RenderPipeline 退化为 Pass 调度器；统一 Renderer 双路径
- `resource-pool`: LRU 升级为 O(1) 实现（list + unordered_map）；Handle 改为 {index, generation} 复合类型，IsValid() 验证 generation

## Impact

- **Pipeline 模块**：Rasterizer.cpp、FragmentShader.cpp、OpaquePass.cpp 逻辑重组，头文件依赖减少
- **Runtime 模块**：ResourcePool.h 接口变更（Handle 类型从 uint32_t 改为复合结构体），TexturePool/MeshPool/MaterialPool 需适配
- **Scene 模块**：RenderQueue.h 的 DrawItem 结构体字段变化，GPUScene.h 的 GPUSceneDrawItem 同步变化
- **Asset 模块**：ImageDecoder.cpp、EXRDecoder.cpp 的 Zlib 代码迁移到共享模块，GLTFTypes.h 新增枚举定义
- **Render 模块**：RenderPipeline.cpp 大幅简化，Renderer.cpp 去重
- **MFCDemo**：RenderView.cpp 适配新接口，无功能变化
- **编译**：头文件依赖链缩短，增量编译加速；无新外部依赖
- **运行时**：无性能退化——LRU O(1) 化反而提升性能，Pass 调度路径不变，SIMD/OpenMP 并行策略完全保留
