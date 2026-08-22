if(NOT DEFINED CXX OR NOT DEFINED PROJECT_ROOT OR NOT DEFINED TEST_OBJECT)
    message(FATAL_ERROR "缺少 CXX、PROJECT_ROOT 或 TEST_OBJECT")
endif()

execute_process(
    COMMAND "${CXX}" -std=c++23 -I "${PROJECT_ROOT}" -c
            "${PROJECT_ROOT}/tests/compile_fail/deity_direct_region_write.cpp"
            -o "${TEST_OBJECT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    file(REMOVE "${TEST_OBJECT}")
    message(FATAL_ERROR "神祇竟可繞過信徒事件中介直接取得 Region 狀態")
endif()
if(NOT error MATCHES "(too many arguments|no matching function)")
    message(FATAL_ERROR "編譯雖失敗，但不是 Region 中介 API 護欄：${output}${error}")
endif()

message(STATUS "神祇直寫 Region 如預期編譯失敗：${error}")
