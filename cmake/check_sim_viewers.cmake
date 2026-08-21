if(NOT DEFINED SIM OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "check_sim_viewers.cmake 需要 SIM 與 TEST_DIR")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")

function(run_viewer NAME)
    execute_process(
        COMMAND "${SIM}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${NAME} 失敗：${result}\nstdout=${output}\nstderr=${error}")
    endif()
    set("${NAME}_OUTPUT" "${output}" PARENT_SCOPE)
endfunction()

run_viewer(local_city gen local --site-seed 0x5A17 --zoning residential --z all
           --output "${TEST_DIR}/local-city")
if(NOT local_city_OUTPUT MATCHES "houses=15 rooms=124 doors=108")
    message(FATAL_ERROR "Local 固定見證漂移：${local_city_OUTPUT}")
endif()

run_viewer(local_wilderness gen local --site-seed 0x5A17 --zoning open --z 0
           --output "${TEST_DIR}/local-wilderness")
run_viewer(site_city gen site --site-seed 0x5A17 --kind city
           --output "${TEST_DIR}/site-city")
run_viewer(site_wilderness gen site --site-seed 0x5A17 --kind wilderness
           --output "${TEST_DIR}/site-wilderness")

set(required_pngs
    "${TEST_DIR}/local-city/local-residential-zm1-ground.png"
    "${TEST_DIR}/local-city/local-residential-zm1-edges.png"
    "${TEST_DIR}/local-city/local-residential-zm1-rooms.png"
    "${TEST_DIR}/local-city/local-residential-zm1-occupants.png"
    "${TEST_DIR}/local-city/local-residential-z0-ground.png"
    "${TEST_DIR}/local-city/local-residential-z0-edges.png"
    "${TEST_DIR}/local-city/local-residential-z0-rooms.png"
    "${TEST_DIR}/local-city/local-residential-z0-occupants.png"
    "${TEST_DIR}/local-city/local-residential-zp1-ground.png"
    "${TEST_DIR}/local-city/local-residential-zp1-edges.png"
    "${TEST_DIR}/local-city/local-residential-zp1-rooms.png"
    "${TEST_DIR}/local-city/local-residential-zp1-occupants.png"
    "${TEST_DIR}/local-wilderness/local-open-z0-ground.png"
    "${TEST_DIR}/local-wilderness/local-open-z0-edges.png"
    "${TEST_DIR}/local-wilderness/local-open-z0-rooms.png"
    "${TEST_DIR}/local-wilderness/local-open-z0-occupants.png"
    "${TEST_DIR}/site-city/site-city-ground.png"
    "${TEST_DIR}/site-city/site-city-blocks.png"
    "${TEST_DIR}/site-city/site-city-roads.png"
    "${TEST_DIR}/site-city/site-city-edges.png"
    "${TEST_DIR}/site-wilderness/site-wilderness-ground.png"
    "${TEST_DIR}/site-wilderness/site-wilderness-roads.png"
    "${TEST_DIR}/site-wilderness/site-wilderness-content.png"
)

foreach(path IN LISTS required_pngs)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "缺少 viewer PNG：${path}")
    endif()
    file(READ "${path}" signature HEX OFFSET 0 LIMIT 8)
    if(NOT signature STREQUAL "89504e470d0a1a0a")
        message(FATAL_ERROR "PNG signature 錯誤：${path}")
    endif()
endforeach()
