#pragma once

#include <vector>

#include "SoftRendererExport.h"
#include "Math/Vec3.h"

namespace SR {

/**
 * @brief 平行光结构体
 */
struct DirectionalLight {
    Vec3 direction{0.0, -1.0, 0.0}; ///< 光照方向
    Vec3 color{1.0, 1.0, 1.0};      ///< 光照颜色
    double intensity = 1.0;          ///< 光照强度
};

/**
 * @brief 场景灯光组，管理场景中所有的光源
 */
class SR_API LightGroup {
public:
    /** @brief 清除所有灯光 */
    void Clear();
    /** @brief 添加一盏平行光 */
    void AddDirectionalLight(const DirectionalLight& light);
    /** @brief 获取平行光列表 */
    const std::vector<DirectionalLight>& GetDirectionalLights() const;

private:
    std::vector<DirectionalLight> m_directionalLights; ///< 存储所有的平行光
};

} // namespace SR
