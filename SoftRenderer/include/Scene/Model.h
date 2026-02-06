#pragma once

#include "SoftRendererExport.h"
#include "Scene/Mesh.h"
#include "Scene/Transform.h"
#include "Material/PBRMaterial.h"

namespace SR {

/**
 * @brief 模型类，将网格 (Mesh)、变换 (Transform) 和材质 (Material) 组合在一起
 * 
 * 一个 Model 代表场景中一个可独立变换的可视组件
 */
class SR_API Model {
public:
    Model() = default;
    /** @brief 构造函数，指定网格 */
    explicit Model(Mesh* mesh);

    /** @brief 设置关联的网格 */
    void SetMesh(Mesh* mesh);
    /** @brief 获取关联的网格 */
    Mesh* GetMesh() const;

    /** @brief 获取变换组件引用 (位置、旋转、缩放) */
    Transform& GetTransform();
    /** @brief 获取只读变换组件引用 */
    const Transform& GetTransform() const;

    /** @brief 获取 PBR 材质引用 */
    PBRMaterial& GetMaterial();
    /** @brief 获取只读 PBR 材质引用 */
    const PBRMaterial& GetMaterial() const;

private:
    Mesh* m_mesh = nullptr;      ///< 共享的网格指针
    Transform m_transform;       ///< 局部变换
    PBRMaterial m_material;      ///< 模型材质
};

} // namespace SR
