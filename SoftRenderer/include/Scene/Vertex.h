#pragma once

#include "Math/Vec2.h"
#include "Math/Vec3.h"

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
    Vec3 tangent;   ///< 切线 (TX, TY, TZ)
};

} // namespace SR
