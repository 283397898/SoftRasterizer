## Context

当前 `OpaquePass::Execute()` 的几何处理阶段使用 OpenMP 并行处理 `DrawItem` 列表。每个线程调用 `GeometryProcessor::BuildTriangles()`，该函数内部需要将材质参数注册到共享的 `MaterialTable` 中以获取 `MaterialHandle`。由于 `MaterialTable::AddMaterial()` 会修改内部 SOA 数组（`push_back` / 空闲槽位复用），不是线程安全的，因此使用了 `static std::mutex g_materialTableMutex` 进行互斥保护。

关键观察：**每个 DrawItem 的材质参数在进入并行循环前就已完全确定**（来自 `item.material` 和 `item.textures`），材质注册完全可以前移到单线程阶段。

## Goals / Non-Goals

**Goals:**
- 消除 `GeometryProcessor.cpp` 中的 `std::mutex`，使几何处理阶段完全无锁
- 将材质注册前置到 OpaquePass 的单线程预处理阶段
- 修改 `BuildTriangles()` 接口，接受已就绪的 `MaterialHandle`
- 保持功能完全等价：渲染结果不变

**Non-Goals:**
- 不修改 `MaterialTable` 本身的 API（`AddMaterial` / `RemoveMaterial` 等）
- 不引入无锁数据结构（如 lock-free queue），因为前置注册方案更简洁
- 不修改光栅化阶段或片段着色阶段的并行策略

## Decisions

### 决策 1：材质注册前置到单线程阶段（选定方案）

在 `OpaquePass::Execute()` 的 `#pragma omp parallel` 之前，新增一个单线程循环：
1. 遍历所有 `sortedItems`，为每个 `DrawItem` 调用 `MaterialTable::AddMaterial()`
2. 将返回的 `MaterialHandle` 存入 `std::vector<MaterialHandle>` 映射表
3. 在并行循环内，直接将预计算的 handle 传递给 `BuildTriangles()`

**理由**：材质注册耗时极低（仅向 SOA 数组追加数据），DrawItem 数量通常在百级，单线程遍历的开销可忽略不计。相比之下，消除锁竞争对多核并行的吞吐量提升是显著的。

**替代方案**：
- *A. 使 MaterialTable 线程安全*：给 `AddMaterial()` 加 `std::atomic` 游标 + 预分配。复杂度高，且 SOA 数组的多列同步写入难以无锁化。
- *B. 每线程独立 MaterialTable 再合并*：需要处理 handle 重映射和合并逻辑，增加不必要的复杂度。
- *C. 保持 mutex 但减小临界区*：仍有锁竞争，违反无锁原则。

### 决策 2：修改 BuildTriangles 签名

将 `BuildTriangles()` 的参数从 `(mesh, material, item, model, normal, frame, materialTable, out)` 改为 `(mesh, item, model, normal, frame, materialHandle, out)`：
- 移除 `material` 参数（不再需要在此处构建 MaterialParams）
- 移除 `materialTable` 参数（不再需要在此处注册材质）
- 新增 `materialHandle` 参数（已预计算的句柄）

**理由**：`BuildTriangles()` 的职责简化为纯顶点变换和三角形装配——所有数据写入和状态修改已被移出。

### 决策 3：材质参数构建逻辑移入 OpaquePass

从 `BuildTriangles()` 中提取 MaterialParams 构建逻辑（~50 行参数赋值代码），移到 OpaquePass 的材质预注册循环中。可提取为一个静态辅助函数 `BuildMaterialParams(material, item) -> MaterialParams`，供 OpaquePass 调用。

## Risks / Trade-offs

- **[微小的串行开销]** → 材质预注册循环是单线程的，但 DrawItem 数量通常很少（百级），每次注册仅为 SOA 数组 push_back，开销远小于消除的锁竞争。实测中该阶段不可测量。
- **[接口变更]** → `BuildTriangles()` 签名变化影响 `OpaquePass.cpp`（唯一调用者），修改范围可控。
- **[静态全局 mutex 残留]** → 确保移除后无其他代码间接依赖 `g_materialTableMutex`。搜索确认仅有 `GeometryProcessor.cpp` 一处使用。
