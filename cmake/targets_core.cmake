# aetheria_core：純 C++ 玩法核心（不得依賴 godot-cpp）。

add_library(aetheria_core STATIC
    core/api/version.cpp
    core/rules/ruleset.cpp
    core/rules/ruleset_load_defs.cpp
    core/rules/ruleset_load_biomes.cpp
    core/rules/ruleset_load_civilization.cpp
    core/rules/ruleset_load_crossings.cpp
    core/serialize/normalized_state_hash.cpp
    core/time/tick.cpp
    core/serialize/zone_encode.cpp
    core/serialize/zone_decode.cpp
    core/zone/file_zone_store.cpp
    core/zone/save_manifest_io.cpp
    core/zone/zone_manager.cpp
    core/zone/zone_store.cpp
    core/world/region_tiles.cpp
    core/world/region_step_cost.cpp
    core/world/region_path.cpp
    core/world/region_turn.cpp
    core/worldgen/civ_tiles.cpp
    core/worldgen/city_scoring.cpp
    core/worldgen/city_sites.cpp
    core/worldgen/road_path.cpp
    core/worldgen/road_loops.cpp
    core/worldgen/road_network.cpp
    core/worldgen/region_seed.cpp
    core/worldgen/stage_plates.cpp
    core/worldgen/stage_height.cpp
    core/worldgen/stage_erosion.cpp
    core/worldgen/stage_climate.cpp
    core/worldgen/stage_rivers.cpp
    core/worldgen/stage_biomes.cpp
    core/worldgen/region_build.cpp
    core/worldgen/region_stage_hash.cpp
    core/worldgen/region_result_hash.cpp
    core/worldgen/region_debug.cpp
)
target_include_directories(aetheria_core PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(aetheria_core
    PUBLIC EnTT::EnTT
    PRIVATE cereal::cereal tomlplusplus::tomlplusplus zstd::libzstd_static
)
aetheria_enable_warnings(aetheria_core)
