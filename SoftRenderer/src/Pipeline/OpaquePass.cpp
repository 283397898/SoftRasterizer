#include "Pipeline/OpaquePass.h"
#include "Core/Framebuffer.h"
#include "Core/DepthBuffer.h"
#include "Scene/RenderQueue.h"
#include "Pipeline/FrameContext.h"
#include "Pipeline/EnvironmentMap.h"

namespace SR {

/**
 * OpaquePass 实现
 *
 * 当前为 Pass-through 实现，实际的几何处理和光栅化由 RenderPipeline::DrawWithMaterialTable 执行。
 * 这种设计允许：
 * 1. Pass 系统提供可配置的渲染流程
 * 2. 保持与现有 RenderPipeline 代码的兼容性
 * 3. 未来可独立实现不透明物体筛选逻辑
 *
 * 完整的不透明物体渲染流程：
 * - RenderPipeline::Render() 调用 DrawWithMaterialTable()
 * - 几何处理器构建三角形并按 alphaMode 分类
 * - 不透明/遮罩三角形立即光栅化
 * - 透明三角形延迟到天空盒之后
 */
PassStats OpaquePass::Execute(RenderContext& context) {
    PassStats stats;

    if (!context.renderQueue || !context.framebuffer || !context.depthBuffer) {
        return stats;
    }

    // Note: 实际的不透明物体渲染由 RenderPipeline::DrawWithMaterialTable 处理
    // 此 Pass 提供管线配置点和未来扩展接口

    return stats;
}

/**
 * TransparentPass 实现
 *
 * 渲染延迟的透明三角形列表。
 * 这些三角形在 OpaquePass 和 SkyboxPass 之后渲染，确保正确的混合顺序。
 *
 * 透明物体使用从后到前的排序 (在 RenderPipeline 中完成)，
 * 确保多个透明物体之间的正确混合。
 */
PassStats TransparentPass::Execute(RenderContext& context) {
    PassStats stats;

    if (!m_deferredTriangles || m_deferredTriangles->empty()) {
        return stats;
    }

    if (!context.framebuffer || !context.depthBuffer || !context.frameContext) {
        return stats;
    }

    Rasterizer rasterizer;
    rasterizer.SetTargets(context.framebuffer, context.depthBuffer);
    rasterizer.SetFrameContext(*context.frameContext);

    RasterStats rastStats = rasterizer.RasterizeTriangles(*m_deferredTriangles);

    stats.trianglesRendered = rastStats.trianglesRaster;
    stats.pixelsShaded = rastStats.pixelsShaded;

    return stats;
}

/**
 * SkyboxPass 条件判断
 *
 * 仅当环境贴图存在且已加载时执行天空盒渲染。
 */
bool SkyboxPass::ShouldExecute(const RenderContext& context) const {
    if (!context.frameContext) return false;
    const FrameContext* fc = context.frameContext;
    return fc->environmentMap != nullptr && fc->environmentMap->IsLoaded();
}

/**
 * SkyboxPass 实现
 *
 * 当前为 Pass-through 实现，实际的天空盒渲染由 RenderPipeline::RenderSkybox 执行。
 *
 * 天空盒渲染特点：
 * - 仅填充深度为远平面 (depth == 1.0) 的像素
 * - 使用环境贴图的辐射度数据
 * - 在不透明物体之后、透明物体之前渲染
 */
PassStats SkyboxPass::Execute(RenderContext& context) {
    PassStats stats;

    if (!ShouldExecute(context)) {
        return stats;
    }

    // Note: 实际的天空盒渲染由 RenderPipeline::RenderSkybox 处理
    // 此 Pass 提供管线配置点和条件执行检查

    return stats;
}

/**
 * PostProcessPass 实现
 *
 * 执行后处理效果：
 * 1. FXAA (快速近似抗锯齿) - 可选
 * 2. Tone Mapping (HDR 到 SDR 转换) - 可选
 * 3. sRGB 转换 - 与 tone mapping 一起执行
 *
 * 这是渲染管线的最后一个 Pass。
 */
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
