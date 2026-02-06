#pragma once

#include "SoftRendererExport.h"
#include "Math/Mat4.h"
#include "Math/Vec3.h"

namespace SR {

/**
 * @brief 变换组件，包含平移、旋转和缩放信息
 */
class SR_API Transform {
public:
    /** @brief 设置位置 */
    void SetPosition(const Vec3& position);
    /** @brief 设置旋转角度 (欧拉角，弧度制) */
    void SetRotation(const Vec3& rotationRadians);
    /** @brief 设置缩放比例 */
    void SetScale(const Vec3& scale);

    /** @brief 获取当前位置 */
    const Vec3& GetPosition() const;
    /** @brief 获取当前旋转角度 */
    const Vec3& GetRotation() const;
    /** @brief 获取当前缩放比例 */
    const Vec3& GetScale() const;

    /** @brief 根据当前 TRS 参数计算 4x4 变换矩阵 */
    Mat4 GetMatrix() const;
    /** @brief 根据当前变换计算法线矩阵 (处理非等比缩放) */
    Mat4 GetNormalMatrix() const;

private:
    Vec3 m_position{0.0f, 0.0f, 0.0f}; ///< 位置 (X, Y, Z)
    Vec3 m_rotation{0.0f, 0.0f, 0.0f}; ///< 旋转 (Pitch, Yaw, Roll)
    Vec3 m_scale{1.0f, 1.0f, 1.0f};    ///< 缩放 (SX, SY, SZ)
};

} // namespace SR
