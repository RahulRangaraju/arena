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
