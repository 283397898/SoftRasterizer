#include "Pipeline/FragmentShader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <immintrin.h>

#include "Asset/GLTFTypes.h"
#include "Pipeline/EnvironmentMap.h"
#include "Utils/MathUtils.h"
#include "Utils/PBRUtils.h"
#include "Utils/TextureSampler.h"

namespace SR {

namespace {

// 快速倒数平方根（Quake III 风格，适配双精度版本）
// 双精度下位运算技巧效果不佳，依赖编译器的 -ffast-math 优化
inline double FastInvSqrt(double x) {
    if (x < 1e-12) return 0.0;
    return 1.0 / std::sqrt(x);
}

double Clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

Vec3 Clamp01(const Vec3& v) {
    return Vec3{Clamp01(v.x), Clamp01(v.y), Clamp01(v.z)};
}

Vec3 Add(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Mul(const Vec3& a, const Vec3& b) {
    return Vec3{a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 Mul(const Vec3& a, double s) {
    return Vec3{a.x * s, a.y * s, a.z * s};
}

Vec3 Div(const Vec3& a, double s) {
    return Vec3{a.x / s, a.y / s, a.z / s};
}

// ============================================================================
// AVX2 SIMD 辅助函数 — Vec3 打包为 __m256d {r, g, b, 0}
// 将 3 分量 Vec3 运算映射到 4 路 SIMD，第 4 lane 保持零值。
// ============================================================================

/// @brief 加载 Vec3 到 __m256d {x, y, z, 0}
inline __m256d v3_load(const Vec3& v) {
    return _mm256_set_pd(0.0, v.z, v.y, v.x);
}

/// @brief 从 __m256d 提取 Vec3（忽略第 4 分量）
inline Vec3 v3_store(__m256d v) {
    alignas(32) double tmp[4];
    _mm256_store_pd(tmp, v);
    return Vec3{tmp[0], tmp[1], tmp[2]};
}

/// @brief Vec3 点积（水平归约：xy 积 + z 积）
inline double v3_dot(__m256d a, __m256d b) {
    __m256d prod = _mm256_mul_pd(a, b);             // {ax*bx, ay*by, az*bz, 0}
    __m128d lo = _mm256_castpd256_pd128(prod);      // {ax*bx, ay*by}
    __m128d hi = _mm256_extractf128_pd(prod, 1);    // {az*bz, 0}
    lo = _mm_add_pd(lo, hi);                         // {ax*bx+az*bz, ay*by}
    __m128d shuf = _mm_unpackhi_pd(lo, lo);          // {ay*by, ay*by}
    lo = _mm_add_sd(lo, shuf);                       // {sum, ...}
    return _mm_cvtsd_f64(lo);
}

/// @brief Fresnel-Schlick 近似（SIMD 版，F0 为 __m256d）
/// F = F0 + (1 - F0) * (1 - cosθ)^5
inline __m256d FresnelSchlick_SIMD(double cosTheta, __m256d F0) {
    double t = 1.0 - std::max(0.0, std::min(1.0, cosTheta));
    double t2 = t * t;
    double t5 = t2 * t2 * t;
    __m256d t5_v = _mm256_set1_pd(t5);
    __m256d one = _mm256_set1_pd(1.0);
    // F0 + (1 - F0) * t5 = fmadd(1-F0, t5, F0)
    return _mm256_fmadd_pd(_mm256_sub_pd(one, F0), t5_v, F0);
}

constexpr double kPi = 3.14159265358979323846;

constexpr bool kDebugTextureIndexTint = false;

Vec3 TintFromIndex(int index) {
    if (index < 0) {
        return Vec3{1.0, 1.0, 1.0};
    }
    uint32_t v = static_cast<uint32_t>(index) * 2654435761u;
    double r = ((v >> 0) & 0xFF) / 255.0;
    double g = ((v >> 8) & 0xFF) / 255.0;
    double b = ((v >> 16) & 0xFF) / 255.0;
    return Vec3{0.5 + 0.5 * r, 0.5 + 0.5 * g, 0.5 + 0.5 * b};
}

// ============================================================================
// 多重散射 GGX 能量补偿（Kulla-Conty 2017 / Fdez-Agüera 2019）
// 单次散射 GGX 在粗糙表面上会丢失能量，该模块对此进行修正
// ============================================================================

/**
 * @brief 近似预积分 DFG 项 (Lazarov 2013 / Karis 2014)
 * @return (scale, bias)，使得 E_ss ≈ F0 * scale + bias
 */
inline Vec2 ApproxDFG(double ndotv, double roughness) {
    double r_a = 1.0 - roughness;
    double r_b = roughness * (-0.0275) + 0.0425;
    double r_c = roughness * (-0.572) + 1.04;
    double r_d = roughness * 0.022 + (-0.04);
    double a004 = std::min(r_a * r_a, std::pow(2.0, -9.28 * ndotv)) * r_a + r_b;
    return Vec2{-1.04 * a004 + r_c, a004 + r_d};
}

/**
 * @brief 计算多重散射能量补偿因子
 * 
 * 单散射 GGX 在粗糙表面上会丢失能量（光线多次弹射未被计算），
 * 导致粗糙金属表面过暗。此函数计算补偿因子来恢复丢失的能量。
 * 
 * @param F0 基础反射率
 * @param dfg 预积分 DFG 项 (scale, bias)
 * @return 每通道的能量补偿乘子
 */
inline Vec3 MultiscatterCompensation(const Vec3& F0, const Vec2& dfg) {
    // E_ss = F0 * scale + bias (单散射方向反照率)
    double Ess_x = std::max(F0.x * dfg.x + dfg.y, 1e-4);
    double Ess_y = std::max(F0.y * dfg.x + dfg.y, 1e-4);
    double Ess_z = std::max(F0.z * dfg.x + dfg.y, 1e-4);
    // energyCompensation = 1 + F0 * (1/E_ss - 1)
    return Vec3{
        1.0 + F0.x * (1.0 / Ess_x - 1.0),
        1.0 + F0.y * (1.0 / Ess_y - 1.0),
        1.0 + F0.z * (1.0 / Ess_z - 1.0)
    };
}

} // namespace

// ============================================================================
// ShadeFast — 分离三角形级常量与像素级变量的优化着色器
// FragmentContext 在三角形粒度上设置一次，FragmentVarying 每像素插值
// ============================================================================

namespace {

// 简化的纹理采样辅助函数，根据采样器配置自动选择最近邻或双线性过滤
SampledColor SampleImageFast(const std::vector<GLTFImage>* images,
                             const std::vector<GLTFSampler>* samplers,
                             int imageIndex, int samplerIndex,
                             const Vec2& texCoord, bool srgb) {
    if (!images || imageIndex < 0 || imageIndex >= static_cast<int>(images->size())) {
        return {};
    }
    const GLTFImage& image = (*images)[imageIndex];
    const GLTFSampler* sampler = nullptr;
    if (samplers && samplerIndex >= 0 && samplerIndex < static_cast<int>(samplers->size())) {
        sampler = &(*samplers)[samplerIndex];
    }
    
    if (UseLinearFilter(sampler)) {
        return SampleImageBilinear(image, sampler, texCoord, srgb);
    }
    return SampleImageNearest(image, sampler, texCoord, srgb);
}

} // namespace

/**
 * @brief 高性能片元着色实现
 */
Vec3 FragmentShader::ShadeFast(const FragmentContext& ctx, const FragmentVarying& varying, double* outEffectiveAlpha) const {
    // 内联归一化法线 N（避免函数调用开销，热路径优化）
    Vec3 N = varying.normal;
    double nLenSq = N.x * N.x + N.y * N.y + N.z * N.z;
    if (nLenSq > 1e-12) {
        double invNLen = 1.0 / std::sqrt(nLenSq);
        N.x *= invNLen; N.y *= invNLen; N.z *= invNLen;
    }

    // 计算视线方向 V（世界空间，从片元指向相机）
    Vec3 V{ctx.cameraPos.x - varying.worldPos.x,
           ctx.cameraPos.y - varying.worldPos.y,
           ctx.cameraPos.z - varying.worldPos.z};
    double vLenSq = V.x * V.x + V.y * V.y + V.z * V.z;
    if (vLenSq > 1e-12) {
        double invVLen = 1.0 / std::sqrt(vLenSq);
        V.x *= invVLen; V.y *= invVLen; V.z *= invVLen;
    }

    // 双面渲染：若法线背向视线则翻转（符合 glTF 规范要求）
    if (ctx.doubleSided) {
        double ndotv_raw = N.x * V.x + N.y * V.y + N.z * V.z;
        if (ndotv_raw < 0.0) {
            N.x = -N.x; N.y = -N.y; N.z = -N.z;
        }
    }

    double roughness = std::max(0.04, ctx.roughness);
    double metallic = Saturate(ctx.metallic);
    Vec3 albedo = Clamp01(ctx.albedo);
    const TextureBinding& baseColorBinding = ctx.textures[static_cast<size_t>(TextureSlot::BaseColor)];
    const TextureBinding& metallicRoughnessBinding = ctx.textures[static_cast<size_t>(TextureSlot::MetallicRoughness)];
    const TextureBinding& normalBinding = ctx.textures[static_cast<size_t>(TextureSlot::Normal)];
    const TextureBinding& occlusionBinding = ctx.textures[static_cast<size_t>(TextureSlot::Occlusion)];
    const TextureBinding& emissiveBinding = ctx.textures[static_cast<size_t>(TextureSlot::Emissive)];
    const TextureBinding& transmissionBinding = ctx.textures[static_cast<size_t>(TextureSlot::Transmission)];

    // ---- 纹理采样阶段 ----
    // 采样基础颜色贴图（sRGB 解码）并与顶点颜色相乘
    double alpha = ctx.alpha;
    if (baseColorBinding.imageIndex >= 0) {
        Vec2 baseUv = (baseColorBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
        SampledColor baseColor = SampleImageFast(ctx.images, ctx.samplers,
            baseColorBinding.imageIndex, baseColorBinding.samplerIndex, baseUv, true);
        Vec3 vertexColor = Clamp01(Vec3{varying.color.x, varying.color.y, varying.color.z});
        albedo = Mul(Mul(albedo, baseColor.rgb), vertexColor);
        alpha *= baseColor.a * Clamp01(varying.color.w);
    }
    if (baseColorBinding.imageIndex < 0) {
        Vec3 vertexColor = Clamp01(Vec3{varying.color.x, varying.color.y, varying.color.z});
        albedo = Mul(albedo, vertexColor);
        alpha *= Clamp01(varying.color.w);
    }
    if (ctx.transmissionFactor > 0.0 || transmissionBinding.imageIndex >= 0) {
        double t = Saturate(ctx.transmissionFactor);
        if (transmissionBinding.imageIndex >= 0) {
            Vec2 tUv = (transmissionBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
            SampledColor transmission = SampleImageFast(ctx.images, ctx.samplers,
                transmissionBinding.imageIndex, transmissionBinding.samplerIndex, tUv, false);
            t *= transmission.rgb.x;
        }
        alpha *= (1.0 - Saturate(t));
    }

    // 采样金属度-粗糙度贴图（线性空间：B=金属度, G=粗糙度）
    if (metallicRoughnessBinding.imageIndex >= 0) {
        Vec2 mrUv = (metallicRoughnessBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
        SampledColor mr = SampleImageFast(ctx.images, ctx.samplers,
            metallicRoughnessBinding.imageIndex, metallicRoughnessBinding.samplerIndex, mrUv, false);
        metallic = Saturate(metallic * mr.rgb.z);
        roughness = std::max(0.04, mr.rgb.y * roughness);
    }

    // 法线贴图：将切线空间法线变换到世界空间，更新 N 向量
    if (normalBinding.imageIndex >= 0) {
        // 内联归一化切线 T（避免函数调用开销）
        Vec3 T = varying.tangent;
        double tLenSq = T.x * T.x + T.y * T.y + T.z * T.z;
        if (tLenSq > 1e-12) {
            double invTLen = 1.0 / std::sqrt(tLenSq);
            T.x *= invTLen; T.y *= invTLen; T.z *= invTLen;

            Vec2 nUv = (normalBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
            SampledColor nm = SampleImageFast(ctx.images, ctx.samplers,
                normalBinding.imageIndex, normalBinding.samplerIndex, nUv, false);
            Vec3 tangentNormal{nm.rgb.x * 2.0 - 1.0, nm.rgb.y * 2.0 - 1.0, nm.rgb.z * 2.0 - 1.0};

            // 计算副切线 B = cross(N, T) * tangentW（tangentW 决定坐标系手性）
            Vec3 B{N.y * T.z - N.z * T.y, N.z * T.x - N.x * T.z, N.x * T.y - N.y * T.x};
            double bLenSq = B.x * B.x + B.y * B.y + B.z * B.z;
            if (bLenSq > 1e-12) {
                double invBLen = ctx.tangentW / std::sqrt(bLenSq);
                B.x *= invBLen; B.y *= invBLen; B.z *= invBLen;
            }

            Vec3 worldNormal{
                T.x * tangentNormal.x + B.x * tangentNormal.y + N.x * tangentNormal.z,
                T.y * tangentNormal.x + B.y * tangentNormal.y + N.y * tangentNormal.z,
                T.z * tangentNormal.x + B.z * tangentNormal.y + N.z * tangentNormal.z
            };

            // 将 TBN 变换后的法线重新归一化
            double wnLenSq = worldNormal.x * worldNormal.x + worldNormal.y * worldNormal.y + worldNormal.z * worldNormal.z;
            if (wnLenSq > 1e-12) {
                double invWnLen = 1.0 / std::sqrt(wnLenSq);
                N.x = worldNormal.x * invWnLen;
                N.y = worldNormal.y * invWnLen;
                N.z = worldNormal.z * invWnLen;
            }
        }
    }

    // ========================================================================
    // PBR 计算阶段 — AVX2 SIMD 优化
    // Vec3 运算全部使用 __m256d {r, g, b, 0}，在寄存器域内完成光照计算。
    // 标量值（ndotl, GGX, Smith 等）保持 scalar double。
    // ========================================================================
    __m256d s_N = v3_load(N);
    __m256d s_V = v3_load(V);
    __m256d s_albedo = v3_load(albedo);
    __m256d s_one = _mm256_set1_pd(1.0);

    // 根据折射率计算电介质 F0：F0 = ((ior-1)/(ior+1))^2
    double iorF0 = (ctx.ior - 1.0) / (ctx.ior + 1.0);
    iorF0 = iorF0 * iorF0;
    // 应用 KHR_materials_specular 修正
    __m256d s_dielectricF0 = _mm256_mul_pd(
        v3_load(ctx.specularColorFactor),
        _mm256_set1_pd(iorF0 * ctx.specularFactor));
    // F0 = lerp(dielectricF0, albedo, metallic)
    __m256d s_F0 = _mm256_fmadd_pd(
        _mm256_sub_pd(s_albedo, s_dielectricF0),
        _mm256_set1_pd(metallic), s_dielectricF0);

    // BLEND 模式下预乘 Alpha
    double premulAlpha = (ctx.alphaMode == GLTFAlphaMode::Blend) ? Saturate(alpha) : 1.0;
    __m256d s_premulAlpha = _mm256_set1_pd(premulAlpha);

    double ndotv = std::max(0.0, v3_dot(s_N, s_V));

    // 多重散射 GGX 能量补偿（Kulla-Conty 2017）
    Vec2 dfg = ApproxDFG(ndotv, roughness);
    __m256d s_Ess = _mm256_max_pd(
        _mm256_fmadd_pd(s_F0, _mm256_set1_pd(dfg.x), _mm256_set1_pd(dfg.y)),
        _mm256_set1_pd(1e-4));
    // Fms = 1 + F0 * (1/Ess - 1)
    __m256d s_Fms = _mm256_fmadd_pd(s_F0,
        _mm256_sub_pd(_mm256_div_pd(s_one, s_Ess), s_one),
        s_one);

    // 预计算 albedo / π（光照循环复用）
    __m256d s_albedoOverPi = _mm256_mul_pd(s_albedo, _mm256_set1_pd(1.0 / kPi));
    __m256d s_oneMinusMetallic = _mm256_set1_pd(1.0 - metallic);

    __m256d s_Lo = _mm256_setzero_pd();

    // 使用预计算的光照数据（指针访问，避免 vector 拷贝开销）
    if (ctx.precomputedLights && ctx.precomputedLightCount > 0) {
        for (size_t i = 0; i < ctx.precomputedLightCount; ++i) {
            const PrecomputedLight& pl = ctx.precomputedLights[i];
            __m256d s_L = v3_load(pl.L);

            double ndotl = v3_dot(s_N, s_L);
            // 光源在背面时跳过（早期退出）
            if (ndotl <= 0.0) continue;

            // H = normalize(L + V)
            __m256d s_H = _mm256_add_pd(s_L, s_V);
            double hLenSq = v3_dot(s_H, s_H);
            if (hLenSq > 1e-12) {
                s_H = _mm256_mul_pd(s_H, _mm256_set1_pd(1.0 / std::sqrt(hLenSq)));
            }

            double ndoth = std::max(0.0, v3_dot(s_N, s_H));
            double vdoth = std::max(0.0, v3_dot(s_V, s_H));

            // Cook-Torrance BRDF（标量 D/G + SIMD F/specular/diffuse）
            __m256d s_F = FresnelSchlick_SIMD(vdoth, s_F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            // specular = F * (D*G / (4*ndotv*ndotl+eps)) * Fms
            double specCoeff = (D * G) / (4.0 * ndotv * ndotl + 1e-12);
            __m256d s_specular = _mm256_mul_pd(
                _mm256_mul_pd(s_F, _mm256_set1_pd(specCoeff)), s_Fms);

            // kD = (1 - F) * (1 - metallic)
            __m256d s_kD = _mm256_mul_pd(
                _mm256_sub_pd(s_one, s_F), s_oneMinusMetallic);
            // diffuse = kD * albedo / π
            __m256d s_diffuse = _mm256_mul_pd(s_kD, s_albedoOverPi);
            if (premulAlpha < 1.0) {
                s_diffuse = _mm256_mul_pd(s_diffuse, s_premulAlpha);
            }

            // contrib = (diffuse + specular) * ndotl * radiance
            __m256d s_contrib = _mm256_mul_pd(
                _mm256_add_pd(s_diffuse, s_specular), _mm256_set1_pd(ndotl));
            s_contrib = _mm256_mul_pd(s_contrib, v3_load(pl.radiance));

            s_Lo = _mm256_add_pd(s_Lo, s_contrib);
        }
    } else if (ctx.lights) {
        // 回退路径（向后兼容旧版光照数据）
        for (const DirectionalLight& light : *ctx.lights) {
            Vec3 L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.Normalized();
            __m256d s_L = v3_load(L);
            __m256d s_H = v3_load((L + V).Normalized());

            double ndotl = std::max(0.0, v3_dot(s_N, s_L));
            double ndoth = std::max(0.0, v3_dot(s_N, s_H));
            double vdoth = std::max(0.0, v3_dot(s_V, s_H));

            __m256d s_F = FresnelSchlick_SIMD(vdoth, s_F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            double specCoeff = (D * G) / (4.0 * ndotv * ndotl + 1e-12);
            __m256d s_specular = _mm256_mul_pd(
                _mm256_mul_pd(s_F, _mm256_set1_pd(specCoeff)), s_Fms);

            __m256d s_kD = _mm256_mul_pd(
                _mm256_sub_pd(s_one, s_F), s_oneMinusMetallic);
            __m256d s_diffuse = _mm256_mul_pd(s_kD, s_albedoOverPi);
            if (premulAlpha < 1.0) {
                s_diffuse = _mm256_mul_pd(s_diffuse, s_premulAlpha);
            }

            __m256d s_radiance = _mm256_mul_pd(v3_load(light.color), _mm256_set1_pd(light.intensity));
            __m256d s_contrib = _mm256_mul_pd(
                _mm256_add_pd(s_diffuse, s_specular), _mm256_set1_pd(ndotl));
            s_contrib = _mm256_mul_pd(s_contrib, s_radiance);

            s_Lo = _mm256_add_pd(s_Lo, s_contrib);
        }
    }

    // === Ambient: split into diffuse + specular（AVX2 SIMD） ===
    __m256d s_kS_env = FresnelSchlick_SIMD(ndotv, s_F0);
    __m256d s_kD_env = _mm256_mul_pd(_mm256_sub_pd(s_one, s_kS_env), s_oneMinusMetallic);

    __m256d s_ambientDiffuse;
    __m256d s_ambientSpecular;

    if (ctx.environmentMap) {
        // --- IBL 路径 (Split-Sum) ---
        __m256d s_irradiance = v3_load(ctx.environmentMap->EvalDiffuseSH(N));
        s_ambientDiffuse = _mm256_mul_pd(
            _mm256_mul_pd(s_kD_env, s_albedo),
            _mm256_mul_pd(s_irradiance, _mm256_set1_pd(1.0 / kPi)));
        if (premulAlpha < 1.0) {
            s_ambientDiffuse = _mm256_mul_pd(s_ambientDiffuse, s_premulAlpha);
        }

        // 反射方向 R = 2*ndotv*N - V
        __m256d s_R = _mm256_fmsub_pd(_mm256_set1_pd(2.0 * ndotv), s_N, s_V);
        double rLenSq = v3_dot(s_R, s_R);
        if (rLenSq > 1e-12) {
            s_R = _mm256_mul_pd(s_R, _mm256_set1_pd(1.0 / std::sqrt(rLenSq)));
        }
        Vec3 R = v3_store(s_R);
        __m256d s_prefilteredColor = v3_load(ctx.environmentMap->SampleSpecular(R, roughness));
        Vec2 brdf = ctx.environmentMap->LookupBRDF(ndotv, roughness);
        // specEnv = prefilteredColor * (F0 * brdf.x + brdf.y) * Fms
        __m256d s_brdfTerm = _mm256_fmadd_pd(s_F0, _mm256_set1_pd(brdf.x), _mm256_set1_pd(brdf.y));
        s_ambientSpecular = _mm256_mul_pd(_mm256_mul_pd(s_prefilteredColor, s_brdfTerm), s_Fms);
    } else {
        // --- 回退：常量环境光 ---
        __m256d s_ambient = v3_load(ctx.ambientColor);
        s_ambientDiffuse = _mm256_mul_pd(s_ambient, _mm256_mul_pd(s_kD_env, s_albedo));
        if (premulAlpha < 1.0) {
            s_ambientDiffuse = _mm256_mul_pd(s_ambientDiffuse, s_premulAlpha);
        }
        double envSmooth = 1.0 - roughness * roughness;
        s_ambientSpecular = _mm256_mul_pd(
            _mm256_mul_pd(s_kS_env, s_Fms),
            _mm256_mul_pd(s_ambient, _mm256_set1_pd(envSmooth)));
    }

    if (occlusionBinding.imageIndex >= 0) {
        Vec2 occUv = (occlusionBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
        SampledColor occ = SampleImageFast(ctx.images, ctx.samplers,
            occlusionBinding.imageIndex, occlusionBinding.samplerIndex, occUv, false);
        // 仅对漫反射环境光应用 AO
        s_ambientDiffuse = _mm256_mul_pd(s_ambientDiffuse, _mm256_set1_pd(occ.rgb.x));
    }

    __m256d s_color = _mm256_add_pd(_mm256_add_pd(s_ambientDiffuse, s_ambientSpecular), s_Lo);

    // 自发光
    __m256d s_emissive = v3_load(ctx.emissiveFactor);
    if (emissiveBinding.imageIndex >= 0) {
        Vec2 emUv = (emissiveBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
        SampledColor emissive = SampleImageFast(ctx.images, ctx.samplers,
            emissiveBinding.imageIndex, emissiveBinding.samplerIndex, emUv, true);
        s_emissive = _mm256_mul_pd(v3_load(emissive.rgb), s_emissive);
    }
    s_color = _mm256_add_pd(s_color, s_emissive);

    if (outEffectiveAlpha) {
        *outEffectiveAlpha = alpha;
    }

    return v3_store(s_color);
}

} // namespace SR
