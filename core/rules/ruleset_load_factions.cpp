// core/rules/ruleset_load_factions.cpp：勢力數與影響力規則的載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::rules {

void RulesetLoader::load_faction_rules(Ruleset& result,
                                       const std::filesystem::path& data_directory) {
    const auto path = data_directory / "civilization.toml";
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }
    toml::table civilization;
    try {
        civilization = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* factions = civilization["factions"].as_table();
    if (factions == nullptr) {
        throw std::runtime_error{"civilization.toml 缺少 [factions]"};
    }
    const auto faction_count = detail::require_integer(*factions, "faction_count", path);
    const auto governance_max_cost =
        detail::require_integer(*factions, "governance_max_cost", path);
    const auto influence_season =
        detail::require_integer(*factions, "influence_season", path);
    if (faction_count <= 0 || faction_count > UINT16_MAX / 2 || governance_max_cost < 0 ||
        influence_season < 1 || influence_season > 4) {
        throw std::runtime_error{"civilization.toml factions 參數無效"};
    }
    auto& rules = result.civilization_rules_.factions;
    rules.faction_count = static_cast<std::uint16_t>(faction_count);
    rules.governance_max_cost = governance_max_cost;
    rules.influence_season = static_cast<std::uint8_t>(influence_season);

    const auto* ai_rules = civilization["faction_ai"].as_table();
    const auto* definitions = civilization["faction_defs"].as_array();
    if (ai_rules == nullptr || definitions == nullptr) {
        throw std::runtime_error{"civilization.toml 缺少 [faction_ai] 或 [[faction_defs]]"};
    }
    const auto goal_switch_threshold =
        detail::require_integer(*ai_rules, "goal_switch_threshold", path);
    const auto full_ai_field_threshold =
        detail::require_integer(*ai_rules, "full_ai_field_threshold", path);
    const auto marked_observer_strength =
        detail::require_integer(*ai_rules, "marked_observer_strength", path);
    const auto war_observer_strength =
        detail::require_integer(*ai_rules, "war_observer_strength", path);
    if (goal_switch_threshold < 0 || goal_switch_threshold > INT32_MAX ||
        full_ai_field_threshold < 0 || full_ai_field_threshold > INT32_MAX ||
        marked_observer_strength < full_ai_field_threshold ||
        marked_observer_strength > INT32_MAX ||
        war_observer_strength < full_ai_field_threshold || war_observer_strength > INT32_MAX) {
        throw std::runtime_error{"civilization.toml faction_ai 參數無效"};
    }
    auto& ai = result.civilization_rules_.faction_ai;
    ai.goal_switch_threshold = static_cast<std::int32_t>(goal_switch_threshold);
    ai.full_ai_field_threshold = static_cast<std::int32_t>(full_ai_field_threshold);
    ai.marked_observer_strength = static_cast<std::int32_t>(marked_observer_strength);
    ai.war_observer_strength = static_cast<std::int32_t>(war_observer_strength);

    std::vector<bool> seen(static_cast<std::size_t>(rules.faction_count) + 1U);
    for (const auto& node : *definitions) {
        const auto* table = node.as_table();
        if (table == nullptr) {
            throw std::runtime_error{"civilization.toml faction_defs 項目不是 table"};
        }
        const auto faction = detail::require_integer(*table, "faction", path);
        const auto id = detail::require_string(*table, "id", path);
        if (faction <= 0 || faction > rules.faction_count || seen[static_cast<std::size_t>(faction)] ||
            id.empty()) {
            throw std::runtime_error{"civilization.toml faction_defs id／faction 無效或重複"};
        }
        const auto weight = [&](const char* name) {
            const auto value = detail::require_integer(*table, name, path);
            if (value < 0 || value > 100) {
                throw std::runtime_error{"civilization.toml 勢力性格權重須介於 0～100"};
            }
            return static_cast<std::int32_t>(value);
        };
        seen[static_cast<std::size_t>(faction)] = true;
        ai.definitions.push_back({
            .faction = static_cast<std::uint16_t>(faction),
            .id = id,
            .expansion = weight("expansion"),
            .aggression = weight("aggression"),
            .fidelity = weight("fidelity"),
            .commerce = weight("commerce"),
            .piety = weight("piety"),
            .caution = weight("caution"),
            .resentment = weight("resentment"),
        });
    }
    if (ai.definitions.size() != rules.faction_count) {
        throw std::runtime_error{"civilization.toml faction_defs 必須逐勢力恰有一筆"};
    }
}

}  // namespace aetheria::rules
