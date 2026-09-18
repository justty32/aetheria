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

} // namespace

ScriptEngine::Impl::Impl() { initialize(); }

ScriptEngine::Impl::Impl(std::vector<ScriptSource> sources) {
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

[[nodiscard]] const Definition& ScriptEngine::Impl::require(std::string_view id, HookKind hook) const {
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

[[nodiscard]] sol::protected_function ScriptEngine::Impl::function(const Definition& definition, HookKind hook,
                                                   sol::environment& environment) {
        const auto table = evaluate_definition(lua_, environment, definition.source);
        return table[hook_name(hook)];
    }

[[nodiscard]] std::vector<std::string> ScriptEngine::Impl::definition_ids() const {
        std::vector<std::string> result;
        result.reserve(definitions_.size());
        for (const auto& [id, definition] : definitions_) {
            static_cast<void>(definition);
            result.push_back(id);
        }
        return result;
    }

} // namespace aetheria::script
