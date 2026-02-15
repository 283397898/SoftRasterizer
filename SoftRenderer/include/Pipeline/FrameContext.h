#pragma once

#include <vector>

#include "Math/Mat4.h"
#include "Math/Vec3.h"
#include "Scene/LightGroup.h"

namespace SR {

struct GLTFImage;
struct GLTFSampler;
class EnvironmentMap;

struct FrameContext {
    Mat4 view = Mat4::Identity();
    Mat4 projection = Mat4::Identity();
    Vec3 cameraPos{0.0, 0.0, 0.0};
    Vec3 ambientColor{0.03, 0.03, 0.03};
    std::vector<DirectionalLight> lights;
    const std::vector<GLTFImage>* images = nullptr;
    const std::vector<GLTFSampler>* samplers = nullptr;
    const EnvironmentMap* environmentMap = nullptr;  ///< IBL 环境贴图（可选）
};

} // namespace SR
