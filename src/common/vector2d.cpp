#include "math_utils.h"

namespace math {
Vector2D::Vector2D(Scalar x, Scalar y) : x{x}, y{y} {};

Vector2D& Vector2D::operator+=(const Vector2D& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
}

Vector2D& Vector2D::operator-=(const Vector2D& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
}

Vector2D& Vector2D::operator*=(Scalar s) {
    x *= s;
    y *= s;
    return *this;
}

[[nodiscard]] Scalar Vector2D::lengthSquared() const {
    return x * x + y * y;
}

[[nodiscard]] Scalar Vector2D::length() const {
    return std::sqrt(lengthSquared());
}

[[nodiscard]] Vector2D Vector2D::normalized() const {
    const Scalar len = length();
    if (len == 0) {
        return {};
    }
    return Vector2D{x / len, y / len};
}

};  // namespace math
