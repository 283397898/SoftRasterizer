#include "Render/RenderPipeline.h"

#include "Pipeline/GeometryProcessor.h"
#include "Pipeline/Rasterizer.h"

#include <algorithm>
#include <chrono>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace SR {

/**
 * @brief 执行每一帧的准备工作，目前主要用于清除帧缓冲和深度缓冲
 * @param pass 当前 Pass 上下文
 */
void RenderPipeline::Prepare(const PassContext& pass) const {
    if (!pass.framebuffer || !pass.depthBuffer) {
        return;
    }
}

/**
 * @brief 执行绘制流程：遍历渲染队列，调用几何处理器和光栅化器
 * @param queue 渲染项目队列
 * @param pass 当前 Pass 上下文
 * @return 绘制过程中的统计数据 (耗时、三角形数等)
 */
RenderStats RenderPipeline::Draw(const RenderQueue& queue, const PassContext& pass) const {
    RenderStats stats{};

    if (!pass.framebuffer || !pass.depthBuffer) {
        return stats;
    }

    OutputDebugStringA("RenderPipeline Draw: begin\n");

    Rasterizer rasterizer;
    rasterizer.SetTargets(pass.framebuffer, pass.depthBuffer);
    rasterizer.SetFrameContext(pass.frame);

    GeometryProcessor geometryProcessor;
    std::vector<Triangle> triangles;

    using Clock = std::chrono::high_resolution_clock;

    // Build a sorted list: opaque first, then blend back-to-front.
    std::vector<DrawItem> sortedItems = queue.GetItems();
    const Vec3 cameraPos = pass.frame.cameraPos;
    auto getItemSortKey = [&cameraPos](const DrawItem& item) {
        Vec3 pos{item.modelMatrix.m[3][0], item.modelMatrix.m[3][1], item.modelMatrix.m[3][2]};
        Vec3 d = pos - cameraPos;
        return d.x * d.x + d.y * d.y + d.z * d.z;
    };
    std::stable_sort(sortedItems.begin(), sortedItems.end(),
        [&getItemSortKey](const DrawItem& a, const DrawItem& b) {
            int alphaModeA = a.material ? a.material->alphaMode : 0;
            int alphaModeB = b.material ? b.material->alphaMode : 0;
            if (alphaModeA != alphaModeB) {
                return alphaModeA < alphaModeB;
            }
            if (alphaModeA == 2) {
                return getItemSortKey(a) > getItemSortKey(b);
            }
            if (a.material != b.material) {
                return a.material < b.material;
            }
            return a.mesh < b.mesh;
        });

    // 遍历渲染队列中每一个绘制项
    for (const DrawItem& item : sortedItems) {
        if (!item.mesh || !item.material) {
            continue;
        }

        // 1. 几何处理阶段：顶点变换、投影以及三角形装配
        auto buildStart = Clock::now();
        geometryProcessor.BuildTriangles(
            *item.mesh,
            *item.material,
            item,
            item.modelMatrix,
            item.normalMatrix,
            pass.frame,
            triangles);

        stats.trianglesBuilt += geometryProcessor.GetLastTriangleCount();

        auto buildEnd = Clock::now();
        stats.buildMs += std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();

        // 2. 光栅化处理阶段：三角形裁剪和像素着色
        if (!triangles.empty()) {
            auto rastStart = Clock::now();
            RasterStats rastStats = rasterizer.RasterizeTriangles(triangles);
            auto rastEnd = Clock::now();
            stats.rastMs += std::chrono::duration<double, std::milli>(rastEnd - rastStart).count();
            stats.trianglesClipped += rastStats.trianglesClipped;
            stats.trianglesRaster += rastStats.trianglesRaster;
            stats.pixelsTested += rastStats.pixelsTested;
            stats.pixelsShaded += rastStats.pixelsShaded;
        }
    }

    OutputDebugStringA("RenderPipeline Draw: end\n");

    return stats;
}

/**
 * @brief 串联起完整的渲染过程
 */
RenderStats RenderPipeline::Render(const RenderQueue& queue, const PassContext& pass) const {
    Prepare(pass);
    RenderStats stats = Draw(queue, pass);
    PostProcess(pass);
    return stats;
}

/**
 * @brief 后处理流程：FXAA 抗锯齿和色调映射/解析
 */
void RenderPipeline::PostProcess(const PassContext& pass) const {
    if (!pass.framebuffer) {
        return;
    }

    // 执行快速近似抗锯齿
    if (pass.enableFXAA) {
        pass.framebuffer->ApplyFXAA();
    }

    // 执行 HDR 到 SDR 的转换和 sRGB 校正
    if (pass.enableToneMap) {
        pass.framebuffer->ResolveToSRGB(pass.exposure, false);
    }
}

} // namespace SR
