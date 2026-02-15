#include "Render/RenderPipeline.h"

#include "Pipeline/GeometryProcessor.h"
#include "Pipeline/Rasterizer.h"
#include "Pipeline/EnvironmentMap.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <vector>
#include <omp.h>
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
 * @param outDeferredBlend 如果非空，透明三角形不在此处光栅化，而是输出到该容器中
 * @return 绘制过程中的统计数据 (耗时、三角形数等)
 */
RenderStats RenderPipeline::Draw(const RenderQueue& queue, const PassContext& pass,
                                  std::vector<Triangle>* outDeferredBlend) const {
    RenderStats stats{};

    if (!pass.framebuffer || !pass.depthBuffer) {
        return stats;
    }

    OutputDebugStringA("RenderPipeline Draw: begin\n");

    Rasterizer rasterizer;
    rasterizer.SetTargets(pass.framebuffer, pass.depthBuffer);
    rasterizer.SetFrameContext(pass.frame);

    GeometryProcessor geometryProcessor;
    std::vector<Triangle> itemTriangles;
    std::vector<Triangle> opaqueMaskTriangles;
    std::vector<Triangle> blendTriangles;

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

    // 遍历渲染队列中每一个绘制项，先做几何阶段并按 alphaMode 分批（多线程并行）
    const int numItems = static_cast<int>(sortedItems.size());
    const int maxThreads = omp_get_max_threads();

    // 每线程独立的三角形收集列表，避免锁竞争
    std::vector<std::vector<Triangle>> perThreadOpaque(maxThreads);
    std::vector<std::vector<Triangle>> perThreadBlend(maxThreads);
    std::vector<uint64_t> perThreadBuilt(maxThreads, 0);

    auto buildStart = Clock::now();

    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        auto& localOpaque = perThreadOpaque[tid];
        auto& localBlend = perThreadBlend[tid];
        GeometryProcessor localGP;
        std::vector<Triangle> localItemTriangles;

        #pragma omp for schedule(dynamic, 1)
        for (int i = 0; i < numItems; ++i) {
            const DrawItem& item = sortedItems[static_cast<size_t>(i)];
            if (!item.mesh || !item.material) {
                continue;
            }

            localGP.BuildTriangles(
                *item.mesh,
                *item.material,
                item,
                item.modelMatrix,
                item.normalMatrix,
                pass.frame,
                localItemTriangles);

            perThreadBuilt[tid] += localGP.GetLastTriangleCount();

            if (localItemTriangles.empty()) {
                continue;
            }

            const int alphaMode = item.material ? item.material->alphaMode : 0;
            if (alphaMode == 2) {
                localBlend.insert(localBlend.end(), localItemTriangles.begin(), localItemTriangles.end());
            } else {
                localOpaque.insert(localOpaque.end(), localItemTriangles.begin(), localItemTriangles.end());
            }
        }
    }

    auto buildEnd = Clock::now();
    stats.buildMs += std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();

    // 合并各线程结果
    for (int t = 0; t < maxThreads; ++t) {
        stats.trianglesBuilt += perThreadBuilt[t];
        if (!perThreadOpaque[t].empty()) {
            opaqueMaskTriangles.insert(opaqueMaskTriangles.end(),
                std::make_move_iterator(perThreadOpaque[t].begin()),
                std::make_move_iterator(perThreadOpaque[t].end()));
        }
        if (!perThreadBlend[t].empty()) {
            blendTriangles.insert(blendTriangles.end(),
                std::make_move_iterator(perThreadBlend[t].begin()),
                std::make_move_iterator(perThreadBlend[t].end()));
        }
    }

    // 2. 光栅化处理阶段：按批次进行，避免每个 DrawItem 重复做 tile 预处理
    auto rasterizeBatch = [&](const std::vector<Triangle>& batch) {
        if (batch.empty()) {
            return;
        }
        auto rastStart = Clock::now();
        RasterStats rastStats = rasterizer.RasterizeTriangles(batch);
        auto rastEnd = Clock::now();
        stats.rastMs += std::chrono::duration<double, std::milli>(rastEnd - rastStart).count();
        stats.trianglesClipped += rastStats.trianglesClipped;
        stats.trianglesRaster += rastStats.trianglesRaster;
        stats.pixelsTested += rastStats.pixelsTested;
        stats.pixelsShaded += rastStats.pixelsShaded;
    };

    rasterizeBatch(opaqueMaskTriangles);

    if (outDeferredBlend) {
        // 透明三角形延迟到天空盒渲染之后
        *outDeferredBlend = std::move(blendTriangles);
    } else {
        rasterizeBatch(blendTriangles);
    }

    OutputDebugStringA("RenderPipeline Draw: end\n");

    return stats;
}

/**
 * @brief 串联起完整的渲染过程
 *
 * 渲染顺序：
 *   1. Prepare        — 清除缓冲区
 *   2. Draw           — 不透明 + 透明几何构建 & 不透明光栅化
 *                       （内部先光栅化不透明批次，返回透明批次）
 *   3. RenderSkybox   — 天空盒（仅填充深度 == 1.0 的远平面像素）
 *   4. 透明光栅化      — 在天空盒之上混合透明几何
 *   5. PostProcess     — 色调映射、FXAA 等
 *
 * 这样保证透明物体能正确混合在天空盒之上，而不被天空盒覆盖。
 */
RenderStats RenderPipeline::Render(const RenderQueue& queue, const PassContext& pass) const {
    Prepare(pass);

    // Draw 内部：先光栅化不透明批次，返回待渲染的透明三角形
    std::vector<Triangle> deferredBlend;
    RenderStats stats = Draw(queue, pass, &deferredBlend);

    // 天空盒填充未被不透明几何覆盖的像素
    RenderSkybox(pass);

    // 在天空盒之上光栅化透明几何
    if (!deferredBlend.empty()) {
        Rasterizer rasterizer;
        rasterizer.SetTargets(pass.framebuffer, pass.depthBuffer);
        rasterizer.SetFrameContext(pass.frame);

        using Clock = std::chrono::high_resolution_clock;
        auto rastStart = Clock::now();
        RasterStats rastStats = rasterizer.RasterizeTriangles(deferredBlend);
        auto rastEnd = Clock::now();
        stats.rastMs += std::chrono::duration<double, std::milli>(rastEnd - rastStart).count();
        stats.trianglesClipped += rastStats.trianglesClipped;
        stats.trianglesRaster += rastStats.trianglesRaster;
        stats.pixelsTested += rastStats.pixelsTested;
        stats.pixelsShaded += rastStats.pixelsShaded;
    }

    PostProcess(pass);
    return stats;
}

/**
 * @brief 天空盒渲染：对深度仍为远平面的像素，用环境贴图采样填充背景
 */
void RenderPipeline::RenderSkybox(const PassContext& pass) const {
    const EnvironmentMap* envMap = pass.frame.environmentMap;
    if (!envMap || !envMap->IsLoaded()) return;
    if (!pass.framebuffer || !pass.depthBuffer) return;

    OutputDebugStringA("RenderPipeline: rendering skybox\n");

    const int width = pass.framebuffer->GetWidth();
    const int height = pass.framebuffer->GetHeight();
    const double* depthData = pass.depthBuffer->Data();
    Vec3* linearPixels = pass.framebuffer->GetLinearPixelsWritable();

    // 逆 VP 矩阵：从 NDC 还原世界方向（行向量约定：VP = View * Projection）
    Mat4 vp = pass.frame.view * pass.frame.projection;
    Mat4 invVP = vp.Inverse();

    const Vec3& camPos = pass.frame.cameraPos;

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * width + x;
            // 仅处理深度 == 1.0（远平面）的像素，即无几何体覆盖的区域
            if (depthData[idx] < 0.9999) continue;

            // 屏幕坐标 → NDC
            double ndcX = (2.0 * (x + 0.5) / width) - 1.0;
            double ndcY = 1.0 - (2.0 * (y + 0.5) / height);

            // 在近平面和远平面上分别反投影得到世界空间点
            // NDC z=0 (近平面), z=1 (远平面)，DirectX 约定
            Vec4 nearClip{ndcX, ndcY, 0.0, 1.0};
            Vec4 farClip{ndcX, ndcY, 1.0, 1.0};
            Vec4 nearWorld = invVP.Multiply(nearClip);
            Vec4 farWorld = invVP.Multiply(farClip);

            if (std::abs(nearWorld.w) < 1e-12 || std::abs(farWorld.w) < 1e-12) continue;

            Vec3 nearPt{nearWorld.x / nearWorld.w, nearWorld.y / nearWorld.w, nearWorld.z / nearWorld.w};
            Vec3 farPt{farWorld.x / farWorld.w, farWorld.y / farWorld.w, farWorld.z / farWorld.w};

            Vec3 dir{farPt.x - nearPt.x, farPt.y - nearPt.y, farPt.z - nearPt.z};
            double len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 1e-12) {
                double inv = 1.0 / len;
                dir.x *= inv; dir.y *= inv; dir.z *= inv;
            }

            Vec3 skyColor = envMap->SampleDirection(dir);
            linearPixels[idx] = skyColor;
        }
    }
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
