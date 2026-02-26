## 1. 提取材质参数构建辅助函数

- [x] 1.1 在 `GeometryProcessor.h` 中声明静态辅助函数 `BuildMaterialParams(const PBRMaterial&, const DrawItem&) -> MaterialParams`，将 `BuildTriangles()` 中 ~50 行的 MaterialParams 赋值逻辑提取为独立函数
- [x] 1.2 在 `GeometryProcessor.cpp` 中实现 `BuildMaterialParams()`，从原 `BuildTriangles()` 中剪切 MaterialParams 构建代码

## 2. 修改 BuildTriangles 签名

- [x] 2.1 修改 `GeometryProcessor::BuildTriangles()` 签名：移除 `const PBRMaterial& material` 和 `MaterialTable& materialTable` 参数，新增 `MaterialHandle materialHandle` 参数
- [x] 2.2 更新 `BuildTriangles()` 实现：移除内部的 MaterialParams 构建和 `materialTable.AddMaterial()` 调用，改为直接使用传入的 `materialHandle` 赋值给 `tri.materialId`

## 3. 移除 mutex

- [x] 3.1 从 `GeometryProcessor.cpp` 中移除 `#include <mutex>`、`static std::mutex g_materialTableMutex` 声明和 `std::lock_guard<std::mutex> lock(...)` 使用

## 4. 修改 OpaquePass 调用逻辑

- [x] 4.1 在 `OpaquePass::Execute()` 的 `#pragma omp parallel` 之前，添加单线程材质预注册循环：遍历 `sortedItems`，为每个有效 DrawItem 调用 `BuildMaterialParams()` + `materialTable->AddMaterial()`，将结果存入 `std::vector<MaterialHandle>`
- [x] 4.2 修改 `OpaquePass::Execute()` 并行循环内的 `BuildTriangles()` 调用，传入预计算的 `MaterialHandle` 而非 `material` 和 `materialTable`

## 5. 验证

- [x] 5.1 全量搜索确认项目中不再包含 `std::mutex`、`std::lock_guard`、`std::unique_lock` 等阻塞锁原语
- [x] 5.2 编译通过，确保无类型错误或链接问题
