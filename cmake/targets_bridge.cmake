# aetheria_bridge：唯一可 include godot-cpp 的自家 target，輸出到 godot/bin。

add_library(aetheria_bridge SHARED
    bridge/aetheria_core.cpp
    bridge/register_types.cpp
)
target_include_directories(aetheria_bridge PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(aetheria_bridge PRIVATE aetheria_core godot-cpp)
set_target_properties(aetheria_bridge PROPERTIES
    OUTPUT_NAME aetheria_bridge
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/godot/bin"
)
aetheria_enable_warnings(aetheria_bridge)
