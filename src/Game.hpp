#pragma once
#include <SDL.h>
#include "Vec2.hpp"

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

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = true;

    Vec2 playerPos_{kWidth / 2.f, kHeight / 2.f};
    Vec2 aimDir_{1.f, 0.f};
    static constexpr float kPlayerSpeed = 320.f;   // pixels per second
    static constexpr float kPlayerRadius = 14.f;
};
