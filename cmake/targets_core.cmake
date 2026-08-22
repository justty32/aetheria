# aetheria_faction_ai_objects 只看得到知識視圖；世界真值 header 不在 include path。
add_library(aetheria_faction_ai_objects OBJECT
    core/ai/faction_view.cpp
)
target_include_directories(aetheria_faction_ai_objects PRIVATE
    "${PROJECT_SOURCE_DIR}/core/ai/include"
)
aetheria_enable_warnings(aetheria_faction_ai_objects)

# aetheria_worldgen_objects 只有 repo 根與純資料公式；執行期跨-zone header 對它不可見。
add_library(aetheria_worldgen_objects OBJECT
    core/worldgen/civ_tiles.cpp
    core/worldgen/city_scoring.cpp
    core/worldgen/settlement_scoring.cpp
    core/worldgen/city_selection.cpp
    core/worldgen/city_sites.cpp
    core/worldgen/feature_placement.cpp
    core/worldgen/history_roads.cpp
    core/worldgen/history_layer.cpp
    core/worldgen/influence_spread.cpp
    core/worldgen/influence_claim.cpp
    core/worldgen/governance_release.cpp
    core/worldgen/capital_selection.cpp
    core/worldgen/portal_generation.cpp
    core/worldgen/portal_candidates.cpp
    core/worldgen/portal_boundary_candidates.cpp
    core/worldgen/faction_generation.cpp
    core/worldgen/road_path.cpp
    core/worldgen/road_loops.cpp
    core/worldgen/road_network.cpp
    core/worldgen/region_seed.cpp
    core/worldgen/field_redistribution.cpp
    core/worldgen/stage_plates.cpp
    core/worldgen/stage_height.cpp
    core/worldgen/stage_erosion.cpp
    core/worldgen/stage_climate.cpp
    core/worldgen/stage_rivers.cpp
    core/worldgen/stage_biomes.cpp
    core/worldgen/stage_features.cpp
    core/worldgen/region_build.cpp
    core/worldgen/region_populate.cpp
    core/worldgen/region_stage_hash.cpp
    core/worldgen/region_result_hash.cpp
    core/worldgen/region_debug.cpp
)
target_include_directories(aetheria_worldgen_objects PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/core/ai/include"
)
target_link_libraries(aetheria_worldgen_objects PRIVATE EnTT::EnTT)
aetheria_enable_warnings(aetheria_worldgen_objects)

# aetheria_core：純 C++ 玩法核心（不得依賴 godot-cpp）。

add_library(aetheria_core STATIC
    $<TARGET_OBJECTS:aetheria_faction_ai_objects>
    $<TARGET_OBJECTS:aetheria_worldgen_objects>
    core/api/version.cpp
    core/narrative/emergent_quest.cpp
    core/narrative/narrative_event.cpp
    core/rules/power.cpp
    core/rules/attributes.cpp
    core/rules/check.cpp
    core/rules/damage.cpp
    core/rules/ruleset.cpp
    core/rules/ruleset_load_defs.cpp
    core/rules/ruleset_load_site.cpp
    core/rules/ruleset_load_site_city.cpp
    core/rules/ruleset_load_site_build.cpp
    core/rules/ruleset_load_site_wilderness.cpp
    core/rules/ruleset_load_local_buildings.cpp
    core/rules/ruleset_load_individual.cpp
    core/rules/ruleset_load_biomes.cpp
    core/rules/ruleset_load_civilization.cpp
    core/rules/ruleset_load_factions.cpp
    core/rules/ruleset_load_history.cpp
    core/rules/ruleset_load_history_values.cpp
    core/rules/ruleset_load_history_references.cpp
    core/rules/ruleset_load_diplomacy.cpp
    core/rules/ruleset_load_crossings.cpp
    core/rules/ruleset_load_world_graph.cpp
    core/rules/ruleset_load_power.cpp
    core/serialize/normalized_state_hash.cpp
    core/spatial/recursive_partition.cpp
    core/local/local_generation.cpp
    core/local/local_building_generation.cpp
    core/local/local_building_geometry.cpp
    core/local/local_building_content.cpp
    core/local/local_underground_generation.cpp
    core/local/local_underground_geometry.cpp
    core/local/local_underground_validation.cpp
    core/local/local_fov.cpp
    core/local/local_movement.cpp
    core/local/local_navigation.cpp
    core/local/local_materialize.cpp
    core/local/local_projection.cpp
    core/local/local_reduction.cpp
    core/site/site_event_escalation.cpp
    core/site/site_build_loop.cpp
    core/site/site_build_economy.cpp
    core/site/site_lifecycle.cpp
    core/site/site_materialize.cpp
    core/site/site_streaming.cpp
    core/site/site_population.cpp
    core/site/site_zoning.cpp
    core/site/site_buildings.cpp
    core/site/site_building_placement.cpp
    core/site/site_fortifications.cpp
    core/site/site_landmarks.cpp
    core/site/site_damage.cpp
    core/site/site_projection.cpp
    core/site/site_skeleton_common.cpp
    core/site/site_skeleton_center.cpp
    core/site/site_skeleton_terrain.cpp
    core/site/site_skeleton_roads.cpp
    core/site/site_skeleton_blocks.cpp
    core/site/site_wilderness.cpp
    core/site/site_wilderness_entities.cpp
    core/site/site_wilderness_boundary.cpp
    core/site/site_wilderness_boundary_seed.cpp
    core/site/site_wilderness_terrain.cpp
    core/site/site_wilderness_pathfinding.cpp
    core/site/site_wilderness_paths.cpp
    core/site/site_wilderness_content.cpp
    core/site/site_reduction.cpp
    core/time/tick.cpp
    core/serialize/zone_encode.cpp
    core/serialize/zone_decode.cpp
    core/serialize/zone_diplomacy_codec.cpp
    core/runtime/cross_zone.cpp
    core/zone/file_zone_store.cpp
    core/zone/save_manifest_io.cpp
    core/zone/zone_manager.cpp
    core/zone/zone_store.cpp
    core/world/region_tiles.cpp
    core/world/region_simulation.cpp
    core/world/region_step_cost.cpp
    core/world/region_path.cpp
    core/world/region_turn.cpp
    core/world/diplomacy.cpp
    core/world/diplomacy_view.cpp
)
target_include_directories(aetheria_core PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/core/runtime/include"
    "${PROJECT_SOURCE_DIR}/core/ai/include"
)
target_link_libraries(aetheria_core
    PUBLIC EnTT::EnTT
    PRIVATE cereal::cereal tomlplusplus::tomlplusplus zstd::libzstd_static
)
aetheria_enable_warnings(aetheria_core)

# M6.4 追加區塊：保持在檔尾，避免多路工作切開既有 source 宣告。
target_sources(aetheria_core PRIVATE
    core/world/named_fate.cpp
)
