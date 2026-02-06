#pragma once

#include "Math/Mat4.h"
#include "Math/Vec4.h"

namespace SR {

/**
 * @brief 顶点着色器类，负责坐标空间变换
 */
class VertexShader {
public:
    /** @brief 设置当前渲染项的 MVP 矩阵 */
    void SetMVP(const Mat4& mvp);
    /** @brief 对输入顶点位置执行变换 (通常为 Object 到 Clip 空间) */
    Vec4 TransformPosition(const Vec4& position) const;

private:
    Mat4 m_mvp = Mat4::Identity();
};

} // namespace SR
