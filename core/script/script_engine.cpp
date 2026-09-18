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

namespace detail {

[[nodiscard]] std::string lua_error(const sol::protected_function_result& result) {
    const sol::error error = result;
    return error.what();
}

} // namespace detail

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

} // namespace aetheria::script
