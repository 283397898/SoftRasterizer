#include "Scene/Scene.h"
#include "Camera/OrbitCamera.h"

namespace SR {

/**
 * @brief 设置场景包含的物体组
 */
void Scene::SetObjectGroup(ObjectGroup* objects) {
    m_objects = objects;
}

/**
 * @brief 设置场景包含的灯光组
 */
void Scene::SetLightGroup(LightGroup* lights) {
    m_lights = lights;
}

/**
 * @brief 设置场景当前使用的相机
 */
void Scene::SetCamera(OrbitCamera* camera) {
    m_camera = camera;
}

/**
 * @brief 获取场景物体组
 */
const ObjectGroup* Scene::GetObjectGroup() const {
    return m_objects;
}

/**
 * @brief 获取场景灯光组
 */
const LightGroup* Scene::GetLightGroup() const {
    return m_lights;
}

/**
 * @brief 获取场景相机
 */
const OrbitCamera* Scene::GetCamera() const {
    return m_camera;
}

} // namespace SR
