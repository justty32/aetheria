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
    message(FATAL_ERROR "三層校準負向控制沒有紅")
endif()
if(NOT output MATCHES "delta_clamp_negative region_loss=[0-9]+ local_unclamped=[0-9]+ army_power=100000")
    message(FATAL_ERROR "δ clamp 負向控制沒有量到單人勝軍：${output}${error}")
endif()
if(NOT output MATCHES "double_settlement_negative site_runs=1 region_damage_writes=1")
    message(FATAL_ERROR "雙重結算負向控制沒有寫入一次 Region 損傷：${output}${error}")
endif()

message(STATUS "三層校準兩項負向控制如預期紅")
