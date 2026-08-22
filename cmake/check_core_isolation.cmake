if(NOT DEFINED COMPILE_COMMANDS OR NOT DEFINED PROJECT_ROOT)
    message(FATAL_ERROR "缺少 COMPILE_COMMANDS 或 PROJECT_ROOT")
endif()

file(STRINGS "${COMPILE_COMMANDS}" _compile_command_lines REGEX "^[ ]*\\\"command\\\":")
file(GLOB_RECURSE _core_sources "${PROJECT_ROOT}/core/*.cpp")
list(LENGTH _core_sources _expected_core_translation_unit_count)
set(_core_translation_unit_count 0)

foreach(_compile_command_line IN LISTS _compile_command_lines)
    if(_compile_command_line MATCHES
       "CMakeFiles/(aetheria_core|aetheria_worldgen_objects)\\.dir/")
        math(EXPR _core_translation_unit_count "${_core_translation_unit_count} + 1")
        if(_compile_command_line MATCHES "godot[-_]cpp")
            message(FATAL_ERROR "core 編譯命令含 godot-cpp：${_compile_command_line}")
        endif()
    endif()
endforeach()

if(NOT _core_translation_unit_count EQUAL _expected_core_translation_unit_count)
    message(FATAL_ERROR
        "core translation unit 數量不符：原始碼 ${_expected_core_translation_unit_count}，"
        "編譯命令 ${_core_translation_unit_count}")
endif()

message(STATUS "已檢查 ${_core_translation_unit_count} 個 core translation units：無 godot-cpp")
