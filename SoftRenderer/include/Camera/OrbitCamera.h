#pragma once

#include "SoftRendererExport.h"
#include "Math/Mat4.h"
#include "Math/Vec3.h"

namespace SR {

/**
 * @brief 轨道相机，通过经纬度、距离和目标点来控制观察视角
 */
class SR_API OrbitCamera {
public:
    /** @brief 设置相机距离目标点的距离 */
    void SetDistance(double distance);
    /** @brief 获取当前观测距离 */
    double GetDistance() const { return m_distance; }

    /** @brief 旋转相机 (偏航角和俯仰角，弧度制) */
    void Rotate(double yaw, double pitch);

    /** @brief 设置观测目标中心点 */
    void SetTarget(const Vec3& target);
    /** @brief 获取当前观测目标点 */
    Vec3 GetTarget() const { return m_target; }

    /** @brief 计算并获取当前的视图矩阵 */
    Mat4 GetViewMatrix() const;
    /** @brief 计算并获取相机在世界空间中的坐标 */
    Vec3 GetPosition() const;

private:
    double m_distance = 5.0;            ///< 离目标点的距离
    double m_yaw = 0.0;                 ///< 偏航角 (左右旋转)
    double m_pitch = 0.0;               ///< 俯仰角 (上下旋转)
    Vec3 m_target{0.0, 0.0, 0.0};      ///< 目标观测点坐标
};

} // namespace SR
