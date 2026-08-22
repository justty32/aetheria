// core/rules/ruleset_load_combat.cpp：combat.toml 的公式、修正值域、潰散與旬損耗載入。

#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/rules/toml_read.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] const toml::table& require_section(const toml::table& document,
                                                  std::string_view name,
                                                  const std::filesystem::path& path) {
    const auto* section = document[name].as_table();
    if (section == nullptr) {
        throw std::runtime_error{"combat.toml 缺少區段 " + std::string{name} + "：" +
                                 path.string()};
    }
    return *section;
}

[[nodiscard]] std::int32_t positive_int32(const toml::table& table, std::string_view field,
                                          const std::filesystem::path& path) {
    const auto value = detail::require_integer(table, field, path);
    if (value <= 0 || value > std::numeric_limits<std::int32_t>::max()) {
        throw std::runtime_error{"combat.toml 正整數欄位無效：" + std::string{field}};
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t permyriad(const toml::table& table, std::string_view field,
                                     const std::filesystem::path& path) {
    const auto value = detail::require_integer(table, field, path);
    if (value < 0 || value > 10'000) {
        throw std::runtime_error{"combat.toml 萬分比欄位超界：" + std::string{field}};
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] CombatModifierBounds modifier_bounds(const toml::table& modifiers,
                                                    std::string_view name,
                                                    const std::filesystem::path& path) {
    const auto& table = require_section(modifiers, name, path);
    CombatModifierBounds result{positive_int32(table, "minimum", path),
                                positive_int32(table, "maximum", path)};
    if (result.minimum >= result.maximum) {
        throw std::runtime_error{"combat.toml 修正值域反轉：" + std::string{name}};
    }
    return result;
}

}  // namespace

void RulesetLoader::load_combat_rules(Ruleset& result,
                                      const std::filesystem::path& data_directory) {
    const auto path = data_directory / "combat.toml";
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

    const auto& fixed = require_section(document, "fixed_point", path);
    const auto& formula = require_section(document, "formula", path);
    const auto& modifiers = require_section(document, "modifiers", path);
    const auto& collapse = require_section(document, "collapse", path);
    const auto& attrition = require_section(document, "attrition", path);
    const auto& morale = require_section(document, "morale", path);

    auto& rules = result.combat_rules_;
    rules.modifier_scale = positive_int32(fixed, "modifier_scale", path);
    rules.ratio_binary_limit = positive_int32(fixed, "ratio_binary_limit", path);
    if (rules.ratio_binary_limit > 30) {
        throw std::runtime_error{"combat.toml ratio_binary_limit 超過 Q28.36 安全範圍"};
    }
    rules.default_exponent.numerator = positive_int32(formula, "exponent_numerator", path);
    rules.default_exponent.denominator = positive_int32(formula, "exponent_denominator", path);
    if (static_cast<std::int64_t>(rules.default_exponent.numerator) >
        static_cast<std::int64_t>(rules.default_exponent.denominator) * 4) {
        throw std::runtime_error{"combat.toml 優勢指數超過實作上限 4"};
    }
    rules.base_loss_permyriad_per_xun =
        permyriad(formula, "base_loss_permyriad_per_xun", path);
    rules.maximum_duration_xun = positive_int32(formula, "maximum_duration_xun", path);
    rules.terrain = modifier_bounds(modifiers, "terrain", path);
    rules.supply = modifier_bounds(modifiers, "supply", path);
    rules.morale = modifier_bounds(modifiers, "morale", path);
    rules.command = modifier_bounds(modifiers, "command", path);
    rules.posture = modifier_bounds(modifiers, "posture", path);

    rules.collapse_at_min_morale_permyriad =
        permyriad(collapse, "at_min_morale_permyriad", path);
    rules.collapse_at_max_morale_permyriad =
        permyriad(collapse, "at_max_morale_permyriad", path);
    rules.pursuit_loss_permyriad = permyriad(collapse, "pursuit_loss_permyriad", path);
    if (rules.collapse_at_min_morale_permyriad >
        rules.collapse_at_max_morale_permyriad) {
        throw std::runtime_error{"combat.toml 士氣潰散閾值反轉"};
    }

    rules.supply_attrition_at_min_permyriad =
        permyriad(attrition, "supply_at_min_permyriad", path);
    rules.besieging_supply_extra_permyriad =
        permyriad(attrition, "besieging_supply_extra_permyriad", path);
    rules.maximum_disease_permyriad =
        permyriad(attrition, "maximum_disease_permyriad", path);
    rules.maximum_season_permyriad =
        permyriad(attrition, "maximum_season_permyriad", path);
    rules.desertion_from_supply_permyriad =
        permyriad(attrition, "desertion_from_supply_permyriad", path);
    rules.desertion_from_morale_permyriad =
        permyriad(attrition, "desertion_from_morale_permyriad", path);
    rules.desertion_from_distance_permyriad =
        permyriad(attrition, "desertion_from_distance_permyriad", path);

    rules.morale_loss_rate_divisor = positive_int32(morale, "loss_rate_divisor", path);
    rules.routed_morale_penalty = permyriad(morale, "routed_penalty", path);
    rules.loaded = true;
}

}  // namespace aetheria::rules
