#pragma once

#include "Scene/RenderQueue.h"

namespace SR {

class GPUScene;

/**
 * @brief GPUScene 到渲染队列的构建器
 * 
 * 将扁平化的 GPUScene 数据转换为渲染管线可执行的 RenderQueue
 */
class GPUSceneRenderQueueBuilder {
public:
    /** @brief 从 GPUScene 提取信息并填充到渲染队列中 */
    void Build(const GPUScene& scene, RenderQueue& outQueue) const;
};

} // namespace SR
