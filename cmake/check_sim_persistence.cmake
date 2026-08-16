if(NOT DEFINED SIM OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "SIM 與 TEST_DIR 必須指定")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")

execute_process(
    COMMAND "${SIM}" --tick 62208000 --save-dir "${TEST_DIR}"
    RESULT_VARIABLE first_result
    OUTPUT_VARIABLE first_output
    ERROR_VARIABLE first_error
)
if(NOT first_result EQUAL 0)
    message(FATAL_ERROR "sim 首次執行失敗 (${first_result}): ${first_error}")
endif()

execute_process(
    COMMAND "${SIM}" --tick 62208000 --save-dir "${TEST_DIR}"
    RESULT_VARIABLE second_result
    OUTPUT_VARIABLE second_output
    ERROR_VARIABLE second_error
)
if(NOT second_result EQUAL 0)
    message(FATAL_ERROR "sim 第二次執行失敗 (${second_result}): ${second_error}")
endif()

if(NOT first_output STREQUAL second_output)
    message(FATAL_ERROR
        "sim 跨程序輸出不同\n--- first ---\n${first_output}\n--- second ---\n${second_output}")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
message(STATUS "sim 兩次執行輸出逐位元組相同")
