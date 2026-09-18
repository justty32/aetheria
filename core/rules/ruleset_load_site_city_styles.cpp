// ruleset_load_site_city_styles.cpp：勢力地標風格載入。

#include "core/rules/ruleset.h"
#include "core/rules/ruleset_load_site_city_detail.h"
#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_site_city_styles(Ruleset& result,
                                             const detail::SiteCityLoadContext& context) {
    const auto* styles = context.styles;
    const auto& path = context.path;
    auto& rules = result.site_fill_rules_;
    std::vector<bool> faction_seen;
    for (const auto& node : *styles) {
        const auto& table = require_table(node, path);
        const auto faction = require_integer(table, "faction", path);
        const auto* landmarks = table["landmarks"].as_array();
        if (faction < 0 || faction > UINT16_MAX || landmarks == nullptr || landmarks->empty()) {
            throw std::runtime_error{"site_city.toml 勢力地標風格無效"};
        }
        const auto faction_index = static_cast<std::size_t>(faction);
        if (faction_seen.size() <= faction_index) {
            faction_seen.resize(faction_index + 1U);
        }
        if (faction_seen[faction_index]) {
            throw std::runtime_error{"site_city.toml 勢力地標風格重複"};
        }
        faction_seen[faction_index] = true;
        FactionLandmarkStyle style;
        style.faction = static_cast<std::uint16_t>(faction);
        for (const auto& landmark_node : *landmarks) {
            const auto id = landmark_node.value<std::string>();
            const auto def = id.has_value() ? result.find_building(*id) : std::nullopt;
            if (!def.has_value() || !result.building(*def)->landmark) {
                throw std::runtime_error{"site_city.toml 勢力風格引用不存在或非地標建築"};
            }
            style.landmarks.push_back(*def);
        }
        rules.faction_styles.push_back(std::move(style));
    }

}

}  // namespace aetheria::rules
