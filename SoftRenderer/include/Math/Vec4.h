#pragma once

namespace SR {

/**
 * @brief 四维向量结构
 */
struct Vec4 {
    double x = 0.0; ///< X 分量
    double y = 0.0; ///< Y 分量
    double z = 0.0; ///< Z 分量
    double w = 0.0; ///< W 分量

    /** @brief 默认构造函数 */
    constexpr Vec4() = default;
    /** @brief 带参构造函数 */
    constexpr Vec4(double xValue, double yValue, double zValue, double wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}

    /** @brief 向量加法 */
    Vec4 operator+(const Vec4& rhs) const;
    /** @brief 向量减法 */
    Vec4 operator-(const Vec4& rhs) const;
    /** @brief 向量缩放 */
    Vec4 operator*(double scalar) const;
    /** @brief 向量除法 */
    Vec4 operator/(double scalar) const;
};

} // namespace SR
