#include "Render/RenderQueueBuilder.h"

#include "Render/GPUSceneRenderQueueBuilder.h"
#include "Runtime/GPUSceneBuilder.h"
#include "Scene/ObjectGroup.h"

namespace SR {

/**
 * @brief 实现从对象组构建渲染队列
 * 
 * 内部实现采用了中间转换方案：ObjectGroup -> GPUScene -> RenderQueue
 * 这样可以复用 GPUScene 的扁平化逻辑和 GPUSceneRenderQueueBuilder 的排序逻辑。
 * 
 * @param objects 源对象组
 * @param outQueue 输出渲染队列
 */
void RenderQueueBuilder::Build(const ObjectGroup& objects, RenderQueue& outQueue) const {
    GPUScene scene;
    GPUSceneBuilder sceneBuilder;
    
    // 1. 将对象组中的层级模型转换为扁平化的运行时场景结构
    sceneBuilder.BuildFromObjectGroup(objects, scene);

    // 2. 从运行时场景构建最终的渲染指令队列
    GPUSceneRenderQueueBuilder queueBuilder;
    queueBuilder.Build(scene, outQueue);
}

} // namespace SR
