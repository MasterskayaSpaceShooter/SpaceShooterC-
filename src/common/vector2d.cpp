#include "math_utils.h"

namespace math{
     Vector2D::Vector2D(Scalar x, Scalar y) : x{x}, y{y} {
     };

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

     bool Vector2D::operator==(const Vector2D& rhs) const {
          return x == rhs.x && y == rhs.y;
     }
};
