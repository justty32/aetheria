if(NOT DEFINED SIM OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "SIM 與 TEST_DIR 必須指定")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")

execute_process(
    COMMAND "${SIM}" gen region --seed 12345 --dump-stages "${TEST_DIR}"
    RESULT_VARIABLE region_result
    OUTPUT_VARIABLE region_output
    ERROR_VARIABLE region_error
)
if(NOT region_result EQUAL 0)
    message(FATAL_ERROR "gen region 失敗：${region_error}")
endif()

foreach(stage IN ITEMS 01-plates.pgm 02-height.pgm 03-erosion.pgm 04-climate.pgm 05-rivers.pgm
                       06-biome.pgm 07-features.pgm 08-history.pgm 09-cities.pgm 10-roads.pgm)
    set(stage_path "${TEST_DIR}/${stage}")
    if(NOT EXISTS "${stage_path}")
        message(FATAL_ERROR "缺少 stage dump：${stage_path}")
    endif()
    file(SIZE "${stage_path}" stage_size)
    if(stage_size LESS 12288)
        message(FATAL_ERROR "stage dump 尺寸異常：${stage_path} (${stage_size})")
    endif()
endforeach()

file(GLOB stage_dumps "${TEST_DIR}/*.pgm")
list(LENGTH stage_dumps stage_dump_count)
if(NOT stage_dump_count EQUAL 10)
    message(FATAL_ERROR "stage dump 數量應為 10，實際 ${stage_dump_count}")
endif()

if(NOT region_output MATCHES "land_connected=true")
    message(FATAL_ERROR "gen region 未回報連通陸地：${region_output}")
endif()

execute_process(
    COMMAND "${SIM}" gen verify --seed 12345 --iterations 3
    RESULT_VARIABLE verify_result
    OUTPUT_VARIABLE verify_output
    ERROR_VARIABLE verify_error
)
if(NOT verify_result EQUAL 0)
    message(FATAL_ERROR "gen verify 失敗：${verify_error}")
endif()
if(NOT verify_output MATCHES "verified_iterations=3")
    message(FATAL_ERROR "gen verify 輸出缺少完成證據：${verify_output}")
endif()
