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

} // namespace aetheria::script
