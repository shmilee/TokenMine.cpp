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
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

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
    src/core/input.cpp
    src/core/camera.cpp
    src/core/png.cpp
    src/core/save.cpp
    src/core/sound.cpp
    src/core/thread_pool.cpp
    src/core/game.cpp
    src/block/block_registry.cpp
    src/entity/entity.cpp
    src/entity/item_entity.cpp
    src/entity/item_mesh.cpp
    src/entity/particle.cpp
    src/entity/projectile.cpp
    src/item/item_registry.cpp
    src/item/inventory.cpp
    src/item/recipe.cpp
    src/item/crafting_grid.cpp
    src/player/player.cpp
    src/world/chunk.cpp
    src/world/raycast.cpp
    src/world/world.cpp
    src/world/chunk_mesher.cpp
    src/world/chunk_streaming.cpp
    src/world/day_cycle.cpp
    src/world/fluid.cpp
    src/world/lighting.cpp
    src/world/noise.cpp
    src/world/biome.cpp
    src/world/worldgen_config.cpp
    src/world/world_generator.cpp
    src/renderer/shader.cpp
    src/renderer/texture.cpp
    src/renderer/mesh.cpp
    src/renderer/renderer.cpp
    src/renderer/texture_atlas.cpp
    src/ui/font.cpp
    src/ui/debug_overlay.cpp
    src/ui/inventory_ui.cpp
)
target_include_directories(mc_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mc_core PUBLIC SDL3::SDL3 PkgConfig::GLEW
                        PkgConfig::ZLIB Threads::Threads)

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
    tests/test_math.cpp
    tests/test_input.cpp
    tests/test_camera.cpp
    tests/test_png.cpp
    tests/test_block.cpp
    tests/test_atlas.cpp
    tests/test_coordinates.cpp
    tests/test_chunk.cpp
    tests/test_raycast.cpp
    tests/test_world.cpp
    tests/test_chunk_mesher.cpp
    tests/test_day_cycle.cpp
    tests/test_fluid.cpp
    tests/test_lighting.cpp
    tests/test_entity.cpp
    tests/test_item.cpp
    tests/test_recipe.cpp
    tests/test_crafting_grid.cpp
    tests/test_save.cpp
    tests/test_thread_pool.cpp
    tests/test_chunk_streaming.cpp
    tests/test_font.cpp
    tests/test_debug_overlay.cpp
    tests/test_inventory_ui.cpp
    tests/test_particle.cpp
    tests/test_sound.cpp
    tests/test_noise.cpp
    tests/test_player.cpp
    tests/test_biome.cpp
    tests/test_worldgen_config.cpp
    tests/test_world_generator.cpp
)
target_compile_definitions(minecraft_tests PRIVATE
    MC_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
target_link_libraries(minecraft_tests PRIVATE mc_core GTest::gtest_main)
add_test(NAME unit_tests COMMAND minecraft_tests)
