#pragma once

#include <vector>
#include <memory>
#include "Render/PassContext.h"
#include "Scene/RenderQueue.h"
#include "Pipeline/RenderPass.h"

namespace SR {

struct Triangle;

/**
 * @brief 渲染管线统计数据，汇总各个阶段的性能和指标
 */
struct RenderStats {
    double buildMs = 0.0;           ///< 几何构建耗时 (毫秒)
    double rastMs = 0.0;            ///< 光栅化耗时 (毫秒)
    uint64_t trianglesBuilt = 0;    ///< 构建出的总三角形数
    uint64_t trianglesClipped = 0;  ///< 裁剪后的总三角形数
    uint64_t trianglesRaster = 0;   ///< 进入光栅化的总三角形数
    uint64_t pixelsTested = 0;      ///< 深度测试总像素数
    uint64_t pixelsShaded = 0;      ///< 最终着色的总像素数
};

/**
 * @brief 渲染管线类，协调几何处理、光栅化和后处理流程
 *
 * 支持两种渲染模式：
 * 1. 传统模式：使用 Render() 方法执行固定管线流程
 * 2. Pass 模式：使用 ExecutePasses() 执行可配置的 Pass 管线
 */
class RenderPipeline {
public:
    /**
     * @brief 执行完整渲染流程
     * @return 该帧的完整渲染统计信息
     */
    RenderStats Render(const RenderQueue& queue, const PassContext& pass) const;

    /**
     * @brief 使用 Pass 系统执行渲染管线
     * @param passes 按依赖关系排序的 Pass 列表
     * @param context 渲染上下文
     * @return 汇总的渲染统计信息
     */
    RenderStats ExecutePasses(std::vector<std::unique_ptr<RenderPass>>& passes,
                              RenderContext& context) const;

    /**
     * @brief 执行单个 Pass
     * @param pass Pass 实例
     * @param context 渲染上下文
     * @return Pass 统计信息
     */
    PassStats ExecutePass(RenderPass& pass, RenderContext& context) const;
};

} // namespace SR
