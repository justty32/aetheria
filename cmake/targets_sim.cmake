# aetheria_sim：不需 Godot 的 headless CLI 探針，外加兩個跨行程 CTest 檢查。

add_executable(aetheria_sim
    sim/main.cpp
    sim/debug_canvas.cpp
    sim/gen_commands.cpp
    sim/local_viewer.cpp
    sim/site_viewer.cpp
    sim/stage_dump.cpp
    sim/pgm_writer.cpp
    sim/world_hash.cpp
)
target_link_libraries(aetheria_sim PRIVATE aetheria_core CLI11::CLI11)
target_compile_definitions(aetheria_sim PRIVATE
    AETHERIA_DEFAULT_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
)
aetheria_enable_warnings(aetheria_sim)
add_test(
    NAME SimPersistence.TwoProcesses
    COMMAND "${CMAKE_COMMAND}"
        -DSIM=$<TARGET_FILE:aetheria_sim>
        -DTEST_DIR=${CMAKE_BINARY_DIR}/sim-persistence-test
        -P "${PROJECT_SOURCE_DIR}/cmake/check_sim_persistence.cmake"
)
add_test(
    NAME SimWorldgen.DumpAndVerify
    COMMAND "${CMAKE_COMMAND}"
        -DSIM=$<TARGET_FILE:aetheria_sim>
        -DTEST_DIR=${CMAKE_BINARY_DIR}/sim-worldgen-test
        -P "${PROJECT_SOURCE_DIR}/cmake/check_sim_worldgen.cmake"
)
add_test(
    NAME SimViewers.GeneratePng
    COMMAND "${CMAKE_COMMAND}"
        -DSIM=$<TARGET_FILE:aetheria_sim>
        -DTEST_DIR=${CMAKE_BINARY_DIR}/sim-viewer-test
        -P "${PROJECT_SOURCE_DIR}/cmake/check_sim_viewers.cmake"
)
add_test(
    NAME SimWorldHash.Command
    COMMAND "${CMAKE_COMMAND}"
        -DSIM=$<TARGET_FILE:aetheria_sim>
        -DTEST_DIR=${CMAKE_BINARY_DIR}/sim-world-hash-test
        -P "${PROJECT_SOURCE_DIR}/cmake/check_sim_world_hash.cmake"
)
add_test(
    NAME VerificationBoundary.WorldHash
    COMMAND "${CMAKE_COMMAND}"
        -DPROJECT_ROOT=${PROJECT_SOURCE_DIR}
        -P "${PROJECT_SOURCE_DIR}/cmake/check_world_hash_boundary.cmake"
)
