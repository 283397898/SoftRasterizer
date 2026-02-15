#pragma once

#include <vector>

namespace SR {

/**
 * @brief HDR 浮点图像，用于存储环境贴图等高动态范围数据
 *        像素以 RGB 三通道 float 紧密排列
 */
struct HDRImage {
    std::vector<float> pixels;  ///< RGB 紧密排列，每像素 3 floats
    int width = 0;
    int height = 0;

    /** @brief 获取指定坐标的 RGB 值（不检查边界） */
    inline void GetPixel(int x, int y, float& r, float& g, float& b) const {
        const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3;
        r = pixels[idx];
        g = pixels[idx + 1];
        b = pixels[idx + 2];
    }

    /** @brief 设置指定坐标的 RGB 值（不检查边界） */
    inline void SetPixel(int x, int y, float r, float g, float b) {
        const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3;
        pixels[idx] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
    }

    /** @brief 检查图像是否有效 */
    bool IsValid() const { return width > 0 && height > 0 && pixels.size() == static_cast<size_t>(width) * height * 3; }
};

} // namespace SR
