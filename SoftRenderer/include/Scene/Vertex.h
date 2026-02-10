#pragma once

#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"

namespace SR {

/**
 * @brief 顶点数据结构
 * 
 * 包含坐标、法线、纹理坐标和切线。使用 16 字节对齐以支持未来的 SIMD 优化。
 */
struct alignas(16) Vertex {
    Vec3 position;  ///< 位置 (X, Y, Z)
    Vec3 normal;    ///< 法线 (NX, NY, NZ)
    Vec2 texCoord;  ///< 纹理坐标 (U, V)
    Vec2 texCoord1; ///< 第二套纹理坐标 (U, V)
    Vec4 color{1.0, 1.0, 1.0, 1.0}; ///< 顶点颜色 (R, G, B, A)
    Vec3 tangent;   ///< 切线 (TX, TY, TZ)
    double tangentW = 1.0; ///< 切线 W 分量 (+1/-1)，决定副切线方向
};

} // namespace SR
