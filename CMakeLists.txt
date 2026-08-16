cmake_minimum_required(VERSION 3.25)

project(aetheria VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

find_package(cereal CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

function(aetheria_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_library(aetheria_core STATIC
    core/api/version.cpp
    core/rules/ruleset.cpp
    core/serialize/normalized_state_hash.cpp
    core/time/tick.cpp
    core/serialize/zone_codec.cpp
    core/zone/file_zone_store.cpp
    core/zone/zone_manager.cpp
    core/zone/zone_store.cpp
    core/world/region_tiles.cpp
    core/world/region_movement.cpp
    core/worldgen/region_generator.cpp
)
target_include_directories(aetheria_core PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(aetheria_core
    PUBLIC EnTT::EnTT
    PRIVATE cereal::cereal tomlplusplus::tomlplusplus zstd::libzstd_static
)
aetheria_enable_warnings(aetheria_core)

add_executable(aetheria_tests
    tests/rules/ruleset_test.cpp
    tests/serialize/registry_codec_test.cpp
    tests/time/tick_test.cpp
    tests/world/region_movement_test.cpp
    tests/world/region_tiles_test.cpp
    tests/worldgen/region_generator_test.cpp
    tests/zone/zone_store_test.cpp
    tests/zone/zone_test.cpp
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
add_executable(aetheria_sim sim/main.cpp)
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

set(AETHERIA_EXPECTED_GODOT_VERSION "4.7.1")
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API version" FORCE)
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "Use the same exception ABI as aetheria" FORCE)
set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "Do not build godot-cpp tests" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Treat third-party headers as system headers" FORCE)
set(GODOTCPP_USE_STATIC_CPP OFF CACHE BOOL "Use the toolchain's standard C++ runtime" FORCE)

set(AETHERIA_GODOT_BIN "" CACHE FILEPATH "Godot executable used to dump the extension API")
if(AETHERIA_GODOT_BIN STREQUAL "")
    find_program(_aetheria_detected_godot_bin NAMES godot godot4 godot-4 godot-mono)
    if(_aetheria_detected_godot_bin)
        set(AETHERIA_GODOT_BIN "${_aetheria_detected_godot_bin}" CACHE FILEPATH
            "Godot executable used to dump the extension API" FORCE)
    endif()
endif()
if(AETHERIA_GODOT_BIN)
    set(_godot_api_dir "${CMAKE_BINARY_DIR}/godot-api")
    set(_godot_api_file "${_godot_api_dir}/extension_api.json")
    set(_godot_api_probe_dir "${CMAKE_BINARY_DIR}/godot-api-probe")
    set(_godot_api_probe_file "${_godot_api_probe_dir}/extension_api.json")
    file(REMOVE_RECURSE "${_godot_api_probe_dir}")
    file(MAKE_DIRECTORY "${_godot_api_probe_dir}")
    execute_process(
        COMMAND "${AETHERIA_GODOT_BIN}" --headless --dump-extension-api
        WORKING_DIRECTORY "${_godot_api_probe_dir}"
        RESULT_VARIABLE _godot_api_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(EXISTS "${_godot_api_probe_file}")
        file(READ "${_godot_api_probe_file}" _godot_api_json)
        string(JSON _godot_version_major GET "${_godot_api_json}" header version_major)
        string(JSON _godot_version_minor GET "${_godot_api_json}" header version_minor)
        string(JSON _godot_version_patch GET "${_godot_api_json}" header version_patch)
        string(JSON _godot_version_full_name GET "${_godot_api_json}" header version_full_name)
        set(_godot_local_version
            "${_godot_version_major}.${_godot_version_minor}.${_godot_version_patch}")
        if(NOT "${_godot_local_version}" VERSION_EQUAL "${AETHERIA_EXPECTED_GODOT_VERSION}")
            message(FATAL_ERROR
                "本機 Godot ${_godot_local_version}（${_godot_version_full_name}），"
                "專案期望 ${AETHERIA_EXPECTED_GODOT_VERSION}")
        endif()
        file(MAKE_DIRECTORY "${_godot_api_dir}")
        configure_file("${_godot_api_probe_file}" "${_godot_api_file}" COPYONLY)
        set(GODOTCPP_CUSTOM_API_FILE "${_godot_api_file}" CACHE FILEPATH
            "API dumped from the local Godot executable" FORCE)
        message(STATUS
            "godot-cpp API 由 ${AETHERIA_GODOT_BIN} 產生（${_godot_local_version}）")
    else()
        unset(GODOTCPP_CUSTOM_API_FILE CACHE)
        message(WARNING "Godot API dump 失敗（exit=${_godot_api_result}），改用 godot-cpp 內建 4.7 API")
    endif()
    file(REMOVE_RECURSE "${_godot_api_probe_dir}")
else()
    unset(GODOTCPP_CUSTOM_API_FILE CACHE)
    message(STATUS "找不到 Godot 執行檔；bridge 將使用 godot-cpp 內建 4.7 API")
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/godot-cpp/CMakeLists.txt")
    message(FATAL_ERROR
        "缺少 third_party/godot-cpp；請執行 git submodule update --init --recursive")
endif()
set(AETHERIA_GODOT_CPP_REVISION "d7b6162249ed52796a8301d216c24ee71d68c2bf")
if(EXISTS "${PROJECT_SOURCE_DIR}/third_party/godot-cpp/.git")
    execute_process(
        COMMAND git -C "${PROJECT_SOURCE_DIR}/third_party/godot-cpp" rev-parse HEAD
        RESULT_VARIABLE _godot_cpp_revision_result
        OUTPUT_VARIABLE _godot_cpp_actual_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _godot_cpp_revision_result EQUAL 0 OR
       NOT _godot_cpp_actual_revision STREQUAL AETHERIA_GODOT_CPP_REVISION)
        message(FATAL_ERROR
            "godot-cpp revision 不符：預期 ${AETHERIA_GODOT_CPP_REVISION}，"
            "實際 ${_godot_cpp_actual_revision}")
    endif()
endif()
add_subdirectory(third_party/godot-cpp EXCLUDE_FROM_ALL)

# godot-cpp 上游預設 C++17；M0 要求 core、bridge、godot-cpp 同為 C++23。
set_target_properties(godot-cpp PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_library(aetheria_bridge SHARED
    bridge/aetheria_core.cpp
    bridge/register_types.cpp
)
target_include_directories(aetheria_bridge PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(aetheria_bridge PRIVATE aetheria_core godot-cpp)
set_target_properties(aetheria_bridge PROPERTIES
    OUTPUT_NAME aetheria_bridge
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
)
aetheria_enable_warnings(aetheria_bridge)

# 所有 target 都定義完後才檢查，避免後續相依繞過 core 的引擎隔離。
foreach(_core_property
        INCLUDE_DIRECTORIES
        INTERFACE_INCLUDE_DIRECTORIES
        LINK_LIBRARIES
        INTERFACE_LINK_LIBRARIES)
    get_target_property(_core_property_value aetheria_core ${_core_property})
    if(_core_property_value AND _core_property_value MATCHES "godot[-_]cpp")
        message(FATAL_ERROR
            "aetheria_core 的 ${_core_property} 不得含 godot-cpp：${_core_property_value}")
    endif()
endforeach()
