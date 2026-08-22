if(NOT DEFINED NEGATIVE_TEST)
    message(FATAL_ERROR "缺少 NEGATIVE_TEST")
endif()

execute_process(
    COMMAND "${NEGATIVE_TEST}" --gtest_color=no
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    message(FATAL_ERROR "半搬負向控制沒有紅")
endif()
if(NOT output MATCHES "half_move_negative source_valid=0 destination_migrated=0")
    message(FATAL_ERROR "半搬雖紅，但沒有抓到來源與目的同時遺失：${output}${error}")
endif()

message(STATUS "半搬負向控制如預期紅：source_valid=0 destination_migrated=0")
