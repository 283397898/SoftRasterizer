#pragma once

#include <vector>

#include "Core/Framebuffer.h"
#include "Material/PBRMaterial.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Scene/LightGroup.h"

namespace SR {

struct GLTFImage;
struct GLTFSampler;

// Precomputed light data for faster shading (computed once per frame)
struct PrecomputedLight {
    Vec3 L;         // Normalized light direction (toward light)
    Vec3 radiance;  // color * intensity
};

// Per-triangle constant data (shared across all pixels in a triangle)
struct FragmentContext {
    Vec3 cameraPos;
    const PBRMaterial* material = nullptr;
    const std::vector<DirectionalLight>* lights = nullptr;
    Vec3 ambientColor{0.03, 0.03, 0.03};
    const std::vector<GLTFImage>* images = nullptr;
    const std::vector<GLTFSampler>* samplers = nullptr;
    int baseColorImageIndex = -1;
    int metallicRoughnessImageIndex = -1;
    int normalImageIndex = -1;
    int occlusionImageIndex = -1;
    int emissiveImageIndex = -1;
    int baseColorSamplerIndex = -1;
    int metallicRoughnessSamplerIndex = -1;
    int normalSamplerIndex = -1;
    int occlusionSamplerIndex = -1;
    int emissiveSamplerIndex = -1;
    
    // Precomputed light data (pointer to avoid vector copy - set by Rasterizer)
    const PrecomputedLight* precomputedLights = nullptr;
    size_t precomputedLightCount = 0;
};

// Per-pixel varying data (interpolated for each pixel)
struct FragmentVarying {
    Vec3 normal;
    Vec3 worldPos;
    Vec2 texCoord;
    Vec3 tangent;
};

// Legacy struct for compatibility
struct FragmentInput {
    Vec3 normal;
    Vec3 worldPos;
    Vec3 cameraPos;
    Vec2 texCoord;
    Vec3 tangent;
    PBRMaterial material;
    const std::vector<DirectionalLight>* lights = nullptr;
    Vec3 ambientColor{0.03f, 0.03f, 0.03f};
    const std::vector<GLTFImage>* images = nullptr;
    const std::vector<GLTFSampler>* samplers = nullptr;
    int meshIndex = -1;
    int materialIndex = -1;
    int primitiveIndex = -1;
    int nodeIndex = -1;
    int baseColorTextureIndex = -1;
    int metallicRoughnessTextureIndex = -1;
    int normalTextureIndex = -1;
    int occlusionTextureIndex = -1;
    int emissiveTextureIndex = -1;
    int baseColorImageIndex = -1;
    int metallicRoughnessImageIndex = -1;
    int normalImageIndex = -1;
    int occlusionImageIndex = -1;
    int emissiveImageIndex = -1;
    int baseColorSamplerIndex = -1;
    int metallicRoughnessSamplerIndex = -1;
    int normalSamplerIndex = -1;
    int occlusionSamplerIndex = -1;
    int emissiveSamplerIndex = -1;
};

/**
 * @brief 片元着色器类，负责 PBR 着色计算
 */
class FragmentShader {
public:
    /** @brief 基础着色方法 (兼容性版本) */
    Vec3 Shade(const FragmentInput& input) const;
    
    /** @brief 优化的着色方法：Context 为三角形级常数，Varying 为像素级变量 */
    Vec3 ShadeFast(const FragmentContext& ctx, const FragmentVarying& varying) const;
};

} // namespace SR
