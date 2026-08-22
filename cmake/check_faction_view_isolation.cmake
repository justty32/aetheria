if(NOT DEFINED CXX OR NOT DEFINED PROJECT_ROOT OR NOT DEFINED TEST_OBJECT)
    message(FATAL_ERROR "缺少 CXX、PROJECT_ROOT 或 TEST_OBJECT")
endif()

execute_process(
    COMMAND "${CXX}" -std=c++23 -I "${PROJECT_ROOT}/core/ai/include" -c
            "${PROJECT_ROOT}/tests/compile_fail/faction_ai_world_truth.cpp"
            -o "${TEST_OBJECT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    file(REMOVE "${TEST_OBJECT}")
    message(FATAL_ERROR "AI target 竟可 include 外交世界真值標頭")
endif()
if(NOT error MATCHES "core/world/diplomacy.h.*(No such file|file not found)")
    message(FATAL_ERROR "編譯雖失敗，但不是世界真值 include 護欄：${output}${error}")
endif()

message(STATUS "AI 直接讀世界真值如預期編譯失敗：${error}")
