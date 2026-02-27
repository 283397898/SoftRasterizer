#include "Render/RenderPipeline.h"

#include "Pipeline/MaterialTable.h"
#include "Pipeline/OpaquePass.h"
#include "Pipeline/PassBuilder.h"
#include "Pipeline/Rasterizer.h"
#include "Utils/DebugLog.h"

#include <chrono>
#include <vector>

namespace SR {

/**
 * @brief 使用 Pass 系统执行渲染管线
 * @param passes 按依赖关系排序的 Pass 列表
 * @param context 渲染上下文
 * @return 汇总的渲染统计信息
 */
RenderStats RenderPipeline::ExecutePasses(std::vector<std::unique_ptr<RenderPass>>& passes,
                                          RenderContext& context) const {
    RenderStats totalStats;

    for (auto& pass : passes) {
        if (!pass) continue;

        // 检查 Pass 是否满足执行条件（如 SkyboxPass 要求环境贴图已加载）
        if (!pass->ShouldExecute(context)) {
            continue;
        }

        PassStats passStats = ExecutePass(*pass, context);

        // 累加各 Pass 的统计信息
        totalStats.buildMs += passStats.buildMs;
        totalStats.rastMs += passStats.rastMs;
        totalStats.trianglesBuilt += passStats.trianglesBuilt;
        totalStats.trianglesClipped += passStats.trianglesClipped;
        totalStats.trianglesRaster += passStats.trianglesRendered;
        totalStats.pixelsTested += passStats.pixelsTested;
        totalStats.pixelsShaded += passStats.pixelsShaded;
    }

    return totalStats;
}

/**
 * @brief 执行单个 Pass 并输出调试日志
 */
PassStats RenderPipeline::ExecutePass(RenderPass& pass, RenderContext& context) const {
    char debugMsg[256];
    snprintf(debugMsg, sizeof(debugMsg), "RenderPipeline: 执行 Pass '%s'\n", pass.GetName().c_str());
    SR_DEBUG_LOG(debugMsg);

    using Clock = std::chrono::high_resolution_clock;
    auto passStart = Clock::now();
    PassStats result = pass.Execute(context);
    auto passEnd = Clock::now();
    double passMs = std::chrono::duration<double, std::milli>(passEnd - passStart).count();

    char perfMsg[256];
    snprintf(perfMsg, sizeof(perfMsg),
        "[SR-PERF] Pass '%s': %.3fms (build=%.3f rast=%.3f tri=%llu pxTest=%llu pxShade=%llu)\n",
        pass.GetName().c_str(), passMs,
        result.buildMs, result.rastMs,
        static_cast<unsigned long long>(result.trianglesRendered),
        static_cast<unsigned long long>(result.pixelsTested),
        static_cast<unsigned long long>(result.pixelsShaded));
    SR_PERF_LOG(perfMsg);

    return result;
}

/**
 * @brief 传统路径：通过 PassContext 执行默认管线
 *
 * 创建帧级 MaterialTable 和延迟混合三角形缓冲，
 * 使用 DefaultPipeline::Create() 构建标准管线并执行。
 */
RenderStats RenderPipeline::Render(const RenderQueue& queue, const PassContext& pass) const {
    using Clock = std::chrono::high_resolution_clock;

    // 清除缓冲区（传统路径由管线自身负责清除）
    auto clearStart = Clock::now();
    if (pass.framebuffer && pass.depthBuffer) {
        pass.framebuffer->ClearLinear(Vec3{0.0, 0.0, 0.0});
        pass.depthBuffer->Clear(1.0);
    }
    auto clearEnd = Clock::now();

    // 创建帧级 MaterialTable（SOA 布局，生命周期覆盖整帧）
    auto pipelineStart = Clock::now();
    MaterialTable materialTable;
    std::vector<Triangle> deferredBlend;  // OpaquePass 产出，TransparentPass 消费

    // 构建默认管线并配置后处理参数
    auto passes = DefaultPipeline::Create();
    for (auto& passPtr : passes) {
        if (auto* post = dynamic_cast<PostProcessPass*>(passPtr.get())) {
            post->SetFXAAEnabled(pass.enableFXAA);
            post->SetToneMappingEnabled(pass.enableToneMap);
            post->SetExposure(pass.exposure);
        }
    }
    auto pipelineEnd = Clock::now();

    RenderContext context{};
    context.framebuffer = pass.framebuffer;
    context.depthBuffer = pass.depthBuffer;
    context.renderQueue = &queue;
    context.frameContext = &pass.frame;
    context.deferredBlendTriangles = &deferredBlend;
    context.materialTable = &materialTable;

    RenderStats stats = ExecutePasses(passes, context);

    double clearMs2 = std::chrono::duration<double, std::milli>(clearEnd - clearStart).count();
    double pipeCreateMs = std::chrono::duration<double, std::milli>(pipelineEnd - pipelineStart).count();
    char perfMsg[256];
    snprintf(perfMsg, sizeof(perfMsg),
        "[SR-PERF] Pipeline overhead: clear2=%.3fms pipeCreate=%.3fms\n",
        clearMs2, pipeCreateMs);
    SR_PERF_LOG(perfMsg);

    return stats;
}

} // namespace SR
