#pragma once
#include <cmath>

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec2 normalized() const {
        float len = length();
        return len > 0.f ? Vec2{x / len, y / len} : Vec2{};
    }
};

// Two circles overlap when the distance between centers is less than the sum of radii.
// Compare squared distances so we never call sqrt. Touching (distance == r) is NOT overlap.
inline bool circlesOverlap(Vec2 a, float ra, Vec2 b, float rb) {
    float r = ra + rb;
    return (a - b).lengthSq() < r * r;
}
