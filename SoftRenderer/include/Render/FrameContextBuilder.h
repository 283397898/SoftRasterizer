#pragma once

#include "Pipeline/FrameContext.h"

namespace SR {

class Scene;

/**
 * @brief 帧上下文构建选项，包含相机和光照的默认参数
 */
struct FrameContextOptions {
    double fovYRadians = 60.0 * 3.14159265358979323846 / 180.0; ///< 垂直视角 (弧度)
    double zNear = 0.1;                                        ///< 近裁剪面
    double zFar = 100.0;                                       ///< 远裁剪面
    Vec3 defaultCameraPos{0.0, 0.0, 5.0};                     ///< 默认相机位置
    Vec3 ambientColor{0.03, 0.03, 0.03};                       ///< 环境光颜色
    Vec3 defaultLightDirection{-0.3, -1.0, -0.2};              ///< 默认平行光方向
    Vec3 defaultLightColor{1.0, 1.0, 1.0};                      ///< 默认平行光颜色
    double defaultLightIntensity = 1.2;                        ///< 默认平行光强度
};

/**
 * @brief 帧上下文构建器，根据场景和窗口大小生成每一帧的上下文信息
 */
class FrameContextBuilder {
public:
    /** @brief 使用默认选项构建帧上下文 */
    FrameContext Build(const Scene& scene, int width, int height) const;
    /** @brief 使用自定义选项构建帧上下文 */
    FrameContext Build(const Scene& scene, int width, int height, const FrameContextOptions& options) const;
};

} // namespace SR
