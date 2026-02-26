#pragma once

/**
 * @file TextureSampler.h
 * @brief 纹理采样工具函数（内联），供 Rasterizer 和 FragmentShader 共用。
 *
 * 包含：
 *   - WrapCoord         — 纹理坐标回绕（ClampToEdge / MirroredRepeat / Repeat）
 *   - UseLinearFilter   — 根据采样器配置判断是否使用双线性过滤
 *   - SRGB8ToLinear     — sRGB 8-bit 到线性空间转换
 *   - SampleImageNearest  — 最近邻采样
 *   - SampleImageBilinear — 双线性采样
 */

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Asset/GLTFTypes.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"

namespace SR {

/**
 * @brief 采样结果：RGB 线性颜色 + Alpha 通道
 */
struct SampledColor {
    Vec3 rgb{1.0, 1.0, 1.0}; ///< 线性空间 RGB 颜色
    double a = 1.0;           ///< Alpha 通道 [0, 1]
};

/**
 * @brief 根据回绕模式处理纹理坐标
 * @param v    原始纹理坐标（可超出 [0,1]）
 * @param mode 回绕模式（Repeat / ClampToEdge / MirroredRepeat）
 * @return 归一化到 [0, 1] 的坐标
 */
inline double WrapCoord(double v, GLTFWrapMode mode) {
    if (mode == GLTFWrapMode::ClampToEdge) {
        return std::max(0.0, std::min(1.0, v));
    }
    if (mode == GLTFWrapMode::MirroredRepeat) {
        double w = std::fmod(v, 2.0);
        if (w < 0.0) w += 2.0;
        return (w > 1.0) ? (2.0 - w) : w;
    }
    double w = std::fmod(v, 1.0);
    if (w < 0.0) w += 1.0;
    return w;
}

/**
 * @brief 判断是否应使用双线性（线性）过滤
 * @param sampler 采样器指针，为 nullptr 时默认使用最近邻
 * @return true 表示使用双线性过滤，false 使用最近邻
 */
inline bool UseLinearFilter(const GLTFSampler* sampler) {
    if (!sampler) {
        return false;
    }
    GLTFFilterMode minFilter = sampler->minFilter;
    GLTFFilterMode magFilter = sampler->magFilter;
    if (magFilter == GLTFFilterMode::Linear) {
        return true;
    }
    return minFilter == GLTFFilterMode::Linear ||
           minFilter == GLTFFilterMode::NearestMipmapNearest ||
           minFilter == GLTFFilterMode::LinearMipmapNearest ||
           minFilter == GLTFFilterMode::NearestMipmapLinear ||
           minFilter == GLTFFilterMode::LinearMipmapLinear;
}

/**
 * @brief 将 sRGB 8-bit 像素值转换为线性亮度
 * @param v sRGB 编码的 8-bit 值 [0, 255]
 * @return 线性亮度 [0.0, 1.0]
 */
inline double SRGB8ToLinear(uint8_t v) {
    double x = static_cast<double>(v) / 255.0;
    if (x <= 0.04045) {
        return x / 12.92;
    }
    return std::pow((x + 0.055) / 1.055, 2.4);
}

/**
 * @brief 最近邻纹理采样
 * @param image   glTF 图像数据（RGBA8）
 * @param sampler 采样器（可为 nullptr，则使用 Repeat 模式）
 * @param uv      纹理坐标
 * @param srgb    是否强制 sRGB 解码（image.isSRGB 或此标志为 true 时解码）
 * @return 线性空间的采样颜色
 */
inline SampledColor SampleImageNearest(const GLTFImage& image, const GLTFSampler* sampler, const Vec2& uv, bool srgb) {
    GLTFWrapMode wrapS = sampler ? sampler->wrapS : GLTFWrapMode::Repeat;
    GLTFWrapMode wrapT = sampler ? sampler->wrapT : GLTFWrapMode::Repeat;
    double u = WrapCoord(uv.x, wrapS);
    double v = WrapCoord(uv.y, wrapT);

    int x = static_cast<int>(u * image.width);
    int y = static_cast<int>(v * image.height);
    x = std::max(0, std::min(x, image.width - 1));
    y = std::max(0, std::min(y, image.height - 1));

    size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
    if (index + 3 >= image.pixels.size()) {
        return {};
    }

    const uint8_t* p = image.pixels.data() + index;
    bool useSrgb = srgb || image.isSRGB;
    double r = useSrgb ? SRGB8ToLinear(p[0]) : static_cast<double>(p[0]) / 255.0;
    double g = useSrgb ? SRGB8ToLinear(p[1]) : static_cast<double>(p[1]) / 255.0;
    double b = useSrgb ? SRGB8ToLinear(p[2]) : static_cast<double>(p[2]) / 255.0;
    double a = static_cast<double>(p[3]) / 255.0;
    return {Vec3{r, g, b}, a};
}

/**
 * @brief 双线性纹理采样（对四个相邻纹素进行双线性插值）
 * @param image   glTF 图像数据（RGBA8）
 * @param sampler 采样器（可为 nullptr，则使用 Repeat 模式）
 * @param uv      纹理坐标
 * @param srgb    是否强制 sRGB 解码
 * @return 线性空间的采样颜色
 */
inline SampledColor SampleImageBilinear(const GLTFImage& image, const GLTFSampler* sampler, const Vec2& uv, bool srgb) {
    GLTFWrapMode wrapS = sampler ? sampler->wrapS : GLTFWrapMode::Repeat;
    GLTFWrapMode wrapT = sampler ? sampler->wrapT : GLTFWrapMode::Repeat;
    double u = WrapCoord(uv.x, wrapS);
    double v = WrapCoord(uv.y, wrapT);

    double fx = u * (image.width - 1);
    double fy = v * (image.height - 1);
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, image.width - 1);
    int y1 = std::min(y0 + 1, image.height - 1);
    double tx = fx - x0;
    double ty = fy - y0;

    auto samplePixel = [&](int x, int y) -> SampledColor {
        size_t index = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4;
        if (index + 3 >= image.pixels.size()) {
            return {};
        }
        const uint8_t* p = image.pixels.data() + index;
        bool useSrgb = srgb || image.isSRGB;
        double r = useSrgb ? SRGB8ToLinear(p[0]) : static_cast<double>(p[0]) / 255.0;
        double g = useSrgb ? SRGB8ToLinear(p[1]) : static_cast<double>(p[1]) / 255.0;
        double b = useSrgb ? SRGB8ToLinear(p[2]) : static_cast<double>(p[2]) / 255.0;
        double a = static_cast<double>(p[3]) / 255.0;
        return {Vec3{r, g, b}, a};
    };

    SampledColor c00 = samplePixel(x0, y0);
    SampledColor c10 = samplePixel(x1, y0);
    SampledColor c01 = samplePixel(x0, y1);
    SampledColor c11 = samplePixel(x1, y1);

    Vec3 c0 = c00.rgb * (1.0 - tx) + c10.rgb * tx;
    Vec3 c1 = c01.rgb * (1.0 - tx) + c11.rgb * tx;
    Vec3 rgb = c0 * (1.0 - ty) + c1 * ty;
    double a0 = c00.a * (1.0 - tx) + c10.a * tx;
    double a1 = c01.a * (1.0 - tx) + c11.a * tx;
    return {rgb, a0 * (1.0 - ty) + a1 * ty};
}

} // namespace SR
