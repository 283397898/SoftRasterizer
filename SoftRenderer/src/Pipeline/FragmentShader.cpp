#include "Pipeline/FragmentShader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

    // 根据折射率计算电介质 F0（菲涅耳垂直入射反射率）：F0 = ((ior-1)/(ior+1))^2
    double iorF0 = (ctx.ior - 1.0) / (ctx.ior + 1.0);
    iorF0 = iorF0 * iorF0;
    // 应用 KHR_materials_specular：镜面反射强度和颜色因子修正 F0
    Vec3 dielectricF0{
        iorF0 * ctx.specularFactor * ctx.specularColorFactor.x,
        iorF0 * ctx.specularFactor * ctx.specularColorFactor.y,
        iorF0 * ctx.specularFactor * ctx.specularColorFactor.z
    };
    Vec3 F0 = Lerp(dielectricF0, albedo, metallic);

    // BLEND 模式下对漫反射进行预乘 Alpha：镜面高光保持全强度（使玻璃仍有反射）
    // 光栅化器使用预乘混合：result = shaded + bg*(1-alpha)
    double premulAlpha = (ctx.alphaMode == GLTFAlphaMode::Blend) ? Saturate(alpha) : 1.0;

    double ndotv = std::max(0.0, Vec3::Dot(N, V));

    // 多重散射 GGX 能量补偿（Kulla-Conty），修正粗糙金属暗化问题
    Vec2 dfg = ApproxDFG(ndotv, roughness);
    Vec3 Fms = MultiscatterCompensation(F0, dfg);

    Vec3 Lo{0.0, 0.0, 0.0};

    // 使用预计算的光照数据（指针访问，避免 vector 拷贝开销）
    if (ctx.precomputedLights && ctx.precomputedLightCount > 0) {
        for (size_t i = 0; i < ctx.precomputedLightCount; ++i) {
            const PrecomputedLight& pl = ctx.precomputedLights[i];
            const Vec3& L = pl.L;

            double ndotl = Vec3::Dot(N, L);

            // 光源在背面时跳过（早期退出，减少无效着色）
            if (ndotl <= 0.0) continue;

            // 计算半角向量 H = normalize(L + V)，内联避免函数调用
            Vec3 H{L.x + V.x, L.y + V.y, L.z + V.z};
            double hLenSq = H.x * H.x + H.y * H.y + H.z * H.z;
            if (hLenSq > 1e-12) {
                double invHLen = 1.0 / std::sqrt(hLenSq);
                H.x *= invHLen; H.y *= invHLen; H.z *= invHLen;
            }

            double ndoth = std::max(0.0, Vec3::Dot(N, H));
            double vdoth = std::max(0.0, Vec3::Dot(V, H));

            Vec3 F = FresnelSchlick(vdoth, F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            Vec3 numerator = Mul(F, D * G);
            double denom = 4.0 * ndotv * ndotl + 1e-12;
            Vec3 specular = Mul(Div(numerator, denom), Fms); // 多重散射补偿

            Vec3 kS = F;
            Vec3 kD = Mul(Sub(Vec3{1.0, 1.0, 1.0}, kS), 1.0 - metallic);
            Vec3 diffuse = Mul(kD, Div(albedo, kPi));
            if (premulAlpha < 1.0) {
                diffuse = Mul(diffuse, premulAlpha);
            }

            Vec3 contrib = Mul(Add(diffuse, specular), ndotl);
            contrib = Mul(contrib, pl.radiance);  // 使用预计算辐照度

            Lo = Add(Lo, contrib);
        }
    } else if (ctx.lights) {
        // 回退路径（向后兼容旧版光照数据）
        for (const DirectionalLight& light : *ctx.lights) {
            Vec3 L = Vec3{-light.direction.x, -light.direction.y, -light.direction.z}.Normalized();
            Vec3 H = (L + V).Normalized();

            double ndotl = std::max(0.0, Vec3::Dot(N, L));
            double ndoth = std::max(0.0, Vec3::Dot(N, H));
            double vdoth = std::max(0.0, Vec3::Dot(V, H));

            Vec3 F = FresnelSchlick(vdoth, F0);
            double D = DistributionGGX(ndoth, roughness);
            double G = GeometrySmith(ndotv, ndotl, roughness);

            Vec3 numerator = Mul(F, D * G);
            double denom = 4.0 * ndotv * ndotl + 1e-12;
            Vec3 specular = Mul(Div(numerator, denom), Fms); // 多重散射补偿

            Vec3 kS = F;
            Vec3 kD = Mul(Sub(Vec3{1.0, 1.0, 1.0}, kS), 1.0 - metallic);
            Vec3 diffuse = Mul(kD, Div(albedo, kPi));
            if (premulAlpha < 1.0) {
                diffuse = Mul(diffuse, premulAlpha);
            }

            Vec3 radiance = Mul(light.color, light.intensity);
            Vec3 contrib = Mul(Add(diffuse, specular), ndotl);
            contrib = Mul(contrib, radiance);

            Lo = Add(Lo, contrib);
        }
    }

    // === Ambient: split into diffuse + specular ===
    // 如果有环境贴图则使用 IBL (Image-Based Lighting)，否则回退到常量环境光
    Vec3 kS_env = FresnelSchlick(ndotv, F0);
    Vec3 kD_env = Mul(Sub(Vec3{1.0, 1.0, 1.0}, kS_env), 1.0 - metallic);

    Vec3 ambientDiffuse;
    Vec3 ambientSpecular;

    if (ctx.environmentMap) {
        // --- IBL 路径 (Split-Sum) ---
        // 漫反射：SH 辐照度
        Vec3 irradiance = ctx.environmentMap->EvalDiffuseSH(N);
        ambientDiffuse = Mul(Mul(kD_env, albedo), Div(irradiance, kPi));
        if (premulAlpha < 1.0) {
            ambientDiffuse = Mul(ambientDiffuse, premulAlpha);
        }

        // 镜面反射：预过滤贴图 + BRDF LUT
        Vec3 R{2.0 * ndotv * N.x - V.x, 2.0 * ndotv * N.y - V.y, 2.0 * ndotv * N.z - V.z};
        double rLen = std::sqrt(R.x * R.x + R.y * R.y + R.z * R.z);
        if (rLen > 1e-12) { double inv = 1.0 / rLen; R.x *= inv; R.y *= inv; R.z *= inv; }
        Vec3 prefilteredColor = ctx.environmentMap->SampleSpecular(R, roughness);
        Vec2 brdf = ctx.environmentMap->LookupBRDF(ndotv, roughness);
        // 镜面环境光 = prefilteredColor * (F0 * scale + bias) * Fms（Split-Sum 第二项）
        ambientSpecular = Vec3{
            prefilteredColor.x * (F0.x * brdf.x + brdf.y) * Fms.x,
            prefilteredColor.y * (F0.y * brdf.x + brdf.y) * Fms.y,
            prefilteredColor.z * (F0.z * brdf.x + brdf.y) * Fms.z
        };
    } else {
        // --- 回退：常量环境光 ---
        ambientDiffuse = Mul(ctx.ambientColor, Mul(kD_env, albedo));
        if (premulAlpha < 1.0) {
            ambientDiffuse = Mul(ambientDiffuse, premulAlpha);
        }
        double envSmooth = 1.0 - roughness * roughness;
        ambientSpecular = Mul(Mul(kS_env, Fms), ctx.ambientColor * envSmooth);
    }

    Vec3 ambient = Add(ambientDiffuse, ambientSpecular);

    if (occlusionBinding.imageIndex >= 0) {
        Vec2 occUv = (occlusionBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
        SampledColor occ = SampleImageFast(ctx.images, ctx.samplers,
            occlusionBinding.imageIndex, occlusionBinding.samplerIndex, occUv, false);
        // 仅对漫反射环境光应用 AO（镜面反射不受局部遮蔽影响）
        ambientDiffuse = Mul(ambientDiffuse, occ.rgb.x);
        ambient = Add(ambientDiffuse, ambientSpecular);
    }

    Vec3 color = Add(ambient, Lo);

    // 自发光：若有纹理则为 texture * emissiveFactor，否则直接使用 emissiveFactor
    Vec3 emissiveContrib = ctx.emissiveFactor;
    if (emissiveBinding.imageIndex >= 0) {
        Vec2 emUv = (emissiveBinding.texCoordSet == 1) ? varying.texCoord1 : varying.texCoord;
        SampledColor emissive = SampleImageFast(ctx.images, ctx.samplers,
            emissiveBinding.imageIndex, emissiveBinding.samplerIndex, emUv, true);
        emissiveContrib = Mul(emissive.rgb, ctx.emissiveFactor);
    }
    color = Add(color, emissiveContrib);

    // 输出 alpha 保持不变（Fresnel 反射已通过预乘镜面反射写入 shaded 颜色；
    // 混合 alpha 维持原始材质透明度）。
    if (outEffectiveAlpha) {
        *outEffectiveAlpha = alpha;
    }

    return color;
}

} // namespace SR
