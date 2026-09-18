// ruleset_load_site_city_fortification.cpp：城防參數與邊定義載入。

#include "core/rules/ruleset.h"
#include "core/rules/ruleset_load_site_city_detail.h"
#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_site_city_fortification(Ruleset& result,
                                             const detail::SiteCityLoadContext& context) {
    const auto* fortification = context.fortification;
    const auto& path = context.path;
    auto& rules = result.site_fill_rules_;
    auto read_nonnegative = [&](std::string_view field, std::int64_t maximum) {
        const auto value = require_integer(*fortification, field, path);
        if (value < 0 || value > maximum) {
            throw std::runtime_error{"site_city.toml 城防參數無效：" + std::string{field}};
        }
        return value;
    };
    auto read_edge = [&](std::string_view field, std::uint32_t required_flags) {
        const auto string_id = require_string(*fortification, field, path);
        const auto edge = result.find_edge(string_id);
        if (!edge.has_value() || (result.edge(*edge)->flags & required_flags) != required_flags) {
            throw std::runtime_error{"site_city.toml 城防引用不存在或旗標不符：" + string_id};
        }
        return *edge;
    };
    auto& walls = rules.fortification;
    walls.double_wall_defense =
        static_cast<std::uint16_t>(read_nonnegative("double_wall_defense", UINT16_MAX));
    walls.tower_defense = static_cast<std::uint16_t>(read_nonnegative("tower_defense", UINT16_MAX));
    walls.tower_spacing = static_cast<std::uint8_t>(read_nonnegative("tower_spacing", UINT8_MAX));
    walls.moat_defense = static_cast<std::uint16_t>(read_nonnegative("moat_defense", UINT16_MAX));
    walls.breach_percent_at_full_damage =
        static_cast<std::uint8_t>(read_nonnegative("breach_percent_at_full_damage", 100));
    if (walls.double_wall_defense == 0 || walls.tower_spacing == 0) {
        throw std::runtime_error{"site_city.toml 城防門檻或塔樓間距不得為 0"};
    }
    walls.wall_edge = read_edge("wall_edge", kEdgeWallFlag);
    walls.gate_edge = read_edge("gate_edge", kEdgeWallFlag | kEdgeGateFlag | kEdgeOpenableFlag);
    walls.tower_edge = read_edge("tower_edge", kEdgeWallFlag | kEdgeTowerFlag);
    walls.moat_edge = read_edge("moat_edge", kEdgeMoatFlag);
}

}  // namespace aetheria::rules
