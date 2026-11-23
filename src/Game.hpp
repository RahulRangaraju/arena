#pragma once
#include <SDL.h>
#include "Pool.hpp"
#include "SpatialGrid.hpp"
#include "Vec2.hpp"
#include <random>

struct Bullet {
    Vec2 pos, vel;
    float life = 0.f;   // seconds left
};

struct Enemy {
    Vec2 pos;
    float speed = 0.f;
    float radius = 12.f;
    int hp = 1;
};

class Game {
public:
    Game();
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    int run();

private:
    void handleEvents();
    void update(float dt);
    void render();
    void drawRect(Vec2 center, float halfSize);
    void updateBullets(float dt);
    void updateEnemies(float dt);
    void spawnEnemy();
    void collideNaive();
    void collideGrid();
    void removeDead();

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = true;

    Vec2 playerPos_{kWidth / 2.f, kHeight / 2.f};
    Vec2 aimDir_{1.f, 0.f};
    static constexpr float kPlayerSpeed = 320.f;   // pixels per second
    static constexpr float kPlayerRadius = 14.f;

    Pool<Bullet> bullets_{20000};
    float fireCooldown_ = 0.f;
    static constexpr float kFireInterval = 0.1f;
    static constexpr float kBulletSpeed = 900.f;
    static constexpr float kBulletRadius = 3.f;

    Pool<Enemy> enemies_{20000};
    std::mt19937 rng_{12345};   // fixed seed = reproducible runs and benchmarks
    float spawnTimer_ = 0.f;
    int wave_ = 1;
    int toSpawnThisWave_ = 5;

    int score_ = 0;
    double collisionMs_ = 0.0;   // last measured collision time

    SpatialGrid grid_{float(kWidth), float(kHeight), 32.f};
    bool useGrid_ = true;        // toggled with G in Step 9
    Uint32 lastTitle_ = 0;
};
