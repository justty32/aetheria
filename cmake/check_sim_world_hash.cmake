if(NOT DEFINED SIM OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "SIM 與 TEST_DIR 必須指定")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
execute_process(
    COMMAND "${SIM}" --tick 62208000 --save-dir "${TEST_DIR}"
    RESULT_VARIABLE create_result
    ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
    message(FATAL_ERROR "建立 world-hash 測試存檔失敗：${create_error}")
endif()

execute_process(
    COMMAND "${SIM}" verify world-hash "${TEST_DIR}"
    RESULT_VARIABLE hash_result
    OUTPUT_VARIABLE hash_output
    ERROR_VARIABLE hash_error
)
if(NOT hash_result EQUAL 0)
    message(FATAL_ERROR "verify world-hash 失敗：${hash_error}")
endif()
if(NOT hash_output MATCHES "world_hash=[0-9]+ zone_count=4 elapsed_ms=[0-9.]+")
    message(FATAL_ERROR "verify world-hash 輸出格式不符：${hash_output}")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
message(STATUS "aetheria_sim verify world-hash 子命令可用：${hash_output}")
