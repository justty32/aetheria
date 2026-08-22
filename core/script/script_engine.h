#pragma once

// script_engine.h：Lua 5.4 沙箱、三種低頻掛勾、原子熱重載與呼叫量測入口。

#include "core/script/script_world.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::script {

enum class HookKind : std::uint8_t {
    EventCondition,
    EventEffect,
    NarrativeTrigger,
    VictoryCheck,
};

enum class Outcome : std::uint8_t {
    Ongoing,
    Victory,
    Defeat,
};

struct ScriptSource {
    std::string origin;
    std::string text;
};

struct ReloadResult {
    bool accepted{};
    std::string error;
};

struct HookCallCounts {
    std::uint64_t event_condition{};
    std::uint64_t event_effect{};
    std::uint64_t narrative_trigger{};
    std::uint64_t victory_check{};

    [[nodiscard]] std::uint64_t total() const noexcept;
    bool operator==(const HookCallCounts&) const = default;
};

class ScriptError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] std::uint64_t derive_script_seed(std::uint64_t world_seed, std::string_view script_id,
                                               HookKind hook) noexcept;

// ScriptEngine 擁有 Lua state 與 C++ 原始碼 catalog，但不擁有任何玩法狀態。
// 每次呼叫都建立新 _ENV；熱重載先完整建候選 state，驗證後才一次交換。
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();
    ScriptEngine(ScriptEngine&&) noexcept;
    ScriptEngine& operator=(ScriptEngine&&) noexcept;
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    [[nodiscard]] ReloadResult replace_sources(std::vector<ScriptSource> sources,
                                               const ScriptWorldState& world);
    [[nodiscard]] ReloadResult reload_directory(const std::filesystem::path& directory,
                                                const ScriptWorldState& world);

    [[nodiscard]] bool event_condition(std::string_view id, ScriptWorldState& world,
                                       std::uint64_t world_seed);
    void event_effect(std::string_view id, ScriptWorldState& world, std::uint64_t world_seed);
    void narrative_trigger(std::string_view id, ScriptWorldState& world, std::uint64_t world_seed);
    [[nodiscard]] Outcome victory_check(std::string_view id, ScriptWorldState& world,
                                        std::uint64_t world_seed);

    void reset_call_counts() noexcept { call_counts_ = {}; }
    [[nodiscard]] HookCallCounts call_counts() const noexcept { return call_counts_; }
    [[nodiscard]] std::vector<std::string> definition_ids() const;
    [[nodiscard]] std::vector<std::string> sandbox_keys() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    HookCallCounts call_counts_;
};

struct TurnScriptPlan {
    std::vector<std::string> event_ids;
    std::vector<std::string> narrative_ids;
    std::vector<std::string> victory_ids;
};

// 固定回合執行器是同步值型呼叫；沒有 queue、future 或跨回合 coroutine。
void run_turn_scripts(ScriptEngine& engine, ScriptWorldState& world, const TurnScriptPlan& plan,
                      std::uint64_t world_seed);

}  // namespace aetheria::script
