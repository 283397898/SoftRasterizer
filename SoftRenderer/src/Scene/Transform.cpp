#include "Scene/Transform.h"

namespace SR {

/**
 * @brief 设置物体的世界坐标
 */
void Transform::SetPosition(const Vec3& position) {
    m_position = position;
}

/**
 * @brief 设置物体的旋转 (欧拉角)
 */
void Transform::SetRotation(const Vec3& rotationRadians) {
    m_rotation = rotationRadians;
}

/**
 * @brief 设置物体的缩放比例
 */
void Transform::SetScale(const Vec3& scale) {
    m_scale = scale;
}

/**
 * @brief 返回当前位置
 */
const Vec3& Transform::GetPosition() const {
    return m_position;
}

/**
 * @brief 返回当前旋转
 */
const Vec3& Transform::GetRotation() const {
    return m_rotation;
}

/**
 * @brief 返回当前缩放
 */
const Vec3& Transform::GetScale() const {
    return m_scale;
}

/**
 * @brief 根据 TRS 属性合成模型变换矩阵
 * @return 组装好的 Mat4 矩阵
 */
Mat4 Transform::GetMatrix() const {
    Mat4 scale = Mat4::Scale(m_scale.x, m_scale.y, m_scale.z);
    Mat4 rotX = Mat4::RotationX(m_rotation.x);
    Mat4 rotY = Mat4::RotationY(m_rotation.y);
    Mat4 rotZ = Mat4::RotationZ(m_rotation.z);
    Mat4 rotation = rotX * rotY * rotZ;
    Mat4 translation = Mat4::Translation(m_position.x, m_position.y, m_position.z);
    return scale * rotation * translation;
}

/**
 * @brief 计算法线变换矩阵
 * 
 * 法线矩阵应该是模型矩阵左上角 3x3 部分的逆转置。
 * 对于旋转+缩放，简化实现为缩放的逆再乘上旋转部分。
 */
Mat4 Transform::GetNormalMatrix() const {
    double sx = (m_scale.x != 0.0) ? (1.0 / m_scale.x) : 0.0;
    double sy = (m_scale.y != 0.0) ? (1.0 / m_scale.y) : 0.0;
    double sz = (m_scale.z != 0.0) ? (1.0 / m_scale.z) : 0.0;

    Mat4 scaleInv = Mat4::Scale(sx, sy, sz);
    Mat4 rotX = Mat4::RotationX(m_rotation.x);
    Mat4 rotY = Mat4::RotationY(m_rotation.y);
    Mat4 rotZ = Mat4::RotationZ(m_rotation.z);
    Mat4 rotation = rotX * rotY * rotZ;

    return scaleInv * rotation;
}

} // namespace SR
