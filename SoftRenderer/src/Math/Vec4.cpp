#include "Math/Vec4.h"

namespace SR {

Vec4 Vec4::operator+(const Vec4& rhs) const {
    return Vec4{x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
}

Vec4 Vec4::operator-(const Vec4& rhs) const {
    return Vec4{x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w};
}

Vec4 Vec4::operator*(double scalar) const {
    return Vec4{x * scalar, y * scalar, z * scalar, w * scalar};
}

Vec4 Vec4::operator/(double scalar) const {
    return Vec4{x / scalar, y / scalar, z / scalar, w / scalar};
}

} // namespace SR
