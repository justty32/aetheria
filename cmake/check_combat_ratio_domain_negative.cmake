execute_process(
    COMMAND "${NEGATIVE_TEST}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(result EQUAL 0)
    message(FATAL_ERROR "超域 R 負向控制未失敗")
endif()
if(NOT error MATCHES "AETH_CHECK failed: ratio_within_domain")
    message(FATAL_ERROR "超域 R 未回報預期訊息：${error}")
endif()
