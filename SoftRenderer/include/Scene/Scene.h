#pragma once

#include "SoftRendererExport.h"

namespace SR {

class ObjectGroup;
class LightGroup;
class OrbitCamera;

/**
 * @brief 场景类，作为容器聚合了对象、灯光和相机
 */
class SR_API Scene {
public:
    /** @brief 设置场景中的模型组 */
    void SetObjectGroup(ObjectGroup* objects);
    /** @brief 设置场景中的灯光组 */
    void SetLightGroup(LightGroup* lights);
    /** @brief 设置场景关联的相机 */
    void SetCamera(OrbitCamera* camera);

    /** @brief 获取模型组指针 */
    const ObjectGroup* GetObjectGroup() const;
    /** @brief 获取灯光组指针 */
    const LightGroup* GetLightGroup() const;
    /** @brief 获取相机指针 */
    const OrbitCamera* GetCamera() const;

private:
    ObjectGroup* m_objects = nullptr; ///< 模型集合
    LightGroup* m_lights = nullptr;   ///< 灯光集合
    OrbitCamera* m_camera = nullptr;   ///< 活动相机
};

} // namespace SR
