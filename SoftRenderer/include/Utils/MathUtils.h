#pragma once

/**
 * @file MathUtils.h
 * @brief 公共数学工具函数（内联），供整个渲染引擎复用。
 *        替代各模块中重复定义的 Lerp / Clamp / Saturate。
 */

#include <algorithm>

#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"

namespace SR {

/** @brief 将值钳制到 [minValue, maxValue] 范围内 */
inline double Clamp(double v, double minValue, double maxValue) {
    return std::max(minValue, std::min(maxValue, v));
}

/** @brief 将值钳制到 [0, 1] 范围内（饱和） */
inline double Saturate(double v) {
    return Clamp(v, 0.0, 1.0);
}

/** @brief 标量线性插值：a + (b - a) * t */
inline double Lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

/** @brief Vec2 线性插值 */
inline Vec2 Lerp(const Vec2& a, const Vec2& b, double t) {
    return Vec2{Lerp(a.x, b.x, t), Lerp(a.y, b.y, t)};
}

/** @brief Vec3 线性插值 */
inline Vec3 Lerp(const Vec3& a, const Vec3& b, double t) {
    return Vec3{Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t)};
}

/** @brief Vec4 线性插值 */
inline Vec4 Lerp(const Vec4& a, const Vec4& b, double t) {
    return Vec4{Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t), Lerp(a.w, b.w, t)};
}

} // namespace SR
