#include "core/script/script_engine.h"

#include "core/script/context_internal.h"
#include "core/worldgen/region_seed.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <utility>

#include "core/script/script_engine_impl.h"

namespace aetheria::script {
using namespace detail;

namespace {

void copy_named_values(sol::table destination, const sol::table& source,
                       std::initializer_list<const char*> names) {
    for (const auto* name : names) {
        const sol::object value = source[name];
        if (value.valid() && value.get_type() != sol::type::nil) {
            destination[name] = value;
        }
    }
}

[[nodiscard]] sol::table clone_table(sol::state_view lua, const sol::table& source) {
    auto result = lua.create_table();
    for (const auto& [key, value] : source) {
        result[key] = value;
    }
    return result;
}

} // namespace

[[nodiscard]] sol::environment ScriptEngine::Impl::make_environment() {
        sol::environment environment{lua_, sol::create};
        for (const auto& [key, value] : base_) {
            environment[key] = value;
        }
        for (const auto* table_name : {"math", "string", "table", "utf8", "outcome"}) {
            environment[table_name] = clone_table(lua_, base_[table_name]);
        }
        environment["_ENV"] = environment;
        return environment;
    }

[[nodiscard]] std::vector<std::string> ScriptEngine::Impl::sandbox_keys() const {
        std::vector<std::string> result;
        for (const auto& [key, value] : base_) {
            static_cast<void>(value);
            if (key.is<std::string>()) {
                result.push_back(key.as<std::string>());
            }
        }
        std::ranges::sort(result);
        return result;
    }

void ScriptEngine::Impl::initialize() {
        lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                            sol::lib::utf8);
        lua_.new_usertype<ScriptRng>("AetheriaScriptRng", sol::no_constructor, "int",
                                     &ScriptRng::integer);
        lua_.new_usertype<Context>(
            "AetheriaScriptContext", sol::no_constructor, "owner",
            [](const Context& context, const std::string& id) { return context.owner(id); },
            "set_owner",
            [](Context& context, const std::string& id, std::int64_t owner) {
                context.set_owner(id, owner);
            },
            "rng", sol::property(&Context::rng));

        base_ = sol::environment{lua_, sol::create};
        const auto globals = lua_.globals();
        copy_named_values(base_, globals,
                          {"_VERSION", "assert", "error", "ipairs", "next", "select", "tonumber",
                           "tostring", "type"});

        auto math = lua_.create_table();
        copy_named_values(math, globals["math"],
                          {"abs", "acos",       "asin", "atan", "ceil", "cos", "deg",
                           "exp", "floor",      "fmod", "huge", "log",  "max", "maxinteger",
                           "min", "mininteger", "modf", "pi",   "rad",  "sin", "sqrt",
                           "tan", "tointeger",  "type", "ult"});
        base_["math"] = math;

        auto string = lua_.create_table();
        copy_named_values(string, globals["string"],
                          {"byte", "char", "find", "format", "gmatch", "gsub", "len", "lower",
                           "match", "pack", "packsize", "rep", "reverse", "sub", "unpack",
                           "upper"});
        base_["string"] = string;

        auto table = lua_.create_table();
        copy_named_values(table, globals["table"],
                          {"concat", "insert", "move", "pack", "remove", "sort", "unpack"});
        base_["table"] = table;

        auto utf8 = lua_.create_table();
        copy_named_values(utf8, globals["utf8"],
                          {"char", "charpattern", "codes", "codepoint", "len", "offset"});
        base_["utf8"] = utf8;

        auto outcome = lua_.create_table();
        outcome["ongoing"] = "ongoing";
        outcome["victory"] = "victory";
        outcome["defeat"] = "defeat";
        base_["outcome"] = outcome;

        constexpr std::string_view ordered_pairs_source = R"lua(
return function(value)
    local keys = {}
    for key in next, value do
        local kind = type(key)
        if kind ~= "number" and kind ~= "string" and kind ~= "boolean" then
            error("ordered_pairs 只接受 number/string/boolean key")
        end
        table.insert(keys, key)
    end
    table.sort(keys, function(left, right)
        local left_type, right_type = type(left), type(right)
        if left_type ~= right_type then
            local rank = { number = 1, string = 2, boolean = 3 }
            return rank[left_type] < rank[right_type]
        end
        if left_type == "boolean" then
            return left == false and right == true
        end
        return left < right
    end)
    local index = 0
    return function()
        index = index + 1
        local key = keys[index]
        if key ~= nil then return key, value[key] end
    end
end
)lua";
        auto result = lua_.safe_script(ordered_pairs_source, base_, sol::script_pass_on_error);
        if (!result.valid()) {
            throw ScriptError{"建立 ordered_pairs 失敗：" + lua_error(result)};
        }
        base_["ordered_pairs"] = sol::object{result};
        base_["_ENV"] = base_;
    }

} // namespace aetheria::script
