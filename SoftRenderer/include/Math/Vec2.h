#pragma once

namespace SR {

/**
 * @brief 二维向量结构
 */
struct Vec2 {
    double x = 0.0; ///< X 分量
    double y = 0.0; ///< Y 分量

    /** @brief 默认构造函数 */
    constexpr Vec2() = default;
    /** @brief 带参构造函数 */
    constexpr Vec2(double xValue, double yValue) : x(xValue), y(yValue) {}

    /** @brief 向量加法 */
    Vec2 operator+(const Vec2& rhs) const;
    /** @brief 向量减法 */
    Vec2 operator-(const Vec2& rhs) const;
    /** @brief 向量缩放 */
    Vec2 operator*(double scalar) const;
    /** @brief 向量除法 */
    Vec2 operator/(double scalar) const;
    /** @brief 计算向量长度 */
    double Length() const;
    /** @brief 返回归一化向量 */
    Vec2 Normalized() const;
};

} // namespace SR
