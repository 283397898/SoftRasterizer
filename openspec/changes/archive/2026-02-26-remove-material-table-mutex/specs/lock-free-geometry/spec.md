## ADDED Requirements

### Requirement: Geometry processing stage is lock-free
几何处理阶段（GeometryProcessor）的并行执行 SHALL NOT 包含任何 `std::mutex`、`std::lock_guard`、`std::unique_lock` 或其他阻塞式互斥原语。

#### Scenario: No mutex in GeometryProcessor source
- **WHEN** `GeometryProcessor.cpp` 的源代码被检查
- **THEN** 文件中 SHALL NOT 包含 `#include <mutex>`
- **AND** SHALL NOT 包含任何 `std::mutex` 声明或 `std::lock_guard` 使用

#### Scenario: BuildTriangles is safe for concurrent invocation without external synchronization
- **WHEN** 多个线程各自使用独立的输出缓冲（`outTriangles`）并行调用 `BuildTriangles()`
- **THEN** 所有调用 SHALL 正确完成而无需调用者提供任何互斥保护
- **AND** 每个线程的输出 SHALL 仅依赖其输入参数和只读共享状态

### Requirement: BuildTriangles accepts pre-computed MaterialHandle
`GeometryProcessor::BuildTriangles()` SHALL 接受一个预计算的 `MaterialHandle` 参数，而非在内部执行材质注册。

#### Scenario: BuildTriangles signature includes MaterialHandle
- **WHEN** `BuildTriangles()` 被调用
- **THEN** 调用者 SHALL 提供一个 `MaterialHandle` 参数
- **AND** 函数 SHALL 将该 handle 直接赋值给每个输出三角形的 `materialId` 字段
- **AND** 函数 SHALL NOT 调用 `MaterialTable::AddMaterial()`

#### Scenario: BuildTriangles does not depend on MaterialTable reference
- **WHEN** `BuildTriangles()` 的参数列表被检查
- **THEN** 参数列表中 SHALL NOT 包含 `MaterialTable&` 参数
- **AND** 函数 SHALL NOT 需要任何可写的共享状态

### Requirement: Material registration is performed in single-threaded pre-pass
所有材质的注册（`MaterialTable::AddMaterial()`）SHALL 在 OpenMP 并行区域之前的单线程阶段完成。

#### Scenario: Materials pre-registered before parallel geometry processing
- **WHEN** OpaquePass 处理 DrawItem 列表
- **THEN** OpaquePass SHALL 先在单线程循环中为每个 DrawItem 注册材质到 MaterialTable
- **AND** SHALL 将返回的 MaterialHandle 存储在索引映射中
- **AND** 之后的并行几何处理循环 SHALL 仅使用预计算的 handle，不再写入 MaterialTable
