cmake_minimum_required(VERSION 3.25)

project(aetheria VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

find_package(CLI11 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

function(aetheria_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_library(aetheria_core STATIC
    core/api/version.cpp
    core/time/tick.cpp
)
target_include_directories(aetheria_core PUBLIC "${PROJECT_SOURCE_DIR}")
aetheria_enable_warnings(aetheria_core)

# 這項檢查讓 core 的引擎隔離在 configure 階段就會失敗，而不是只靠約定。
get_target_property(_core_include_dirs aetheria_core INCLUDE_DIRECTORIES)
get_target_property(_core_interface_include_dirs aetheria_core INTERFACE_INCLUDE_DIRECTORIES)
set(_core_all_include_dirs "${_core_include_dirs};${_core_interface_include_dirs}")
if(_core_all_include_dirs MATCHES "godot[-_]cpp")
    message(FATAL_ERROR "aetheria_core 不得包含 godot-cpp include 路徑：${_core_all_include_dirs}")
endif()

add_executable(aetheria_tests tests/time/tick_test.cpp)
target_link_libraries(aetheria_tests PRIVATE aetheria_core GTest::gtest_main)
aetheria_enable_warnings(aetheria_tests)
include(GoogleTest)
gtest_discover_tests(aetheria_tests)

add_executable(aetheria_sim sim/main.cpp)
target_link_libraries(aetheria_sim PRIVATE aetheria_core CLI11::CLI11)
aetheria_enable_warnings(aetheria_sim)

set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API version" FORCE)
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "Use the same exception ABI as aetheria" FORCE)
set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "Do not build godot-cpp tests" FORCE)
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "Treat third-party headers as system headers" FORCE)
set(GODOTCPP_USE_STATIC_CPP OFF CACHE BOOL "Use the toolchain's standard C++ runtime" FORCE)

find_program(AETHERIA_GODOT_BIN NAMES godot godot4 godot-4 godot-mono)
if(AETHERIA_GODOT_BIN)
    set(_godot_api_dir "${CMAKE_BINARY_DIR}/godot-api")
    set(_godot_api_file "${_godot_api_dir}/extension_api.json")
    if(NOT EXISTS "${_godot_api_file}")
        file(MAKE_DIRECTORY "${_godot_api_dir}")
        execute_process(
            COMMAND "${AETHERIA_GODOT_BIN}" --headless --dump-extension-api
            WORKING_DIRECTORY "${_godot_api_dir}"
            RESULT_VARIABLE _godot_api_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
    endif()
    if(EXISTS "${_godot_api_file}")
        set(GODOTCPP_CUSTOM_API_FILE "${_godot_api_file}" CACHE FILEPATH
            "API dumped from the local Godot executable" FORCE)
        message(STATUS "godot-cpp API 由 ${AETHERIA_GODOT_BIN} 產生")
    else()
        message(WARNING "Godot API dump 失敗（exit=${_godot_api_result}），改用 godot-cpp 內建 4.7 API")
    endif()
else()
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
