#pragma once

#include <vector>

#include "Core/Framebuffer.h"
#include "Core/DepthBuffer.h"
#include "Material/PBRMaterial.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Pipeline/FrameContext.h"

namespace SR {

/**
 * @brief 表示待光栅化的三角形及其属性
 */
struct Triangle {
    Vec4 v0{};
    Vec4 v1{};
    Vec4 v2{};

    Vec2 t0{};
    Vec2 t1{};
    Vec2 t2{};

    Vec3 tg0{};
    Vec3 tg1{};
    Vec3 tg2{};

    Vec3 w0{};
    Vec3 w1{};
    Vec3 w2{};

    Vec3 n0{};
    Vec3 n1{};
    Vec3 n2{};

    PBRMaterial material{};

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
 * @brief 光栅化统计数据
 */
struct RasterStats {
    uint64_t trianglesInput = 0;   ///< 输入三角形数量
    uint64_t trianglesClipped = 0; ///< 裁剪后剩余三角形数量
    uint64_t trianglesRaster = 0;  ///< 进入光栅化阶段的三角形数量
    uint64_t pixelsTested = 0;     ///< 深度测试执行次数
    uint64_t pixelsShaded = 0;     ///< 片元着色器执行次数
};

/**
 * @brief 光栅化器类，执行三角形遍历和片元着色
 */
class Rasterizer {
public:
    /** @brief 设置渲染目标 */
    void SetTargets(Framebuffer* framebuffer, DepthBuffer* depthBuffer);
    /** @brief 设置当前帧渲染上下文 */
    void SetFrameContext(const FrameContext& context);
    /** @brief 执行光栅化渲染 */
    RasterStats RasterizeTriangles(const std::vector<Triangle>& triangles);

private:
    Framebuffer* m_framebuffer = nullptr;
    DepthBuffer* m_depthBuffer = nullptr;
    FrameContext m_frameContext{};
};

} // namespace SR
