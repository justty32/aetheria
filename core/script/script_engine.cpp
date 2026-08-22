// script_engine.cpp：每次掛勾重建 _ENV 的 Lua 5.4/sol2 沙箱與原子 catalog 重載。

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

namespace aetheria::script {
namespace {

struct Definition {
    ScriptSource source;
    std::string id;
    bool has_condition{};
    bool has_effect{};
    bool has_narrative{};
    bool has_victory{};
};

[[nodiscard]] std::string lua_error(const sol::protected_function_result& result) {
    const sol::error error = result;
    return error.what();
}

[[nodiscard]] bool is_identifier_start(char value) noexcept {
    return std::isalpha(static_cast<unsigned char>(value)) != 0 || value == '_';
}

[[nodiscard]] bool is_identifier_continue(char value) noexcept {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
}

void lint_source(const ScriptSource& source) {
    std::size_t offset{};
    while (offset < source.text.size()) {
        const auto value = source.text[offset];
        if (value == '-' && offset + 1U < source.text.size() && source.text[offset + 1U] == '-') {
            offset += 2U;
            if (offset + 1U < source.text.size() && source.text[offset] == '[' &&
                source.text[offset + 1U] == '[') {
                const auto end = source.text.find("]]", offset + 2U);
                offset = end == std::string::npos ? source.text.size() : end + 2U;
            } else {
                const auto end = source.text.find('\n', offset);
                offset = end == std::string::npos ? source.text.size() : end + 1U;
            }
            continue;
        }
        if (value == '\'' || value == '"') {
            const auto quote = value;
            ++offset;
            while (offset < source.text.size()) {
                if (source.text[offset] == '\\') {
                    offset += std::min<std::size_t>(2U, source.text.size() - offset);
                } else {
                    if (source.text[offset] == quote) {
                        ++offset;
                        break;
                    }
                    ++offset;
                }
            }
            continue;
        }
        if (!is_identifier_start(value)) {
            ++offset;
            continue;
        }
        const auto begin = offset++;
        while (offset < source.text.size() && is_identifier_continue(source.text[offset])) {
            ++offset;
        }
        if (source.text.substr(begin, offset - begin) != "pairs") {
            continue;
        }
        auto after = offset;
        while (after < source.text.size() &&
               std::isspace(static_cast<unsigned char>(source.text[after])) != 0) {
            ++after;
        }
        if (after < source.text.size() && source.text[after] == '(') {
            throw ScriptError{source.origin + ": lint 禁止使用 pairs；請改用 ordered_pairs"};
        }
    }
}

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

[[nodiscard]] bool has_function(const sol::table& table, const char* name) {
    const sol::object value = table[name];
    return value.valid() && value.get_type() == sol::type::function;
}

[[nodiscard]] sol::table evaluate_definition(sol::state& lua, sol::environment& environment,
                                             const ScriptSource& source) {
    auto result = lua.safe_script(source.text, environment, sol::script_pass_on_error);
    if (!result.valid()) {
        throw ScriptError{source.origin + ": " + lua_error(result)};
    }
    const sol::object returned = result;
    if (!returned.is<sol::table>()) {
        throw ScriptError{source.origin + ": 腳本必須 return definition table"};
    }
    return returned.as<sol::table>();
}

[[nodiscard]] std::string hook_name(HookKind hook) {
    switch (hook) {
    case HookKind::EventCondition:
        return "condition";
    case HookKind::EventEffect:
        return "effect";
    case HookKind::NarrativeTrigger:
        return "on_trigger";
    case HookKind::VictoryCheck:
        return "check";
    }
    return "unknown";
}

}  // namespace

class ScriptEngine::Impl {
public:
    Impl() { initialize(); }

    explicit Impl(std::vector<ScriptSource> sources) {
        initialize();
        for (auto& source : sources) {
            lint_source(source);
            auto environment = make_environment();
            const auto table = evaluate_definition(lua_, environment, source);
            const sol::object id_value = table["id"];
            if (!id_value.is<std::string>()) {
                throw ScriptError{source.origin + ": definition.id 必須是非空字串"};
            }
            auto id = id_value.as<std::string>();
            if (id.empty()) {
                throw ScriptError{source.origin + ": definition.id 必須是非空字串"};
            }
            Definition definition{
                .source = std::move(source),
                .id = id,
                .has_condition = has_function(table, "condition"),
                .has_effect = has_function(table, "effect"),
                .has_narrative = has_function(table, "on_trigger"),
                .has_victory = has_function(table, "check"),
            };
            if (!definition.has_condition && !definition.has_effect && !definition.has_narrative &&
                !definition.has_victory) {
                throw ScriptError{definition.source.origin + ": definition 沒有支援的掛勾"};
            }
            const auto [position, inserted] = definitions_.try_emplace(id, std::move(definition));
            if (!inserted) {
                throw ScriptError{"Lua definition id 重複：" + position->first};
            }
        }
    }

    [[nodiscard]] sol::environment make_environment() {
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

    [[nodiscard]] const Definition& require(std::string_view id, HookKind hook) const {
        const auto found = definitions_.find(id);
        if (found == definitions_.end()) {
            throw ScriptError{"Lua definition id 不存在：" + std::string{id}};
        }
        const auto& definition = found->second;
        const bool present = hook == HookKind::EventCondition     ? definition.has_condition
                             : hook == HookKind::EventEffect      ? definition.has_effect
                             : hook == HookKind::NarrativeTrigger ? definition.has_narrative
                                                                  : definition.has_victory;
        if (!present) {
            throw ScriptError{definition.source.origin + ": 缺少掛勾 " + hook_name(hook)};
        }
        return definition;
    }

    [[nodiscard]] sol::protected_function function(const Definition& definition, HookKind hook,
                                                   sol::environment& environment) {
        const auto table = evaluate_definition(lua_, environment, definition.source);
        return table[hook_name(hook)];
    }

    [[nodiscard]] std::vector<std::string> definition_ids() const {
        std::vector<std::string> result;
        result.reserve(definitions_.size());
        for (const auto& [id, definition] : definitions_) {
            static_cast<void>(definition);
            result.push_back(id);
        }
        return result;
    }

    [[nodiscard]] std::vector<std::string> sandbox_keys() const {
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

private:
    void initialize() {
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

    sol::state lua_;
    sol::environment base_;
    std::map<std::string, Definition, std::less<>> definitions_;
};

std::uint64_t HookCallCounts::total() const noexcept {
    return event_condition + event_effect + narrative_trigger + victory_check;
}

std::uint64_t derive_script_seed(std::uint64_t world_seed, std::string_view script_id,
                                 HookKind hook) noexcept {
    auto id_hash = UINT64_C(14695981039346656037);
    for (const auto value : script_id) {
        id_hash ^= static_cast<std::uint8_t>(value);
        id_hash *= UINT64_C(1099511628211);
    }
    return worldgen::splitmix64(world_seed ^ id_hash ^ static_cast<std::uint64_t>(hook));
}

ScriptEngine::ScriptEngine() : impl_{std::make_unique<Impl>()} {}
ScriptEngine::~ScriptEngine() = default;
ScriptEngine::ScriptEngine(ScriptEngine&&) noexcept = default;
ScriptEngine& ScriptEngine::operator=(ScriptEngine&&) noexcept = default;

ReloadResult ScriptEngine::replace_sources(std::vector<ScriptSource> sources,
                                           const ScriptWorldState& world) {
    try {
        auto candidate = std::make_unique<Impl>(std::move(sources));
        const auto ids = candidate->definition_ids();
        for (const auto& required : world.required_definition_ids()) {
            if (!std::ranges::binary_search(ids, required)) {
                return {false, "拒絕熱重載：現存物件引用缺少的 Lua id：" + required};
            }
        }
        impl_ = std::move(candidate);
        return {true, {}};
    } catch (const std::exception& error) {
        return {false, error.what()};
    }
}

ReloadResult ScriptEngine::reload_directory(const std::filesystem::path& directory,
                                            const ScriptWorldState& world) {
    try {
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                paths.push_back(entry.path());
            }
        }
        std::ranges::sort(paths);
        std::vector<ScriptSource> sources;
        sources.reserve(paths.size());
        for (const auto& path : paths) {
            std::ifstream stream{path};
            if (!stream) {
                return {false, "無法讀取 Lua 腳本：" + path.string()};
            }
            std::ostringstream text;
            text << stream.rdbuf();
            sources.push_back({path.string(), std::move(text).str()});
        }
        return replace_sources(std::move(sources), world);
    } catch (const std::exception& error) {
        return {false, "拒絕熱重載：" + std::string{error.what()}};
    }
}

bool ScriptEngine::event_condition(std::string_view id, ScriptWorldState& world,
                                   std::uint64_t world_seed) {
    const auto& definition = impl_->require(id, HookKind::EventCondition);
    auto environment = impl_->make_environment();
    auto function = impl_->function(definition, HookKind::EventCondition, environment);
    std::mt19937_64 rng{derive_script_seed(world_seed, id, HookKind::EventCondition)};
    detail::ContextAccess access{world, rng};
    auto context = access.make_context();
    ++call_counts_.event_condition;
    auto result = function(std::ref(context));
    if (!result.valid()) {
        throw ScriptError{definition.source.origin + ": condition: " + lua_error(result)};
    }
    try {
        return result.get<bool>();
    } catch (const std::exception& error) {
        throw ScriptError{definition.source.origin + ": condition 必須回傳 bool：" + error.what()};
    }
}

void ScriptEngine::event_effect(std::string_view id, ScriptWorldState& world,
                                std::uint64_t world_seed) {
    const auto& definition = impl_->require(id, HookKind::EventEffect);
    auto environment = impl_->make_environment();
    auto function = impl_->function(definition, HookKind::EventEffect, environment);
    std::mt19937_64 rng{derive_script_seed(world_seed, id, HookKind::EventEffect)};
    detail::ContextAccess access{world, rng};
    auto context = access.make_context();
    ++call_counts_.event_effect;
    auto result = function(std::ref(context));
    if (!result.valid()) {
        throw ScriptError{definition.source.origin + ": effect: " + lua_error(result)};
    }
    access.commit();
}

void ScriptEngine::narrative_trigger(std::string_view id, ScriptWorldState& world,
                                     std::uint64_t world_seed) {
    const auto& definition = impl_->require(id, HookKind::NarrativeTrigger);
    auto environment = impl_->make_environment();
    auto function = impl_->function(definition, HookKind::NarrativeTrigger, environment);
    std::mt19937_64 rng{derive_script_seed(world_seed, id, HookKind::NarrativeTrigger)};
    detail::ContextAccess access{world, rng};
    auto context = access.make_context();
    ++call_counts_.narrative_trigger;
    auto result = function(std::ref(context));
    if (!result.valid()) {
        throw ScriptError{definition.source.origin + ": on_trigger: " + lua_error(result)};
    }
    access.commit();
}

Outcome ScriptEngine::victory_check(std::string_view id, ScriptWorldState& world,
                                    std::uint64_t world_seed) {
    const auto& definition = impl_->require(id, HookKind::VictoryCheck);
    auto environment = impl_->make_environment();
    auto function = impl_->function(definition, HookKind::VictoryCheck, environment);
    std::mt19937_64 rng{derive_script_seed(world_seed, id, HookKind::VictoryCheck)};
    detail::ContextAccess access{world, rng};
    auto context = access.make_context();
    ++call_counts_.victory_check;
    auto result = function(std::ref(context));
    if (!result.valid()) {
        throw ScriptError{definition.source.origin + ": check: " + lua_error(result)};
    }
    try {
        const auto outcome = result.get<std::string>();
        if (outcome == "ongoing") {
            return Outcome::Ongoing;
        }
        if (outcome == "victory") {
            return Outcome::Victory;
        }
        if (outcome == "defeat") {
            return Outcome::Defeat;
        }
        throw ScriptError{definition.source.origin + ": check 回傳未知 Outcome：" + outcome};
    } catch (const ScriptError&) {
        throw;
    } catch (const std::exception& error) {
        throw ScriptError{definition.source.origin + ": check 必須回傳 Outcome：" + error.what()};
    }
}

std::vector<std::string> ScriptEngine::definition_ids() const { return impl_->definition_ids(); }

std::vector<std::string> ScriptEngine::sandbox_keys() const { return impl_->sandbox_keys(); }

void run_turn_scripts(ScriptEngine& engine, ScriptWorldState& world, const TurnScriptPlan& plan,
                      std::uint64_t world_seed) {
    engine.reset_call_counts();
    for (const auto& id : plan.event_ids) {
        if (engine.event_condition(id, world, world_seed)) {
            engine.event_effect(id, world, world_seed);
        }
    }
    for (const auto& id : plan.narrative_ids) {
        engine.narrative_trigger(id, world, world_seed);
    }
    for (const auto& id : plan.victory_ids) {
        static_cast<void>(engine.victory_check(id, world, world_seed));
    }
}

}  // namespace aetheria::script
