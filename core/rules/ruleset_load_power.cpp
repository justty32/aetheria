// core/rules/ruleset_load_power.cpp：power.toml 位階、quality、門檻與破階 def 載入。

#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "core/rules/toml_read.h"

namespace aetheria::rules {

void RulesetLoader::load_power_rules(Ruleset& result,
                                     const std::filesystem::path& data_directory,
                                     std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "power.toml";
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }

    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* tiers = document["tier_weights"].as_table();
    const auto* quality = document["quality"].as_table();
    const auto* gate = document["individual_gate"].as_table();
    const auto* breakthroughs = document["breakthroughs"].as_array();
    if (tiers == nullptr || quality == nullptr || gate == nullptr || breakthroughs == nullptr ||
        breakthroughs->empty()) {
        throw std::runtime_error{"power.toml 缺少權重、quality、個體門檻或破階 def"};
    }

    constexpr std::array<std::string_view, kPowerTierCount> tier_fields{
        "ambient", "local", "site", "region", "world"};
    auto& rules = result.power_rules_;
    for (std::size_t index = 0; index < tier_fields.size(); ++index) {
        const auto value = detail::require_integer(*tiers, tier_fields[index], path);
        if (value <= 0) {
            throw std::runtime_error{"power.toml 位階權重必須為正整數：" +
                                     std::string{tier_fields[index]}};
        }
        rules.tier_weights[index] = value;
    }

    const auto minimum = detail::require_integer(*quality, "minimum_percent", path);
    const auto reference = detail::require_integer(*quality, "reference_percent", path);
    const auto maximum = detail::require_integer(*quality, "maximum_percent", path);
    if (minimum <= 0 || minimum > reference || reference > maximum ||
        maximum > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error{"power.toml quality 百分比值域無效"};
    }
    rules.minimum_quality_percent = static_cast<std::int32_t>(minimum);
    rules.reference_quality_percent = static_cast<std::int32_t>(reference);
    rules.maximum_quality_percent = static_cast<std::int32_t>(maximum);

    const auto minimum_gap = detail::require_integer(*gate, "minimum_gap", path);
    const auto numerator = detail::require_integer(*gate, "damage_numerator", path);
    const auto denominator = detail::require_integer(*gate, "damage_denominator", path);
    if (minimum_gap <= 0 || minimum_gap >= static_cast<std::int64_t>(kPowerTierCount) ||
        numerator < 0 || numerator > denominator || denominator <= 0 ||
        denominator > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error{"power.toml 個體門檻參數無效"};
    }
    rules.individual_gate_minimum_gap = static_cast<std::uint8_t>(minimum_gap);
    rules.gated_damage_numerator = static_cast<std::uint16_t>(numerator);
    rules.gated_damage_denominator = static_cast<std::uint16_t>(denominator);

    for (const auto& node : *breakthroughs) {
        const auto& table = detail::require_table(node, path);
        PowerBreakthroughDef def{detail::require_string(table, "id", path),
                                 detail::require_string(table, "name_key", path)};
        detail::register_global_id(global_ids, def.id, "breakthrough.");
        const auto id = detail::append_def<PowerBreakthroughDefId>(result.breakthroughs_,
                                                                   std::move(def));
        result.breakthrough_index_.emplace(result.breakthroughs_.back().id, id);
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules
