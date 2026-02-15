#pragma once

#include "Render/FrameContextBuilder.h"
#include "Math/Mat4.h"

namespace SR {

class EnvironmentMap;

/**
 * @brief 渲染器全局配置选项
 */
struct RendererConfig {
    FrameContextOptions frameContext{};   ///< 帧上下文构建选项
    bool enableFXAA = true;               ///< 是否开启抗锯齿
    bool enableToneMap = true;            ///< 是否开启色调映射
    double exposure = 1.0;                 ///< 渲染曝光强度
    int debugOnlyMaterialIndex = -1;      ///< 调试: 仅渲染指定材质索引, -1 表示不过滤
    bool useViewOverride = false;         ///< 是否覆盖视图矩阵 (如用于调试)
    Mat4 viewOverride = Mat4::Identity();  ///< 覆盖用的视图矩阵
    bool useCameraPosOverride = false;    ///< 是否覆盖相机位置
    Vec3 cameraPosOverride{0.0, 0.0, 0.0}; ///< 覆盖用的相机位置
    const EnvironmentMap* environmentMap = nullptr; ///< IBL 环境贴图（可选）

    /** @brief 获取默认配置 */
    static RendererConfig Default();
};

} // namespace SR
