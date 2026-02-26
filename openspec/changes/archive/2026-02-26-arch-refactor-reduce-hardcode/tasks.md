## 1. glTF 强类型枚举（基础设施，无外部依赖）

- [x] 1.1 在 `Asset/GLTFTypes.h` 中定义 `GLTFWrapMode` 枚举（ClampToEdge=33071, MirroredRepeat=33648, Repeat=10497）
- [x] 1.2 在 `Asset/GLTFTypes.h` 中定义 `GLTFAlphaMode` 枚举（Opaque=0, Mask=1, Blend=2）
- [x] 1.3 在 `Asset/GLTFTypes.h` 中定义 `GLTFFilterMode` 枚举（Nearest=9728, Linear=9729）
- [x] 1.4 在 `Asset/GLTFTypes.h` 中定义 `GLTFComponentType` 枚举（Byte=5120 至 Float=5126）
- [x] 1.5 更新 `PBRMaterial.h` 中 `alphaMode` 字段使用 `GLTFAlphaMode`（或添加类型别名和转换）
- [x] 1.6 更新 `GLTFLoader.cpp` 和 `BufferAccessor.h` 中的魔法数字为对应枚举

## 2. TextureBinding 结构体与 TextureSlot 枚举

- [x] 2.1 创建 `TextureSlot` 枚举和 `TextureBinding` 结构体（可放在 `Scene/RenderQueue.h` 或独立头文件 `Pipeline/TextureBinding.h`）
- [x] 2.2 重构 `DrawItem`：移除 24 个散落纹理字段，替换为 `TextureBinding textures[TextureSlot::Count]`
- [x] 2.3 重构 `GPUSceneDrawItem`：同样替换为 TextureBinding 数组
- [x] 2.4 更新 `GPUScene.cpp` 中的 DrawItem 构建逻辑，用循环或辅助函数替代逐字段赋值
- [x] 2.5 更新 `GPUSceneBuilder.cpp` 中的 DrawItem 构建逻辑
- [x] 2.6 更新 `GPUSceneRenderQueueBuilder.cpp`：从 GPUSceneDrawItem 到 DrawItem 的纹理字段拷贝改为数组拷贝
- [x] 2.7 更新 `RenderQueueBuilder.cpp` 中引用纹理字段的代码
- [x] 2.8 更新 `GeometryProcessor.cpp` 中读取 DrawItem 纹理字段的代码
- [x] 2.9 更新 `Rasterizer.cpp` 和 `FragmentShader.cpp` 中读取纹理绑定的代码

## 3. 共享工具模块提取

- [x] 3.1 创建 `Utils/MathUtils.h`：从 Clipper.cpp 和 FragmentShader.cpp 提取 Lerp（Vec2/Vec3/Vec4/double）、Clamp、Saturate 为 inline 函数
- [x] 3.2 更新 Clipper.cpp：删除本地 Lerp 定义，改用 `#include "Utils/MathUtils.h"`
- [x] 3.3 更新 FragmentShader.cpp：删除本地 Lerp 定义，改用 MathUtils
- [x] 3.4 创建 `Utils/PBRUtils.h`：从 FragmentShader.cpp 和 EnvironmentMap.cpp 提取 GeometrySmith、FresnelSchlick、DistributionGGX 为 inline 函数
- [x] 3.5 更新 FragmentShader.cpp：删除本地 PBR 函数定义，改用 PBRUtils
- [x] 3.6 更新 EnvironmentMap.cpp：删除本地 GeometrySmith，改用 PBRUtils
- [x] 3.7 创建 `Utils/TextureSampler.h`：从 Rasterizer.cpp 和 FragmentShader.cpp 提取 WrapCoord（参数改为 GLTFWrapMode）、SampleNearest、SampleBilinear 为 inline 函数
- [x] 3.8 更新 Rasterizer.cpp：删除本地 WrapCoord/SampleBaseColorAlpha，改用 TextureSampler
- [x] 3.9 更新 FragmentShader.cpp：删除本地 WrapCoord/SampleImage*，改用 TextureSampler
- [x] 3.10 创建 `Utils/Compression.h/cpp`：从 ImageDecoder.cpp 提取 BitReader、BuildHuffmanTable、InflateZlib、InflateDeflate
- [x] 3.11 更新 ImageDecoder.cpp：删除本地压缩代码，改用 `#include "Utils/Compression.h"`
- [x] 3.12 更新 EXRDecoder.cpp：删除本地压缩代码，改用 Compression 模块
- [x] 3.13 更新 CMakeLists.txt：将新的 Utils 源文件加入编译

## 4. ResourcePool LRU O(1) 化与代际句柄

- [x] 4.1 在 `ResourcePool.h` 中定义复合 Handle 结构体（uint16_t index + uint16_t generation，打包为 uint32_t）
- [x] 4.2 修改 `IsValid()` 同时验证 index 范围、alive 状态和 generation 匹配
- [x] 4.3 替换 `m_lruList` 为 `std::list<uint16_t>` + `std::unordered_map<uint16_t, std::list<uint16_t>::iterator>` 实现 O(1) Touch/Release
- [x] 4.4 替换 `std::vector<bool> m_alive` 为 `std::vector<uint8_t>`
- [x] 4.5 更新 `Allocate()`/`Release()`/`Get()` 使用新的 Handle 类型和 LRU 结构
- [x] 4.6 更新 `TexturePool.h`、`MeshPool.h`、`MaterialPool.h` 适配新 Handle 类型
- [x] 4.7 更新 `GPUScene.cpp` 等所有使用 ResourcePool::Handle 的调用方

## 5. Pass 系统实体化

- [x] 5.1 扩展 `RenderContext`：添加 `std::vector<Triangle>* deferredBlendTriangles` 和 `MaterialTable* materialTable` 字段，移除 `void* passData`
- [x] 5.2 将 `RenderPipeline::DrawWithMaterialTable()` 中的几何构建+不透明光栅化逻辑移入 `OpaquePass::Execute()`
- [x] 5.3 将 `RenderPipeline::RenderSkybox()` 逻辑移入 `SkyboxPass::Execute()`
- [x] 5.4 更新 `TransparentPass::Execute()`：从 `context.deferredBlendTriangles` 读取数据，移除 `SetDeferredTriangles()` 方法和 `m_deferredTriangles` 成员
- [x] 5.5 简化 `RenderPipeline::Render()`：改为 清缓冲 → 创建 MaterialTable → ExecutePasses() → 返回统计
- [x] 5.6 删除 `RenderPipeline::DrawWithMaterialTable()`、`RenderPipeline::Draw()`、`RenderPipeline::RenderSkybox()`、`RenderPipeline::PostProcess()` 等不再需要的方法
- [x] 5.7 验证 Pass 依赖顺序：Opaque(100) → Skybox(200) → Transparent(300) → PostProcess(400) 的 priority 保持正确

## 6. Renderer 双路径统一

- [x] 6.1 在 `SoftRenderer.h` 中声明三个私有方法：`ClearBuffers()`、`BuildPassContext(const FrameContext&)`、`LogFrameStats()`
- [x] 6.2 在 `Renderer.cpp` 中实现 `ClearBuffers()`：统一帧缓冲和深度缓冲清除逻辑
- [x] 6.3 在 `Renderer.cpp` 中实现 `BuildPassContext()`：统一 PassContext 构建
- [x] 6.4 在 `Renderer.cpp` 中实现 `LogFrameStats()`：统一性能统计输出
- [x] 6.5 简化 `Render(const Scene&)` 和 `Render(const GPUScene&)`：各自只负责 FrameContext 差异化构建，然后调用公共方法

## 7. MaterialTable 访问器模板化

- [x] 7.1 在 `MaterialTable.h` 中添加私有 `GetProperty<T>()` 模板辅助方法
- [x] 7.2 将所有 `GetMetallic()`、`GetRoughness()` 等 20+ 个 getter 内部委托给 `GetProperty()`
- [x] 7.3 验证公开 API 签名不变，调用方无需修改

## 8. 头文件依赖清理与死代码删除

- [x] 8.1 `OpaquePass.h`：将 `#include "Pipeline/Rasterizer.h"` 改为 `Triangle` 前向声明（或引入轻量级 `Pipeline/Triangle.h`）
- [x] 8.2 `GeometryProcessor.h`：将 `#include "Pipeline/Rasterizer.h"` 改为前向声明
- [x] 8.3 删除 `SceneManager.h/cpp`（未使用），更新 CMakeLists.txt
- [x] 8.4 删除空文件 `Vec3.cpp`，更新 CMakeLists.txt
- [x] 8.5 删除 `RenderPipeline::Prepare()` 空方法（若 Pass 实体化后不再需要）
- [x] 8.6 将散布的 `OutputDebugStringA()` 替换为条件编译宏 `SR_DEBUG_LOG()`（仅 Debug 配置生效）

## 9. 验证与收尾

- [x] 9.1 全量编译 Release 配置，确保零 warning
- [x] 9.2 运行 MFCDemo 加载测试模型，对比重构前后渲染结果一致
- [x] 9.3 检查重构后无残留魔法数字（搜索 33071、33648、10497、5120-5126）
- [x] 9.4 检查无残留重复函数（搜索 WrapCoord、GeometrySmith、InflateZlib 确认只存在于 Utils 模块）
