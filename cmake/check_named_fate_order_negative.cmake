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
    message(FATAL_ERROR "具名命運 unordered_map 順序負向控制沒有紅")
endif()
if(NOT output MATCHES "named_fate_negative mismatched_entities=[1-9][0-9]* container=unordered_map")
    message(FATAL_ERROR "負向控制雖紅，但不是 uid 命運錯配：${output}${error}")
endif()

message(STATUS "具名命運負向控制如預期紅：unordered_map 造成 uid 命運錯配")
