# aetheria_bridge：唯一可 include godot-cpp 的自家 target，輸出到 godot/bin。

add_library(aetheria_bridge SHARED
    bridge/aetheria_core.cpp
    bridge/register_types.cpp
)
target_include_directories(aetheria_bridge PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(aetheria_bridge PRIVATE aetheria_core godot-cpp)
target_compile_definitions(aetheria_bridge PRIVATE
    AETHERIA_DEFAULT_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
)
set_target_properties(aetheria_bridge PROPERTIES
    OUTPUT_NAME aetheria_bridge
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
)
add_custom_command(TARGET aetheria_bridge POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/data" "${PROJECT_SOURCE_DIR}/godot/data"
    COMMENT "Stage baseline TOML raws for Godot editor/export"
)
aetheria_enable_warnings(aetheria_bridge)
