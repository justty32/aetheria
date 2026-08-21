// core/rules/ruleset_load_site_city.cpp：城區 F1 配額與 F2 建築 def 載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace aetheria::rules {
namespace {

[[nodiscard]] SiteFillZone read_zone(std::string_view value,
                                     const std::filesystem::path& path) {
    if (value == "residential") {
        return SiteFillZone::Residential;
    }
    if (value == "commercial") {
        return SiteFillZone::Commercial;
    }
    throw std::runtime_error{"site_city.toml 含未支援分區：" + std::string{value} +
                             "（" + path.string() + "）"};
}

[[nodiscard]] SiteQuotaDriver read_driver(std::string_view value,
                                          const std::filesystem::path& path) {
    if (value == "population") {
        return SiteQuotaDriver::Population;
    }
    if (value == "development_level") {
        return SiteQuotaDriver::DevelopmentLevel;
    }
    throw std::runtime_error{"site_city.toml 含未支援配額快變數：" + std::string{value} +
                             "（" + path.string() + "）"};
}

[[nodiscard]] constexpr std::size_t zone_index(SiteFillZone zone) noexcept {
    return static_cast<std::size_t>(zone);
}

}  // namespace

using namespace detail;

void RulesetLoader::load_site_city(Ruleset& result,
                                   const std::filesystem::path& data_directory,
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
    const auto* quotas = document["quotas"].as_array();
    const auto* buildings = document["building_defs"].as_array();
    if (fill == nullptr || quotas == nullptr || buildings == nullptr) {
        throw std::runtime_error{"site_city.toml 缺少 fill、quotas 或 building_defs"};
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
        rules.quotas.push_back({zone, driver, static_cast<std::uint32_t>(units),
                                static_cast<std::uint8_t>(maximum)});
    }

    for (const auto& node : *buildings) {
        const auto& table = require_table(node, path);
        BuildingDef def;
        def.id = require_string(table, "id", path);
        register_global_id(global_ids, def.id, "building.");
        def.zone = read_zone(require_string(table, "zone", path), path);
        const auto frontage = require_integer(table, "frontage", path);
        const auto depth = require_integer(table, "depth", path);
        if (frontage <= 0 || frontage > 8 || depth <= 0 || depth > 8) {
            throw std::runtime_error{"site_city.toml 建築尺寸無效：" + def.id};
        }
        def.frontage = static_cast<std::uint8_t>(frontage);
        def.depth = static_cast<std::uint8_t>(depth);
        const auto id = append_def<BuildingDefId>(result.buildings_, std::move(def));
        result.building_index_.emplace(result.buildings_.back().id, id);
        building_seen.at(zone_index(result.buildings_.back().zone)) = true;
    }

    for (std::size_t index = 0; index < kZoneCount; ++index) {
        if (!quota_seen[index]) {
            throw std::runtime_error{"site_city.toml 缺少已啟用分區的配額 def"};
        }
        if (!building_seen[index]) {
            throw std::runtime_error{"site_city.toml 缺少已啟用分區的建築 def"};
        }
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules
