#pragma once
#include <SDL.h>
#include "Pool.hpp"
#include "Vec2.hpp"

struct Bullet {
    Vec2 pos, vel;
    float life = 0.f;   // seconds left
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
};
