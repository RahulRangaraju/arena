// AoS vs SoA micro-benchmark for the enemy "chase the player" update (Step 7).
//
// Both versions do exactly the same math; only the memory layout differs:
//   AoS: std::vector<EnemyAoS>, 20-byte structs (pos, speed, radius, hp)
//   SoA: Enemies, one contiguous array per field (the loop touches x, y, speed only)
//
// Build: part of the normal CMake build (target bench_layout). Run: ./build/bench_layout
#include "Enemies.hpp"
#include "Vec2.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

struct EnemyAoS {   // the Step 4-6 layout
    Vec2 pos;
    float speed = 0.f;
    float radius = 12.f;
    int hp = 1;
};

static void updateAoS(std::vector<EnemyAoS>& es, Vec2 player, float dt) {
    for (EnemyAoS& e : es) {
        float dx = player.x - e.pos.x, dy = player.y - e.pos.y;
        float inv = 1.f / std::sqrt(dx * dx + dy * dy + 1e-6f);
        e.pos.x += dx * inv * e.speed * dt;
        e.pos.y += dy * inv * e.speed * dt;
    }
}

static void updateSoA(Enemies& es, Vec2 player, float dt) {
    float* __restrict ex = es.x.data();
    float* __restrict ey = es.y.data();
    const float* __restrict sp = es.speed.data();
    const std::size_t n = es.count;
    for (std::size_t i = 0; i < n; ++i) {
        float dx = player.x - ex[i], dy = player.y - ey[i];
        float inv = 1.f / std::sqrt(dx * dx + dy * dy + 1e-6f);
        ex[i] += dx * inv * sp[i] * dt;
        ey[i] += dy * inv * sp[i] * dt;
    }
}

// Runs fn `iters` times per repeat and returns the median ns per enemy-update.
template <class Fn>
static double medianNsPerEnemy(std::size_t n, int iters, Fn&& fn) {
    std::vector<double> runs;
    for (int rep = 0; rep < 7; ++rep) {
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i) fn();
        auto t1 = std::chrono::steady_clock::now();
        runs.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / (double(iters) * n));
    }
    std::sort(runs.begin(), runs.end());
    return runs[runs.size() / 2];
}

int main() {
    constexpr float kDt = 1.f / 120.f;
    const Vec2 player{640.f, 360.f};
    std::printf("layout,enemies,ns_per_enemy,ms_per_update\n");

    double checksum = 0.0;   // printed at the end so the compiler can't delete the work
    for (std::size_t n : {1000ul, 10000ul, 20000ul, 1000000ul}) {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> X(0.f, 1280.f), Y(0.f, 720.f), S(80.f, 200.f);

        std::vector<EnemyAoS> aos(n);
        Enemies soa(n);
        for (std::size_t i = 0; i < n; ++i) {
            float x = X(rng), y = Y(rng), s = S(rng);
            aos[i].pos = {x, y};
            aos[i].speed = s;
            soa.spawn(x, y, s, 12.f, 1);
        }

        const int iters = n >= 1000000 ? 20 : int(4'000'000 / n);
        double a = medianNsPerEnemy(n, iters, [&] { updateAoS(aos, player, kDt); });
        double s = medianNsPerEnemy(n, iters, [&] { updateSoA(soa, player, kDt); });
        std::printf("aos,%zu,%.3f,%.4f\n", n, a, a * n / 1e6);
        std::printf("soa,%zu,%.3f,%.4f\n", n, s, s * n / 1e6);
        std::printf("# %zu enemies: SoA is %.2fx the speed of AoS (%+.0f%% time)\n", n, a / s,
                    (s - a) / a * 100.0);
        checksum += aos[n / 2].pos.x + soa.x[n / 2];
    }
    std::printf("# checksum %.3f\n", checksum);
    return 0;
}
