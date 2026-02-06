#pragma once

#include <vector>

namespace SR {

/**
 * @brief 深度缓冲区类，用于存储每像素的深度值 (Z-Buffer)
 */
class DepthBuffer {
public:
    /** @brief 调整缓冲区大小 */
    void Resize(int width, int height);
    /** @brief 清除深度值 (默认为 1.0，即最远距离) */
    void Clear(double depthValue = 1.0);

    /** @brief 获取原始数据指针 */
    double* Data();
    /** @brief 获取只读原始数据指针 */
    const double* Data() const;
    /** @brief 获取缓冲区宽度 */
    int GetWidth() const;
    /** @brief 获取缓冲区高度 */
    int GetHeight() const;

private:
    int m_width = 0;              ///< 缓冲区宽度
    int m_height = 0;             ///< 缓冲区高度
    std::vector<double> m_depth; ///< 存储深度值的数组 (double 精度)
};

} // namespace SR
