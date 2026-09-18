#pragma once

#include "core/script/script_engine.h"
#include <sol/sol.hpp>

namespace aetheria::script::detail {

struct Definition {
    ScriptSource source;
    std::string id;
    bool has_condition{};
    bool has_effect{};
    bool has_narrative{};
    bool has_victory{};
};

[[nodiscard]] std::string lua_error(const sol::protected_function_result& result);

} // namespace aetheria::script::detail
