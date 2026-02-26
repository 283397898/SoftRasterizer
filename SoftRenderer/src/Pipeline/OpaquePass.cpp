#include "Pipeline/OpaquePass.h"

#include "Pipeline/EnvironmentMap.h"
#include "Pipeline/FrameContext.h"
#include "Pipeline/GeometryProcessor.h"
#include "Pipeline/MaterialTable.h"
#include "Pipeline/Rasterizer.h"
#include "Core/Framebuffer.h"
#include "Core/DepthBuffer.h"
#include "Scene/RenderQueue.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <omp.h>

namespace SR {

PassStats OpaquePass::Execute(RenderContext& context) {
    PassStats stats;

    if (!context.renderQueue || !context.framebuffer || !context.depthBuffer || !context.frameContext || !context.materialTable) {
        return stats;
    }

    // 将帧级材质表注入 FrameContext，供 Rasterizer 中 FragmentShader 访问
    FrameContext frameWithMaterials = *context.frameContext;
    frameWithMaterials.materialTable = context.materialTable;

    Rasterizer rasterizer;
    rasterizer.SetTargets(context.framebuffer, context.depthBuffer);
    rasterizer.SetFrameContext(frameWithMaterials);

    std::vector<Triangle> opaqueMaskTriangles;
    std::vector<Triangle> blendTriangles;
    std::vector<DrawItem> sortedItems = context.renderQueue->GetItems();

    // 排序键：取模型矩阵的平移列到相机的距离平方（避免开方以节省时间）
    const Vec3 cameraPos = frameWithMaterials.cameraPos;
    auto getItemSortKey = [&cameraPos](const DrawItem& item) {
        Vec3 pos{item.modelMatrix.m[3][0], item.modelMatrix.m[3][1], item.modelMatrix.m[3][2]};
        Vec3 d = pos - cameraPos;
        return d.x * d.x + d.y * d.y + d.z * d.z;
    };

    // 排序策略：
    //   1. 不透明/Mask 物体先于半透明物体（alphaMode 枚举值：Opaque=0 < Mask=1 < Blend=2）
    //   2. 半透明物体按从远到近排序（正确的 Alpha 混合需后绘远处）
    //   3. 同材质、同网格的物体合批（减少状态切换，提升 CPU 局部性）
    std::stable_sort(sortedItems.begin(), sortedItems.end(), [&getItemSortKey](const DrawItem& a, const DrawItem& b) {
        GLTFAlphaMode alphaModeA = a.material ? a.material->alphaMode : GLTFAlphaMode::Opaque;
        GLTFAlphaMode alphaModeB = b.material ? b.material->alphaMode : GLTFAlphaMode::Opaque;
        if (alphaModeA != alphaModeB) {
            return alphaModeA < alphaModeB;  // 不透明排前
        }
        if (alphaModeA == GLTFAlphaMode::Blend) {
            return getItemSortKey(a) > getItemSortKey(b);  // 半透明从远到近
        }
        // 不透明物体按材质/网格分组，减少状态切换
        if (a.material != b.material) {
            return a.material < b.material;
        }
        return a.mesh < b.mesh;
    });

    const int numItems = static_cast<int>(sortedItems.size());
    const int maxThreads = omp_get_max_threads();

    // 单线程预注册：为每个 DrawItem 注册材质到 MaterialTable，获取预计算的 MaterialHandle
    std::vector<MaterialHandle> materialHandles(static_cast<size_t>(numItems), InvalidMaterialHandle);
    for (int i = 0; i < numItems; ++i) {
        const DrawItem& item = sortedItems[static_cast<size_t>(i)];
        if (!item.mesh || !item.material) {
            continue;
        }
        MaterialParams params = BuildMaterialParams(*item.material, item);
        materialHandles[static_cast<size_t>(i)] = context.materialTable->AddMaterial(params);
    }

    std::vector<std::vector<Triangle>> perThreadOpaque(maxThreads);
    std::vector<std::vector<Triangle>> perThreadBlend(maxThreads);
    std::vector<uint64_t> perThreadBuilt(maxThreads, 0);

    using Clock = std::chrono::high_resolution_clock;
    auto buildStart = Clock::now();

    // 并行几何处理：每个线程使用独立的 GeometryProcessor 和三角形缓冲
    // schedule(dynamic, 1) 适合不同 DrawItem 耗时差异较大的场景
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
                item,
                item.modelMatrix,
                item.normalMatrix,
                frameWithMaterials,
                materialHandles[static_cast<size_t>(i)],
                localItemTriangles);

            perThreadBuilt[tid] += localGP.GetLastTriangleCount();
            if (localItemTriangles.empty()) {
                continue;
            }

            GLTFAlphaMode alphaMode = item.material ? item.material->alphaMode : GLTFAlphaMode::Opaque;
            if (alphaMode == GLTFAlphaMode::Blend) {
                localBlend.insert(localBlend.end(), localItemTriangles.begin(), localItemTriangles.end());
            } else {
                localOpaque.insert(localOpaque.end(), localItemTriangles.begin(), localItemTriangles.end());
            }
        }
    }
    auto buildEnd = Clock::now();
    stats.buildMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();

    // 合并各线程的三角形缓冲（move 语义，避免拷贝）
    for (int t = 0; t < maxThreads; ++t) {
        stats.trianglesBuilt += perThreadBuilt[t];
        opaqueMaskTriangles.insert(opaqueMaskTriangles.end(),
            std::make_move_iterator(perThreadOpaque[t].begin()),
            std::make_move_iterator(perThreadOpaque[t].end()));
        blendTriangles.insert(blendTriangles.end(),
            std::make_move_iterator(perThreadBlend[t].begin()),
            std::make_move_iterator(perThreadBlend[t].end()));
    }

    // 光栅化不透明/Mask 三角形（启用 Early-Z）
    if (!opaqueMaskTriangles.empty()) {
        auto rastStart = Clock::now();
        RasterStats rastStats = rasterizer.RasterizeTriangles(opaqueMaskTriangles);
        auto rastEnd = Clock::now();
        stats.rastMs += std::chrono::duration<double, std::milli>(rastEnd - rastStart).count();
        stats.trianglesClipped += rastStats.trianglesClipped;
        stats.trianglesRendered += rastStats.trianglesRaster;
        stats.pixelsTested += rastStats.pixelsTested;
        stats.pixelsShaded += rastStats.pixelsShaded;
    }

    // 将半透明三角形传递给 TransparentPass（通过 RenderContext 共享）
    if (context.deferredBlendTriangles) {
        *context.deferredBlendTriangles = std::move(blendTriangles);
    }

    return stats;
}

PassStats TransparentPass::Execute(RenderContext& context) {
    PassStats stats;
    if (!context.deferredBlendTriangles || context.deferredBlendTriangles->empty()) {
        return stats;
    }
    if (!context.framebuffer || !context.depthBuffer || !context.frameContext || !context.materialTable) {
        return stats;
    }

    FrameContext frameWithMaterials = *context.frameContext;
    frameWithMaterials.materialTable = context.materialTable;

    Rasterizer rasterizer;
    rasterizer.SetTargets(context.framebuffer, context.depthBuffer);
    rasterizer.SetFrameContext(frameWithMaterials);

    RasterStats rastStats = rasterizer.RasterizeTriangles(*context.deferredBlendTriangles);

    stats.trianglesRendered = rastStats.trianglesRaster;
    stats.trianglesClipped = rastStats.trianglesClipped;
    stats.pixelsTested = rastStats.pixelsTested;
    stats.pixelsShaded = rastStats.pixelsShaded;

    return stats;
}

// SkyboxPass 仅在 EnvironmentMap 已成功加载时执行
bool SkyboxPass::ShouldExecute(const RenderContext& context) const {
    if (!context.frameContext) return false;
    const FrameContext* fc = context.frameContext;
    return fc->environmentMap != nullptr && fc->environmentMap->IsLoaded();
}

PassStats SkyboxPass::Execute(RenderContext& context) {
    PassStats stats;
    if (!ShouldExecute(context) || !context.framebuffer || !context.depthBuffer || !context.frameContext) {
        return stats;
    }

    const FrameContext& frame = *context.frameContext;
    const EnvironmentMap* envMap = frame.environmentMap;

    const int width = context.framebuffer->GetWidth();
    const int height = context.framebuffer->GetHeight();
    const double* depthData = context.depthBuffer->Data();
    Vec3* linearPixels = context.framebuffer->GetLinearPixelsWritable();
    if (!depthData || !linearPixels) {
        return stats;
    }

    // 计算 VP 矩阵的逆，用于将屏幕像素反投影到世界空间方向
    Mat4 vp = frame.view * frame.projection;
    Mat4 invVP = vp.Inverse();

    // 并行遍历所有像素：只处理深度为 1.0（远平面）的像素（未被几何覆盖）
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            if (depthData[idx] < 0.9999) continue;  // 已有不透明几何，跳过

            // 将像素中心转换为 NDC 坐标
            double ndcX = (2.0 * (x + 0.5) / width) - 1.0;
            double ndcY = 1.0 - (2.0 * (y + 0.5) / height);

            // 反投影近平面和远平面点到世界空间，求射线方向
            Vec4 nearClip{ndcX, ndcY, 0.0, 1.0};
            Vec4 farClip{ndcX, ndcY, 1.0, 1.0};
            Vec4 nearWorld = invVP.Multiply(nearClip);
            Vec4 farWorld = invVP.Multiply(farClip);
            if (std::abs(nearWorld.w) < 1e-12 || std::abs(farWorld.w) < 1e-12) continue;

            Vec3 nearPt{nearWorld.x / nearWorld.w, nearWorld.y / nearWorld.w, nearWorld.z / nearWorld.w};
            Vec3 farPt{farWorld.x / farWorld.w, farWorld.y / farWorld.w, farWorld.z / farWorld.w};
            Vec3 dir{farPt.x - nearPt.x, farPt.y - nearPt.y, farPt.z - nearPt.z};
            // 归一化方向向量（用于采样等距柱形投影环境贴图）
            double len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 1e-12) {
                double inv = 1.0 / len;
                dir.x *= inv; dir.y *= inv; dir.z *= inv;
            }
            linearPixels[idx] = envMap->SampleDirection(dir);
        }
    }

    return stats;
}

PassStats PostProcessPass::Execute(RenderContext& context) {
    PassStats stats;

    if (!context.framebuffer) {
        return stats;
    }

    // 执行 FXAA 抗锯齿
    if (m_fxaaEnabled) {
        context.framebuffer->ApplyFXAA();
    }

    // 执行色调映射和 sRGB 转换
    if (m_toneMappingEnabled) {
        context.framebuffer->ResolveToSRGB(m_exposure, false);
    }

    return stats;
}

} // namespace SR
