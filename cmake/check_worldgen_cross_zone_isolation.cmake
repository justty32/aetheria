if(NOT DEFINED CXX OR NOT DEFINED PROJECT_ROOT OR NOT DEFINED TEST_OBJECT)
    message(FATAL_ERROR "缺少 CXX、PROJECT_ROOT 或 TEST_OBJECT")
endif()

execute_process(
    COMMAND "${CXX}" -std=c++23 -I "${PROJECT_ROOT}" -c
            "${PROJECT_ROOT}/tests/compile_fail/worldgen_cross_zone_include.cpp"
            -o "${TEST_OBJECT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    file(REMOVE "${TEST_OBJECT}")
    message(FATAL_ERROR "生成 target 竟可 include 執行期跨-zone header")
endif()
if(NOT error MATCHES "aetheria/runtime/cross_zone.h.*(No such file|file not found)")
    message(FATAL_ERROR "編譯雖失敗，但不是 header 可見性護欄：${output}${error}")
endif()

message(STATUS "生成 target include 跨-zone header 如預期編譯失敗：${error}")
