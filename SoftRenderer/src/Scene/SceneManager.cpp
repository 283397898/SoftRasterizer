#include "Scene/SceneManager.h"

namespace SR {

/**
 * @brief 获取当前场景
 */
Scene& SceneManager::GetScene() {
    return m_scene;
}

/**
 * @brief 获取当前场景 (只读)
 */
const Scene& SceneManager::GetScene() const {
    return m_scene;
}

} // namespace SR
