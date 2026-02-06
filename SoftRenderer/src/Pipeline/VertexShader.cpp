#include "Pipeline/VertexShader.h"

namespace SR {

/**
 * @brief 设置变换矩阵
 */
void VertexShader::SetMVP(const Mat4& mvp) {
    m_mvp = mvp;
}

/**
 * @brief 执行顶点投影变换
 */
Vec4 VertexShader::TransformPosition(const Vec4& position) const {
    return m_mvp.Multiply(position);
}

} // namespace SR
