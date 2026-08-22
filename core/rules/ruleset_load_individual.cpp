// core/rules/ruleset_load_individual.cpp：載入四屬性公式、d100 餘量分段與
// 傷害型別／抗性上限，並在載入期 fail-fast 驗證。

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

#include "core/rules/ruleset.h"
#include "core/rules/toml_read.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] toml::table parse_document(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error{"Ruleset 檔案不存在：" + path.string()};
    }
    try {
        return toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
}

[[nodiscard]] const toml::table& require_named_table(const toml::table& document,
                                                     std::string_view name,
                                                     const std::filesystem::path& path) {
    const auto* table = document[name].as_table();
    if (table == nullptr) {
        throw std::runtime_error{"Ruleset 缺少 [" + std::string{name} + "] 區段：" + path.string()};
    }
    return *table;
}

[[nodiscard]] DerivedStatFormula read_formula(const toml::table& derived, std::string_view name,
                                              const std::filesystem::path& path) {
    const auto& table = require_named_table(derived, name, path);
    DerivedStatFormula result{
        .base = detail::require_int32(table, "base", path),
        .body = detail::require_int32(table, "body", path),
        .skill = detail::require_int32(table, "skill", path),
        .mind = detail::require_int32(table, "mind", path),
        .spirit = detail::require_int32(table, "spirit", path),
        .tier = detail::require_int32(table, "tier", path),
        .divisor = detail::require_int32(table, "divisor", path),
    };
    if (result.divisor <= 0) {
        throw std::runtime_error{"attributes.toml 衍生值 divisor 必須大於零：" + std::string{name}};
    }
    return result;
}

void load_attributes(AttributeRules& rules, CheckRules& check_rules,
                     const std::filesystem::path& data_directory) {
    const auto path = data_directory / "attributes.toml";
    const auto document = parse_document(path);
    const auto& bounds = require_named_table(document, "bounds", path);
    rules.minimum = detail::require_int32(bounds, "minimum", path);
    rules.maximum = detail::require_int32(bounds, "maximum", path);
    if (rules.minimum >= rules.maximum) {
        throw std::runtime_error{"attributes.toml 屬性值域無效"};
    }

    const auto& tier = require_named_table(document, "tier", path);
    const auto* thresholds = tier["thresholds"].as_array();
    if (thresholds == nullptr || thresholds->size() != rules.tier_thresholds.size()) {
        throw std::runtime_error{"attributes.toml tier.thresholds 必須恰有四項"};
    }
    for (std::size_t index = 0; index < rules.tier_thresholds.size(); ++index) {
        const auto threshold = (*thresholds)[index].value<std::int64_t>();
        if (!threshold.has_value() || *threshold < rules.minimum || *threshold > rules.maximum) {
            throw std::runtime_error{"attributes.toml tier.thresholds 超出屬性值域"};
        }
        rules.tier_thresholds[index] = static_cast<std::int32_t>(*threshold);
    }
    if (!std::ranges::is_sorted(rules.tier_thresholds) ||
        std::ranges::adjacent_find(rules.tier_thresholds) != rules.tier_thresholds.end()) {
        throw std::runtime_error{"attributes.toml tier.thresholds 必須嚴格遞增"};
    }

    const auto& derived = require_named_table(document, "derived", path);
    rules.health = read_formula(derived, "health", path);
    rules.mana = read_formula(derived, "mana", path);
    rules.accuracy = read_formula(derived, "accuracy", path);
    rules.evasion = read_formula(derived, "evasion", path);
    rules.defense = read_formula(derived, "defense", path);
    rules.resistance = read_formula(derived, "resistance", path);
    rules.movement = read_formula(derived, "movement", path);
    rules.carry_capacity = read_formula(derived, "carry_capacity", path);
    rules.vision = read_formula(derived, "vision", path);

    const auto* margin_bands = document["margin_bands"].as_array();
    if (margin_bands == nullptr || margin_bands->empty()) {
        throw std::runtime_error{"attributes.toml 缺少 [[margin_bands]]"};
    }
    std::set<std::string, std::less<>> ids;
    for (const auto& node : *margin_bands) {
        const auto& table = detail::require_table(node, path);
        MarginBand band{
            .id = detail::require_string(table, "id", path),
            .minimum_margin = detail::require_integer(table, "minimum_margin", path),
            .effect_percent = detail::require_int32(table, "effect_percent", path),
        };
        if (!ids.insert(band.id).second || band.effect_percent < 0) {
            throw std::runtime_error{"attributes.toml margin_bands id 重複或效果為負：" + band.id};
        }
        check_rules.margin_bands.push_back(std::move(band));
    }
    const auto& bands = check_rules.margin_bands;
    if (!std::ranges::is_sorted(bands, {}, &MarginBand::minimum_margin) ||
        std::ranges::adjacent_find(bands, {}, &MarginBand::minimum_margin) != bands.end()) {
        throw std::runtime_error{"attributes.toml margin_bands 必須依 minimum_margin 嚴格遞增"};
    }
}

void load_damage(DamageRules& damage_rules, std::vector<DamageTypeDef>& damage_types,
                 std::map<std::string, DamageTypeId, std::less<>>& damage_type_index,
                 const std::filesystem::path& data_directory,
                 std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "damage.toml";
    const auto document = parse_document(path);
    const auto& rules = require_named_table(document, "rules", path);
    damage_rules.max_resistance_percent =
        detail::require_int32(rules, "max_resistance_percent", path);
    if (damage_rules.max_resistance_percent < 0 || damage_rules.max_resistance_percent >= 100) {
        throw std::runtime_error{"damage.toml 抗性上限必須介於 0 與 99"};
    }
    const auto* defs = document["defs"].as_array();
    if (defs == nullptr || defs->empty()) {
        throw std::runtime_error{"damage.toml 缺少 [[defs]]"};
    }
    for (const auto& node : *defs) {
        const auto& table = detail::require_table(node, path);
        DamageTypeDef def{
            .id = detail::require_string(table, "id", path),
            .name_key = detail::require_string(table, "name_key", path),
            .category = detail::require_string(table, "category", path),
        };
        detail::register_global_id(global_ids, def.id, "damage.");
        const auto id = detail::append_def<DamageTypeId>(damage_types, std::move(def));
        damage_type_index.emplace(damage_types.back().id, id);
    }
}

}  // namespace

void RulesetLoader::load_individual_rules(Ruleset& result,
                                          const std::filesystem::path& data_directory,
                                          std::set<std::string, std::less<>>& global_ids) {
    load_attributes(result.attribute_rules_, result.check_rules_, data_directory);
    load_damage(result.damage_rules_, result.damage_types_, result.damage_type_index_,
                data_directory, global_ids);
}

}  // namespace aetheria::rules
