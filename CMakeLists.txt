cmake_minimum_required(VERSION 3.25)
project(MineCraft VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(SDL3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
find_package(GTest REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# Core library: all non-GL-API game logic. Kept free of main().
# ---------------------------------------------------------------------------
add_library(mc_core STATIC
    src/core/logger.cpp
    src/core/json.cpp
    src/core/config.cpp
    src/core/asset_manager.cpp
    src/core/window.cpp
    src/core/game.cpp
)
target_include_directories(mc_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mc_core PUBLIC SDL3::SDL3 PkgConfig::GLEW)

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
add_executable(minecraft src/core/main.cpp)
target_link_libraries(minecraft PRIVATE mc_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(minecraft_tests
    tests/test_json.cpp
    tests/test_config.cpp
    tests/test_asset_manager.cpp
    tests/test_result.cpp
)
target_link_libraries(minecraft_tests PRIVATE mc_core GTest::gtest_main)
add_test(NAME unit_tests COMMAND minecraft_tests)
