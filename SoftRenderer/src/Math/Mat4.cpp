#include "Math/Mat4.h"

#include <cmath>

namespace SR {

/**
 * @brief 创建并返回一个单位矩阵
 */
Mat4 Mat4::Identity() {
    Mat4 result{};
    result.m[0][0] = 1.0;
    result.m[1][1] = 1.0;
    result.m[2][2] = 1.0;
    result.m[3][3] = 1.0;
    return result;
}

/**
 * @brief 创建平移矩阵
 * @param x X 轴平移量
 * @param y Y 轴平移量
 * @param z Z 轴平移量
 */
Mat4 Mat4::Translation(double x, double y, double z) {
    Mat4 result = Identity();
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

/**
 * @brief 创建缩放矩阵
 * @param x X 轴缩放比例
 * @param y Y 轴缩放比例
 * @param z Z 轴缩放比例
 */
Mat4 Mat4::Scale(double x, double y, double z) {
    Mat4 result{};
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1.0;
    return result;
}

/**
 * @brief 创建绕 X 轴旋转的矩阵
 * @param radians 旋转弧度
 */
Mat4 Mat4::RotationX(double radians) {
    Mat4 result = Identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[1][1] = c;
    result.m[1][2] = s;
    result.m[2][1] = -s;
    result.m[2][2] = c;
    return result;
}

/**
 * @brief 创建绕 Y 轴旋转的矩阵
 * @param radians 旋转弧度
 */
Mat4 Mat4::RotationY(double radians) {
    Mat4 result = Identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[0][0] = c;
    result.m[0][2] = -s;
    result.m[2][0] = s;
    result.m[2][2] = c;
    return result;
}

/**
 * @brief 创建绕 Z 轴旋转的矩阵
 * @param radians 旋转弧度
 */
Mat4 Mat4::RotationZ(double radians) {
    Mat4 result = Identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[0][0] = c;
    result.m[0][1] = s;
    result.m[1][0] = -s;
    result.m[1][1] = c;
    return result;
}

/**
 * @brief 创建透视投影矩阵 (左手坐标系, [0, 1] 深度)
 * @param fovYRadians 垂直视野角度(弧度)
 * @param aspect 宽高比
 * @param zNear 近平面距离
 * @param zFar 远平面距离
 */
Mat4 Mat4::Perspective(double fovYRadians, double aspect, double zNear, double zFar) {
    Mat4 result{};
    double f = 1.0 / std::tan(fovYRadians * 0.5);

    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = zFar / (zFar - zNear);
    result.m[2][3] = 1.0;
    result.m[3][2] = (-zNear * zFar) / (zFar - zNear);
    return result;
}

/**
 * @brief 创建观察矩阵 (LookAt)
 * @param eye 相机位置
 * @param target 目标观察位置
 * @param up 上向量
 */
Mat4 Mat4::LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 zAxis = (target - eye).Normalized();
    Vec3 xAxis = Vec3::Cross(up, zAxis).Normalized();
    Vec3 yAxis = Vec3::Cross(zAxis, xAxis);

    Mat4 result = Identity();
    result.m[0][0] = xAxis.x;
    result.m[1][0] = xAxis.y;
    result.m[2][0] = xAxis.z;

    result.m[0][1] = yAxis.x;
    result.m[1][1] = yAxis.y;
    result.m[2][1] = yAxis.z;

    result.m[0][2] = zAxis.x;
    result.m[1][2] = zAxis.y;
    result.m[2][2] = zAxis.z;

    result.m[3][0] = -Vec3::Dot(xAxis, eye);
    result.m[3][1] = -Vec3::Dot(yAxis, eye);
    result.m[3][2] = -Vec3::Dot(zAxis, eye);
    return result;
}

/**
 * @brief 矩阵乘法
 */
Mat4 Mat4::operator*(const Mat4& rhs) const {
    Mat4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += m[row][k] * rhs.m[k][col];
            }
            result.m[row][col] = sum;
        }
    }
    return result;
}

/**
 * @brief 4x4 矩阵求逆（伴随矩阵法 / Cramer's rule）
 */
Mat4 Mat4::Inverse() const {
    // 展开 2x2 行列式的中间项
    double s0 = m[0][0] * m[1][1] - m[1][0] * m[0][1];
    double s1 = m[0][0] * m[1][2] - m[1][0] * m[0][2];
    double s2 = m[0][0] * m[1][3] - m[1][0] * m[0][3];
    double s3 = m[0][1] * m[1][2] - m[1][1] * m[0][2];
    double s4 = m[0][1] * m[1][3] - m[1][1] * m[0][3];
    double s5 = m[0][2] * m[1][3] - m[1][2] * m[0][3];

    double c5 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    double c4 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    double c3 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    double c2 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    double c1 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    double c0 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

    double det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    if (det == 0.0) return Identity(); // 奇异矩阵回退

    double invDet = 1.0 / det;

    Mat4 inv;
    inv.m[0][0] = ( m[1][1] * c5 - m[1][2] * c4 + m[1][3] * c3) * invDet;
    inv.m[0][1] = (-m[0][1] * c5 + m[0][2] * c4 - m[0][3] * c3) * invDet;
    inv.m[0][2] = ( m[3][1] * s5 - m[3][2] * s4 + m[3][3] * s3) * invDet;
    inv.m[0][3] = (-m[2][1] * s5 + m[2][2] * s4 - m[2][3] * s3) * invDet;

    inv.m[1][0] = (-m[1][0] * c5 + m[1][2] * c2 - m[1][3] * c1) * invDet;
    inv.m[1][1] = ( m[0][0] * c5 - m[0][2] * c2 + m[0][3] * c1) * invDet;
    inv.m[1][2] = (-m[3][0] * s5 + m[3][2] * s2 - m[3][3] * s1) * invDet;
    inv.m[1][3] = ( m[2][0] * s5 - m[2][2] * s2 + m[2][3] * s1) * invDet;

    inv.m[2][0] = ( m[1][0] * c4 - m[1][1] * c2 + m[1][3] * c0) * invDet;
    inv.m[2][1] = (-m[0][0] * c4 + m[0][1] * c2 - m[0][3] * c0) * invDet;
    inv.m[2][2] = ( m[3][0] * s4 - m[3][1] * s2 + m[3][3] * s0) * invDet;
    inv.m[2][3] = (-m[2][0] * s4 + m[2][1] * s2 - m[2][3] * s0) * invDet;

    inv.m[3][0] = (-m[1][0] * c3 + m[1][1] * c1 - m[1][2] * c0) * invDet;
    inv.m[3][1] = ( m[0][0] * c3 - m[0][1] * c1 + m[0][2] * c0) * invDet;
    inv.m[3][2] = (-m[3][0] * s3 + m[3][1] * s1 - m[3][2] * s0) * invDet;
    inv.m[3][3] = ( m[2][0] * s3 - m[2][1] * s1 + m[2][2] * s0) * invDet;

    return inv;
}

} // namespace SR
