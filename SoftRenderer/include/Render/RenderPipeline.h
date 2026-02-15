#pragma once

#include <vector>
#include "Render/PassContext.h"
#include "Scene/RenderQueue.h"

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
 */
class RenderPipeline {
public:
    /** @brief 渲染前准备 (如缓冲区清除) */
    void Prepare(const PassContext& pass) const;
    /** @brief 执行渲染队列任务（可选延迟透明批次） */
    RenderStats Draw(const RenderQueue& queue, const PassContext& pass,
                     std::vector<Triangle>* outDeferredBlend = nullptr) const;
    /** @brief 天空盒渲染（环境贴图背景） */
    void RenderSkybox(const PassContext& pass) const;
    /** @brief 执行后处理流程 (FXAA, ToneMapping 等) */
    void PostProcess(const PassContext& pass) const;
    /** 
     * @brief 执行完整渲染流程 (Prepare + Draw + Skybox + PostProcess) 
     * @return 该帧的完整渲染统计信息
     */
    RenderStats Render(const RenderQueue& queue, const PassContext& pass) const;
};

} // namespace SR
