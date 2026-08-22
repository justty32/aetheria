# aetheria_tests：GoogleTest 單元測試，外加 core 引擎隔離的 CTest 檢查。

add_executable(aetheria_tests
    tests/narrative/emergent_quest_test.cpp
    tests/narrative/narrative_event_test.cpp
    tests/local/local_boundary_test.cpp
    tests/local/local_building_test.cpp
    tests/local/local_generation_test.cpp
    tests/local/local_fov_test.cpp
    tests/local/local_materialize_test.cpp
    tests/local/local_movement_test.cpp
    tests/local/local_reduction_test.cpp
    tests/local/local_underground_test.cpp
    tests/rules/ruleset_load_test.cpp
    tests/rules/power_test.cpp
    tests/rules/combat_test.cpp
    tests/rules/individual_rules_test.cpp
    tests/rules/ruleset_error_test.cpp
    tests/rules/ruleset_zone_codec_test.cpp
    tests/rules/diplomacy_rules_test.cpp
    tests/serialize/registry_codec_test.cpp
    tests/site/site_event_escalation_test.cpp
    tests/site/site_build_loop_test.cpp
    tests/site/site_build_persistence_test.cpp
    tests/site/site_fill_test.cpp
    tests/site/site_projection_error_test.cpp
    tests/site/site_materialize_hash_test.cpp
    tests/site/site_materialize_test.cpp
    tests/site/site_streaming_test.cpp
    tests/site/site_migration_test.cpp
    tests/site/site_unload_equivalence_test.cpp
    tests/site/site_reduction_calibration_test.cpp
    tests/site/site_reduction_test.cpp
    tests/site/site_roundtrip_negative_test.cpp
    tests/site/site_roundtrip_test.cpp
    tests/site/site_projection_test.cpp
    tests/site/site_skeleton_test.cpp
    tests/site/site_wilderness_boundary_test.cpp
    tests/site/site_wilderness_generation_test.cpp
    tests/site/site_wilderness_content_test.cpp
    tests/site/site_wilderness_lifecycle_test.cpp
    tests/sim/world_hash_error_test.cpp
    tests/sim/world_hash_state_test.cpp
    sim/world_hash.cpp
    tests/time/tick_test.cpp
    tests/world/region_step_cost_test.cpp
    tests/world/region_path_test.cpp
    tests/world/region_turn_test.cpp
    tests/world/region_tiles_test.cpp
    tests/world/diplomacy_test.cpp
    tests/worldgen/city_sites_test.cpp
    tests/worldgen/history_cataclysm_test.cpp
    tests/worldgen/history_feedback_test.cpp
    tests/worldgen/history_identity_test.cpp
    tests/worldgen/history_isolation_test.cpp
    tests/worldgen/history_layer_test.cpp
    tests/worldgen/capital_selection_test.cpp
    tests/worldgen/influence_spread_test.cpp
    tests/worldgen/governance_release_test.cpp
    tests/worldgen/portal_collision_test.cpp
    tests/worldgen/portal_stage_test.cpp
    tests/worldgen/faction_stage_test.cpp
    tests/worldgen/faction_metrics_test.cpp
    tests/worldgen/late_stage_isolation_test.cpp
    tests/worldgen/region_determinism_test.cpp
    tests/worldgen/region_parameter_isolation_test.cpp
    tests/worldgen/region_output_validation_test.cpp
    tests/worldgen/region_perf_test.cpp
    tests/worldgen/region_stage_functions_test.cpp
    tests/worldgen/region_climate_functions_test.cpp
    tests/worldgen/remeasurement_test.cpp
    tests/worldgen/road_network_test.cpp
    tests/worldgen/terrain_bottleneck_test.cpp
    tests/zone/zone_key_test.cpp
    tests/zone/zone_lifecycle_test.cpp
    tests/zone/zone_codec_test.cpp
    tests/zone/zone_store_contract_test.cpp
    tests/zone/file_zone_store_test.cpp
    tests/zone/file_zone_store_manifest_test.cpp
    tests/zone/zone_manager_test.cpp
    tests/zone/zone_manager_tick_test.cpp
    tests/zone/cross_zone_test.cpp
    tests/zone/diplomacy_save_test.cpp
)
target_compile_definitions(aetheria_tests PRIVATE
    AETHERIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(aetheria_tests
    PRIVATE aetheria_core cereal::cereal GTest::gtest_main zstd::libzstd_static
)
aetheria_enable_warnings(aetheria_tests)
include(GoogleTest)
gtest_discover_tests(aetheria_tests)
add_test(
    NAME CoreIsolation.CompileCommands
    COMMAND "${CMAKE_COMMAND}"
        -DCOMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json
        -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
        -P "${PROJECT_SOURCE_DIR}/cmake/check_core_isolation.cmake"
)

add_executable(aetheria_cross_zone_half_move_negative
    tests/zone/cross_zone_half_move_negative.cpp
)
target_link_libraries(aetheria_cross_zone_half_move_negative
    PRIVATE aetheria_core GTest::gtest_main
)
aetheria_enable_warnings(aetheria_cross_zone_half_move_negative)
add_test(
    NAME CrossZoneNegative.HalfMove
    COMMAND "${CMAKE_COMMAND}"
        -DNEGATIVE_TEST=$<TARGET_FILE:aetheria_cross_zone_half_move_negative>
        -P "${PROJECT_SOURCE_DIR}/cmake/check_cross_zone_half_move_negative.cmake"
)
add_test(
    NAME GenerationIsolation.CrossZoneCompileFailure
    COMMAND "${CMAKE_COMMAND}"
        -DCXX=${CMAKE_CXX_COMPILER}
        -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
        -DTEST_OBJECT=${CMAKE_BINARY_DIR}/worldgen-cross-zone-negative.o
        -P "${PROJECT_SOURCE_DIR}/cmake/check_worldgen_cross_zone_isolation.cmake"
)
add_test(
    NAME FactionAiIsolation.WorldTruthCompileFailure
    COMMAND "${CMAKE_COMMAND}"
        -DCXX=${CMAKE_CXX_COMPILER}
        -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
        -DTEST_OBJECT=${CMAKE_BINARY_DIR}/faction-ai-world-truth-negative.o
        -P "${PROJECT_SOURCE_DIR}/cmake/check_faction_view_isolation.cmake"
)

add_executable(aetheria_combat_ratio_domain_negative
    tests/rules/combat_ratio_domain_negative.cpp
)
target_compile_definitions(aetheria_combat_ratio_domain_negative PRIVATE
    AETHERIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(aetheria_combat_ratio_domain_negative PRIVATE aetheria_core)
aetheria_enable_warnings(aetheria_combat_ratio_domain_negative)
add_test(
    NAME CombatNegative.RatioDomain
    COMMAND "${CMAKE_COMMAND}"
        -DNEGATIVE_TEST=$<TARGET_FILE:aetheria_combat_ratio_domain_negative>
        -P "${PROJECT_SOURCE_DIR}/cmake/check_combat_ratio_domain_negative.cmake"
)

# M6.4 追加區塊：保持在檔尾，避免多路工作切開既有 target 宣告。
target_sources(aetheria_tests PRIVATE
    tests/world/named_fate_test.cpp
)
add_executable(aetheria_named_fate_order_negative
    tests/world/named_fate_order_negative.cpp
)
target_link_libraries(aetheria_named_fate_order_negative
    PRIVATE aetheria_core GTest::gtest_main
)
aetheria_enable_warnings(aetheria_named_fate_order_negative)
add_test(
    NAME NamedFateNegative.UnorderedIteration
    COMMAND "${CMAKE_COMMAND}"
        -DNEGATIVE_TEST=$<TARGET_FILE:aetheria_named_fate_order_negative>
        -P "${PROJECT_SOURCE_DIR}/cmake/check_named_fate_order_negative.cmake"
)

# M6.6c 與 M6.5 整合追加區：測試來源只在檔尾擴充。
target_sources(aetheria_tests PRIVATE
    tests/site/site_observation_persistence_test.cpp
    tests/world/faction_ai_test.cpp
)

# M6.7 追加區塊：三層校準與兩項必紅故障注入。
target_sources(aetheria_tests PRIVATE
    tests/world/combat_scaling_test.cpp
)
add_executable(aetheria_combat_scaling_negative
    tests/world/combat_scaling_negative.cpp
)
target_compile_definitions(aetheria_combat_scaling_negative PRIVATE
    AETHERIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(aetheria_combat_scaling_negative
    PRIVATE aetheria_core GTest::gtest_main
)
aetheria_enable_warnings(aetheria_combat_scaling_negative)
add_test(
    NAME CombatScalingNegative.RequiredFailures
    COMMAND "${CMAKE_COMMAND}"
        -DNEGATIVE_TEST=$<TARGET_FILE:aetheria_combat_scaling_negative>
        -P "${PROJECT_SOURCE_DIR}/cmake/check_combat_scaling_negative.cmake"
)
