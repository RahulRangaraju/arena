cmake_minimum_required(VERSION 3.20)
project(arena LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # helps VS Code / clangd

find_package(SDL2 REQUIRED)

add_executable(arena src/main.cpp src/Game.cpp)
target_link_libraries(arena PRIVATE SDL2::SDL2)
target_compile_options(arena PRIVATE -Wall -Wextra -Wpedantic)

# AoS vs SoA micro-benchmark (Step 7 / Step 9). No SDL needed.
add_executable(bench_layout bench/layout_bench.cpp)
target_include_directories(bench_layout PRIVATE src)
target_compile_options(bench_layout PRIVATE -Wall -Wextra -Wpedantic)

# ---- Tests (Step 10) ------------------------------------------------------------------------
option(ARENA_BUILD_TESTS "Build the GoogleTest suite" ON)
if(ARENA_BUILD_TESTS)
  # Use an installed GoogleTest if there is one (Ubuntu: sudo apt install libgtest-dev),
  # otherwise download a pinned release.
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  enable_testing()
  add_executable(tests tests/test_grid.cpp)
  target_include_directories(tests PRIVATE src)
  target_link_libraries(tests PRIVATE GTest::gtest_main)
  target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
  include(GoogleTest)
  gtest_discover_tests(tests)

  # Headless smoke test of the real game: runs the quick benchmark (which also checks that
  # grid and naive agree). Under the sanitizer build this exercises the whole engine.
  add_test(NAME bench_smoke COMMAND arena --bench --quick)
endif()
