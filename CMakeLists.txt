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

# 各 target 的來源清單與測試登記分檔在 cmake/，本檔只保留專案組態與載入順序。
include("${PROJECT_SOURCE_DIR}/cmake/targets_core.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/targets_tests.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/targets_sim.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/godot_toolchain.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/targets_bridge.cmake")

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
