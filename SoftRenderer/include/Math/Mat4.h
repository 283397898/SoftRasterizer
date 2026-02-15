#pragma once

#include "Math/Vec3.h"
#include "Math/Vec4.h"

namespace SR {

/**
 * @brief 4x4 矩阵结构
 */
struct Mat4 {
    double m[4][4] = {}; ///< 矩阵数据 [行][列]

    /** @brief 返回单位矩阵 */
    static Mat4 Identity();
    /** @brief 创建平移矩阵 */
    static Mat4 Translation(double x, double y, double z);
    /** @brief 创建缩放矩阵 */
    static Mat4 Scale(double x, double y, double z);
    /** @brief 创建绕 X 轴旋转矩阵 */
    static Mat4 RotationX(double radians);
    /** @brief 创建绕 Y 轴旋转矩阵 */
    static Mat4 RotationY(double radians);
    /** @brief 创建绕 Z 轴旋转矩阵 */
    static Mat4 RotationZ(double radians);
    /** @brief 创建透视投影矩阵 */
    static Mat4 Perspective(double fovYRadians, double aspect, double zNear, double zFar);
    /** @brief 创建观察矩阵 */
    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up);

    /** @brief 矩阵乘以四维向量 */
    Vec4 Multiply(const Vec4& v) const {
        Vec4 out{};
        out.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + v.w * m[3][0];
        out.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + v.w * m[3][1];
        out.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + v.w * m[3][2];
        out.w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + v.w * m[3][3];
        return out;
    }
    /** @brief 矩阵乘法 */
    Mat4 operator*(const Mat4& rhs) const;
    /** @brief 计算逆矩阵（通用 4x4 逆矩阵，基于伴随矩阵法） */
    Mat4 Inverse() const;
};

} // namespace SR
