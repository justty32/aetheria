// core/rules/ruleset_load_factions.cpp：勢力數與影響力規則的載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>

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
}

}  // namespace aetheria::rules
