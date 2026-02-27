#pragma once

#include <vector>
#include <cstdint>

#include "Core/Framebuffer.h"
#include "Core/DepthBuffer.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Pipeline/FrameContext.h"
#include "Pipeline/MaterialTable.h"

namespace SR {

/**
 * @brief 表示待光栅化的三角形及其属性
 *
 * 优化后的结构使用 MaterialHandle 引用 MaterialTable 中的材质数据，
 * 大幅减少单个 Triangle 的内存占用。
 */
struct Triangle {
    // 顶点位置 (裁剪空间)
    Vec4 v0{};
    Vec4 v1{};
    Vec4 v2{};

    // 纹理坐标 (主UV集和次UV集)
    Vec2 t0{};
    Vec2 t1{};
    Vec2 t2{};
    Vec2 t0_1{};
    Vec2 t1_1{};
    Vec2 t2_1{};

    // 顶点颜色
    Vec4 c0{};
    Vec4 c1{};
    Vec4 c2{};

    // 切线数据
    Vec3 tg0{};
    Vec3 tg1{};
    Vec3 tg2{};
    double tangentW = 1.0; ///< 切线 W 分量 (+1/-1)，决定副切线方向

    // 世界空间位置
    Vec3 w0{};
    Vec3 w1{};
    Vec3 w2{};

    // 法线
    Vec3 n0{};
    Vec3 n1{};
    Vec3 n2{};

    // 材质句柄 (引用 MaterialTable)
    MaterialHandle materialId = 0;
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
    /** @brief 执行光栅化渲染（原始指针版本，避免 vector 开销） */
    RasterStats RasterizeTriangles(const Triangle* triangles, size_t count);

private:
    Framebuffer* m_framebuffer = nullptr;
    DepthBuffer* m_depthBuffer = nullptr;
    FrameContext m_frameContext{};
};

} // namespace SR
