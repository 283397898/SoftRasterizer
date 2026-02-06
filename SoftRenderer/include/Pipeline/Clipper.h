#pragma once

#include <vector>

#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Vec4.h"

namespace SR {

/**
 * @brief 裁剪顶点结构，包含裁剪空间坐标及插值属性
 */
struct ClipVertex {
    Vec4 clip;      ///< 裁剪空间坐标 (NDC 变换前)
    Vec3 normal;    ///< 法线
    Vec3 world;     ///< 世界空间位置
    Vec2 texCoord;  ///< 纹理坐标
    Vec3 tangent;   ///< 切线
};

/**
 * @brief 裁剪器类，实现 Sutherland-Hodgman 裁剪算法
 */
class Clipper {
public:
    /** 
     * @brief 对单个三角形进行近平面裁剪 (Z < 0)
     * @return 裁剪后生成的顶点多边形列表
     */
    std::vector<ClipVertex> ClipTriangle(const ClipVertex& a,
                                         const ClipVertex& b,
                                         const ClipVertex& c) const;
};

} // namespace SR
