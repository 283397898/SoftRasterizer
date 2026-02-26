#pragma once

/**
 * @file PBRUtils.h
 * @brief PBR（基于物理的渲染）公共计算函数（内联）。
 *        实现 Cook-Torrance BRDF 的核心子函数，供 FragmentShader 和 EnvironmentMap 共用。
 *
 * 包含：
 *   - FresnelSchlick  — Fresnel 近似（Schlick 公式）
 *   - DistributionGGX — GGX 法线分布函数（NDF）
 *   - GeometrySmith   — Smith 遮蔽-遮挡函数
 */

#include "Math/Vec3.h"
#include "Utils/MathUtils.h"

namespace SR {

constexpr double kPiPBR = 3.14159265358979323846;  ///< π 常量
constexpr double kInvPiPBR = 1.0 / kPiPBR;         ///< 1/π 常量

/**
 * @brief Fresnel-Schlick 近似
 * @param cosTheta 视线方向与半角向量的夹角余弦
 * @param F0       材质在垂直入射时的反射率 (0° Fresnel 反射颜色)
 * @return 插值后的 Fresnel 反射率
 */
inline Vec3 FresnelSchlick(double cosTheta, const Vec3& F0) {
    double t = 1.0 - Saturate(cosTheta);
    double t2 = t * t;
    double t5 = t2 * t2 * t;
    return Vec3{
        F0.x + (1.0 - F0.x) * t5,
        F0.y + (1.0 - F0.y) * t5,
        F0.z + (1.0 - F0.z) * t5
    };
}

/**
 * @brief GGX（Trowbridge-Reitz）法线分布函数
 * @param ndoth     法线与半角向量的点积
 * @param roughness 粗糙度 [0, 1]
 * @return 微面元分布概率密度（NDF 值）
 */
inline double DistributionGGX(double ndoth, double roughness) {
    double a = roughness * roughness;
    double a2 = a * a;
    double denom = (ndoth * ndoth) * (a2 - 1.0) + 1.0;
    return a2 * kInvPiPBR / (denom * denom + 1e-12);
}

/**
 * @brief Schlick-GGX 单向几何遮蔽函数
 * @param ndotv 法线与指定方向（视线或光线）的点积
 * @param roughness 粗糙度 [0, 1]
 * @return 几何遮蔽因子 [0, 1]
 */
inline double GeometrySchlickGGX(double ndotv, double roughness) {
    double r = roughness + 1.0;
    double k = (r * r) * 0.125;
    return ndotv / (ndotv * (1.0 - k) + k + 1e-12);
}

/**
 * @brief Smith 双向几何遮蔽函数（视线 + 光线）
 * @param ndotv 法线与视线方向的点积
 * @param ndotl 法线与光线方向的点积
 * @param roughness 粗糙度 [0, 1]
 * @return 综合几何遮蔽因子
 */
inline double GeometrySmith(double ndotv, double ndotl, double roughness) {
    return GeometrySchlickGGX(ndotv, roughness) * GeometrySchlickGGX(ndotl, roughness);
}

} // namespace SR
