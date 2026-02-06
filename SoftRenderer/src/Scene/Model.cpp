#include "Scene/Model.h"

namespace SR {

/**
 * @brief 带网格的 Model 构造函数
 */
Model::Model(Mesh* mesh) : m_mesh(mesh) {
}

/**
 * @brief 设置模型引用的网格数据
 */
void Model::SetMesh(Mesh* mesh) {
    m_mesh = mesh;
}

/**
 * @brief 获取当前模型所使用的网格
 */
Mesh* Model::GetMesh() const {
    return m_mesh;
}

/**
 * @brief 获取模型变换对象以修改位置/旋转/缩放
 */
Transform& Model::GetTransform() {
    return m_transform;
}

/**
 * @brief 获取模型变换对象的只读引用
 */
const Transform& Model::GetTransform() const {
    return m_transform;
}

/**
 * @brief 获取模型的 PBR 材质对象以修改外观属性
 */
PBRMaterial& Model::GetMaterial() {
    return m_material;
}

/**
 * @brief 获取模型的 PBR 材质对象的只读引用
 */
const PBRMaterial& Model::GetMaterial() const {
    return m_material;
}

} // namespace SR
