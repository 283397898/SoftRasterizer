#pragma once

#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include <immintrin.h>

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

    /**
     * @brief 矩阵乘以四维向量（AVX2+FMA3 优化）
     *
     * 计算 result[j] = Σ_i v[i] * m[i][j]，即 v^T * M。
     * 每行 m[i] 恰好 4 个 double = 1 个 __m256d 寄存器。
     * 用 3 次 FMA 替代 16 mul + 12 add，指令数减少 ~4×。
     */
    inline Vec4 Multiply(const Vec4& v) const {
        // 加载矩阵的 4 行（每行 4 个 double，连续内存）
        __m256d row0 = _mm256_loadu_pd(m[0]);
        __m256d row1 = _mm256_loadu_pd(m[1]);
        __m256d row2 = _mm256_loadu_pd(m[2]);
        __m256d row3 = _mm256_loadu_pd(m[3]);

        // 广播向量的每个分量
        __m256d vx = _mm256_set1_pd(v.x);
        __m256d vy = _mm256_set1_pd(v.y);
        __m256d vz = _mm256_set1_pd(v.z);
        __m256d vw = _mm256_set1_pd(v.w);

        // result = v.x*row0 + v.y*row1 + v.z*row2 + v.w*row3
        // 使用 FMA3: fmadd(a, b, c) = a*b + c，流水线友好
        __m256d result = _mm256_mul_pd(vx, row0);
        result = _mm256_fmadd_pd(vy, row1, result);
        result = _mm256_fmadd_pd(vz, row2, result);
        result = _mm256_fmadd_pd(vw, row3, result);

        // Vec4 有 4 个连续 double (x,y,z,w)，直接存储
        Vec4 out;
        _mm256_storeu_pd(&out.x, result);
        return out;
    }
    /** @brief 矩阵乘法（AVX2+FMA3 优化） */
    Mat4 operator*(const Mat4& rhs) const;
    /** @brief 计算逆矩阵（通用 4x4 逆矩阵，基于伴随矩阵法） */
    Mat4 Inverse() const;
};

} // namespace SR
