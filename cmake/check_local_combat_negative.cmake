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
    message(FATAL_ERROR "Local 戰鬥負向控制沒有紅")
endif()
if(NOT output MATCHES "local_boundary_negative expected_site_edge=1 locally_chosen_edge=0")
    message(FATAL_ERROR "入口權威故障注入沒有量到 East→North：${output}${error}")
endif()
if(NOT output MATCHES "local_m5_negative expected_wall_attacks=0 broken_attacks=1")
    message(FATAL_ERROR "M5 FOV 故障注入沒有量到多一次穿牆攻擊：${output}${error}")
endif()

message(STATUS "Local 戰鬥兩項負向控制如預期紅：入口 East→North、穿牆攻擊 0→1")
