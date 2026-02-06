#pragma once

#include "Scene/Scene.h"

namespace SR {

/**
 * @brief 场景管理器，负责管理当前活动的场景实例
 */
class SceneManager {
public:
    /** @brief 获取当前场景活跃实例的引用 */
    Scene& GetScene();
    /** @brief 获取当前场景活跃实例的只读引用 */
    const Scene& GetScene() const;

private:
    Scene m_scene; ///< 单一场景实例 (未来可扩展为管理多个场景)
};

} // namespace SR
