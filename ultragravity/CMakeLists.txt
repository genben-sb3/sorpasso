cmake_minimum_required(VERSION 3.25)
project(ultragravity CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Physics package: the sources live in the Unreal module Source/UltraGravity. Only the standard
# library plus the bundled JSON header.
add_subdirectory(physics)

# Stand-in for Unreal in the terminal, only for the tests.
add_subdirectory(test_world)

# Tuning tool: test driver, test maneuvers and the command that measures a car (ADR 0003: no Unreal).
add_subdirectory(tuning)

enable_testing()
add_subdirectory(tests)
