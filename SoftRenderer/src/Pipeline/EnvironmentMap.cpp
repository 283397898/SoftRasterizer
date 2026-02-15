#include "Pipeline/EnvironmentMap.h"
#include "Asset/EXRDecoder.h"

#include <algorithm>
#include <cmath>
#include <omp.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace SR {

namespace {

constexpr double kPi  = 3.14159265358979323846;
constexpr double kInvPi = 0.31830988618379067154;
constexpr double k2Pi = 6.28318530717958647693;

// ============================================================================
// 低差异序列 (Hammersley)
// ============================================================================

double RadicalInverseVdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<double>(bits) * 2.3283064365386963e-10; // / 0x100000000
}

Vec2 Hammersley(uint32_t i, uint32_t N) {
    return Vec2{static_cast<double>(i) / static_cast<double>(N), RadicalInverseVdC(i)};
}

// ============================================================================
// GGX 重要性采样
// ============================================================================

Vec3 ImportanceSampleGGX(const Vec2& Xi, const Vec3& N, double roughness) {
    double a = roughness * roughness;
    double a2 = a * a;

    double phi = k2Pi * Xi.x;
    double cosTheta = std::sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y + 1e-12));
    double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));

    // 切线空间半程向量
    Vec3 H{std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta};

    // 构造 TBN
    Vec3 up = (std::abs(N.z) < 0.999) ? Vec3{0.0, 0.0, 1.0} : Vec3{1.0, 0.0, 0.0};
    Vec3 tangent = Vec3{
        up.y * N.z - up.z * N.y,
        up.z * N.x - up.x * N.z,
        up.x * N.y - up.y * N.x
    }.Normalized();
    Vec3 bitangent{
        N.y * tangent.z - N.z * tangent.y,
        N.z * tangent.x - N.x * tangent.z,
        N.x * tangent.y - N.y * tangent.x
    };

    Vec3 sampleDir{
        tangent.x * H.x + bitangent.x * H.y + N.x * H.z,
        tangent.y * H.x + bitangent.y * H.y + N.y * H.z,
        tangent.z * H.x + bitangent.z * H.y + N.z * H.z
    };
    return sampleDir.Normalized();
}

// ============================================================================
// Geometry Smith (与 FragmentShader 一致)
// ============================================================================

double GeometrySchlickGGX(double NdotV, double roughness) {
    double r = roughness + 1.0;
    double k = (r * r) * 0.125;
    return NdotV / (NdotV * (1.0 - k) + k + 1e-12);
}

double GeometrySmith(double NdotV, double NdotL, double roughness) {
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// 方向 → 等距柱形 UV
Vec2 DirToEquirectUV(const Vec3& dir) {
    double theta = std::atan2(dir.x, dir.z); // [-pi, pi]
    double phi = std::asin(std::max(-1.0, std::min(1.0, dir.y)));   // [-pi/2, pi/2]
    double u = theta * kInvPi * 0.5 + 0.5;   // [0, 1]
    double v = phi * kInvPi + 0.5;            // [0, 1]  (底部=0, 顶部=1)
    v = 1.0 - v; // 翻转：图像顶行 = 天空顶部
    return Vec2{u, v};
}

} // namespace

// ============================================================================
// 等距柱形投影双线性采样
// ============================================================================

Vec3 EnvironmentMap::SampleEquirectBilinear(const HDRImage& img, const Vec3& dir) const {
    Vec2 uv = DirToEquirectUV(dir);
    double fx = uv.x * img.width - 0.5;
    double fy = uv.y * img.height - 0.5;

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    double tx = fx - x0;
    double ty = fy - y0;

    auto wrap = [](int v, int size) -> int {
        v = v % size;
        if (v < 0) v += size;
        return v;
    };
    auto clampY = [&](int v) -> int { return std::max(0, std::min(v, img.height - 1)); };

    int x1 = wrap(x0 + 1, img.width);
    x0 = wrap(x0, img.width);
    int y1 = clampY(y0 + 1);
    y0 = clampY(y0);

    float r00, g00, b00, r10, g10, b10, r01, g01, b01, r11, g11, b11;
    img.GetPixel(x0, y0, r00, g00, b00);
    img.GetPixel(x1, y0, r10, g10, b10);
    img.GetPixel(x0, y1, r01, g01, b01);
    img.GetPixel(x1, y1, r11, g11, b11);

    double w00 = (1.0 - tx) * (1.0 - ty);
    double w10 = tx * (1.0 - ty);
    double w01 = (1.0 - tx) * ty;
    double w11 = tx * ty;

    return Vec3{
        r00 * w00 + r10 * w10 + r01 * w01 + r11 * w11,
        g00 * w00 + g10 * w10 + g01 * w01 + g11 * w11,
        b00 * w00 + b10 * w10 + b01 * w01 + b11 * w11
    };
}

// ============================================================================
// 公开接口
// ============================================================================

bool EnvironmentMap::LoadFromEXR(const std::string& path) {
    m_loaded = false;

    EXRDecoder decoder;
    if (!decoder.LoadFromFile(path, m_envMap)) {
        m_lastError = decoder.GetLastError();
        return false;
    }

    OutputDebugStringA(("EnvironmentMap: loaded " + std::to_string(m_envMap.width) + "x"
        + std::to_string(m_envMap.height) + " from " + path + "\n").c_str());

    // 预计算 IBL 数据
    ComputeSH9();
    ComputePrefilteredSpecular();
    ComputeBRDFLUT();

    m_loaded = true;
    OutputDebugStringA("EnvironmentMap: all precomputation done\n");
    return true;
}

Vec3 EnvironmentMap::SampleDirection(const Vec3& dir) const {
    if (!m_loaded) return Vec3{0.0, 0.0, 0.0};
    return SampleEquirectBilinear(m_envMap, dir);
}

Vec3 EnvironmentMap::EvalDiffuseSH(const Vec3& normal) const {
    if (!m_loaded) return Vec3{0.03, 0.03, 0.03};

    double x = normal.x, y = normal.y, z = normal.z;

    // Ramamoorthi & Hanrahan 辐照度 SH 常数
    constexpr double c1 = 0.429043;
    constexpr double c2 = 0.511664;
    constexpr double c3 = 0.743125;
    constexpr double c4 = 0.886227;
    constexpr double c5 = 0.247708;

    Vec3 irr{
        c4 * m_sh[0].x - c5 * m_sh[6].x
            + 2.0 * c2 * (m_sh[3].x * x + m_sh[1].x * y + m_sh[2].x * z)
            + 2.0 * c1 * (m_sh[4].x * x * y + m_sh[5].x * y * z + m_sh[7].x * x * z)
            + c3 * m_sh[6].x * z * z
            + c1 * m_sh[8].x * (x * x - y * y),
        c4 * m_sh[0].y - c5 * m_sh[6].y
            + 2.0 * c2 * (m_sh[3].y * x + m_sh[1].y * y + m_sh[2].y * z)
            + 2.0 * c1 * (m_sh[4].y * x * y + m_sh[5].y * y * z + m_sh[7].y * x * z)
            + c3 * m_sh[6].y * z * z
            + c1 * m_sh[8].y * (x * x - y * y),
        c4 * m_sh[0].z - c5 * m_sh[6].z
            + 2.0 * c2 * (m_sh[3].z * x + m_sh[1].z * y + m_sh[2].z * z)
            + 2.0 * c1 * (m_sh[4].z * x * y + m_sh[5].z * y * z + m_sh[7].z * x * z)
            + c3 * m_sh[6].z * z * z
            + c1 * m_sh[8].z * (x * x - y * y)
    };

    // 钳制负值（SH 可能产生极小负值）
    irr.x = std::max(0.0, irr.x);
    irr.y = std::max(0.0, irr.y);
    irr.z = std::max(0.0, irr.z);
    return irr;
}

Vec3 EnvironmentMap::SampleSpecular(const Vec3& R, double roughness) const {
    if (!m_loaded) return Vec3{0.0, 0.0, 0.0};

    // 根据 roughness 在 mip 之间线性插值
    double t = roughness * (kSpecularMipCount - 1);
    int mip0 = std::max(0, std::min(static_cast<int>(t), kSpecularMipCount - 1));
    int mip1 = std::min(mip0 + 1, kSpecularMipCount - 1);
    double frac = t - mip0;

    Vec3 c0 = SampleEquirectBilinear(m_specularMips[mip0], R);
    Vec3 c1 = SampleEquirectBilinear(m_specularMips[mip1], R);

    return Vec3{
        c0.x * (1.0 - frac) + c1.x * frac,
        c0.y * (1.0 - frac) + c1.y * frac,
        c0.z * (1.0 - frac) + c1.z * frac
    };
}

Vec2 EnvironmentMap::LookupBRDF(double NdotV, double roughness) const {
    if (m_brdfLUT.empty()) return Vec2{1.0, 0.0};

    double fx = NdotV * (kBRDFLutSize - 1);
    double fy = roughness * (kBRDFLutSize - 1);
    int x0 = std::max(0, std::min(static_cast<int>(fx), kBRDFLutSize - 1));
    int y0 = std::max(0, std::min(static_cast<int>(fy), kBRDFLutSize - 1));

    size_t idx = (static_cast<size_t>(y0) * kBRDFLutSize + x0) * 2;
    return Vec2{static_cast<double>(m_brdfLUT[idx]), static_cast<double>(m_brdfLUT[idx + 1])};
}

// ============================================================================
// SH L=2 漫反射辐照度预计算
// ============================================================================

void EnvironmentMap::ComputeSH9() {
    OutputDebugStringA("EnvironmentMap: computing SH9...\n");

    for (int i = 0; i < 9; ++i) m_sh[i] = Vec3{0.0, 0.0, 0.0};

    const int w = m_envMap.width;
    const int h = m_envMap.height;

    // SH 基函数常数
    constexpr double Y00  = 0.282095;   // 1/(2*sqrt(pi))
    constexpr double Y1n  = 0.488603;   // sqrt(3/(4*pi))
    constexpr double Y2n2 = 1.092548;   // sqrt(15/(4*pi)) ... 实际是 1/2*sqrt(15/pi)
    constexpr double Y20  = 0.315392;   // 1/4*sqrt(5/pi)
    constexpr double Y22  = 0.546274;   // 1/4*sqrt(15/pi)

    // 并行累加，使用 per-thread 局部 SH
    const int maxThreads = omp_get_max_threads();
    std::vector<Vec3> threadSH(static_cast<size_t>(maxThreads) * 9, Vec3{0.0, 0.0, 0.0});

    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        Vec3* localSH = &threadSH[static_cast<size_t>(tid) * 9];

        #pragma omp for schedule(static)
        for (int y = 0; y < h; ++y) {
            // 等距柱形投影：像素→方向映射（匹配 DirToEquirectUV 的逆）
            double v = (static_cast<double>(y) + 0.5) / h;
            double elevation = (0.5 - v) * kPi; // [-pi/2, pi/2]
            double cosElev = std::cos(elevation);
            double sinElev = std::sin(elevation);
            double dOmega = k2Pi * kPi * cosElev / (w * h); // 立体角微元

            double ny = sinElev;

            for (int x = 0; x < w; ++x) {
                double u = (static_cast<double>(x) + 0.5) / w;
                double azimuth = (u - 0.5) * k2Pi; // [-pi, pi]
                double nx = cosElev * std::sin(azimuth);
                double nz = cosElev * std::cos(azimuth);

                float fr, fg, fb;
                m_envMap.GetPixel(x, y, fr, fg, fb);
                Vec3 L{static_cast<double>(fr), static_cast<double>(fg), static_cast<double>(fb)};

                // 投影到 SH 基函数
                double basis[9];
                basis[0] = Y00;
                basis[1] = Y1n * ny;
                basis[2] = Y1n * nz;
                basis[3] = Y1n * nx;
                basis[4] = Y2n2 * nx * ny;
                basis[5] = Y2n2 * ny * nz;
                basis[6] = Y20  * (3.0 * nz * nz - 1.0);
                basis[7] = Y2n2 * nx * nz;
                basis[8] = Y22  * (nx * nx - ny * ny);

                for (int i = 0; i < 9; ++i) {
                    double w_basis = basis[i] * dOmega;
                    localSH[i].x += L.x * w_basis;
                    localSH[i].y += L.y * w_basis;
                    localSH[i].z += L.z * w_basis;
                }
            }
        }
    }

    // 合并各线程
    for (int t = 0; t < maxThreads; ++t) {
        for (int i = 0; i < 9; ++i) {
            const Vec3& v = threadSH[static_cast<size_t>(t) * 9 + i];
            m_sh[i].x += v.x;
            m_sh[i].y += v.y;
            m_sh[i].z += v.z;
        }
    }

    OutputDebugStringA("EnvironmentMap: SH9 done\n");
}

// ============================================================================
// Split-Sum 预过滤镜面反射 mip 链
// ============================================================================

void EnvironmentMap::ComputePrefilteredSpecular() {
    OutputDebugStringA("EnvironmentMap: computing prefiltered specular...\n");

    constexpr int baseMipWidth = 256;
    constexpr uint32_t numSamples = 256;

    for (int mip = 0; mip < kSpecularMipCount; ++mip) {
        double roughness = kMipRoughness[mip];
        int mipW = std::max(16, baseMipWidth >> mip);
        int mipH = mipW / 2;

        HDRImage& mipImg = m_specularMips[mip];
        mipImg.width = mipW;
        mipImg.height = mipH;
        mipImg.pixels.resize(static_cast<size_t>(mipW) * mipH * 3);

        // 像素→方向映射（匹配 DirToEquirectUV 的逆）
        auto pixelToDir = [&](int px, int py, int pw, int ph) -> Vec3 {
            double u = (static_cast<double>(px) + 0.5) / pw;
            double v = (static_cast<double>(py) + 0.5) / ph;
            double azimuth = (u - 0.5) * k2Pi;
            double elevation = (0.5 - v) * kPi;
            double cosElev = std::cos(elevation);
            return Vec3{cosElev * std::sin(azimuth), std::sin(elevation), cosElev * std::cos(azimuth)};
        };

        if (roughness < 1e-6) {
            // Mip 0 (roughness≈0): 直接从原图降采样（完美反射）
            #pragma omp parallel for schedule(static)
            for (int y = 0; y < mipH; ++y) {
                for (int x = 0; x < mipW; ++x) {
                    Vec3 dir = pixelToDir(x, y, mipW, mipH);
                    Vec3 c = SampleEquirectBilinear(m_envMap, dir);
                    mipImg.SetPixel(x, y, static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z));
                }
            }
        } else {
            // GGX 重要性采样预过滤
            #pragma omp parallel for schedule(dynamic, 1)
            for (int y = 0; y < mipH; ++y) {
                for (int x = 0; x < mipW; ++x) {
                    Vec3 N = pixelToDir(x, y, mipW, mipH);

                    Vec3 R = N; // 假设 V = N（Split-Sum 近似）
                    Vec3 V = R;

                    Vec3 prefilteredColor{0.0, 0.0, 0.0};
                    double totalWeight = 0.0;

                    for (uint32_t i = 0; i < numSamples; ++i) {
                        Vec2 Xi = Hammersley(i, numSamples);
                        Vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                        double VdotH = V.x * H.x + V.y * H.y + V.z * H.z;
                        Vec3 L{2.0 * VdotH * H.x - V.x,
                               2.0 * VdotH * H.y - V.y,
                               2.0 * VdotH * H.z - V.z};

                        double NdotL = N.x * L.x + N.y * L.y + N.z * L.z;
                        if (NdotL > 0.0) {
                            Vec3 s = SampleEquirectBilinear(m_envMap, L);
                            prefilteredColor.x += s.x * NdotL;
                            prefilteredColor.y += s.y * NdotL;
                            prefilteredColor.z += s.z * NdotL;
                            totalWeight += NdotL;
                        }
                    }

                    if (totalWeight > 0.0) {
                        double invW = 1.0 / totalWeight;
                        prefilteredColor.x *= invW;
                        prefilteredColor.y *= invW;
                        prefilteredColor.z *= invW;
                    }

                    mipImg.SetPixel(x, y,
                        static_cast<float>(prefilteredColor.x),
                        static_cast<float>(prefilteredColor.y),
                        static_cast<float>(prefilteredColor.z));
                }
            }
        }

        {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "EnvironmentMap: specular mip %d (%dx%d, roughness=%.1f) done\n",
                mip, mipW, mipH, roughness);
            OutputDebugStringA(buf);
        }
    }
}

// ============================================================================
// BRDF 积分 LUT (Split-Sum 第二项)
// ============================================================================

void EnvironmentMap::ComputeBRDFLUT() {
    OutputDebugStringA("EnvironmentMap: computing BRDF LUT...\n");

    constexpr int size = kBRDFLutSize;
    constexpr uint32_t numSamples = 512;

    m_brdfLUT.resize(static_cast<size_t>(size) * size * 2);

    #pragma omp parallel for schedule(static)
    for (int iy = 0; iy < size; ++iy) {
        double roughness = (static_cast<double>(iy) + 0.5) / size;
        roughness = std::max(roughness, 0.01); // 避免 roughness=0 导致的数值问题

        for (int ix = 0; ix < size; ++ix) {
            double NdotV = (static_cast<double>(ix) + 0.5) / size;
            NdotV = std::max(NdotV, 1e-4);

            Vec3 V{std::sqrt(1.0 - NdotV * NdotV), 0.0, NdotV};
            Vec3 N{0.0, 0.0, 1.0};

            double A = 0.0, B = 0.0;

            for (uint32_t i = 0; i < numSamples; ++i) {
                Vec2 Xi = Hammersley(i, numSamples);
                Vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                double VdotH = V.x * H.x + V.y * H.y + V.z * H.z;
                Vec3 L{2.0 * VdotH * H.x - V.x,
                       2.0 * VdotH * H.y - V.y,
                       2.0 * VdotH * H.z - V.z};

                double NdotL = std::max(L.z, 0.0);
                double NdotH = std::max(H.z, 0.0);
                VdotH = std::max(VdotH, 0.0);

                if (NdotL > 0.0) {
                    double G = GeometrySmith(NdotV, NdotL, roughness);
                    double G_Vis = (G * VdotH) / (NdotH * NdotV + 1e-12);
                    double Fc = std::pow(1.0 - VdotH, 5.0);

                    A += (1.0 - Fc) * G_Vis;
                    B += Fc * G_Vis;
                }
            }

            A /= numSamples;
            B /= numSamples;

            size_t idx = (static_cast<size_t>(iy) * size + ix) * 2;
            m_brdfLUT[idx + 0] = static_cast<float>(A);
            m_brdfLUT[idx + 1] = static_cast<float>(B);
        }
    }

    OutputDebugStringA("EnvironmentMap: BRDF LUT done\n");
}

} // namespace SR
