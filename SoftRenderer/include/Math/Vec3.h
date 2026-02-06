#pragma once

#include "SoftRendererExport.h"
#include <cmath>

namespace SR {

/**
 * @brief 三维向量结构
 */
struct SR_API Vec3 {
    double x = 0.0; ///< X 分量
    double y = 0.0; ///< Y 分量
    double z = 0.0; ///< Z 分量

    /** @brief 默认构造函数 */
    constexpr Vec3() = default;
    /** @brief 带参构造函数 */
    constexpr Vec3(double xValue, double yValue, double zValue) : x(xValue), y(yValue), z(zValue) {}

    /** @brief 向量加法 */
    inline Vec3 operator+(const Vec3& rhs) const {
        return Vec3{x + rhs.x, y + rhs.y, z + rhs.z};
    }
    
    /** @brief 向量减法 */
    inline Vec3 operator-(const Vec3& rhs) const {
        return Vec3{x - rhs.x, y - rhs.y, z - rhs.z};
    }
    
    /** @brief 向量缩放 */
    inline Vec3 operator*(double scalar) const {
        return Vec3{x * scalar, y * scalar, z * scalar};
    }
    
    /** @brief 向量除法 */
    inline Vec3 operator/(double scalar) const {
        double inv = 1.0 / scalar;
        return Vec3{x * inv, y * inv, z * inv};
    }
    
    /** @brief 计算向量长度的平方 */
    inline double LengthSquared() const {
        return x * x + y * y + z * z;
    }
    
    /** @brief 计算向量长度 */
    inline double Length() const {
        return std::sqrt(LengthSquared());
    }
    
    /** @brief 返回归一化向量 */
    inline Vec3 Normalized() const {
        double lenSq = LengthSquared();
        if (lenSq < 1e-12) {
            return Vec3{};
        }
        double invLen = 1.0 / std::sqrt(lenSq);
        return Vec3{x * invLen, y * invLen, z * invLen};
    }
    
    /** @brief 原地归一化 */
    inline void NormalizeInPlace() {
        double lenSq = LengthSquared();
        if (lenSq < 1e-12) {
            x = y = z = 0.0;
            return;
        }
        double invLen = 1.0 / std::sqrt(lenSq);
        x *= invLen;
        y *= invLen;
        z *= invLen;
    }
    
    /** @brief 向量点积 */
    static inline double Dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    
    /** @brief 向量叉积 */
    static inline Vec3 Cross(const Vec3& a, const Vec3& b) {
        return Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

} // namespace SR
