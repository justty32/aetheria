# aetheria_tests：GoogleTest 單元測試；來源清單依 tests/ 子目錄分檔。

add_executable(aetheria_tests)
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_narrative.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_local.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_rules.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_serialize.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_site.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_sim.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_time.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_world.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_worldgen.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_zone.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_script.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_runtime.cmake")

target_compile_definitions(aetheria_tests PRIVATE
    AETHERIA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(aetheria_tests
    PRIVATE aetheria_core cereal::cereal GTest::gtest_main zstd::libzstd_static
)
aetheria_enable_warnings(aetheria_tests)
include(GoogleTest)
gtest_discover_tests(aetheria_tests)
include("${CMAKE_CURRENT_LIST_DIR}/targets_tests_checks.cmake")
