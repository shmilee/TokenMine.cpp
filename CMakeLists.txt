cmake_minimum_required(VERSION 3.25)
project(TokenMine VERSION 0.15.0 LANGUAGES CXX)

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
    src/core/status_effect.cpp
    src/core/thread_pool.cpp
    src/core/game.cpp
    src/block/block_registry.cpp
    src/block/block_sound.cpp
    src/block/chance_drop.cpp
    src/entity/arrow_mesh.cpp
    src/entity/entity.cpp
    src/entity/item_entity.cpp
    src/entity/item_mesh.cpp
    src/entity/item_model_mesh.cpp
    src/entity/mob_entity.cpp
    src/entity/mob_mesh.cpp
    src/entity/mob_registry.cpp
    src/entity/mob_spawning.cpp
    src/entity/mob_swing.cpp
    src/entity/mob_swing_clip.cpp
    src/entity/particle.cpp
    src/entity/projectile.cpp
    src/item/item_registry.cpp
    src/item/inventory.cpp
    src/item/recipe.cpp
    src/item/crafting_grid.cpp
    src/item/furnace.cpp
    src/item/campfire.cpp
    src/item/armor.cpp
    src/item/durability.cpp
    src/player/player.cpp
    src/player/drown.cpp
    src/world/chunk.cpp
    src/world/raycast.cpp
    src/world/world.cpp
    src/world/chunk_mesher.cpp
    src/world/chunk_streaming.cpp
    src/world/day_cycle.cpp
    src/world/fluid.cpp
    src/world/fire_spread.cpp
    src/world/growth.cpp
    src/world/tree_shape.cpp
    src/world/ore_vein.cpp
    src/world/beacon.cpp
    src/world/mesh_job.cpp
    src/world/light_job.cpp
    src/world/lighting.cpp
    src/world/noise.cpp
    src/world/biome.cpp
    src/world/worldgen_config.cpp
    src/world/world_generator.cpp
    src/renderer/fog.cpp
    src/renderer/shader.cpp
    src/renderer/texture.cpp
    src/renderer/mesh.cpp
    src/renderer/renderer.cpp
    src/renderer/texture_atlas.cpp
    src/ui/font.cpp
    src/ui/debug_overlay.cpp
    src/ui/inventory_ui.cpp
    src/ui/ui_scale.cpp
)
target_include_directories(mc_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
# Single source of truth for the version string (project VERSION above):
# the startup log (main.cpp) and the F3 overlay first line (debug_overlay.cpp)
# both consume TM_VERSION, so a version bump touches CMakeLists only.
target_compile_definitions(mc_core PUBLIC TM_VERSION="${PROJECT_VERSION}")
target_link_libraries(mc_core PUBLIC SDL3::SDL3 PkgConfig::GLEW
                        PkgConfig::ZLIB Threads::Threads)

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
add_executable(tokenmine src/core/main.cpp)
target_link_libraries(tokenmine PRIVATE mc_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(tokenmine_tests
    tests/test_json.cpp
    tests/test_config.cpp
    tests/test_asset_manager.cpp
    tests/test_result.cpp
    tests/test_math.cpp
    tests/test_input.cpp
    tests/test_camera.cpp
    tests/test_png.cpp
    tests/test_block.cpp
    tests/test_block_sound.cpp
    tests/test_atlas.cpp
    tests/test_coordinates.cpp
    tests/test_chunk.cpp
    tests/test_raycast.cpp
    tests/test_world.cpp
    tests/test_chunk_mesher.cpp
    tests/test_day_cycle.cpp
    tests/test_fluid.cpp
    tests/test_fire_spread.cpp
    tests/test_growth.cpp
    tests/test_tree_shape.cpp
    tests/test_ore_vein.cpp
    tests/test_beacon.cpp
    tests/test_mesh_job.cpp
    tests/test_light_job.cpp
    tests/test_fluid_light.cpp
    tests/test_chance_drop.cpp
    tests/test_lighting.cpp
    tests/test_arrow.cpp
    tests/test_bow.cpp
    tests/test_food.cpp
    tests/test_difficulty.cpp
    tests/test_entity.cpp
    tests/test_mob.cpp
    tests/test_mob_model.cpp
    tests/test_item.cpp
    tests/test_item_model_mesh.cpp
    tests/test_recipe.cpp
    tests/test_crafting_grid.cpp
    tests/test_furnace.cpp
    tests/test_campfire.cpp
    tests/test_armor.cpp
    tests/test_drown.cpp
    tests/test_durability.cpp
    tests/test_save.cpp
    tests/test_save_journal.cpp
    tests/test_auto_save.cpp
    tests/test_status_effect.cpp
    tests/test_thread_pool.cpp
    tests/test_chunk_streaming.cpp
    tests/test_font.cpp
    tests/test_debug_overlay.cpp
    tests/test_inventory_ui.cpp
    tests/test_ui_scale.cpp
    tests/test_fog.cpp
    tests/test_particle.cpp
    tests/test_sound.cpp
    tests/test_noise.cpp
    tests/test_player.cpp
    tests/test_biome.cpp
    tests/test_worldgen_config.cpp
    tests/test_world_generator.cpp
    tests/test_cave_probe.cpp
)
target_compile_definitions(tokenmine_tests PRIVATE
    MC_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
target_link_libraries(tokenmine_tests PRIVATE mc_core GTest::gtest_main)
add_test(NAME unit_tests COMMAND tokenmine_tests)

# ---------------------------------------------------------------------------
# GEN-4: the standalone cave-fragmentation calibration driver (kept OUT of
# the suite so the full 9-combo x 3-seed x 96-col cross-check table stays a
# one-shot ~5 s run, not a per-suite cost). Run from the repository root:
#   ./build/bin/cave_probe                 (the 32-col grid, ~0.5 s)
#   ./build/bin/cave_probe 96              (the 96-col cross-check grid)
#   ./build/bin/cave_probe 0.07 0.07 9     (one combo)
# ---------------------------------------------------------------------------
add_executable(cave_probe tests/cave_probe_main.cpp)
target_link_libraries(cave_probe PRIVATE mc_core)
