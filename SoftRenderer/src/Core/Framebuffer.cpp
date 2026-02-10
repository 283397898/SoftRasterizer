#include "Core/Framebuffer.h"

#include <cmath>
#include <omp.h>

namespace SR {

/**
 * @brief 调整缓冲区大小
 */
void Framebuffer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    m_linearPixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), Vec3{0.0, 0.0, 0.0});
    m_fxaaTemp.assign(static_cast<size_t>(width) * static_cast<size_t>(height), Vec3{0.0, 0.0, 0.0});
}

/**
 * @brief 清除 SDR 像素缓冲
 */
void Framebuffer::Clear(const Color& color) {
    uint32_t packed = static_cast<uint32_t>(color.b)
        | (static_cast<uint32_t>(color.g) << 8)
        | (static_cast<uint32_t>(color.r) << 16)
        | (static_cast<uint32_t>(color.a) << 24);

    for (auto& pixel : m_pixels) {
        pixel = packed;
    }
}

/**
 * @brief 并行清除线性线性 HDR 缓冲
 */
void Framebuffer::ClearLinear(const Vec3& color) {
    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(m_linearPixels.size()); ++i) {
        m_linearPixels[static_cast<size_t>(i)] = color;
    }
}

/**
 * @brief 设置特定坐标的 SDR 像素
 */
void Framebuffer::SetPixel(int x, int y, const Color& color) {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) {
        return;
    }

    uint32_t packed = static_cast<uint32_t>(color.b)
        | (static_cast<uint32_t>(color.g) << 8)
        | (static_cast<uint32_t>(color.r) << 16)
        | (static_cast<uint32_t>(color.a) << 24);

    m_pixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)] = packed;
}

/**
 * @brief 设置特定坐标的线性线性 HDR 像素
 */
void Framebuffer::SetPixelLinear(int x, int y, const Vec3& color) {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) {
        return;
    }

    m_linearPixels[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)] = color;
}

/**
 * @brief 执行 FXAA 抗锯齿算法实现 (快速近似抗锯齿)
 */
void Framebuffer::ApplyFXAA() {
    if (m_linearPixels.empty()) {
        return;
    }

    if (m_fxaaTemp.size() != m_linearPixels.size()) {
        m_fxaaTemp.assign(m_linearPixels.size(), Vec3{0.0, 0.0, 0.0});
    }

    auto clampIndex = [this](int x, int y) {
        x = (x < 0) ? 0 : (x >= m_width ? m_width - 1 : x);
        y = (y < 0) ? 0 : (y >= m_height ? m_height - 1 : y);
        return static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
    };

    auto luma = [](const Vec3& c) {
        return 0.299 * c.x + 0.587 * c.y + 0.114 * c.z;
    };

    const double reduceMin = 1.0 / 128.0;
    const double reduceMul = 1.0 / 8.0;
    const double spanMax = 8.0;
    const double edgeThresholdMin = 1.0 / 24.0;
    const double edgeThreshold = 1.0 / 12.0;

    #pragma omp parallel for
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const Vec3& cM = m_linearPixels[clampIndex(x, y)];
            const Vec3& cNW = m_linearPixels[clampIndex(x - 1, y - 1)];
            const Vec3& cNE = m_linearPixels[clampIndex(x + 1, y - 1)];
            const Vec3& cSW = m_linearPixels[clampIndex(x - 1, y + 1)];
            const Vec3& cSE = m_linearPixels[clampIndex(x + 1, y + 1)];

            double lumaM = luma(cM);
            double lumaNW = luma(cNW);
            double lumaNE = luma(cNE);
            double lumaSW = luma(cSW);
            double lumaSE = luma(cSE);

            double lumaMin = std::min(lumaM, std::min(std::min(lumaNW, lumaNE), std::min(lumaSW, lumaSE)));
            double lumaMax = std::max(lumaM, std::max(std::max(lumaNW, lumaNE), std::max(lumaSW, lumaSE)));

            Vec3 result = cM;

            double lumaRange = lumaMax - lumaMin;
            if (lumaRange >= std::max(edgeThresholdMin, lumaMax * edgeThreshold)) {
                double dirX = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
                double dirY =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

                double dirReduce = std::max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * reduceMul), reduceMin);
                double rcpDirMin = 1.0 / (std::min(std::abs(dirX), std::abs(dirY)) + dirReduce);

                dirX = std::max(-spanMax, std::min(spanMax, dirX * rcpDirMin));
                dirY = std::max(-spanMax, std::min(spanMax, dirY * rcpDirMin));

                auto sample = [&](int ox, int oy) {
                    return m_linearPixels[clampIndex(x + ox, y + oy)];
                };

                int ox1 = static_cast<int>(dirX * (1.0 / 3.0));
                int oy1 = static_cast<int>(dirY * (1.0 / 3.0));
                int ox2 = static_cast<int>(dirX * (2.0 / 3.0));
                int oy2 = static_cast<int>(dirY * (2.0 / 3.0));

                Vec3 rgbA = (sample(ox1, oy1) + sample(ox2, oy2)) * 0.5;

                int ox3 = static_cast<int>(dirX * 1.0);
                int oy3 = static_cast<int>(dirY * 1.0);
                Vec3 rgbB = (rgbA * 0.5) + ((sample(0, 0) + sample(ox3, oy3)) * 0.25);

                double lumaB = luma(rgbB);
                result = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
            }

            m_fxaaTemp[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)] = result;
        }
    }

    m_linearPixels.swap(m_fxaaTemp);
}

// Precomputed linear-to-sRGB LUT (1024 entries for high precision)
static uint8_t kLinearToSRGBLUT[1024];
static bool kLinearToSRGBTableInitialized = false;

/**
 * @brief 初始化线性到 sRGB 的查找表 (LUT)
 */
static void InitLinearToSRGBTable() {
    if (kLinearToSRGBTableInitialized) return;
    for (int i = 0; i < 1024; ++i) {
        double v = i / 1023.0;
        double srgb = std::pow(v, 1.0 / 2.2);
        kLinearToSRGBLUT[i] = static_cast<uint8_t>(srgb * 255.0 + 0.5);
    }
    kLinearToSRGBTableInitialized = true;
}

/**
 * @brief 快速线性到 sRGB 转换
 */
inline uint8_t LinearToSRGBFast(double v) {
    if (v <= 0.0) return 0;
    if (v >= 1.0) return 255;
    int idx = static_cast<int>(v * 1023.0 + 0.5);
    return kLinearToSRGBLUT[idx];
}

/**
 * @brief ACES Filmic 色调映射
 *
 * 将 HDR 线性值 [0, ∞) 映射到 [0, 1)。
 * 保持色彩比例，避免逐通道裁剪导致的色移。
 * 参考：Stephen Hill, ACES Filmic Tone Mapping Curve (Krzysztof Narkowicz fit)
 */
inline double ACESToneMap(double x) {
    if (x <= 0.0) return 0.0;
    constexpr double a = 2.51;
    constexpr double b = 0.03;
    constexpr double c = 2.43;
    constexpr double d = 0.59;
    constexpr double e = 0.14;
    double mapped = (x * (a * x + b)) / (x * (c * x + d) + e);
    return (mapped < 0.0) ? 0.0 : ((mapped > 1.0) ? 1.0 : mapped);
}

/**
 * @brief 执行色调映射和 sRGB 空间转换并存入对应像素缓冲区
 */
void Framebuffer::ResolveToSRGB(double exposure, bool dither) {
    if (m_linearPixels.empty() || m_pixels.empty()) {
        return;
    }

    InitLinearToSRGBTable();

    const int totalPixels = m_width * m_height;
    const Vec3* srcPixels = m_linearPixels.data();
    uint32_t* dstPixels = m_pixels.data();

    // Precompute dither pattern
    static const double kDitherPattern[4] = {-0.375 / 255.0, -0.125 / 255.0, 0.125 / 255.0, 0.375 / 255.0};

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < m_height; ++y) {
        const int rowBase = y * m_width;
        const int yPattern = (y & 1) << 1;
        
        for (int x = 0; x < m_width; ++x) {
            const int idx = rowBase + x;
            const Vec3& c = srcPixels[idx];
            
            // Apply exposure then ACES tone mapping (preserves color ratios)
            double r = ACESToneMap(c.x * exposure);
            double g = ACESToneMap(c.y * exposure);
            double b = ACESToneMap(c.z * exposure);

            if (dither) {
                const double t = kDitherPattern[yPattern | (x & 1)];
                r += t;
                g += t;
                b += t;
            }

            uint8_t rb = LinearToSRGBFast(b);
            uint8_t rg = LinearToSRGBFast(g);
            uint8_t rr = LinearToSRGBFast(r);
            
            dstPixels[idx] = static_cast<uint32_t>(rb)
                | (static_cast<uint32_t>(rg) << 8)
                | (static_cast<uint32_t>(rr) << 16)
                | 0xFF000000u;
        }
    }
}

/**
 * @brief 获取导出的像素数组 (BGRA8)
 */
const uint32_t* Framebuffer::GetPixels() const {
    return m_pixels.empty() ? nullptr : m_pixels.data();
}

/**
 * @brief 获取内部线性 HDR 数组
 */
const Vec3* Framebuffer::GetLinearPixels() const {
    return m_linearPixels.empty() ? nullptr : m_linearPixels.data();
}

/**
 * @brief 获取宽度
 */
int Framebuffer::GetWidth() const {
    return m_width;
}

/**
 * @brief 获取高度
 */
int Framebuffer::GetHeight() const {
    return m_height;
}

} // namespace SR
