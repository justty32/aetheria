# 純 C++ 核心 target 骨架；來源清單依 core/ 子系統分檔。

# aetheria_faction_ai_objects 只看得到知識視圖；世界真值 header 不在 include path。
add_library(aetheria_faction_ai_objects OBJECT)
target_include_directories(aetheria_faction_ai_objects PRIVATE
    "${PROJECT_SOURCE_DIR}/core/ai/include"
)
aetheria_enable_warnings(aetheria_faction_ai_objects)

# aetheria_worldgen_objects 只有 repo 根與純資料公式；執行期跨-zone header 對它不可見。
add_library(aetheria_worldgen_objects OBJECT)
target_include_directories(aetheria_worldgen_objects PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/core/ai/include"
)
target_link_libraries(aetheria_worldgen_objects PRIVATE EnTT::EnTT)
aetheria_enable_warnings(aetheria_worldgen_objects)

# aetheria_core：純 C++ 玩法核心（不得依賴 godot-cpp）。
add_library(aetheria_core STATIC
    $<TARGET_OBJECTS:aetheria_faction_ai_objects>
    $<TARGET_OBJECTS:aetheria_worldgen_objects>
)
target_include_directories(aetheria_core PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/core/runtime/include"
    "${PROJECT_SOURCE_DIR}/core/ai/include"
)
target_link_libraries(aetheria_core
    PUBLIC EnTT::EnTT
    PRIVATE cereal::cereal tomlplusplus::tomlplusplus zstd::libzstd_static
)
aetheria_enable_warnings(aetheria_core)
target_include_directories(aetheria_core PUBLIC
    "${PROJECT_SOURCE_DIR}/core/script/include"
)
target_include_directories(aetheria_core SYSTEM PRIVATE
    "${LUA_INCLUDE_DIR}"
)
target_link_libraries(aetheria_core PRIVATE
    sol2::sol2
    "${LUA_LIBRARIES}"
)
target_compile_definitions(aetheria_core PRIVATE
    SOL_ALL_SAFETIES_ON=1
)

include("${CMAKE_CURRENT_LIST_DIR}/targets_core_ai.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_worldgen.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_api.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_narrative.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_rules.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_serialize.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_spatial.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_local.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_site.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_time.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_runtime.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_zone.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_world.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_script.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/targets_core_history.cmake")
