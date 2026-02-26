## Context

SoftRasterizer 是一个纯 C++20 CPU 软件光栅化器，采用 Tile-Based 并行光栅化 + OpenMP + AVX2 SIMD 的性能策略。当前代码在功能上已基本完备（PBR、IBL、FXAA、透明排序），但架构层面存在以下问题阻碍后续扩展：

1. **硬编码泛滥**：glTF 魔法数字（33071/33648/10497 等）散布在 Rasterizer.cpp、FragmentShader.cpp 中，新增采样模式需逐处查找替换
2. **重复代码**：Zlib 解压在 ImageDecoder 和 EXRDecoder 各实现一次；纹理采样、Lerp、WrapCoord、GeometrySmith 在多个文件重复
3. **抽象泄漏**：Pass 系统声明了 OpaquePass/SkyboxPass 但 Execute() 为空壳，真实逻辑在 RenderPipeline 中硬编码
4. **结构体臃肿**：DrawItem/GPUSceneDrawItem 各含 24 个纹理索引字段（6 种纹理 × 4 字段），新增纹理类型需改 6+ 处
5. **ResourcePool 实现缺陷**：LRU Touch() 为 O(n)，代际句柄声明了但未验证

约束条件：
- 不引入任何外部依赖
- 不改变 OpenMP/SIMD 并行策略
- 不改变 NDC 约定（DirectX 风格 Z ∈ [0,1]）
- 保持 double 精度
- 保持 DLL 导出兼容性（SR_API 宏）

## Goals / Non-Goals

**Goals:**
- 消除所有 glTF 魔法数字，用强类型枚举替代，使编译器能在类型层面捕获错误
- 提取重复代码为可复用的共享工具模块，每个工具函数只存在一处定义
- 让 Pass 系统真正工作——每个 Pass 的 Execute() 包含完整渲染逻辑，RenderPipeline 退化为调度器
- 用 TextureBinding 结构体 + 枚举索引替代散落的纹理字段，使新增纹理类型只需改一处
- 修复 ResourcePool 的 LRU 和代际句柄，使其成为可靠的资源管理基础设施
- 统一 Renderer 的两条 Render() 路径，消除帧级重复代码

**Non-Goals:**
- 不重写 Rasterizer 的 SIMD 光栅化核心（仅提取其中的工具函数）
- 不改变 MaterialTable 的 SOA 存储布局（仅模板化访问器）
- 不重构 Math 库（Vec3/Vec4/Mat4 保持不变）
- 不修改 MFCDemo 的 D3D12 呈现逻辑
- 不引入 ECS 或其他重量级架构模式
- 不做性能优化（本次重构目标是可扩展性，性能至少不退化）

## Decisions

### Decision 1: 共享工具模块的组织方式

**选择**：在 `SoftRenderer/include/Utils/` 和 `SoftRenderer/src/Utils/` 下创建独立工具文件

- `Utils/Compression.h/cpp` — Zlib/Huffman/Deflate 解压（从 ImageDecoder 和 EXRDecoder 提取）
- `Utils/TextureSampler.h` — 内联的 WrapCoord、SampleNearest、SampleBilinear（从 Rasterizer 和 FragmentShader 提取）
- `Utils/MathUtils.h` — 内联的 Lerp（Vec2/Vec3/Vec4）、Clamp、Saturate（从 Clipper 和 FragmentShader 提取）
- `Utils/PBRUtils.h` — 内联的 GeometrySmith、FresnelSchlick、DistributionGGX（从 FragmentShader 和 EnvironmentMap 提取）

**替代方案**：将工具函数放入现有 Math/ 模块。**否决原因**：Math/ 仅包含向量/矩阵基础类型，纹理采样和 PBR 函数与数学基础设施职责不同。

**替代方案**：创建单一 `Utils.h` 万能头文件。**否决原因**：违反单一职责，修改 PBR 工具会导致纹理采样相关文件重编译。

### Decision 2: TextureBinding 结构体设计

**选择**：引入带枚举索引的固定大小数组

```cpp
enum class TextureSlot : uint8_t {
    BaseColor = 0,
    MetallicRoughness,
    Normal,
    Occlusion,
    Emissive,
    Transmission,
    Count
};

struct TextureBinding {
    int textureIndex = -1;
    int imageIndex = -1;
    int samplerIndex = -1;
    int texCoordSet = 0;
};

// DrawItem 中：
TextureBinding textures[static_cast<size_t>(TextureSlot::Count)];
```

**替代方案**：使用 `std::unordered_map<TextureSlot, TextureBinding>`。**否决原因**：DrawItem 在热路径中被频繁拷贝，哈希表开销不可接受。

**替代方案**：保持现有字段，用宏生成。**否决原因**：宏不提供类型安全，调试困难。

**扩展性**：新增纹理类型只需在 `TextureSlot` 枚举中添加一项，数组自动扩容，无需修改 DrawItem 结构体。

### Decision 3: glTF 枚举放置位置

**选择**：在现有 `Asset/GLTFTypes.h` 中新增强类型枚举

```cpp
enum class GLTFWrapMode : int {
    ClampToEdge = 33071,
    MirroredRepeat = 33648,
    Repeat = 10497
};

enum class GLTFAlphaMode : int {
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

enum class GLTFFilterMode : int {
    Nearest = 9728,
    Linear = 9729,
    // mipmap variants...
};
```

**理由**：GLTFTypes.h 已存在且专门用于 glTF 数据类型定义，枚举与其职责一致。Pipeline 模块通过 `#include "Asset/GLTFTypes.h"` 引用，不增加新的依赖路径。

**替代方案**：在 Pipeline/ 中定义渲染器自有的枚举，与 glTF 常量映射。**否决原因**：增加一层转换，目前渲染器就是 glTF 渲染器，额外抽象层无实际收益。

### Decision 4: Pass 系统实体化策略

**选择**：渐进式迁移——先将 RenderPipeline::Render() 中的逻辑分块移入各 Pass 的 Execute()，RenderPipeline::Render() 变为调用 ExecutePasses() 的薄包装。

迁移步骤：
1. `OpaquePass::Execute()` 接管几何构建 + 不透明光栅化（当前在 DrawWithMaterialTable 中）
2. `SkyboxPass::Execute()` 接管天空盒渲染（当前在 RenderSkybox 中）
3. `TransparentPass::Execute()` 接管透明光栅化（已部分实现）
4. `PostProcessPass::Execute()` 保持不变（已实现）
5. `RenderPipeline::Render()` 简化为：清缓冲 → ExecutePasses() → 返回统计

**关键约束**：OpaquePass 产生的延迟透明三角形需要通过 `RenderContext` 传递给 TransparentPass，不再使用 `SetDeferredTriangles()` 的紧耦合方式。

**替代方案**：CRTP 消除虚函数调度。**否决原因**：Pass 数量 ≤ 5，每帧调用 ≤ 5 次虚函数，开销可忽略；CRTP 会丧失运行时动态组合 Pass 的能力。

### Decision 5: Renderer 双路径统一

**选择**：提取三个私有方法

```cpp
void Renderer::ClearBuffers();
PassContext Renderer::BuildPassContext(const FrameContext& frame) const;
void Renderer::LogFrameStats(const RenderStats& stats, double clearMs, double setupMs, double totalMs) const;
```

两个 `Render()` 重载各自只负责 FrameContext 的差异化构建，然后调用统一的渲染流程。

### Decision 6: ResourcePool Handle 复合化

**选择**：Handle 改为 `{ uint16_t index; uint16_t generation }` 打包到 uint32_t

```cpp
struct Handle {
    uint16_t index;
    uint16_t generation;
    // pack/unpack 到 uint32_t 以保持 ABI 大小不变
};
```

`IsValid()` 同时验证 index 范围、alive 状态和 generation 匹配。

LRU 改为 `std::list<uint16_t>` + `std::unordered_map<uint16_t, std::list<uint16_t>::iterator>`，Touch() 为 O(1) splice。

**替代方案**：Handle 使用 uint64_t（32-bit index + 32-bit generation）。**否决原因**：当前资源数远不到 65535，uint16_t 足够且保持 Handle 为 uint32_t 大小，不影响现有接口。

### Decision 7: MaterialTable 访问器模板化

**选择**：引入私有模板辅助方法

```cpp
template<typename T>
T GetProperty(MaterialHandle h, const std::vector<T>& storage, T defaultVal) const {
    return IsValid(h) ? storage[h] : defaultVal;
}
```

所有 `GetMetallic()`、`GetRoughness()` 等公开方法内部委托给 `GetProperty()`。公开 API 保持不变，仅减少内部重复。

## Risks / Trade-offs

- **[风险] Pass 迁移过程中功能回退** → 每迁移一个 Pass 后运行 MFCDemo 验证渲染结果，保持渐进式迁移
- **[风险] ResourcePool Handle ABI 变更** → Handle 仍为 uint32_t 大小，二进制兼容；但使用方需从 `uint32_t` 改为 `ResourcePool::Handle` 类型
- **[风险] TextureBinding 数组大小硬编码** → 使用 `TextureSlot::Count` 编译期常量，新增 slot 自动扩容
- **[权衡] 共享工具模块增加头文件** → 增加 4 个头文件，但消除 ~10 处代码重复，净收益明显
- **[权衡] glTF 枚举与 Pipeline 的跨模块依赖** → Pipeline 需 include Asset/GLTFTypes.h；可接受，因为渲染器本身就是 glTF 渲染器
- **[权衡] MaterialTable 模板辅助方法对编译器内联的依赖** → 方法定义在头文件中，编译器必然内联，无运行时开销
