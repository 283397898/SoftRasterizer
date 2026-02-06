#include "Core/Texture.h"

namespace SR {

/**
 * @brief 设置并初始化纹理像素数据
 */
void Texture::SetPixels(int width, int height, std::vector<uint32_t> pixels) {
    m_width = width;
    m_height = height;
    m_pixels = std::move(pixels);
}

/**
 * @brief 点采样给定坐标的像素
 * @param x 图像空间横坐标
 * @param y 图像空间纵坐标
 * @return 像素颜色 (BGRA8)
 */
uint32_t Texture::Sample(int x, int y) const {
    if (m_width <= 0 || m_height <= 0 || m_pixels.empty()) {
        return 0u;
    }
    // 实现边界 Clamp 行为
    x = (x < 0) ? 0 : (x >= m_width ? m_width - 1 : x);
    y = (y < 0) ? 0 : (y >= m_height ? m_height - 1 : y);
    return m_pixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
}

/**
 * @brief 获取宽度
 */
int Texture::GetWidth() const {
    return m_width;
}

/**
 * @brief 获取高度
 */
int Texture::GetHeight() const {
    return m_height;
}

} // namespace SR
