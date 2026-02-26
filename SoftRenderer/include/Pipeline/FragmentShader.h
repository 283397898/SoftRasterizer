#pragma once

#include <vector>

#include "Core/Framebuffer.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Scene/LightGroup.h"

namespace SR {

struct GLTFImage;
struct GLTFSampler;
class EnvironmentMap;

// Precomputed light data for faster shading (computed once per frame)
struct PrecomputedLight {
    Vec3 L;         // Normalized light direction (toward light)
    Vec3 radiance;  // color * intensity
};

/**
 * @brief Per-triangle constant data (shared across all pixels in a triangle)
 *
 * Material properties are copied here from MaterialTable during rasterization.
 * This keeps Triangle small while providing fast access in the hot path.
 */
struct FragmentContext {
    Vec3 cameraPos;

    // PBR Material properties (copied from MaterialTable)
    Vec3 albedo{1.0, 1.0, 1.0};
    double metallic = 0.0;
    double roughness = 0.5;
    bool doubleSided = false;
    double alpha = 1.0;
    double transmissionFactor = 0.0;
    int alphaMode = 0;
    double alphaCutoff = 0.5;
    Vec3 emissiveFactor{0.0, 0.0, 0.0};
    double ior = 1.5;
    double specularFactor = 1.0;
    Vec3 specularColorFactor{1.0, 1.0, 1.0};

    // Texture indices (copied from MaterialTable)
    int32_t baseColorImageIndex = -1;
    int32_t metallicRoughnessImageIndex = -1;
    int32_t normalImageIndex = -1;
    int32_t occlusionImageIndex = -1;
    int32_t emissiveImageIndex = -1;
    int32_t transmissionImageIndex = -1;
    int32_t baseColorSamplerIndex = -1;
    int32_t metallicRoughnessSamplerIndex = -1;
    int32_t normalSamplerIndex = -1;
    int32_t occlusionSamplerIndex = -1;
    int32_t emissiveSamplerIndex = -1;
    int32_t transmissionSamplerIndex = -1;
    int32_t baseColorTexCoordSet = 0;
    int32_t metallicRoughnessTexCoordSet = 0;
    int32_t normalTexCoordSet = 0;
    int32_t occlusionTexCoordSet = 0;
    int32_t emissiveTexCoordSet = 0;
    int32_t transmissionTexCoordSet = 0;

    // Scene references
    const std::vector<DirectionalLight>* lights = nullptr;
    Vec3 ambientColor{0.03, 0.03, 0.03};
    const std::vector<GLTFImage>* images = nullptr;
    const std::vector<GLTFSampler>* samplers = nullptr;
    const EnvironmentMap* environmentMap = nullptr;

    double tangentW = 1.0; ///< 切线 W 分量 (+1/-1)，决定副切线方向

    // Precomputed light data (pointer to avoid vector copy - set by Rasterizer)
    const PrecomputedLight* precomputedLights = nullptr;
    size_t precomputedLightCount = 0;
};

// Per-pixel varying data (interpolated for each pixel)
struct FragmentVarying {
    Vec3 normal;
    Vec3 worldPos;
    Vec2 texCoord;
    Vec2 texCoord1;
    Vec4 color;
    Vec3 tangent;
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
