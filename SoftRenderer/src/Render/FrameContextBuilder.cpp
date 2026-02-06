#include "Render/FrameContextBuilder.h"

#include "Scene/Scene.h"
#include "Camera/OrbitCamera.h"
#include "Scene/LightGroup.h"

namespace SR {

/**
 * @brief 使用默认参数构建帧上下文
 * @param scene 源场景
 * @param width 视口宽度
 * @param height 视口高度
 * @return 组装好的 FrameContext
 */
FrameContext FrameContextBuilder::Build(const Scene& scene, int width, int height) const {
    FrameContextOptions options{};
    return Build(scene, width, height, options);
}

/**
 * @brief 根据场景数据、渲染目标尺寸和自定义选项，计算视图、投影矩阵以及光照信息
 * @param scene 源场景，从中提取相机和灯光
 * @param width 渲染视图宽度
 * @param height 渲染视图高度
 * @param options 构建选项 (FOV, 裁剪面, 默认值等)
 * @return 包含着色所需全部全局数据的 FrameContext
 */
FrameContext FrameContextBuilder::Build(const Scene& scene, int width, int height, const FrameContextOptions& options) const {
    FrameContext frameContext{};

    // 1. 处理相机视图
    const OrbitCamera* camera = scene.GetCamera();
    frameContext.view = camera ? camera->GetViewMatrix() : Mat4::Identity();
    frameContext.cameraPos = camera ? camera->GetPosition() : options.defaultCameraPos;

    // 2. 计算透视投影矩阵 (DirectX 风格：左手系, [0, 1] 深度)
    double aspect = (height > 0) ? (static_cast<double>(width) / static_cast<double>(height)) : 1.0;
    frameContext.projection = Mat4::Perspective(options.fovYRadians, aspect, options.zNear, options.zFar);

    // 3. 设置光照环境
    frameContext.ambientColor = options.ambientColor;

    const LightGroup* lights = scene.GetLightGroup();
    if (lights && !lights->GetDirectionalLights().empty()) {
        frameContext.lights = lights->GetDirectionalLights();
    } else {
        // 如果场景中没有灯光，则添加一个默认的平行光
        DirectionalLight defaultLight;
        defaultLight.direction = options.defaultLightDirection;
        defaultLight.color = options.defaultLightColor;
        defaultLight.intensity = options.defaultLightIntensity;
        frameContext.lights.push_back(defaultLight);
    }

    return frameContext;
}

} // namespace SR
