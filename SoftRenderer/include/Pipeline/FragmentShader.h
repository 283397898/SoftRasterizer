#pragma once

#include <vector>

#include "Asset/GLTFTypes.h"
#include "Core/Framebuffer.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Scene/TextureBinding.h"
#include "Scene/LightGroup.h"

namespace SR {

struct GLTFImage;
struct GLTFSampler;
class EnvironmentMap;

/// @brief 每帧预计算的光照数据（避免在每个片元重复计算）
struct PrecomputedLight {
    Vec3 L;        ///< 归一化光照方向（指向光源）
    Vec3 radiance; ///< 辐射亮度 = 光照颜色 × 强度
};

/**
 * @brief 三角形级常量数据（同一三角形内所有像素共享）
 *
 * 光栅化时从 MaterialTable 中复制材质属性到此结构，
 * 既保持 Triangle 结构体小巧，又为热路径提供快速访问。
 */
struct FragmentContext {
    Vec3 cameraPos; ///< 相机世界空间位置（用于计算视线方向）

    // PBR 材质属性（从 MaterialTable 复制）
    Vec3  albedo{1.0, 1.0, 1.0}; ///< 基础颜色（反照率）
    double metallic           = 0.0;  ///< 金属度 [0,1]
    double roughness          = 0.5;  ///< 粗糙度 [0,1]
    bool   doubleSided        = false; ///< 是否双面渲染
    double alpha              = 1.0;  ///< 材质透明度
    double transmissionFactor = 0.0;  ///< 透射强度 [0,1]
    GLTFAlphaMode alphaMode   = GLTFAlphaMode::Opaque; ///< Alpha 模式
    double alphaCutoff        = 0.5;  ///< Mask 模式下的 Alpha 裁剪阈值
    Vec3   emissiveFactor{0.0, 0.0, 0.0}; ///< 自发光因子
    double ior                = 1.5;  ///< 折射率（KHR_materials_ior）
    double specularFactor     = 1.0;  ///< 镜面反射强度（KHR_materials_specular）
    Vec3   specularColorFactor{1.0, 1.0, 1.0}; ///< 镜面反射颜色（KHR_materials_specular）

    // 纹理绑定（从 MaterialTable 复制）
    TextureBindingArray textures{};

    // 场景引用（指针，不持有所有权）
    const std::vector<DirectionalLight>* lights = nullptr; ///< 平行光列表
    Vec3 ambientColor{0.03, 0.03, 0.03};                   ///< 全局环境光（无 IBL 时使用）
    const std::vector<GLTFImage>*   images   = nullptr;    ///< 纹理图像数组
    const std::vector<GLTFSampler>* samplers = nullptr;    ///< 纹理采样器数组
    const EnvironmentMap* environmentMap     = nullptr;    ///< IBL 环境贴图（可选）

    double tangentW = 1.0; ///< 切线 W 分量 (+1/-1)，决定副切线方向

    // 预计算光照数据（指针，避免 vector 拷贝，由 Rasterizer 赋值）
    const PrecomputedLight* precomputedLights = nullptr; ///< 预计算光照数组
    size_t precomputedLightCount = 0;                    ///< 预计算光照数量
};

/// @brief 像素级插值数据（在每个像素处由重心坐标插值得到）
struct FragmentVarying {
    Vec3 normal;    ///< 插值后的世界空间法线
    Vec3 worldPos;  ///< 插值后的世界空间位置
    Vec2 texCoord;  ///< 主 UV 坐标（插值后）
    Vec2 texCoord1; ///< 次 UV 坐标（插值后）
    Vec4 color;     ///< 顶点颜色（插值后，RGBA）
    Vec3 tangent;   ///< 世界空间切线（用于法线贴图）
};

/**
 * @brief 片元着色器类，负责 PBR 着色计算
 */
class FragmentShader {
public:
    /** @brief 优化的着色方法：Context 为三角形级常数，Varying 为像素级变量
     *  @param outEffectiveAlpha 如果非 nullptr，将输出 Fresnel 调制后的有效混合 alpha
     */
    Vec3 ShadeFast(const FragmentContext& ctx, const FragmentVarying& varying, double* outEffectiveAlpha = nullptr) const;
};

} // namespace SR
