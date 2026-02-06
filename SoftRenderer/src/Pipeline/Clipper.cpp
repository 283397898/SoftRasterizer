#include "Pipeline/Clipper.h"

namespace SR {

namespace {

Vec4 Lerp(const Vec4& a, const Vec4& b, double t) {
    return Vec4{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

Vec3 Lerp(const Vec3& a, const Vec3& b, double t) {
    return Vec3{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

Vec2 Lerp(const Vec2& a, const Vec2& b, double t) {
    return Vec2{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    };
}

/**
 * @brief 判断顶点相对于裁剪平面的位置值
 * @param plane 0-5 分别对应 x+, x-, y+, y-, z+, z- 六个视锥面
 */
double PlaneValue(const ClipVertex& v, int plane) {
    switch (plane) {
    case 0: return v.clip.x + v.clip.w; // x >= -w
    case 1: return v.clip.w - v.clip.x; // x <= w
    case 2: return v.clip.y + v.clip.w; // y >= -w
    case 3: return v.clip.w - v.clip.y; // y <= w
    case 4: return v.clip.z;            // z >= 0 (近平面)
    case 5: return v.clip.w - v.clip.z; // z <= w (远平面)
    default: return 1.0;
    }
}

/**
 * @brief 使用单个裁剪平面对多边形进行裁剪
 */
std::vector<ClipVertex> ClipPolygonAgainstPlane(const std::vector<ClipVertex>& input, int plane) {
    std::vector<ClipVertex> output;
    if (input.empty()) {
        return output;
    }

    ClipVertex prev = input.back();
    double prevValue = PlaneValue(prev, plane);
    bool prevInside = prevValue >= 0.0;

    for (const ClipVertex& curr : input) {
        double currValue = PlaneValue(curr, plane);
        bool currInside = currValue >= 0.0;

        if (prevInside && currInside) {
            output.push_back(curr);
        } else if (prevInside && !currInside) {
            double t = prevValue / (prevValue - currValue);
            ClipVertex intersect{};
            intersect.clip = Lerp(prev.clip, curr.clip, t);
            intersect.normal = Lerp(prev.normal, curr.normal, t);
            intersect.world = Lerp(prev.world, curr.world, t);
            intersect.texCoord = Lerp(prev.texCoord, curr.texCoord, t);
            intersect.tangent = Lerp(prev.tangent, curr.tangent, t);
            output.push_back(intersect);
        } else if (!prevInside && currInside) {
            double t = prevValue / (prevValue - currValue);
            ClipVertex intersect{};
            intersect.clip = Lerp(prev.clip, curr.clip, t);
            intersect.normal = Lerp(prev.normal, curr.normal, t);
            intersect.world = Lerp(prev.world, curr.world, t);
            intersect.texCoord = Lerp(prev.texCoord, curr.texCoord, t);
            intersect.tangent = Lerp(prev.tangent, curr.tangent, t);
            output.push_back(intersect);
            output.push_back(curr);
        }

        prev = curr;
        prevValue = currValue;
        prevInside = currInside;
    }

    return output;
}

} // namespace

/**
 * @brief 对三角形进行视锥体裁剪 (六个平面全裁剪)
 */
std::vector<ClipVertex> Clipper::ClipTriangle(const ClipVertex& a,
                                              const ClipVertex& b,
                                              const ClipVertex& c) const {
    std::vector<ClipVertex> poly{a, b, c};
    for (int plane = 0; plane < 6; ++plane) {
        poly = ClipPolygonAgainstPlane(poly, plane);
        if (poly.empty()) {
            break;
        }
    }
    return poly;
}

} // namespace SR
