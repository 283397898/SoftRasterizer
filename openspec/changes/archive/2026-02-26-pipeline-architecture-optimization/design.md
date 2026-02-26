## Context

SoftRasterizer 是一个纯 CPU 软光栅器，使用 C++20 实现，模拟现代 GPU 管线。当前架构中：

- **Triangle 结构体** 包含完整的顶点数据 + 30+ 个材质索引字段，每个三角形复制一次
- **RenderPipeline** 硬编码了 Prepare → Draw(opaque) → Skybox → Draw(transparent) → PostProcess 的流程
- **Fragment Shader** 在主线程单线程执行，是性能瓶颈
- **资源管理** 分散在 Scene、GPUScene、TextureManager 等多个组件中

约束条件：
- 必须保持 C++20 兼容
- 保持 OpenMP 作为并行后端
- 不引入外部依赖
- 保持现有的 PBR 渲染效果

## Goals / Non-Goals

**Goals:**
- 减少 Triangle 结构体内存占用 40-60%
- 实现片元着色并行化，提升 2-4x 帧率
- 支持可配置的渲染管线，允许自定义 Pass
- 统一资源管理，提供生命周期控制

**Non-Goals:**
- 不实现 Shader 虚函数抽象（保持硬编码 PBR）
- 不引入 GPU 加速或 GPGPU
- 不改变现有的数学精度（double）
- 不修改 MFCDemo 的 D3D12 显示层

## Decisions

### D1: MaterialTable SOA 布局

**决定**: 使用 Structure of Arrays 布局存储材质数据，Triangle 仅保留 32-bit 材质句柄。

```
Before (AOS):
struct Triangle {
    Vertex v0, v1, v2;
    int32_t baseColorTextureIndex;
    int32_t metallicRoughnessTextureIndex;
    int32_t normalTextureIndex;
    int32_t occlusionTextureIndex;
    int32_t emissiveTextureIndex;
    // ... 25+ more fields
};

After (SOA):
struct Triangle {
    Vertex v0, v1, v2;
    uint32_t materialId;  // 单一句柄
};

struct MaterialTable {
    std::vector<int32_t> baseColorTextureIndex;
    std::vector<int32_t> metallicRoughnessTextureIndex;
    // ... 紧凑存储
};
```

**理由**:
- 减少每个三角形内存占用约 120 bytes → 4 bytes
- 提升 cache locality，Triangle 数组连续存储
- 材质数据共享，减少冗余

**替代方案**:
- 间接指针：增加解引用开销
- 完全分离：需要更多代码重构

### D2: Pass 抽象设计

**决定**: 使用策略模式 + 构建器模式实现可配置 Pass 系统。

```cpp
class RenderPass {
public:
    virtual ~RenderPass() = default;
    virtual void Execute(RenderContext& ctx) = 0;
    virtual bool ShouldExecute(const RenderContext& ctx) const { return true; }
};

class PassBuilder {
public:
    PassBuilder& AddPass(std::unique_ptr<RenderPass> pass);
    PassBuilder& AddDependency(std::string_view from, std::string_view to);
    PassBuilder& SetCondition(std::string_view pass, PassCondition condition);
    std::unique_ptr<RenderPipeline> Build();
};
```

**理由**:
- 策略模式允许运行时配置 Pass
- 构建器模式提供流畅的 API
- 条件执行支持可选功能（如 SSAO）

**替代方案**:
- 模板元编程：编译期确定，缺乏灵活性
- 完全数据驱动：过于复杂

### D3: Tile 并行着色策略

**决定**: 在 Rasterizer 中实现 Tile 级并行，使用 OpenMP `#pragma omp parallel for schedule(dynamic)`。

```cpp
void Rasterizer::ShadeTiles(std::vector<TileTask>& tiles) {
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < tiles.size(); ++i) {
        ShadeTile(tiles[i]);
    }
}
```

**理由**:
- dynamic schedule 适应复杂度不均匀的 Tile
- OpenMP 已有基础设施
- Tile 级粒度平衡并行开销和负载均衡

**替代方案**:
- 像素级并行：粒度太细，开销大
- DrawCall 级并行：已在 Geometry 阶段实现

### D4: ResourcePool 模板设计

**决定**: 使用 CRTP 模式实现类型安全的资源池。

```cpp
template<typename T>
class ResourcePool {
public:
    using Handle = uint32_t;
    static constexpr Handle InvalidHandle = UINT32_MAX;

    Handle Allocate(Args&&... args);
    void Release(Handle h);
    T* Get(Handle h);
    const T* Get(Handle h) const;

    void SetMemoryBudget(size_t bytes);
    void EvictLRU();

private:
    std::vector<std::unique_ptr<T>> m_resources;
    std::deque<Handle> m_freeList;
    std::list<Handle> m_lruList;
    size_t m_memoryUsage = 0;
};
```

**理由**:
- Handle 机制避免原始指针
- LRU 支持内存压力管理
- 类型安全，编译期检查

**替代方案**:
- shared_ptr：引用计数开销
- 纯原始指针：内存不安全

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| SOA 增加间接访问开销 | MaterialTable 缓存友好，实际访问更快 |
| Pass 抽象引入虚函数调用 | 内联关键路径，非热点可接受 |
| OpenMP false sharing | Tile 边界对齐，使用 local buffer |
| ResourcePool 句柄失效 | 使用代际句柄 (generational handle) |
| 重构影响现有功能 | 增量迁移，保持兼容层 |

## Migration Plan

### Phase 1: MaterialTable (低风险)
1. 创建 `MaterialTable` 类
2. 修改 `Triangle` 添加 `materialId`
3. 迁移现有材质索引到 `MaterialTable`
4. 验证渲染结果一致

### Phase 2: ResourcePool (低风险)
1. 实现 `ResourcePool<T>` 模板
2. 特化 `TexturePool`, `MeshPool`
3. 迁移 GPUScene 使用资源池
4. 添加 LRU 驱逐测试

### Phase 3: Tile 并行 (中风险)
1. 重构 Rasterizer 分离 Tile 生成和着色
2. 添加 OpenMP 并行
3. 性能测试验证加速比
4. 处理边界条件

### Phase 4: Pass 系统 (高风险)
1. 定义 `RenderPass` 基类
2. 实现现有流程为独立 Pass
3. 实现 `PassBuilder`
4. 迁移 MFCDemo 使用新 API

### Rollback
- 每个阶段独立提交
- 保留旧 API 作为兼容层
- 使用 feature flag 控制新功能

## Open Questions

1. **MaterialTable 线程安全**: 是否需要 atomic 访问？
   - 当前假设只读访问，不需要
2. **Pass 执行顺序**: 是否需要拓扑排序？
   - 当前使用显式依赖声明
3. **Tile 大小调优**: 32x32 是否最优？
   - 需要性能测试验证
