if(NOT DEFINED PROJECT_ROOT)
    message(FATAL_ERROR "缺少 PROJECT_ROOT")
endif()

foreach(_tool_file IN ITEMS sim/world_hash.h sim/world_hash.cpp)
    if(NOT EXISTS "${PROJECT_ROOT}/${_tool_file}")
        message(FATAL_ERROR "世界雜湊工具必須留在 sim/：缺少 ${_tool_file}")
    endif()
endforeach()

file(GLOB_RECURSE _gameplay_sources
    "${PROJECT_ROOT}/core/*.h"
    "${PROJECT_ROOT}/core/*.cpp"
    "${PROJECT_ROOT}/bridge/*.h"
    "${PROJECT_ROOT}/bridge/*.cpp"
    "${PROJECT_ROOT}/godot/*.gd"
)
foreach(_source IN LISTS _gameplay_sources)
    file(READ "${_source}" _contents)
    if(_contents MATCHES "sim/world_hash\\.h|world_state_hash|run_world_hash")
        message(FATAL_ERROR "玩法路徑不得呼叫世界雜湊工具：${_source}")
    endif()
endforeach()

message(STATUS "世界雜湊只存在 sim 驗證工具層，玩法來源無引用")
