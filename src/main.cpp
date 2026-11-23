#include "Game.hpp"
#include <cstdio>
#include <cstring>
#include <exception>

static void usage() {
    std::puts("usage: arena [--no-vsync] [--bench [--quick]]\n"
              "  --no-vsync   uncap the frame rate (use this when measuring FPS)\n"
              "  --bench      run the headless stress benchmark, print CSV to stdout, exit\n"
              "  --quick      with --bench: small, fast run (CI smoke test)");
}

int main(int argc, char** argv) {
    GameOptions opts;
    bool bench = false, quick = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-vsync") == 0) opts.vsync = false;
        else if (std::strcmp(argv[i], "--bench") == 0) bench = true;
        else if (std::strcmp(argv[i], "--quick") == 0) quick = true;
        else { usage(); return std::strcmp(argv[i], "--help") == 0 ? 0 : 2; }
    }
    opts.headless = bench;

    try {
        Game game(opts);
        return bench ? game.runBenchmark(quick) : game.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
}
