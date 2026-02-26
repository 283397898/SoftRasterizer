## Why

`GeometryProcessor.cpp` 中使用了 `std::mutex g_materialTableMutex` 来保护 `MaterialTable::AddMaterial()` 调用。该锁在 `OpaquePass::Execute()` 的 OpenMP 并行几何处理阶段被所有线程争抢，形成了序列化瓶颈，违反了项目的无锁设计原则。光栅化器的 Bin 填充阶段已正确使用 `_InterlockedExchangeAdd64` 实现无锁并行，几何处理阶段应保持一致。

## What Changes

- 将材质注册从并行的 `BuildTriangles()` 中提取到 OpaquePass 的单线程预处理阶段
- 修改 `GeometryProcessor::BuildTriangles()` 签名，接受预计算的 `MaterialHandle` 而非在内部注册材质
- 从 `GeometryProcessor.cpp` 中移除 `#include <mutex>`、`static std::mutex` 和 `std::lock_guard`
- 确保整个渲染管线不再包含任何 `std::mutex` 或阻塞锁

## Capabilities

### New Capabilities
- `lock-free-geometry`: 几何处理阶段的无锁并行设计规约——材质注册前置到单线程阶段，消除 OpenMP 并行区域内的所有互斥锁

### Modified Capabilities
- `pass-pipeline`: OpaquePass 需在并行循环前增加单线程材质预注册步骤，将预计算的 MaterialHandle 传递给 BuildTriangles

## Impact

- **代码文件**：`GeometryProcessor.h`、`GeometryProcessor.cpp`、`OpaquePass.cpp`
- **接口变更**：`BuildTriangles()` 签名变化（新增 `MaterialHandle` 参数，移除内部材质注册逻辑）——此变更仅影响内部 API，不影响 `SR_API` 导出接口
- **性能**：消除并行几何处理阶段的锁竞争，在多核系统上应有可测量的吞吐量提升
- **风险**：低——材质注册逻辑仅从并行循环内移到循环前，语义完全等价
