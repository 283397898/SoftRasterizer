#pragma once

#include "Scene/RenderQueue.h"

namespace SR {

class ObjectGroup;

/**
 * @brief 传统场景对象组到渲染队列的构建器
 * 
 * 将 ObjectGroup 中的模型及其层级结构转换为扁平化的绘制项列表
 */
class RenderQueueBuilder {
public:
    /** @brief 从 ObjectGroup 构建用于渲染的 DrawItem 列表 */
    void Build(const ObjectGroup& objects, RenderQueue& outQueue) const;
};

} // namespace SR
