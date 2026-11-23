#include "Enemies.hpp"
#include "Pool.hpp"
#include "SpatialGrid.hpp"
#include "Vec2.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <random>
#include <vector>

// Property test: for random scenes, the grid must find EXACTLY the same hits as checking
// every pair. This is the test that proves the optimization didn't change behavior.
TEST(SpatialGrid, MatchesBruteForce) {
    constexpr float W = 1280, H = 720, R_ENEMY = 12, R_BULLET = 3;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> X(-20.f, W + 20.f), Y(-20.f, H + 20.f);

    for (int trial = 0; trial < 50; ++trial) {
        std::vector<Vec2> enemies(2000), bullets(500);
        for (auto& e : enemies) e = {X(rng), Y(rng)};
        for (auto& b : bullets) b = {X(rng), Y(rng)};

        SpatialGrid grid(W, H, 32.f);
        grid.build(enemies.size(), [&](std::size_t i) { return enemies[i]; });

        for (const Vec2& b : bullets) {
            std::vector<int> brute, fromGrid;
            for (int i = 0; i < int(enemies.size()); ++i)
                if (circlesOverlap(b, R_BULLET, enemies[i], R_ENEMY)) brute.push_back(i);
            grid.query(b, R_BULLET + R_ENEMY, [&](int i) {
                if (circlesOverlap(b, R_BULLET, enemies[i], R_ENEMY)) fromGrid.push_back(i);
                return false;   // don't stop: collect all
            });
            std::sort(fromGrid.begin(), fromGrid.end());
            ASSERT_EQ(brute, fromGrid);
        }
    }
}

// Same property for every cell size we benchmark, including cells smaller than an enemy.
TEST(SpatialGrid, MatchesBruteForceAtEveryCellSize) {
    constexpr float W = 1280, H = 720, R_ENEMY = 12, R_BULLET = 3;
    for (float cell : {8.f, 16.f, 32.f, 64.f, 128.f}) {
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> X(-20.f, W + 20.f), Y(-20.f, H + 20.f);
        std::vector<Vec2> enemies(3000), bullets(300);
        for (auto& e : enemies) e = {X(rng), Y(rng)};
        for (auto& b : bullets) b = {X(rng), Y(rng)};

        SpatialGrid grid(W, H, cell);
        grid.build(enemies.size(), [&](std::size_t i) { return enemies[i]; });
        for (const Vec2& b : bullets) {
            int brute = 0, fromGrid = 0;
            for (const Vec2& e : enemies) brute += circlesOverlap(b, R_BULLET, e, R_ENEMY);
            grid.query(b, R_BULLET + R_ENEMY, [&](int i) {
                fromGrid += circlesOverlap(b, R_BULLET, enemies[i], R_ENEMY);
                return false;
            });
            ASSERT_EQ(brute, fromGrid) << "cell size " << cell;
        }
    }
}

TEST(SpatialGrid, EveryItemAppearsExactlyOnce) {
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> X(-100.f, 1400.f), Y(-100.f, 800.f);
    std::vector<Vec2> pts(5000);
    for (auto& p : pts) p = {X(rng), Y(rng)};
    SpatialGrid grid(1280, 720, 32.f);
    grid.build(pts.size(), [&](std::size_t i) { return pts[i]; });

    std::vector<int> seen(pts.size(), 0);   // a query covering the whole arena sees everything
    grid.query({640, 360}, 10000.f, [&](int i) { ++seen[i]; return false; });
    for (int s : seen) ASSERT_EQ(s, 1);
}

TEST(SpatialGrid, QueryStopsEarlyWhenCallbackReturnsTrue) {
    std::vector<Vec2> pts(100, Vec2{100.f, 100.f});
    SpatialGrid grid(1280, 720, 32.f);
    grid.build(pts.size(), [&](std::size_t i) { return pts[i]; });
    int calls = 0;
    grid.query({100, 100}, 15.f, [&](int) { ++calls; return true; });
    EXPECT_EQ(calls, 1);
}

TEST(SpatialGrid, RebuildWithFewerItems) {   // stale data from a bigger build must not leak
    SpatialGrid grid(1280, 720, 32.f);
    std::vector<Vec2> many(1000, Vec2{50.f, 50.f});
    grid.build(many.size(), [&](std::size_t i) { return many[i]; });
    std::vector<Vec2> few{{50.f, 50.f}, {600.f, 300.f}};
    grid.build(few.size(), [&](std::size_t i) { return few[i]; });
    int found = 0;
    grid.query({640, 360}, 10000.f, [&](int i) { EXPECT_LT(i, 2); ++found; return false; });
    EXPECT_EQ(found, 2);
}

// ---- circlesOverlap edge cases ------------------------------------------------------------

TEST(CirclesOverlap, TouchingIsNotOverlapping) {
    EXPECT_FALSE(circlesOverlap({0, 0}, 3.f, {15, 0}, 12.f));   // distance == r1 + r2
}

TEST(CirclesOverlap, SlightlyCloserOverlaps) {
    EXPECT_TRUE(circlesOverlap({0, 0}, 3.f, {14.99f, 0}, 12.f));
}

TEST(CirclesOverlap, SameCenterAndDiagonal) {
    EXPECT_TRUE(circlesOverlap({5, 5}, 1.f, {5, 5}, 1.f));
    EXPECT_TRUE(circlesOverlap({0, 0}, 5.f, {6, 6}, 5.f));    // distance ~8.49 < 10
    EXPECT_FALSE(circlesOverlap({0, 0}, 5.f, {8, 8}, 5.f));   // distance ~11.3 > 10
}

// ---- Pool ----------------------------------------------------------------------------------

TEST(Pool, SpawnUpToCapacityThenNull) {
    Pool<int> p(3);
    EXPECT_NE(p.spawn(), nullptr);
    EXPECT_NE(p.spawn(), nullptr);
    EXPECT_NE(p.spawn(), nullptr);
    EXPECT_EQ(p.spawn(), nullptr);
    EXPECT_EQ(p.size(), 3u);
    EXPECT_EQ(p.capacity(), 3u);
}

TEST(Pool, KillFromMiddleSwapsLastIn) {
    Pool<int> p(5);
    for (int v = 0; v < 5; ++v) *p.spawn() = v * 10;   // [0, 10, 20, 30, 40]
    p.kill(1);                                          // -> [0, 40, 20, 30]
    ASSERT_EQ(p.size(), 4u);
    EXPECT_EQ(p[0], 0);
    EXPECT_EQ(p[1], 40);
    EXPECT_EQ(p[2], 20);
    EXPECT_EQ(p[3], 30);
}

TEST(Pool, BackwardKillLoopRemovesExactlyTheMarked) {
    Pool<int> p(100);
    for (int v = 0; v < 100; ++v) *p.spawn() = v;
    for (std::size_t i = p.size(); i-- > 0;)            // the pattern used in the game
        if (p[i] % 3 == 0) p.kill(i);
    std::vector<int> left;
    for (std::size_t i = 0; i < p.size(); ++i) left.push_back(p[i]);
    std::sort(left.begin(), left.end());
    std::vector<int> expected;
    for (int v = 0; v < 100; ++v) if (v % 3 != 0) expected.push_back(v);
    EXPECT_EQ(left, expected);
}

TEST(Pool, SpawnResetsReusedSlot) {
    struct Item { int a = 7; };
    Pool<Item> p(1);
    p.spawn()->a = 99;
    p.kill(0);
    EXPECT_EQ(p.spawn()->a, 7);   // a reused slot starts fresh
}

// ---- Enemies (SoA) -------------------------------------------------------------------------

TEST(Enemies, KillSwapsEveryField) {
    Enemies e(4);
    e.spawn(1, 2, 3, 4, 5);
    e.spawn(10, 20, 30, 40, 50);
    e.vx[1] = 7.f; e.vy[1] = 8.f;
    e.kill(0);
    ASSERT_EQ(e.count, 1u);
    EXPECT_EQ(e.x[0], 10.f);
    EXPECT_EQ(e.y[0], 20.f);
    EXPECT_EQ(e.speed[0], 30.f);
    EXPECT_EQ(e.radius[0], 40.f);
    EXPECT_EQ(e.hp[0], 50);
    EXPECT_EQ(e.vx[0], 7.f);
    EXPECT_EQ(e.vy[0], 8.f);
}

TEST(Enemies, SpawnFailsWhenFull) {
    Enemies e(1);
    EXPECT_EQ(e.spawn(0, 0, 0, 12, 1), 0);
    EXPECT_EQ(e.spawn(0, 0, 0, 12, 1), -1);
}
