# Negative 執行檔與 CTest 結構檢查；保留原登記順序及參數。

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

add_test(
    NAME FaithIsolation.DeityRegionCompileFailure
    COMMAND "${CMAKE_COMMAND}"
        -DCXX=${CMAKE_CXX_COMPILER}
        -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
        -DTEST_OBJECT=${CMAKE_BINARY_DIR}/deity-region-negative.o
        -P "${PROJECT_SOURCE_DIR}/cmake/check_deity_region_mediation.cmake"
)

add_executable(aetheria_local_combat_negative
    tests/local/local_combat_negative.cpp
)

target_link_libraries(aetheria_local_combat_negative
    PRIVATE aetheria_core GTest::gtest_main
)

aetheria_enable_warnings(aetheria_local_combat_negative)

add_test(
    NAME LocalCombatNegative.RequiredFailures
    COMMAND "${CMAKE_COMMAND}"
        -DNEGATIVE_TEST=$<TARGET_FILE:aetheria_local_combat_negative>
        -P "${PROJECT_SOURCE_DIR}/cmake/check_local_combat_negative.cmake"
)

add_test(
    NAME ScriptContextIsolation.WorldTruthCompileFailure
    COMMAND "${CMAKE_COMMAND}"
        -DCXX=${CMAKE_CXX_COMPILER}
        -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
        -DTEST_OBJECT=${CMAKE_BINARY_DIR}/script-context-world-truth-negative.o
        -P "${PROJECT_SOURCE_DIR}/cmake/check_script_context_isolation.cmake"
)

add_test(
    NAME principle5.ContentKindsAreData
    COMMAND python3 "${PROJECT_SOURCE_DIR}/tools/check_principle5.py"
        --project-root "${PROJECT_SOURCE_DIR}"
        --core "${PROJECT_SOURCE_DIR}/core"
        --allowlist "${PROJECT_SOURCE_DIR}/tests/principle5_allowlist.txt"
)
