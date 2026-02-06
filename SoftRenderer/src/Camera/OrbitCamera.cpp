#include "Camera/OrbitCamera.h"

#include <algorithm>
#include <cmath>

namespace SR {

/**
 * @brief 设置相机与观察点之间的线性距离
 */
void OrbitCamera::SetDistance(double distance) {
    m_distance = distance;
}

/**
 * @brief 更新相机的旋转角度，并对俯仰角进行限位
 * @param yaw 偏航角增量 (弧度)
 * @param pitch 俯仰角增量 (弧度)
 */
void OrbitCamera::Rotate(double yaw, double pitch) {
    m_yaw += yaw;
    m_pitch += pitch;

    // 限制俯仰角，防止越过顶部
    constexpr double kHalfPi = 3.14159265358979323846 * 0.5;
    constexpr double kEpsilon = 1e-4;
    m_pitch = std::clamp(m_pitch, -kHalfPi + kEpsilon, kHalfPi - kEpsilon);

    // 标准化偏航角到 [-PI, PI] 范围
    if (m_yaw > 3.14159265358979323846 || m_yaw < -3.14159265358979323846) {
        m_yaw = std::fmod(m_yaw, 3.14159265358979323846 * 2.0);
    }
}

/**
 * @brief 设置相机观察的中心点坐标
 */
void OrbitCamera::SetTarget(const Vec3& target) {
    m_target = target;
}

/**
 * @brief 计算相机的视图矩阵 (基于 LookAt 算法)
 * @return 旋转和平移后的视图变换矩阵
 */
Mat4 OrbitCamera::GetViewMatrix() const {
    double cosPitch = std::cos(m_pitch);
    double sinPitch = std::sin(m_pitch);
    double cosYaw = std::cos(m_yaw);
    double sinYaw = std::sin(m_yaw);

    // 根据球面坐标计算相机位置
    Vec3 eye{
        m_target.x + m_distance * cosPitch * sinYaw,
        m_target.y + m_distance * sinPitch,
        m_target.z + m_distance * cosPitch * cosYaw
    };

    Vec3 up{0.0, 1.0, 0.0};
    return Mat4::LookAt(eye, m_target, up);
}

/**
 * @brief 获取相机在世界空间中的实时三维位置
 */
Vec3 OrbitCamera::GetPosition() const {
    double cosPitch = std::cos(m_pitch);
    double sinPitch = std::sin(m_pitch);
    double cosYaw = std::cos(m_yaw);
    double sinYaw = std::sin(m_yaw);

    return Vec3{
        m_target.x + m_distance * cosPitch * sinYaw,
        m_target.y + m_distance * sinPitch,
        m_target.z + m_distance * cosPitch * cosYaw
    };
}

} // namespace SR
