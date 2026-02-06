#pragma once

#include <cstdint>
#include <vector>

#include "Math/Vec3.h"

namespace SR {

struct Color {
    uint8_t b = 0;
    uint8_t g = 0;
    uint8_t r = 0;
    uint8_t a = 255;
};

/**
 * @brief 表示帧缓冲区的类，管理 SDR 和 HDR 像素缓冲
 */
class Framebuffer {
public:
    /** @brief 调整缓冲区大小 */
    void Resize(int width, int height);
    /** @brief 使用特定颜色清除 SDR 缓冲 */
    void Clear(const Color& color);
    /** @brief 使用特定颜色清除线性线性 HDR 缓冲 */
    void ClearLinear(const Vec3& color);
    /** @brief 设置特定位置的 SDR 像素颜色 */
    void SetPixel(int x, int y, const Color& color);
    /** @brief 设置特定位置的线性 HDR 像素颜色 */
    void SetPixelLinear(int x, int y, const Vec3& color);
    
    /**
     * @brief 设置线性 HDR 像素颜色 (不检查边界)
     */
    inline void SetPixelLinearUnchecked(int x, int y, const Vec3& color) {
        m_linearPixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)] = color;
    }
    
    /** @brief 获取内部可写的线性像素缓冲区指针 */
    inline Vec3* GetLinearPixelsWritable() { return m_linearPixels.data(); }
    
    /** @brief 执行 FXAA 抗锯齿算法 */
    void ApplyFXAA();
    /**
     * @brief 将线性 HDR 数据色调映射并转换为 sRGB 存入 SDR 缓冲
     * @param exposure 曝光值
     * @param dither 是否启用抖动 (暂未广泛应用)
     */
    void ResolveToSRGB(double exposure = 1.0, bool dither = false);

    /** @brief 获取导出的像素数组 (BGRA8) */
    const uint32_t* GetPixels() const;
    /** @brief 获取内部线性 HDR 数组 */
    const Vec3* GetLinearPixels() const;
    /** @brief 获取宽度 */
    int GetWidth() const;
    /** @brief 获取高度 */
    int GetHeight() const;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<uint32_t> m_pixels;
    std::vector<Vec3> m_linearPixels;
    std::vector<Vec3> m_fxaaTemp;
};

} // namespace SR
