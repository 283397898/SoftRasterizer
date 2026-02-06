# SoftRasterizer - AI Coding Agent Instructions

## 项目概述

纯自研软件光栅化器，**零第三方依赖**，实现完整 PBR 渲染管线。

| 模块 | 说明 |
|------|------|
| **SoftRenderer/** | 渲染器核心 DLL (C++20, OpenMP) |
| **MFCDemo/** | Win32 演示程序 (D3D12/HDR) |

## 已实现功能

- Cook-Torrance PBR BRDF (GGX + Fresnel-Schlick + Smith 几何遮蔽)
- Sutherland-Hodgman 裁剪 + 透视校正插值
- FXAA 抗锯齿
- HDR 输出 (D3D12 + DXGI_FORMAT_R16G16B16A16_FLOAT)
- 轨道相机 (左键旋转/右键平移/滚轮缩放)
- 程序化球体网格生成
- 三层架构 (Asset/Runtime/Render) 骨架已搭建

---

## 架构分层

项目采用三层架构设计，职责分离清晰：

| 层 | 目录 | 职责 | 状态 |
|---|---|---|---|
| **Asset** | `include/Asset/`, `src/Asset/` | glTF/GLB 解析、Buffer 解包、图像解码 | 骨架已建立，待实现 |
| **Runtime** | `include/Runtime/`, `src/Runtime/` | 渲染友好数据 (GPUScene)、坐标系转换 | 骨架已建立 |
| **Render** | `include/Render/`, `src/Render/` | 渲染队列构建、管线调度、后处理 | 已重构完成 |

---

## 渲染管线数据流

```
Scene/GPUScene → RenderQueueBuilder → RenderQueue
    → RenderPipeline.Render(queue, passContext)
        → GeometryProcessor.BuildTriangles()
        → Clipper (Sutherland-Hodgman)
        → Rasterizer (重心插值 + 深度测试)
        → FragmentShader (PBR)
        → Framebuffer.ApplyFXAA() → 输出
```

## 核心类职责

| 类 | 职责 | 位置 |
|---|---|---|
| `Renderer` | 渲染入口，orchestration | src/Renderer.cpp |
| `RenderPipeline` | 管线执行 (Prepare/Draw/PostProcess) | Render/RenderPipeline.cpp |
| `RenderQueueBuilder` | 从 ObjectGroup 构建 RenderQueue | Render/RenderQueueBuilder.cpp |
| `GPUSceneRenderQueueBuilder` | 从 GPUScene 构建 RenderQueue | Render/GPUSceneRenderQueueBuilder.cpp |
| `FrameContextBuilder` | 构建帧上下文 (相机/光源) | Render/FrameContextBuilder.cpp |
| `GeometryProcessor` | MVP 变换，构建裁剪前三角形 | Pipeline/GeometryProcessor.cpp |
| `Rasterizer` | 光栅化 + 透视校正 + 深度测试 | Pipeline/Rasterizer.cpp |
| `FragmentShader` | Cook-Torrance BRDF 着色 | Pipeline/FragmentShader.cpp |
| `Framebuffer` | 双缓冲 (BGRA + Linear HDR) + FXAA | Core/Framebuffer.cpp |
| `HDRPresenter` | D3D12 HDR 呈现器 | MFCDemo/src/HDRPresenter.cpp |

## 上下文结构

| 结构 | 职责 |
|---|---|
| `FrameContext` | 帧级只读数据：view/projection 矩阵、cameraPos、环境光、光源列表 |
| `PassContext` | Pass 级数据：FrameContext + framebuffer/depthBuffer + 后处理开关 |
| `RendererConfig` | 渲染器配置：FrameContextOptions + 后处理参数 |

## 场景组织

**传统场景 (Scene)**：
- **ObjectGroup**: 模型列表，每个 Model 包含 Mesh 指针、Transform、PBRMaterial
- **LightGroup**: 方向光列表
- **OrbitCamera**: 轨道相机

**GPUScene (运行时场景)**：
- 扁平化 DrawItem 列表 (mesh + material + modelMatrix + normalMatrix)
- 由 GPUSceneBuilder 从 ObjectGroup 构建
- 支持 Renderer::Render(const GPUScene&) 入口

---

## 坐标系约定 (DirectX 风格)

| 约定 | 说明 |
|------|------|
| 手系 | 左手坐标系：+X 右, +Y 上, +Z 入屏 |
| NDC 深度 | [0, 1]，近平面=0，远平面=1 |
| 矩阵存储 | 行主序，向量左乘 `v' = v * M` |
| MVP 顺序 | Model × View × Projection |
| Translation | 存储在 Mat4 第 4 行 (m[3][0], m[3][1], m[3][2]) |

---

## 编码规范

### 数学类型
- 所有数学类型使用 `double` 精度 (Vec3, Vec4, Mat4)
- 不使用 float，除非是 GPU 传输格式

### DLL 导出
- 公开类必须使用 `SR_API` 宏标记
- 宏定义在 SoftRendererExport.h

### OpenMP 并行
- 光栅化逐行并行：`#pragma omp parallel for`
- 帧缓冲清除并行处理

### Framebuffer 双缓冲
- `m_linearPixels`: Vec3 线性 HDR 缓冲 (渲染目标)
- `m_pixels`: uint32_t BGRA8 SDR 缓冲 (GDI 兼容输出)

---

## glTF 2.0 Core 模块规范

### 设计原则

1. **缓存友好**: SOA 布局、Tile-Based 光栅化、Morton 编码纹理
2. **零拷贝**: Buffer 直接映射、延迟解包
3. **SIMD 就绪**: 16/32 字节对齐、批量处理
4. **分离关注点**: 加载层 (Asset/) vs 运行时表示 (Runtime/)

### Asset 层模块状态

| 文件 | 职责 | 状态 |
|------|------|------|
| `JSONParser.h/cpp` | 零依赖递归下降 JSON 解析器 | **骨架** (待实现 Parse) |
| `BufferAccessor.h/inl` | bufferView/accessor 模板解包器 | **骨架** (待实现 Read) |
| `ImageDecoder.h/cpp` | PNG/JPEG 解码器 | **骨架** (待实现 Decode) |
| `GLTFTypes.h` | glTF 规范数据结构 | **已完成** |
| `GLTFAsset.h` | 资产容器 | **已完成** |
| `GLTFLoader.h/cpp` | glTF/GLB 解析入口 | **骨架** (待实现 LoadGLB/LoadGLTF) |

### Runtime 层模块状态

| 文件 | 职责 | 状态 |
|------|------|------|
| `GPUScene.h/cpp` | 扁平化渲染场景 | **已完成** |
| `GPUSceneBuilder.h/cpp` | ObjectGroup → GPUScene 构建器 | **已完成** |

### Render 层模块状态

| 文件 | 职责 | 状态 |
|------|------|------|
| `RenderQueue.h` | 渲染队列 (DrawItem 列表) | **已完成** |
| `RenderQueueBuilder.h/cpp` | ObjectGroup → RenderQueue | **已完成** |
| `GPUSceneRenderQueueBuilder.h/cpp` | GPUScene → RenderQueue | **已完成** |
| `FrameContextBuilder.h/cpp` | 帧上下文构建 | **已完成** |
| `PassContext.h` | Pass 上下文 | **已完成** |
| `RenderPipeline.h/cpp` | 管线执行器 | **已完成** |
| `RendererConfig.h/cpp` | 渲染器配置 | **已完成** |

---

### glTF 数据结构 (GLTFTypes.h)

**已定义的核心结构**（位于 `include/Asset/GLTFTypes.h`）：

| 结构 | 字段 |
|------|------|
| `GLTFBuffer` | data (vector<uint8_t>) |
| `GLTFBufferView` | bufferIndex, byteOffset, byteLength, byteStride, target |
| `GLTFAccessor` | bufferViewIndex, byteOffset, count, componentType, type, normalized |
| `GLTFImage` | pixels, width, height, channels, isSRGB, mimeType |
| `GLTFSampler` | wrapS, wrapT, minFilter, magFilter |
| `GLTFTexture` | imageIndex, samplerIndex |
| `GLTFTextureInfo` | textureIndex, texCoord, scale, strength |
| `GLTFPBRMetallicRoughness` | baseColorFactor[4], baseColorTexture, metallicFactor, roughnessFactor, metallicRoughnessTexture |
| `GLTFMaterial` | name, pbr, normalTexture, occlusionTexture, emissiveTexture, emissiveFactor[3], alphaMode, alphaCutoff, doubleSided |
| `GLTFPrimitive` | materialIndex (待扩展顶点属性) |
| `GLTFMesh` | name, primitives |
| `GLTFNode` | meshIndex, children, translation[3], rotation[4], scale[3], hasMatrix, matrix[16] |
| `GLTFScene` | rootNodes |

**GLTFAsset 容器** (位于 `include/Asset/GLTFAsset.h`)：
- buffers, bufferViews, accessors, images, samplers, textures
- materials, meshes, nodes, scenes
- defaultSceneIndex, generator

---

### 顶点布局设计 (待实现)

**避免 AOS 布局**（缓存不友好，浪费带宽）

**使用 SOA 布局** - VertexStreams 结构：
- 分离的属性数组：positions, normals, texCoords0, texCoords1, tangents, colors0, joints0, weights0
- 属性掩码 (attributeMask) 标记存在的属性
- 按需加载，仅访问渲染所需属性

属性掩码位定义：POSITION=0, NORMAL=1, TEXCOORD_0=2, TEXCOORD_1=3, TANGENT=4, COLOR_0=5, JOINTS_0=6, WEIGHTS_0=7

---

### 场景节点

Node 类包含：
- 父子关系：parent 指针 + children 列表
- TRS 变换：translation (Vec3), rotation (四元数 xyzw), scale (Vec3)
- 可选 matrix 覆盖 TRS
- 资源索引：meshIndex, cameraIndex, lightIndex, skinIndex (均为 -1 表示无)
- 缓存的世界矩阵，支持脏标记更新

四元数转矩阵遵循 glTF 规范公式。

---

### 网格结构

**Primitive** (子网格单元)：
- SOA 顶点数据 (VertexStreams)
- 可选索引数组（空表示非索引绘制）
- 拓扑类型：Points/Lines/LineLoop/LineStrip/Triangles/TriangleStrip/TriangleFan
- 材质索引 (-1 表示默认材质)
- 预计算 AABB 包围盒

**Mesh**: 包含多个 Primitive 和名称

**GPUMesh** (运行时格式)：
- 紧凑 16 字节对齐顶点：position, normal, texCoord, color (RGBA8), materialId
- 合并后的索引数组
- 每个 primitive 的起始偏移

---

### PBR 材质 (Metallic-Roughness)

**GLTFMaterial** 包含：
- PBRMetallicRoughness: baseColorFactor, baseColorTexture, metallicFactor, roughnessFactor, metallicRoughnessTexture
- normalTexture, occlusionTexture, emissiveTexture, emissiveFactor
- alphaMode (Opaque/Mask/Blend), alphaCutoff, doubleSided

**TextureInfo**: textureIndex, texCoord 通道, scale/strength

**GPUMaterial** (运行时扁平化)：
- 64 字节对齐，无指针
- 常量因子 + 纹理索引 + 标志位
- metallicRoughnessTexture: B 通道=metallic, G 通道=roughness

---

### 纹理系统

**Image** (加载时)：
- 线性布局 RGBA8 像素数据
- width, height, channels, isSRGB, mimeType

**Sampler**：
- WrapMode: Repeat, ClampToEdge, MirroredRepeat
- FilterMode: Nearest, Linear, 及 Mipmap 变体

**GPUTexture** (运行时)：
- Morton 编码像素（缓存友好 2D 访问）
- Mipchain 层级数组
- 支持点采样、双线性、三线性过滤

**颜色空间**：
- sRGB ↔ Linear 转换使用 256 项预计算查表

---

### Buffer/Accessor 解包

**BufferView**：bufferIndex, byteOffset, byteLength, byteStride (0=紧密排列), target

**Accessor**：
- bufferViewIndex, byteOffset, count
- ComponentType: Byte, UnsignedByte, Short, UnsignedShort, UnsignedInt, Float
- Type: Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
- normalized 标志
- 可选 min/max 边界

**BufferAccessor** 模板解包器 (位于 `include/Asset/BufferAccessor.h`)：
- 根据 accessor 类型自动转换为目标类型
- **当前状态**: 骨架已建立，Read() 返回空向量

---

### 零依赖解析器

**JSONParser** (位于 `include/Asset/JSONParser.h`)：
- 递归下降解析
- JSONValue 支持类型：Null, Bool, Number, String, Array, Object
- 通过下标运算符访问数组/对象
- 返回 optional 表示解析结果
- **当前状态**: 骨架已建立，Parse() 返回 nullopt

**ImageDecoder** (位于 `include/Asset/ImageDecoder.h`)：
- PNG 解码：解析 chunks → Zlib 解压 → Unfilter scanlines
- JPEG 解码：解析 segments → Huffman 解码 → IDCT 8x8 → YCbCr 转 RGB
- 统一入口根据 mimeType 分发
- **当前状态**: 骨架已建立，Decode() 返回 false

---

### GLTFLoader (位于 `include/Asset/GLTFLoader.h`)

```cpp
class GLTFLoader {
public:
    GLTFAsset LoadGLB(const std::string& path);
    GLTFAsset LoadGLTF(const std::string& path);
    const std::string& GetLastError() const;
};
```

**当前状态**: 骨架已建立，LoadGLB/LoadGLTF 返回空 GLTFAsset

---

### 相机与光源

**Camera**：
- 类型：Perspective (fovY, aspectRatio, zNear, zFar) 或 Orthographic (xMag, yMag)
- GetProjectionMatrix() 生成投影矩阵

**Light** (KHR_lights_punctual)：
- 类型：Directional, Point, Spot
- 属性：color, intensity, range (0=无限)
- Spot 额外参数：innerConeAngle, outerConeAngle
- GetAttenuation() 计算基于物理的距离衰减

---

### Tile-Based 光栅化

**设计**：
- TILE_SIZE = 32×32 像素
- Tile 结构：坐标 + 覆盖该 Tile 的三角形索引列表

**流程**：
1. Binning：将三角形分配到覆盖的 Tile
2. RasterizeTiles：并行处理每个 Tile

**TileRenderQueue**：
- 分离 opaque (前→后排序) 和 blend (后→前排序) 物体
- TileDrawItem：meshIndex, primitiveIndex, materialIndex, depth

---

### GPUScene 运行时场景

当前已实现的 GPUScene 结构 (位于 `include/Runtime/GPUScene.h`)：

```cpp
struct GPUSceneDrawItem {
    const Mesh* mesh = nullptr;
    const PBRMaterial* material = nullptr;
    Mat4 modelMatrix = Mat4::Identity();
    Mat4 normalMatrix = Mat4::Identity();
};

class GPUScene {
    void Reserve(size_t count);
    void AddDrawable(const GPUSceneDrawItem& item);
    void SetItems(std::vector<GPUSceneDrawItem>&& items);
    void Clear();
    const std::vector<GPUSceneDrawItem>& GetItems() const;
};
```

**待扩展**：未来从 GLTFAsset 构建时需要支持：
- meshes, materials, textures, samplers 数组
- 实例变换：worldMatrices, normalMatrices, meshIndices
- lights + lightTransforms

---

### FragmentShader glTF 扩展

**FragmentInput**：
- 世界空间 position, normal, tangent, bitangent
- texCoord0, texCoord1, 顶点颜色
- viewDir (指向相机)
- materialIndex, scene 指针

**ShadeGLTF 流程**：
1. BaseColor = factor × texture × vertexColor
2. Alpha 测试 (Mask 模式)
3. Metallic/Roughness 采样
4. Normal mapping
5. Occlusion 采样
6. Emissive 计算
7. 遍历光源计算 Cook-Torrance BRDF
8. 合成最终颜色

---

### glTF 坐标系转换

| 项目 | glTF | DirectX | 转换 |
|------|------|---------|------|
| 手系 | 右手 (+Z 出屏) | 左手 (+Z 入屏) | 翻转 Z |
| Position | (x, y, z) | (x, y, -z) | Z 取反 |
| Normal | (x, y, z) | (x, y, -z) | Z 取反 |
| Tangent | (x, y, z, w) | (x, y, -z, -w) | Z, W 取反 |
| Quaternion | (x, y, z, w) | (-x, -y, z, w) | X, Y 取反 |
| 三角形绕序 | CCW | CW | 加载时翻转索引或翻转面剔除 |

矩阵转换：翻转第 3 列和第 3 行的 Z 分量

---

### 实现优先级

| 阶段 | 模块 | 复杂度 | 预估 | 依赖 |
|------|------|--------|------|------|
| 1 | JSONParser | ⭐⭐⭐ | 1 周 | 无 |
| 2 | BufferAccessor | ⭐⭐ | 0.5 周 | 1 |
| 3 | GLTFLoader 基础 | ⭐⭐⭐ | 1 周 | 1, 2 |
| 4 | ImageDecoder PNG | ⭐⭐⭐⭐⭐ | 2 周 | 无 |
| 5 | ImageDecoder JPEG | ⭐⭐⭐⭐⭐ | 2 周 | 无 |
| 6 | GPUTexture + Sampler | ⭐⭐⭐ | 1 周 | 4, 5 |
| 7 | Node + SceneGraph | ⭐⭐ | 0.5 周 | 3 |
| 8 | GLTFMaterial + GPUMaterial | ⭐⭐ | 0.5 周 | 3 |
| 9 | Mesh + Primitive | ⭐⭐ | 0.5 周 | 2, 3 |
| 10 | GPUScene 构建 | ⭐⭐⭐ | 1 周 | 6-9 |
| 11 | FragmentShader 扩展 | ⭐⭐⭐ | 1 周 | 10 |
| 12 | TileRasterizer | ⭐⭐⭐⭐ | 1.5 周 | 10 |
| 13 | Camera + Light | ⭐⭐ | 0.5 周 | 3 |

**总计：约 12-14 周**

---

## 扩展开发指南

### 添加新 Mesh 生成器
1. 在 Mesh.h 声明静态工厂方法
2. 在 Mesh.cpp 实现顶点/索引生成
3. 调用 GenerateNormals() 生成法线

### 添加新材质属性
1. 扩展 PBRMaterial 结构体
2. 更新 FragmentShader::Shade() 计算
3. 在 Triangle 结构中传递材质

### 添加纹理采样模式
当前仅支持点采样，待实现：双线性插值、Mipmap、UV 包裹模式

---

## 构建命令

```powershell
# 配置
cmake -B build -G "Visual Studio 17 2022" -A x64

# 构建 Release
cmake --build build --config Release

# 运行
.\build\MFCDemo\Release\MFCDemo.exe
```
