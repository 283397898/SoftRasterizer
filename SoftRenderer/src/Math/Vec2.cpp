#include "Math/Vec2.h"

#include <cmath>

namespace SR {

Vec2 Vec2::operator+(const Vec2& rhs) const {
    return Vec2{x + rhs.x, y + rhs.y};
}

Vec2 Vec2::operator-(const Vec2& rhs) const {
    return Vec2{x - rhs.x, y - rhs.y};
}

Vec2 Vec2::operator*(double scalar) const {
    return Vec2{x * scalar, y * scalar};
}

Vec2 Vec2::operator/(double scalar) const {
    return Vec2{x / scalar, y / scalar};
}

double Vec2::Length() const {
    return std::sqrt(x * x + y * y);
}

Vec2 Vec2::Normalized() const {
    double len = Length();
    if (len == 0.0) {
        return Vec2{};
    }
    return (*this) / len;
}

} // namespace SR
