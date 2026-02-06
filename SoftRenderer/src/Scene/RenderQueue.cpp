#include "Scene/RenderQueue.h"

#include <utility>

namespace SR {

/**
 * @brief 使用移动语义设置渲染队列中的绘制项
 */
void RenderQueue::SetItems(std::vector<DrawItem>&& items) {
    m_items = std::move(items);
}

/**
 * @brief 清空当前渲染队列
 */
void RenderQueue::Clear() {
    m_items.clear();
}

/**
 * @brief 获取渲染队列中所有的绘制项
 */
const std::vector<DrawItem>& RenderQueue::GetItems() const {
    return m_items;
}

} // namespace SR
