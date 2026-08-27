if(NOT DEFINED SIM OR NOT DEFINED TEST_DIR OR NOT DEFINED SOURCE_DATA_DIR)
    message(FATAL_ERROR "SIM、TEST_DIR 與 SOURCE_DATA_DIR 必須指定")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
set(OLD_SLOT "${TEST_DIR}/old-world")
set(NEW_SLOT "${TEST_DIR}/new-world")
set(PRIVATE_DATA "${TEST_DIR}/private-data")
file(MAKE_DIRECTORY "${PRIVATE_DATA}")
file(COPY "${SOURCE_DATA_DIR}/" DESTINATION "${PRIVATE_DATA}")
execute_process(
    COMMAND "${SIM}" --tick 62208000 --data-dir "${PRIVATE_DATA}" --save-dir "${OLD_SLOT}"
    RESULT_VARIABLE create_result
    ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
    message(FATAL_ERROR "建立 world-hash 測試存檔失敗：${create_error}")
endif()

file(READ "${PRIVATE_DATA}/terrain.toml" terrain_toml)
string(REPLACE
    "id = \"terrain.swamp\"\nname_key = \"terrain.swamp.name\"\nmove_cost = 3"
    "id = \"terrain.swamp\"\nname_key = \"terrain.swamp.name\"\nmove_cost = 9"
    changed_terrain_toml "${terrain_toml}")
if(changed_terrain_toml STREQUAL terrain_toml)
    message(FATAL_ERROR "未改到 private terrain.swamp move_cost")
endif()
file(WRITE "${PRIVATE_DATA}/terrain.toml" "${changed_terrain_toml}")
execute_process(
    COMMAND "${SIM}" --tick 62208000 --data-dir "${PRIVATE_DATA}" --save-dir "${NEW_SLOT}"
    RESULT_VARIABLE create_new_result
    ERROR_VARIABLE create_new_error
)
if(NOT create_new_result EQUAL 0)
    message(FATAL_ERROR "建立修改 data 後的新槽失敗：${create_new_error}")
endif()

execute_process(
    COMMAND "${SIM}" --data-dir "${TEST_DIR}/must-not-be-read" verify world-hash "${OLD_SLOT}"
    RESULT_VARIABLE old_hash_result
    OUTPUT_VARIABLE old_hash_output
    ERROR_VARIABLE old_hash_error
)
if(NOT old_hash_result EQUAL 0)
    message(FATAL_ERROR "舊槽 verify world-hash 失敗：${old_hash_error}")
endif()
execute_process(
    COMMAND "${SIM}" verify world-hash "${NEW_SLOT}"
    RESULT_VARIABLE new_hash_result
    OUTPUT_VARIABLE new_hash_output
    ERROR_VARIABLE new_hash_error
)
if(NOT new_hash_result EQUAL 0)
    message(FATAL_ERROR "新槽 verify world-hash 失敗：${new_hash_error}")
endif()
foreach(hash_output IN ITEMS "${old_hash_output}" "${new_hash_output}")
    if(NOT hash_output MATCHES "zone_hash=[0-9]+ zone_count=4" OR
       NOT hash_output MATCHES "raws_hash=[0-9]+" OR
       NOT hash_output MATCHES "history_head_hash=[0-9]+ history_seq=0" OR
       NOT hash_output MATCHES "world_hash=[0-9]+ elapsed_ms=[0-9.]+")
        message(FATAL_ERROR "verify world-hash 輸出格式不符：${hash_output}")
    endif()
endforeach()

if(NOT EXISTS "${OLD_SLOT}/raws/terrain.toml" OR
   NOT EXISTS "${NEW_SLOT}/raws/terrain.toml")
    message(FATAL_ERROR "新舊槽未各自帶 raws")
endif()

file(REMOVE_RECURSE "${TEST_DIR}")
message(STATUS "aetheria_sim 舊槽 verify：${old_hash_output}")
message(STATUS "aetheria_sim 新槽 verify：${new_hash_output}")
