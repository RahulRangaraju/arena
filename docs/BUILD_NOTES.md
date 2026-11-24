# Build notes: where this differs from the guide, and why

The build follows "Build a 2D Shooter Engine in C++ with SDL2" step by step (one tagged commit
per step). These are the places where it deliberately differs.

| Step | Change | Why |
|---|---|---|
| 0 | `add_executable` lists only `main.cpp` | The guide's Step 0 CMake references `Game.cpp`, which doesn't exist until Step 1. |
| 1 | Falls back to a software renderer if the accelerated one fails | WSLg without GPU passthrough, VMs and CI often have no accelerated renderer. |
| 8 | Screen shake uses its own RNG (`fxRng_`) | `render()` runs a machine-dependent number of times. Drawing from the simulation's seeded `rng_` there would make benchmark runs non-reproducible. |
| 8 | Particles keep updating on the game-over screen | So the final explosion finishes instead of freezing. |
| 9 | In stress mode bullets pierce and nothing dies | Entity counts stay fixed, so every run does the same work and naive vs grid is a fair comparison. Hits are counted instead. |
| 9 | Stress bullets fire in a rotating ring (no RNG), player parked in the center | The scene is identical every run and doesn't depend on input. |
| 9 | `--bench` mode | Runs the whole table headless and checks that grid and naive find the same number of hits. No manual key-pressing. |
| 9 | `bench_layout` executable | Measures AoS vs SoA in one binary instead of benchmarking two git branches. Both loops use identical math, so only the layout differs. |
| 9 | `bench/plot.py` | Turns the CSV into the README table and the log-scale chart. |
| 9 | Capacities raised to 25,000 | The guide's note for the 20,000-enemy test. |
| 10 | CMake prefers an installed GoogleTest, else downloads it | Faster CI (`apt install libgtest-dev`), and works offline. |
| 10 | More tests: every cell size, rebuild with fewer items, early stop, SoA kill, Pool reuse | Cheap to write, and each one covers a bug that's easy to introduce. |
| 10 | Batched rendering (`SDL_RenderFillRectsF`), B toggles it | The guide's "natural next fix" for rendering becoming the bottleneck. The toggle lets you measure it yourself. |
| 10 | Stress mode draws entities smaller | At 5,000+ full-size enemies the screen is a solid red wall. Visual only; collision radii are unchanged. |
| 10 | ASan job in CI | CI should catch memory errors, not just failing tests. |

Not done (stretch goals from the guide): SDL_ttf on-screen text, Emscripten, multithreading,
quadtree, sprites and sound, SDL3 port.
