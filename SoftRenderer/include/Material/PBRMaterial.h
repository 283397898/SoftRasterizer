#pragma once

#include "Math/Vec3.h"

namespace SR {

/**
 * @brief 基于物理的渲染 (PBR) 材质属性
 * 
 * 符合 glTF 2.0 的金属度-粗糙度工作流
 */
struct PBRMaterial {
    Vec3 albedo{1.0, 1.0, 1.0}; ///< 基础反射率/基色
    double metallic = 0.0;     ///< 金属度 [0, 1]
    double roughness = 0.5;    ///< 粗糙度 [0, 1]
    bool doubleSided = false;  ///< 是否双面渲染
    double alpha = 1.0;         ///< 透明度
    double transmissionFactor = 0.0; ///< 透射强度 [0, 1] (KHR_materials_transmission)
    int alphaMode = 0;         ///< 混合模式 (0:Opaque, 1:Mask, 2:Blend)
    double alphaCutoff = 0.5;   ///< Mask 模式下的裁剪阈值
    Vec3 emissiveFactor{0.0, 0.0, 0.0}; ///< 自发光因子 (RGB 乘子)
    double ior = 1.5;           ///< 折射率 (KHR_materials_ior, 默认 1.5)
    double specularFactor = 1.0; ///< 镜面反射强度 [0, 1] (KHR_materials_specular)
    Vec3 specularColorFactor{1.0, 1.0, 1.0}; ///< 镜面反射颜色 (KHR_materials_specular)
};

} // namespace SR
