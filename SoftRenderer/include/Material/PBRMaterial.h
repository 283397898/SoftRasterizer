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
    int alphaMode = 0;         ///< 混合模式 (0:Opaque, 1:Mask, 2:Blend)
    double alphaCutoff = 0.5;   ///< Mask 模式下的裁剪阈值
};

} // namespace SR
