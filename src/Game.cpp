#include "Game.hpp"
#include <stdexcept>

Game::Game() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    window_ = SDL_CreateWindow("Arena", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               kWidth, kHeight, 0);
    if (!window_) throw std::runtime_error(SDL_GetError());
    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    // Fallback: some setups (VMs, WSLg without GPU, headless CI) have no accelerated renderer.
    if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) throw std::runtime_error(SDL_GetError());
}

Game::~Game() {
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

int Game::run() {
    constexpr double kDt = 1.0 / 120.0;                      // simulation step
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 prev = SDL_GetPerformanceCounter();
    double accumulator = 0.0;

    while (running_) {
        Uint64 now = SDL_GetPerformanceCounter();
        double frameTime = static_cast<double>(now - prev) / freq;
        prev = now;
        if (frameTime > 0.25) frameTime = 0.25;              // avoid the "spiral of death"
        accumulator += frameTime;

        handleEvents();
        while (accumulator >= kDt) {                         // 0, 1, or several steps per frame
            update(static_cast<float>(kDt));
            accumulator -= kDt;
        }
        render();
    }
    return 0;
}

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) running_ = false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running_ = false;
    }
}

void Game::update(float /*dt*/) {}

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 18, 18, 28, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);
}
