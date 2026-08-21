// core/rules/ruleset_load_site_city.cpp：城區 F1 配額與 F2 建築 def 載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::rules {
namespace {

[[nodiscard]] SiteFillZone read_zone(std::string_view value, const std::filesystem::path& path) {
    if (value == "residential") {
        return SiteFillZone::Residential;
    }
    if (value == "commercial") {
        return SiteFillZone::Commercial;
    }
    throw std::runtime_error{"site_city.toml 含未支援分區：" + std::string{value} + "（" +
                             path.string() + "）"};
}

[[nodiscard]] SiteQuotaDriver read_driver(std::string_view value,
                                          const std::filesystem::path& path) {
    if (value == "population") {
        return SiteQuotaDriver::Population;
    }
    if (value == "development_level") {
        return SiteQuotaDriver::DevelopmentLevel;
    }
    throw std::runtime_error{"site_city.toml 含未支援配額快變數：" + std::string{value} + "（" +
                             path.string() + "）"};
}

[[nodiscard]] constexpr std::size_t zone_index(SiteFillZone zone) noexcept {
    return static_cast<std::size_t>(zone);
}

}  // namespace

using namespace detail;

void RulesetLoader::load_site_city(Ruleset& result, const std::filesystem::path& data_directory,
                                   std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "site_city.toml";
    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* fill = document["fill"].as_table();
    const auto* fortification = document["fortification"].as_table();
    const auto* quotas = document["quotas"].as_array();
    const auto* buildings = document["building_defs"].as_array();
    const auto* styles = document["faction_styles"].as_array();
    if (fill == nullptr || fortification == nullptr || quotas == nullptr || buildings == nullptr ||
        styles == nullptr) {
        throw std::runtime_error{
            "site_city.toml 缺少 fill、fortification、quotas、building_defs 或 "
            "faction_styles"};
    }

    auto read_percent = [&](std::string_view field) {
        const auto value = require_integer(*fill, field, path);
        if (value < 0 || value > 100) {
            throw std::runtime_error{"site_city.toml 百分比無效：" + std::string{field}};
        }
        return static_cast<std::uint8_t>(value);
    };
    auto& rules = result.site_fill_rules_;
    rules.base_density_percent = read_percent("base_density_percent");
    rules.development_density_per_level = read_percent("development_density_per_level");
    rules.max_density_percent = read_percent("max_density_percent");
    if (rules.base_density_percent > rules.max_density_percent) {
        throw std::runtime_error{"site_city.toml 基礎建築密度高於上限"};
    }

    constexpr std::size_t kZoneCount = 2;
    std::array<bool, kZoneCount> quota_seen{};
    std::array<bool, kZoneCount> building_seen{};
    for (const auto& node : *quotas) {
        const auto& table = require_table(node, path);
        const auto zone = read_zone(require_string(table, "zone", path), path);
        const auto driver = read_driver(require_string(table, "driver", path), path);
        const auto units = require_integer(table, "units_per_block", path);
        const auto maximum = require_integer(table, "max_percent", path);
        if (units <= 0 || units > UINT32_MAX || maximum <= 0 || maximum > 100) {
            throw std::runtime_error{"site_city.toml 配額數值無效"};
        }
        auto& seen = quota_seen.at(zone_index(zone));
        if (seen) {
            throw std::runtime_error{"site_city.toml 分區配額重複"};
        }
        seen = true;
        rules.quotas.push_back(
            {zone, driver, static_cast<std::uint32_t>(units), static_cast<std::uint8_t>(maximum)});
    }

    for (const auto& node : *buildings) {
        const auto& table = require_table(node, path);
        BuildingDef def;
        def.id = require_string(table, "id", path);
        register_global_id(global_ids, def.id, "building.");
        def.zone = read_zone(require_string(table, "zone", path), path);
        def.landmark = table["landmark"].value_or(false);
        const auto frontage = require_integer(table, "frontage", path);
        const auto depth = require_integer(table, "depth", path);
        if (frontage <= 0 || frontage > 8 || depth <= 0 || depth > 8) {
            throw std::runtime_error{"site_city.toml 建築尺寸無效：" + def.id};
        }
        def.frontage = static_cast<std::uint8_t>(frontage);
        def.depth = static_cast<std::uint8_t>(depth);
        const auto id = append_def<BuildingDefId>(result.buildings_, std::move(def));
        result.building_index_.emplace(result.buildings_.back().id, id);
        if (!result.buildings_.back().landmark) {
            building_seen.at(zone_index(result.buildings_.back().zone)) = true;
        }
    }

    for (std::size_t index = 0; index < kZoneCount; ++index) {
        if (!quota_seen[index]) {
            throw std::runtime_error{"site_city.toml 缺少已啟用分區的配額 def"};
        }
        if (!building_seen[index]) {
            throw std::runtime_error{"site_city.toml 缺少已啟用分區的建築 def"};
        }
    }

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
    rules.loaded = true;
}

}  // namespace aetheria::rules
