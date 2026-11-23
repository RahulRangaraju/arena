#pragma once
#include <SDL.h>

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

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = true;
};
