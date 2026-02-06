#include "Core/DepthBuffer.h"

namespace SR {

/**
 * @brief 重新分配深度缓冲区大小并初始化为 1.0 (远裁剪面)
 */
void DepthBuffer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_depth.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 1.0);
}

/**
 * @brief 清除整个深度缓冲区
 */
void DepthBuffer::Clear(double depthValue) {
    for (auto& d : m_depth) {
        d = depthValue;
    }
}

/**
 * @brief 获取深度数据的原始数组，用于直接内存读写
 */
double* DepthBuffer::Data() {
    return m_depth.empty() ? nullptr : m_depth.data();
}

/**
 * @brief 获取深度数据的原始数组 (只读版)
 */
const double* DepthBuffer::Data() const {
    return m_depth.empty() ? nullptr : m_depth.data();
}

/**
 * @brief 获取当前宽度
 */
int DepthBuffer::GetWidth() const {
    return m_width;
}

/**
 * @brief 获取当前高度
 */
int DepthBuffer::GetHeight() const {
    return m_height;
}

} // namespace SR
