#pragma once

#include <cstdint>
#include <vector>

namespace SR {

/**
 * @brief 纹理类，存储像素数据并提供采样接口
 */
class Texture {
public:
    /** @brief 分配像素数据并设置尺寸 */
    void SetPixels(int width, int height, std::vector<uint32_t> pixels);
    /** @brief 对纹理进行采样 (当前为整数坐标点采样) */
    uint32_t Sample(int x, int y) const;

    /** @brief 获取纹理宽度 */
    int GetWidth() const;
    /** @brief 获取纹理高度 */
    int GetHeight() const;

private:
    int m_width = 0;                  ///< 纹理宽度
    int m_height = 0;                 ///< 纹理高度
    std::vector<uint32_t> m_pixels;  ///< 像素数据 (BGRA8 格式)
};

} // namespace SR
