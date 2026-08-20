# aetheria_tests：GoogleTest 單元測試，外加 core 引擎隔離的 CTest 檢查。

add_executable(aetheria_tests
    tests/rules/ruleset_load_test.cpp
    tests/rules/ruleset_error_test.cpp
    tests/rules/ruleset_zone_codec_test.cpp
    tests/serialize/registry_codec_test.cpp
    tests/time/tick_test.cpp
    tests/world/region_step_cost_test.cpp
    tests/world/region_path_test.cpp
    tests/world/region_turn_test.cpp
    tests/world/region_tiles_test.cpp
    tests/worldgen/city_sites_test.cpp
    tests/worldgen/history_feedback_test.cpp
    tests/worldgen/history_identity_test.cpp
    tests/worldgen/history_isolation_test.cpp
    tests/worldgen/history_layer_test.cpp
    tests/worldgen/influence_spread_test.cpp
    tests/worldgen/region_determinism_test.cpp
    tests/worldgen/region_output_validation_test.cpp
    tests/worldgen/region_perf_test.cpp
    tests/worldgen/region_stage_functions_test.cpp
    tests/worldgen/road_network_test.cpp
    tests/zone/zone_key_test.cpp
    tests/zone/zone_lifecycle_test.cpp
    tests/zone/zone_codec_test.cpp
    tests/zone/zone_store_contract_test.cpp
    tests/zone/file_zone_store_test.cpp
    tests/zone/file_zone_store_manifest_test.cpp
    tests/zone/zone_manager_test.cpp
    tests/zone/zone_manager_tick_test.cpp
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
