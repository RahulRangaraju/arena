#pragma once
#include <cstddef>
#include <vector>

// Struct-of-arrays enemy storage. Each field lives in its own contiguous array, so a loop
// that only needs x, y and speed streams through just those bytes (better cache use, and
// the compiler can vectorize it). Same swap-and-pop semantics as Pool<T>.
struct Enemies {
    std::vector<float> x, y, speed, radius;
    std::vector<float> vx, vy;   // drift velocity, only used in stress mode (Step 9)
    std::vector<int> hp;
    std::size_t count = 0;

    explicit Enemies(std::size_t cap)
        : x(cap), y(cap), speed(cap), radius(cap), vx(cap), vy(cap), hp(cap) {}

    std::size_t capacity() const { return x.size(); }

    // Returns the new index, or -1 when full.
    long spawn(float px, float py, float spd, float r, int health) {
        if (count == capacity()) return -1;
        x[count] = px; y[count] = py; speed[count] = spd;
        radius[count] = r; hp[count] = health;
        vx[count] = 0.f; vy[count] = 0.f;
        return static_cast<long>(count++);
    }

    void kill(std::size_t i) {   // swap-and-pop every field
        --count;
        x[i] = x[count]; y[i] = y[count]; speed[i] = speed[count];
        radius[i] = radius[count]; hp[i] = hp[count];
        vx[i] = vx[count]; vy[i] = vy[count];
    }

    void clear() { count = 0; }
};
