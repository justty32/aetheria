// diplomacy.toml 的關係速率、條約、宣戰理由、厭戰與和談規則載入。
// 所有可調數值都在此驗證後進入不可變 Ruleset。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace aetheria::rules {
namespace {

[[nodiscard]] const toml::table&
require_section(const toml::table& document, std::string_view section,
                const std::filesystem::path& path) {
    const auto* result = document[section].as_table();
    if (result == nullptr) {
        throw std::runtime_error{"diplomacy.toml 缺少 [" +
                                 std::string{section} + "]：" + path.string()};
    }
    return *result;
}

[[nodiscard]] bool require_bool(const toml::table& table,
                                std::string_view field,
                                const std::filesystem::path& path) {
    const auto value = table[field].value<bool>();
    if (!value.has_value()) {
        throw std::runtime_error{"Ruleset 缺少布林欄位 " + std::string{field} +
                                 "：" + path.string()};
    }
    return *value;
}

[[nodiscard]] std::uint16_t positive_uint16(const toml::table& table,
                                            std::string_view field,
                                            const std::filesystem::path& path) {
    const auto value = detail::require_integer(table, field, path);
    if (value <= 0 || value > UINT16_MAX) {
        throw std::runtime_error{"diplomacy.toml 欄位須為正 uint16：" +
                                 std::string{field}};
    }
    return static_cast<std::uint16_t>(value);
}

[[nodiscard]] std::uint32_t duration_xun(const toml::table& table,
                                         const std::filesystem::path& path) {
    const auto value = detail::require_integer(table, "duration_xun", path);
    if (value < 0 || static_cast<std::uint64_t>(value) > UINT32_MAX) {
        throw std::runtime_error{"diplomacy.toml duration_xun 超出 uint32"};
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace

void RulesetLoader::load_diplomacy_rules(
    Ruleset& result, const std::filesystem::path& data_directory,
    std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "diplomacy.toml";
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }

    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() +
                                 "：" + std::string{error.description()}};
    }

    auto& rules = result.diplomacy_rules_;
    const auto& relations = require_section(document, "relations", path);
    rules.relation_min = detail::require_int32(relations, "value_min", path);
    rules.relation_max = detail::require_int32(relations, "value_max", path);
    rules.reversion.denominator =
        positive_uint16(relations, "reversion_denominator", path);
    rules.reversion.favor = positive_uint16(relations, "favor_reversion", path);
    rules.reversion.trust = positive_uint16(relations, "trust_reversion", path);
    rules.reversion.fear = positive_uint16(relations, "fear_reversion", path);
    rules.reversion.grievance =
        positive_uint16(relations, "grievance_reversion", path);
    rules.unjustified_war_trust_penalty =
        detail::require_int32(relations, "unjustified_war_trust_penalty", path);
    if (rules.relation_min >= 0 || rules.relation_max <= 0 ||
        rules.unjustified_war_trust_penalty >= 0 ||
        !(rules.reversion.favor > rules.reversion.fear &&
          rules.reversion.fear > rules.reversion.trust &&
          rules.reversion.trust > rules.reversion.grievance) ||
        rules.reversion.favor >= rules.reversion.denominator) {
        throw std::runtime_error{"diplomacy.toml 關係值域或四種回歸速率無效"};
    }

    const auto& war = require_section(document, "war", path);
    rules.war_weariness.base_per_xun =
        detail::require_int32(war, "base_weariness_per_xun", path);
    rules.war_weariness.per_thousand_casualties =
        detail::require_int32(war, "weariness_per_thousand_casualties", path);
    rules.war_weariness.peace_threshold =
        detail::require_int32(war, "peace_threshold", path);
    if (rules.war_weariness.base_per_xun <= 0 ||
        rules.war_weariness.per_thousand_casualties < 0 ||
        rules.war_weariness.peace_threshold <= 0) {
        throw std::runtime_error{"diplomacy.toml 厭戰參數無效"};
    }

    const auto& peace = require_section(document, "peace", path);
    rules.peace_weights = {
        .war_score = detail::require_int32(peace, "war_score_weight", path),
        .own_weariness =
            detail::require_int32(peace, "own_weariness_weight", path),
        .opponent_weariness =
            detail::require_int32(peace, "opponent_weariness_weight", path),
        .third_party_pressure =
            detail::require_int32(peace, "third_party_pressure_weight", path),
        .divisor = detail::require_int32(peace, "weight_divisor", path),
    };
    rules.peace_thresholds = {
        .reparations =
            detail::require_int32(peace, "reparations_threshold", path),
        .cede_territory =
            detail::require_int32(peace, "cede_territory_threshold", path),
        .vassalage = detail::require_int32(peace, "vassalage_threshold", path),
    };
    if (rules.peace_weights.war_score < 0 ||
        rules.peace_weights.own_weariness < 0 ||
        rules.peace_weights.opponent_weariness < 0 ||
        rules.peace_weights.third_party_pressure < 0 ||
        rules.peace_weights.divisor <= 0 ||
        rules.peace_thresholds.reparations <= 0 ||
        rules.peace_thresholds.reparations >=
            rules.peace_thresholds.cede_territory ||
        rules.peace_thresholds.cede_territory >=
            rules.peace_thresholds.vassalage) {
        throw std::runtime_error{"diplomacy.toml 和談權重或門檻無效"};
    }

    const auto* treaties = document["treaties"].as_array();
    const auto* casus_belli = document["casus_belli"].as_array();
    if (treaties == nullptr || casus_belli == nullptr || treaties->empty() ||
        casus_belli->empty()) {
        throw std::runtime_error{"diplomacy.toml 缺少條約或宣戰理由資料"};
    }
    for (const auto& node : *treaties) {
        const auto& table = detail::require_table(node, path);
        TreatyDef def{
            .id = detail::require_string(table, "id", path),
            .duration_xun = duration_xun(table, path),
            .condition = detail::require_string(table, "condition", path),
            .breach = {.favor =
                           detail::require_int32(table, "breach_favor", path),
                       .trust =
                           detail::require_int32(table, "breach_trust", path),
                       .grievance = detail::require_int32(
                           table, "breach_grievance", path)},
            .renewable = require_bool(table, "renewable", path),
        };
        detail::register_global_id(global_ids, def.id, "treaty.");
        const auto id =
            detail::append_def<TreatyDefId>(rules.treaties, std::move(def));
        result.treaty_index_.emplace(rules.treaties.back().id, id);
    }
    for (const auto& node : *casus_belli) {
        const auto& table = detail::require_table(node, path);
        CasusBelliDef def{
            .id = detail::require_string(table, "id", path),
            .duration_xun = duration_xun(table, path),
            .condition = detail::require_string(table, "condition", path),
        };
        detail::register_global_id(global_ids, def.id, "casus_belli.");
        const auto id = detail::append_def<CasusBelliDefId>(rules.casus_belli,
                                                            std::move(def));
        result.casus_belli_index_.emplace(rules.casus_belli.back().id, id);
    }
    rules.loaded = true;
}

} // namespace aetheria::rules
