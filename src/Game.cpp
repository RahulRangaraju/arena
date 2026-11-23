#include "Game.hpp"
#include <algorithm>   // std::clamp, std::max
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

Game::Game(GameOptions opts) {
    if (opts.headless) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);   // no window needed
    if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
    window_ = SDL_CreateWindow("Arena", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               kWidth, kHeight, 0);
    if (!window_) throw std::runtime_error(SDL_GetError());
    Uint32 flags = SDL_RENDERER_ACCELERATED | (opts.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    renderer_ = SDL_CreateRenderer(window_, -1, flags);
    // Fallback: some setups (VMs, WSLg without GPU, headless CI) have no accelerated renderer.
    if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer_) throw std::runtime_error(SDL_GetError());
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);   // for fading particles
}

Game::~Game() {
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

int Game::run() {
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
            Uint64 u0 = SDL_GetPerformanceCounter();
            update(static_cast<float>(kDt));
            if (stress_)
                recordStressStats(static_cast<double>(SDL_GetPerformanceCounter() - u0) / freq * 1000.0);
            accumulator -= kDt;
        }
        render();
    }
    return 0;
}

void Game::reset() {
    bullets_.clear();
    enemies_.clear();
    particles_.clear();
    playerPos_ = {kWidth / 2.f, kHeight / 2.f};
    aimDir_ = {1.f, 0.f};
    playerHp_ = 3;
    invuln_ = 0.f;
    gameOver_ = false;
    fireCooldown_ = 0.f;
    spawnTimer_ = 0.f;
    wave_ = 1;
    toSpawnThisWave_ = 5;
    shake_ = 0.f;
    score_ = 0;
    rng_.seed(12345);
    stress_ = false;
    stressPhase_ = 0.f;
    hits_ = 0;
    collideSumMs_ = enemySumMs_ = updateSumMs_ = 0.0;
    collideSamples_ = 0;
}

void Game::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) running_ = false;
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running_ = false; break;
                case SDLK_r: reset(); break;
                case SDLK_F1:                      // toggle stress mode
                    if (stress_) reset(); else startStress(1000);
                    break;
                case SDLK_1: startStress(1000); break;
                case SDLK_2: startStress(2000); break;
                case SDLK_3: startStress(5000); break;
                case SDLK_4: startStress(10000); break;
                case SDLK_5: startStress(20000); break;
                case SDLK_g:                       // naive vs grid
                    useGrid_ = !useGrid_;
                    collideSumMs_ = enemySumMs_ = updateSumMs_ = 0.0;
                    collideSamples_ = 0;
                    break;
                case SDLK_c: {                     // cycle grid cell size 16 -> 32 -> 64
                    float next = grid_.cellSize() >= 64.f ? 16.f : grid_.cellSize() * 2.f;
                    grid_ = SpatialGrid(float(kWidth), float(kHeight), next);
                    std::printf("# cell size now %.0f px\n", next);
                    collideSumMs_ = enemySumMs_ = updateSumMs_ = 0.0;
                    collideSamples_ = 0;
                    break;
                }
                default: break;
            }
        }
    }
}

void Game::update(float dt) {
    shake_ = std::max(0.f, shake_ - 40.f * dt);
    updateParticles(dt);           // particles keep fading even on the game-over screen
    if (gameOver_) return;

    if (stress_) {   // fixed scene: player parked in the center, auto-fire, no damage
        playerPos_ = {kWidth / 2.f, kHeight / 2.f};
        updateStressBullets(dt);
        auto te0 = std::chrono::steady_clock::now();
        updateEnemies(dt);
        auto te1 = std::chrono::steady_clock::now();
        enemyUpdateMs_ = std::chrono::duration<double, std::milli>(te1 - te0).count();
        auto t0 = std::chrono::steady_clock::now();
        useGrid_ ? collideGrid() : collideNaive();
        auto t1 = std::chrono::steady_clock::now();
        collisionMs_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
        removeDead();
        return;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    Vec2 move{};
    if (keys[SDL_SCANCODE_W]) move.y -= 1.f;
    if (keys[SDL_SCANCODE_S]) move.y += 1.f;
    if (keys[SDL_SCANCODE_A]) move.x -= 1.f;
    if (keys[SDL_SCANCODE_D]) move.x += 1.f;
    playerPos_ += move.normalized() * (kPlayerSpeed * dt);   // normalized: diagonals aren't faster

    playerPos_.x = std::clamp(playerPos_.x, kPlayerRadius, kWidth - kPlayerRadius);
    playerPos_.y = std::clamp(playerPos_.y, kPlayerRadius, kHeight - kPlayerRadius);

    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    Vec2 toMouse = Vec2{float(mx), float(my)} - playerPos_;
    if (toMouse.lengthSq() > 1.f) aimDir_ = toMouse.normalized();

    updateBullets(dt);

    auto te0 = std::chrono::steady_clock::now();
    updateEnemies(dt);
    auto te1 = std::chrono::steady_clock::now();
    enemyUpdateMs_ = std::chrono::duration<double, std::milli>(te1 - te0).count();

    auto t0 = std::chrono::steady_clock::now();
    useGrid_ ? collideGrid() : collideNaive();
    auto t1 = std::chrono::steady_clock::now();
    collisionMs_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
    removeDead();

    collidePlayer(dt);
}

// Checks every bullet against every enemy: O(B * E).
void Game::collideNaive() {
    for (std::size_t b = 0; b < bullets_.size(); ++b) {
        Bullet& bullet = bullets_[b];
        for (std::size_t e = 0; e < enemies_.count; ++e) {
            if (enemies_.hp[e] <= 0) continue;
            if (circlesOverlap(bullet.pos, kBulletRadius, enemyPos(e), enemies_.radius[e])) {
                if (stress_) { ++hits_; continue; }   // stress: bullets pierce, count every hit
                --enemies_.hp[e];
                bullet.life = 0.f;   // mark; removal happens after the loop
                break;               // one bullet hits one enemy
            }
        }
    }
}

// Each bullet only checks enemies in nearby cells: roughly O(B + E).
void Game::collideGrid() {
    grid_.build(enemies_.count, [&](std::size_t i) { return enemyPos(i); });

    constexpr float kMaxEnemyRadius = 12.f;
    for (std::size_t b = 0; b < bullets_.size(); ++b) {
        Bullet& bullet = bullets_[b];
        grid_.query(bullet.pos, kBulletRadius + kMaxEnemyRadius, [&](int e) {
            if (enemies_.hp[e] <= 0) return false;
            if (!circlesOverlap(bullet.pos, kBulletRadius, enemyPos(e), enemies_.radius[e]))
                return false;
            if (stress_) { ++hits_; return false; }  // stress: bullets pierce, count every hit
            --enemies_.hp[e];
            bullet.life = 0.f;
            return true;   // stop: this bullet is spent
        });
    }
}

// One entity against all enemies is O(E), so a plain loop is fine here.
// Not optimizing this is a deliberate choice.
void Game::collidePlayer(float dt) {
    invuln_ -= dt;
    if (invuln_ > 0.f) return;
    for (std::size_t i = 0; i < enemies_.count; ++i) {
        if (circlesOverlap(playerPos_, kPlayerRadius, enemyPos(i), enemies_.radius[i])) {
            --playerHp_;
            invuln_ = 1.0f;   // 1 second of safety
            shake_ = 10.f;
            if (playerHp_ <= 0) gameOver_ = true;
            break;
        }
    }
}

// Mark-then-remove: killing inside the double loop would swap a different entity
// into the current index, so we'd skip it or test it twice.
void Game::removeDead() {
    for (std::size_t i = bullets_.size(); i-- > 0;)
        if (bullets_[i].life <= 0.f) bullets_.kill(i);
    for (std::size_t i = enemies_.count; i-- > 0;)
        if (enemies_.hp[i] <= 0) {
            ++score_;
            burst(enemyPos(i));
            enemies_.kill(i);
        }
}

void Game::burst(Vec2 at) {
    std::uniform_real_distribution<float> ang(0.f, 6.2831853f), spd(60.f, 260.f);
    for (int k = 0; k < 12; ++k)
        if (Particle* p = particles_.spawn()) {
            float a = ang(rng_), s = spd(rng_);
            p->pos = at;
            p->vel = {std::cos(a) * s, std::sin(a) * s};
            p->life = 0.5f;
        }
}

void Game::updateParticles(float dt) {
    for (std::size_t i = particles_.size(); i-- > 0;) {
        Particle& p = particles_[i];
        p.pos += p.vel * dt;
        p.life -= dt;
        if (p.life <= 0.f) particles_.kill(i);
    }
}

void Game::spawnEnemy() {
    std::uniform_real_distribution<float> t(0.f, 1.f);
    Vec2 p;
    switch (rng_() % 4) {   // pick an edge
        case 0: p = {t(rng_) * kWidth, -20.f}; break;
        case 1: p = {t(rng_) * kWidth, kHeight + 20.f}; break;
        case 2: p = {-20.f, t(rng_) * kHeight}; break;
        default: p = {kWidth + 20.f, t(rng_) * kHeight}; break;
    }
    float speed = 80.f + 60.f * t(rng_) + 10.f * wave_;
    enemies_.spawn(p.x, p.y, speed, 12.f, 1 + wave_ / 4);
}

void Game::updateEnemies(float dt) {
    if (stress_) {   // drift in a straight line and bounce off the walls
        float* __restrict ex = enemies_.x.data();
        float* __restrict ey = enemies_.y.data();
        float* __restrict vx = enemies_.vx.data();
        float* __restrict vy = enemies_.vy.data();
        const std::size_t n = enemies_.count;
        for (std::size_t i = 0; i < n; ++i) {
            ex[i] += vx[i] * dt;
            ey[i] += vy[i] * dt;
            if (ex[i] < 0.f || ex[i] > kWidth) vx[i] = -vx[i];
            if (ey[i] < 0.f || ey[i] > kHeight) vy[i] = -vy[i];
        }
        return;
    }

    spawnTimer_ -= dt;
    if (toSpawnThisWave_ > 0 && spawnTimer_ <= 0.f) {
        spawnEnemy();
        --toSpawnThisWave_;
        spawnTimer_ = 0.4f;
    }
    if (toSpawnThisWave_ == 0 && enemies_.count == 0) {   // wave cleared
        ++wave_;
        toSpawnThisWave_ = 5 + wave_ * 3;
    }

    // Flat, branch-free math over plain arrays: friendly to auto-vectorization.
    const float px = playerPos_.x, py = playerPos_.y;
    float* __restrict ex = enemies_.x.data();
    float* __restrict ey = enemies_.y.data();
    const float* __restrict sp = enemies_.speed.data();
    const std::size_t n = enemies_.count;
    for (std::size_t i = 0; i < n; ++i) {
        float dx = px - ex[i], dy = py - ey[i];
        float inv = 1.f / std::sqrt(dx * dx + dy * dy + 1e-6f);   // +epsilon: no divide-by-zero
        ex[i] += dx * inv * sp[i] * dt;
        ey[i] += dy * inv * sp[i] * dt;
    }
}

void Game::updateBullets(float dt) {
    fireCooldown_ -= dt;
    Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
    if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) && fireCooldown_ <= 0.f) {
        if (Bullet* b = bullets_.spawn()) {
            b->pos = playerPos_ + aimDir_ * (kPlayerRadius + 4.f);
            b->vel = aimDir_ * kBulletSpeed;
            b->life = 1.5f;
        }
        fireCooldown_ = kFireInterval;
    }

    for (std::size_t i = bullets_.size(); i-- > 0;) {   // backwards: kill() is safe
        Bullet& b = bullets_[i];
        b.pos += b.vel * dt;
        b.life -= dt;
        bool offscreen = b.pos.x < 0 || b.pos.x > kWidth || b.pos.y < 0 || b.pos.y > kHeight;
        if (b.life <= 0.f || offscreen) bullets_.kill(i);
    }
}

// ---------------------------------------------------------------------------------------
// Stress mode and benchmarking (Step 9)
// ---------------------------------------------------------------------------------------

// Fresh, reproducible scene: same seed every time, so a given count always looks the same.
void Game::startStress(std::size_t enemyCount, bool announce) {
    reset();
    stress_ = true;
    std::uniform_real_distribution<float> X(0.f, float(kWidth)), Y(0.f, float(kHeight));
    std::uniform_real_distribution<float> ang(0.f, 6.2831853f), spd(20.f, 60.f);
    enemyCount = std::min(enemyCount, enemies_.capacity());
    for (std::size_t i = 0; i < enemyCount; ++i) {
        float px = X(rng_), py = Y(rng_), a = ang(rng_), s = spd(rng_);
        long idx = enemies_.spawn(px, py, 0.f, 12.f, 1);
        enemies_.vx[idx] = std::cos(a) * s;
        enemies_.vy[idx] = std::sin(a) * s;
    }
    if (announce) {
        std::printf("# stress mode: %zu enemies, %s\n", enemies_.count, useGrid_ ? "grid" : "naive");
        std::printf("mode,enemies,bullets,cell,collide_ms,enemy_update_ms,update_ms\n");
    }
}

// A ring of 200 bullets per update, rotated a little each time. No RNG, no input.
void Game::updateStressBullets(float dt) {
    for (int k = 0; k < kStressBulletsPerUpdate; ++k) {
        Bullet* b = bullets_.spawn();
        if (!b) break;
        float a = stressPhase_ + 6.2831853f * float(k) / kStressBulletsPerUpdate;
        Vec2 dir{std::cos(a), std::sin(a)};
        b->pos = playerPos_ + dir * (kPlayerRadius + 4.f);
        b->vel = dir * kBulletSpeed;
        b->life = 1.5f;
    }
    stressPhase_ += 0.0137f;

    for (std::size_t i = bullets_.size(); i-- > 0;) {
        Bullet& b = bullets_[i];
        b.pos += b.vel * dt;
        b.life -= dt;
        bool offscreen = b.pos.x < 0 || b.pos.x > kWidth || b.pos.y < 0 || b.pos.y > kHeight;
        if (b.life <= 0.f || offscreen) bullets_.kill(i);
    }
}

// One update is noisy, so print the average of every 240 updates (2 s of game time).
void Game::recordStressStats(double updateMs) {
    collideSumMs_ += collisionMs_;
    enemySumMs_ += enemyUpdateMs_;
    updateSumMs_ += updateMs;
    if (++collideSamples_ == 240) {
        std::printf("%s,%zu,%zu,%.0f,%.4f,%.4f,%.4f\n", useGrid_ ? "grid" : "naive",
                    enemies_.count, bullets_.size(), grid_.cellSize(), collideSumMs_ / 240.0,
                    enemySumMs_ / 240.0, updateSumMs_ / 240.0);
        std::fflush(stdout);
        collideSumMs_ = enemySumMs_ = updateSumMs_ = 0.0;
        collideSamples_ = 0;
    }
}

Game::BenchResult Game::measure(std::size_t enemyCount, bool grid, float cellSize, int warmup,
                                int samples) {
    grid_ = SpatialGrid(float(kWidth), float(kHeight), cellSize);
    useGrid_ = true;                          // warm up with the fast path...
    startStress(enemyCount, /*announce=*/false);
    for (int i = 0; i < warmup; ++i) update(float(kDt));   // ...until the bullet ring is full

    useGrid_ = grid;
    hits_ = 0;
    BenchResult r;
    for (int i = 0; i < samples; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        update(float(kDt));
        auto t1 = std::chrono::steady_clock::now();
        r.updateMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
        r.collideMs += collisionMs_;
        r.enemyMs += enemyUpdateMs_;
        r.bullets += double(bullets_.size());
    }
    r.collideMs /= samples; r.enemyMs /= samples; r.updateMs /= samples; r.bullets /= samples;
    r.hits = hits_;
    return r;
}

static std::string cpuModel() {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("model name", 0) == 0) return line.substr(line.find(':') + 2);
    return "unknown (write it down yourself)";
}

int Game::runBenchmark(bool quick) {
#ifdef NDEBUG
    const char* build = "Release";
#else
    const char* build = "Debug (do NOT use these numbers)";
#endif
    std::printf("# arena benchmark | build: %s | cpu: %s | hw threads: %u\n", build,
                cpuModel().c_str(), std::thread::hardware_concurrency());
    std::printf("# stress scene: enemies drift, %d bullets/update, bullets pierce (fixed work)\n",
                kStressBulletsPerUpdate);
    std::printf("mode,enemies,bullets,cell,collide_ms,enemy_update_ms,update_ms,hits\n");

    const std::vector<std::size_t> counts =
        quick ? std::vector<std::size_t>{1000, 2000}
              : std::vector<std::size_t>{1000, 2000, 5000, 10000, 20000};
    const int warmup = 120;                   // 1 s of game time: the bullet ring fills up
    const int samples = quick ? 10 : 120;
    int failures = 0;

    auto row = [&](const char* mode, std::size_t n, float cell, const BenchResult& r) {
        std::printf("%s,%zu,%.0f,%.0f,%.4f,%.4f,%.4f,%lld\n", mode, n, r.bullets, cell,
                    r.collideMs, r.enemyMs, r.updateMs, r.hits);
        std::fflush(stdout);
    };

    for (std::size_t n : counts) {
        BenchResult g = measure(n, true, 32.f, warmup, samples);
        row("grid", n, 32.f, g);
        BenchResult nv = measure(n, false, 32.f, warmup, samples);
        row("naive", n, 32.f, nv);
        // Same seed + same scene => both must find exactly the same overlaps.
        if (g.hits != nv.hits) {
            std::fprintf(stderr, "MISMATCH at %zu enemies: grid %lld hits, naive %lld\n", n,
                         g.hits, nv.hits);
            ++failures;
        }
    }

    if (!quick) {   // cell-size experiment at 10,000 enemies
        for (float cell : {16.f, 64.f}) {
            BenchResult r = measure(10000, true, cell, warmup, samples);
            row("grid", 10000, cell, r);
        }
    }

    std::printf("# hit counts %s\n", failures ? "DIFFER (bug!)" : "match between grid and naive");
    return failures ? 1 : 0;
}

void Game::drawRect(Vec2 c, float h) {
    c += camOffset_;
    SDL_FRect r{c.x - h, c.y - h, 2 * h, 2 * h};
    SDL_RenderFillRectF(renderer_, &r);
}

void Game::render() {
    // Screen shake: a random offset in [-shake_, shake_] applied to everything drawn this frame.
    std::uniform_real_distribution<float> jitter(-shake_, shake_);
    camOffset_ = shake_ > 0.f ? Vec2{jitter(fxRng_), jitter(fxRng_)} : Vec2{};

    SDL_SetRenderDrawColor(renderer_, 18, 18, 28, 255);
    SDL_RenderClear(renderer_);

    for (std::size_t i = 0; i < particles_.size(); ++i) {   // particles fade out
        const Particle& p = particles_[i];
        SDL_SetRenderDrawColor(renderer_, 255, 170, 90, Uint8(255 * p.life / 0.5f));
        drawRect(p.pos, 2.f);
    }

    SDL_SetRenderDrawColor(renderer_, 255, 230, 120, 255);   // bullets
    for (std::size_t i = 0; i < bullets_.size(); ++i) drawRect(bullets_[i].pos, kBulletRadius);

    SDL_SetRenderDrawColor(renderer_, 240, 80, 90, 255);     // enemies
    for (std::size_t i = 0; i < enemies_.count; ++i) drawRect(enemyPos(i), enemies_.radius[i]);

    bool blinkOff = invuln_ > 0.f && (SDL_GetTicks() / 80) % 2 == 0;
    if (!gameOver_ && !blinkOff) {
        SDL_SetRenderDrawColor(renderer_, 90, 200, 255, 255);   // player
        drawRect(playerPos_, kPlayerRadius);
        Vec2 base = playerPos_ + camOffset_;
        Vec2 tip = base + aimDir_ * 28.f;                       // aim line
        SDL_RenderDrawLineF(renderer_, base.x, base.y, tip.x, tip.y);
    }

    SDL_RenderPresent(renderer_);

    ++framesSinceTitle_;
    if (SDL_GetTicks() - lastTitle_ > 500) {
        Uint32 nowMs = SDL_GetTicks();
        fps_ = framesSinceTitle_ * 1000.0 / double(nowMs - lastTitle_);
        framesSinceTitle_ = 0;
        lastTitle_ = nowMs;
        char buf[224];
        if (stress_)
            std::snprintf(buf, sizeof buf,
                          "Arena STRESS [%s, cell %.0f] | %.0f FPS | enemies %zu | bullets %zu"
                          " | collide %.3f ms | update %.3f ms   (F1 exit, 1-5 count, G, C)",
                          useGrid_ ? "grid" : "NAIVE", grid_.cellSize(), fps_, enemies_.count,
                          bullets_.size(), collisionMs_, enemyUpdateMs_);
        else if (gameOver_)
            std::snprintf(buf, sizeof buf, "Arena | GAME OVER (score %d, wave %d) - press R",
                          score_, wave_);
        else
            std::snprintf(buf, sizeof buf,
                          "Arena | %.0f FPS | HP %d | wave %d | score %d | enemies %zu"
                          " | bullets %zu | collide %.3f ms | enemy update %.3f ms",
                          fps_, playerHp_, wave_, score_, enemies_.count, bullets_.size(),
                          collisionMs_, enemyUpdateMs_);
        SDL_SetWindowTitle(window_, buf);
    }
}
