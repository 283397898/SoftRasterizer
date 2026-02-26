#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace SR {

/// @brief 纹理 UV 坐标环绕模式（对应 WebGL/OpenGL 枚举值）
enum class GLTFWrapMode : int {
    Repeat         = 10497, ///< 平铺重复
    ClampToEdge    = 33071, ///< 边缘钳制
    MirroredRepeat = 33648  ///< 镜像重复
};

/// @brief 材质 Alpha 混合模式
enum class GLTFAlphaMode : int {
    Opaque = 0, ///< 完全不透明，忽略 Alpha 通道
    Mask   = 1, ///< Alpha 测试模式，低于阈值的像素被丢弃
    Blend  = 2  ///< Alpha 混合模式，支持半透明
};

/// @brief 纹理过滤模式（对应 WebGL/OpenGL 枚举值）
enum class GLTFFilterMode : int {
    None                   = 0,    ///< 未指定（使用默认）
    Nearest                = 9728, ///< 最近邻点采样
    Linear                 = 9729, ///< 双线性过滤
    NearestMipmapNearest   = 9984, ///< Mipmap 最近邻
    LinearMipmapNearest    = 9985, ///< Mipmap 线性选级 + 最近邻
    NearestMipmapLinear    = 9986, ///< Mipmap 最近邻选级 + 线性
    LinearMipmapLinear     = 9987  ///< 三线性过滤（全 Mipmap 线性）
};

/// @brief glTF 访问器分量数据类型（对应 WebGL/OpenGL 枚举值）
enum class GLTFComponentType : int {
    Byte          = 5120, ///< 有符号 8 位整数
    UnsignedByte  = 5121, ///< 无符号 8 位整数
    Short         = 5122, ///< 有符号 16 位整数
    UnsignedShort = 5123, ///< 无符号 16 位整数
    UnsignedInt   = 5125, ///< 无符号 32 位整数
    Float         = 5126  ///< 32 位浮点
};

/// @brief glTF 二进制缓冲区
struct GLTFBuffer {
    std::vector<uint8_t> data; ///< 原始字节数据
};

/// @brief glTF 缓冲区视图，描述 Buffer 的一个子区间
struct GLTFBufferView {
    int    bufferIndex = -1;   ///< 所属 Buffer 的索引
    size_t byteOffset  = 0;    ///< 视图在 Buffer 中的字节偏移
    size_t byteLength  = 0;    ///< 视图字节长度
    size_t byteStride  = 0;    ///< 步长（0 表示紧密排列）
    int    target      = 0;    ///< 绑定目标（顶点缓冲/索引缓冲）
};

/// @brief glTF 访问器，描述如何解释 BufferView 中的数据
struct GLTFAccessor {
    int    bufferViewIndex = -1; ///< 对应的 BufferView 索引
    size_t byteOffset      = 0;  ///< 访问器在 BufferView 中的偏移
    size_t count           = 0;  ///< 元素个数
    int    componentType   = 0;  ///< 分量类型（参见 GLTFComponentType）
    int    type            = 0;  ///< 数据类型（SCALAR/VEC2/VEC3/VEC4 等）
    bool   normalized      = false; ///< 是否将整数值归一化到 [0,1] 或 [-1,1]
    std::vector<double> minValues;  ///< 各分量最小值
    std::vector<double> maxValues;  ///< 各分量最大值
};

/// @brief glTF 图像，存储解码后的像素数据（RGBA8）
struct GLTFImage {
    std::vector<uint8_t> pixels; ///< 解码后的像素字节（RGBA 排列，每像素 4 字节）
    int  width    = 0;           ///< 图像宽度（像素）
    int  height   = 0;           ///< 图像高度（像素）
    int  channels = 0;           ///< 通道数（通常为 4）
    bool isSRGB   = false;       ///< 是否为 sRGB 色彩空间（颜色贴图通常为 true）
    std::string mimeType;        ///< MIME 类型（如 "image/png"）
};

/// @brief glTF 采样器，描述纹理的过滤和环绕方式
struct GLTFSampler {
    GLTFWrapMode   wrapS      = GLTFWrapMode::Repeat;    ///< U 方向环绕模式
    GLTFWrapMode   wrapT      = GLTFWrapMode::Repeat;    ///< V 方向环绕模式
    GLTFFilterMode minFilter  = GLTFFilterMode::None;    ///< 缩小过滤模式
    GLTFFilterMode magFilter  = GLTFFilterMode::None;    ///< 放大过滤模式
};

/// @brief glTF 纹理，将图像与采样器关联
struct GLTFTexture {
    int imageIndex   = -1; ///< 引用的 Image 索引
    int samplerIndex = -1; ///< 引用的 Sampler 索引
};

/// @brief glTF 纹理引用信息（含坐标集和强度参数）
struct GLTFTextureInfo {
    int    textureIndex = -1; ///< 纹理索引
    int    texCoord     = 0;  ///< 使用的 UV 坐标集（0 或 1）
    double scale        = 1.0; ///< 法线贴图强度缩放
    double strength     = 1.0; ///< 遮蔽贴图强度
};

/// @brief glTF PBR 金属度-粗糙度工作流参数
struct GLTFPBRMetallicRoughness {
    double          baseColorFactor[4]           = {1.0, 1.0, 1.0, 1.0}; ///< 基础颜色因子 (RGBA)
    GLTFTextureInfo baseColorTexture{};                                    ///< 基础颜色贴图
    double          metallicFactor               = 1.0;                   ///< 金属度因子 [0,1]
    double          roughnessFactor              = 1.0;                   ///< 粗糙度因子 [0,1]
    GLTFTextureInfo metallicRoughnessTexture{};                           ///< 金属度-粗糙度贴图
};

/// @brief glTF 材质，包含 PBR 属性、法线/遮蔽/自发光贴图及扩展
struct GLTFMaterial {
    std::string               name;                  ///< 材质名称
    GLTFPBRMetallicRoughness  pbr{};                 ///< PBR 金属度-粗糙度参数
    GLTFTextureInfo           normalTexture{};       ///< 法线贴图
    GLTFTextureInfo           occlusionTexture{};    ///< 环境遮蔽贴图
    GLTFTextureInfo           emissiveTexture{};     ///< 自发光贴图
    /// KHR_materials_transmission 扩展
    struct {
        double          transmissionFactor = 0.0;   ///< 透射强度 [0,1]
        GLTFTextureInfo transmissionTexture{};       ///< 透射强度贴图
        bool            hasTransmission    = false; ///< 是否有透射扩展
    } transmission{};
    /// KHR_materials_ior 扩展
    struct {
        double ior    = 1.5;   ///< 折射率
        bool   hasIOR = false; ///< 是否有折射率扩展
    } iorExt{};
    /// KHR_materials_specular 扩展
    struct {
        double          specularFactor        = 1.0;             ///< 镜面反射强度 [0,1]
        double          specularColorFactor[3]= {1.0, 1.0, 1.0}; ///< 镜面反射颜色因子
        GLTFTextureInfo specularTexture{};                        ///< 镜面强度贴图
        GLTFTextureInfo specularColorTexture{};                   ///< 镜面颜色贴图
        bool            hasSpecular           = false;            ///< 是否有镜面扩展
    } specular{};
    double       emissiveFactor[3] = {0.0, 0.0, 0.0}; ///< 自发光因子 (RGB)
    GLTFAlphaMode alphaMode        = GLTFAlphaMode::Opaque; ///< Alpha 混合模式
    double        alphaCutoff      = 0.5;  ///< Mask 模式下的 Alpha 阈值
    bool          doubleSided      = false; ///< 是否双面渲染
};

/// @brief glTF 网格图元，对应一次绘制调用
struct GLTFPrimitive {
    int materialIndex = -1; ///< 使用的材质索引，-1 表示默认材质
    int indices       = -1; ///< 索引访问器索引，-1 表示非索引绘制
    int mode          = 4;  ///< 图元类型（4 = TRIANGLES）
    std::unordered_map<std::string, int> attributes; ///< 顶点属性名称到访问器索引的映射
};

/// @brief glTF 网格，包含多个 Primitive
struct GLTFMesh {
    std::string              name;       ///< 网格名称
    std::vector<GLTFPrimitive> primitives; ///< 图元列表
};

/// @brief glTF 场景节点，可持有网格、变换及子节点
struct GLTFNode {
    int              meshIndex         = -1;  ///< 关联的网格索引，-1 表示无网格
    std::vector<int> children;                ///< 子节点索引列表
    double           translation[3]    = {0.0, 0.0, 0.0}; ///< 平移 (X, Y, Z)
    double           rotation[4]       = {0.0, 0.0, 0.0, 1.0}; ///< 旋转四元数 (X, Y, Z, W)
    double           scale[3]          = {1.0, 1.0, 1.0};  ///< 缩放 (X, Y, Z)
    bool             hasMatrix         = false; ///< 是否使用矩阵变换（代替 TRS）
    double           matrix[16]        = {      ///< 列主序 4x4 变换矩阵（hasMatrix=true 时有效）
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
};

/// @brief glTF 场景，包含根节点列表
struct GLTFScene {
    std::vector<int> rootNodes; ///< 场景根节点索引列表
};

} // namespace SR
