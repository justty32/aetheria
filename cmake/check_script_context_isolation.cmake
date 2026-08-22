if(NOT DEFINED CXX OR NOT DEFINED PROJECT_ROOT OR NOT DEFINED TEST_OBJECT)
    message(FATAL_ERROR "缺少 CXX、PROJECT_ROOT 或 TEST_OBJECT")
endif()

set(_source "${PROJECT_ROOT}/tests/compile_fail/script_context_world_truth.cpp")
set(_public_include "${PROJECT_ROOT}/core/script/include")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=CPLUS_INCLUDE_PATH --unset=CPATH
            "${CXX}" -std=c++23 -I "${_public_include}" -c "${_source}" -o "${TEST_OBJECT}"
    RESULT_VARIABLE restricted_result
    OUTPUT_VARIABLE restricted_output
    ERROR_VARIABLE restricted_error
)
if(restricted_result EQUAL 0)
    file(REMOVE "${TEST_OBJECT}")
    message(FATAL_ERROR "受限 script target 竟可 include Context 世界真值接點")
endif()
if(NOT restricted_error MATCHES "core/script/context_internal.h.*(No such file|file not found)")
    message(FATAL_ERROR
        "受限編譯雖失敗，但不是 include 隔離護欄：${restricted_output}${restricted_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=CPLUS_INCLUDE_PATH --unset=CPATH
            "${CXX}" -std=c++23 -I "${_public_include}" -I "${PROJECT_ROOT}"
            -c "${_source}" -o "${TEST_OBJECT}"
    RESULT_VARIABLE relaxed_result
    OUTPUT_VARIABLE relaxed_output
    ERROR_VARIABLE relaxed_error
)
if(NOT relaxed_result EQUAL 0)
    message(FATAL_ERROR
        "解除 include 隔離後仍編不過，可能是假路徑通過：${relaxed_output}${relaxed_error}")
endif()
file(REMOVE "${TEST_OBJECT}")
message(STATUS "Context 隔離成立：受限編譯失敗，解除隔離後編譯成功")
