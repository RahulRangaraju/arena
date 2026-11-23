#pragma once
#include <SDL.h>
#include "Enemies.hpp"
#include "Pool.hpp"
#include "SpatialGrid.hpp"
#include "Vec2.hpp"
#include <random>

struct Bullet {
    Vec2 pos, vel;
    float life = 0.f;   // seconds left
};

struct Particle {
    Vec2 pos, vel;
    float life = 0.f;
};

struct GameOptions {
    bool vsync = true;      // turn off (--no-vsync) when measuring FPS
    bool headless = false;  // no visible window (used by --bench and CI)
};

class Game {
public:
    explicit Game(GameOptions opts = {});
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    int run();
    int runBenchmark(bool quick);   // headless stress benchmark, CSV to stdout

private:
    void handleEvents();
    void update(float dt);
    void render();
    void drawRect(Vec2 center, float halfSize);
    void reset();

    void updateBullets(float dt);
    void updateEnemies(float dt);
    void updateParticles(float dt);
    void spawnEnemy();
    void collideNaive();
    void collideGrid();
    void collidePlayer(float dt);
    void removeDead();
    void burst(Vec2 at);

    // Stress mode (Step 9)
    void startStress(std::size_t enemyCount, bool announce = true);
    void updateStressBullets(float dt);
    void recordStressStats(double updateMs);

    struct BenchResult {
        double collideMs = 0, enemyMs = 0, updateMs = 0, bullets = 0;
        long long hits = 0;
    };
    BenchResult measure(std::size_t enemyCount, bool grid, float cellSize, int warmup, int samples);
    Vec2 enemyPos(std::size_t i) const { return {enemies_.x[i], enemies_.y[i]}; }

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;
    static constexpr double kDt = 1.0 / 120.0;   // fixed simulation step

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = true;

    // Player
    Vec2 playerPos_{kWidth / 2.f, kHeight / 2.f};
    Vec2 aimDir_{1.f, 0.f};
    static constexpr float kPlayerSpeed = 320.f;   // pixels per second
    static constexpr float kPlayerRadius = 14.f;
    int playerHp_ = 3;
    float invuln_ = 0.f;                           // seconds of invulnerability left
    bool gameOver_ = false;

    // Bullets
    Pool<Bullet> bullets_{25000};
    float fireCooldown_ = 0.f;
    static constexpr float kFireInterval = 0.1f;
    static constexpr float kBulletSpeed = 900.f;
    static constexpr float kBulletRadius = 3.f;

    // Enemies and waves
    Enemies enemies_{25000};   // struct-of-arrays (Step 7)
    std::mt19937 rng_{12345};  // fixed seed = reproducible runs and benchmarks
    float spawnTimer_ = 0.f;
    int wave_ = 1;
    int toSpawnThisWave_ = 5;

    // Game feel
    Pool<Particle> particles_{30000};
    float shake_ = 0.f;        // screen-shake strength in pixels, decays over time
    Vec2 camOffset_{};         // added to every draw position
    std::mt19937 fxRng_{777};  // render-only randomness. Kept separate from rng_ so the
                               // number of frames drawn never changes the simulation.

    // Collisions and stats
    int score_ = 0;
    double collisionMs_ = 0.0;     // last measured collision time
    double enemyUpdateMs_ = 0.0;   // last measured updateEnemies time
    SpatialGrid grid_{float(kWidth), float(kHeight), 32.f};
    bool useGrid_ = true;          // toggled with G
    Uint32 lastTitle_ = 0;
    int framesSinceTitle_ = 0;
    double fps_ = 0.0;

    // Stress mode: enemies drift, bullets pierce, nothing dies, so the work per update is
    // fixed and the numbers don't depend on how you play.
    bool stress_ = false;
    float stressPhase_ = 0.f;              // rotates the 200-bullet ring each update
    static constexpr int kStressBulletsPerUpdate = 200;
    long long hits_ = 0;                   // bullet-enemy overlaps found (stress mode)
    double collideSumMs_ = 0.0, enemySumMs_ = 0.0, updateSumMs_ = 0.0;
    int collideSamples_ = 0;
};
