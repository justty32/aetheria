// power_sources.toml：三條位階來源 def 與戰略魔法下限的 fail-fast 載入器。

#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/rules/toml_read.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] toml::table parse(const std::filesystem::path& path) {
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

[[nodiscard]] const toml::array& array(const toml::table& document, std::string_view key,
                                       const std::filesystem::path& path) {
    const auto* value = document[key].as_array();
    if (value == nullptr) {
        throw std::runtime_error{"power_sources.toml 缺少陣列 " + std::string{key} + "：" +
                                 path.string()};
    }
    return *value;
}

[[nodiscard]] std::vector<std::string> strings(const toml::table& table, std::string_view key,
                                               const std::filesystem::path& path) {
    const auto* values = table[key].as_array();
    if (values == nullptr) {
        throw std::runtime_error{"power_sources.toml 缺少字串陣列 " + std::string{key}};
    }
    std::vector<std::string> result;
    std::set<std::string, std::less<>> unique;
    for (const auto& node : *values) {
        const auto value = node.value<std::string>();
        if (!value.has_value() || value->empty() || !unique.insert(*value).second) {
            throw std::runtime_error{"power_sources.toml 字串陣列含空值或重複：" +
                                     path.string()};
        }
        result.push_back(*value);
    }
    return result;
}

[[nodiscard]] Attributes attributes(const toml::table& table,
                                    const std::filesystem::path& path) {
    const auto* values = table["source_attribute_modifier"].as_array();
    if (values == nullptr || values->size() != 4U) {
        throw std::runtime_error{"power_sources.toml source_attribute_modifier 必須恰有四項"};
    }
    std::array<std::int32_t, 4> parsed{};
    for (std::size_t index = 0; index < parsed.size(); ++index) {
        const auto value = (*values)[index].value<std::int64_t>();
        if (!value.has_value() || *value < std::numeric_limits<std::int32_t>::min() ||
            *value > std::numeric_limits<std::int32_t>::max()) {
            throw std::runtime_error{"power_sources.toml 屬性修正超出 int32：" + path.string()};
        }
        parsed[index] = static_cast<std::int32_t>(*value);
    }
    return {.body = parsed[0], .skill = parsed[1], .mind = parsed[2], .spirit = parsed[3]};
}

[[nodiscard]] PowerSourceDef source(const toml::table& table, PowerSourceKind kind,
                                    std::string id, const std::filesystem::path& path) {
    const auto bonus = detail::require_integer(table, "source_tier_bonus", path);
    if (bonus < 0 || bonus > static_cast<std::int64_t>(world::Significance::World)) {
        throw std::runtime_error{"power_sources.toml source_tier_bonus 超出位階範圍"};
    }
    return {
        .kind = kind,
        .origin_id = std::move(id),
        .tier_bonus = static_cast<std::int8_t>(bonus),
        .ability_ids = strings(table, "source_abilities", path),
        .attribute_modifier = attributes(table, path),
        .removal_condition = detail::require_string(table, "source_removal_condition", path),
    };
}

[[nodiscard]] world::Significance significance(std::string_view value,
                                                const std::filesystem::path& path) {
    constexpr std::array names{"ambient", "local", "site", "region", "world"};
    const auto found = std::ranges::find(names, value);
    if (found == names.end()) {
        throw std::runtime_error{"power_sources.toml 位階名稱無效：" + path.string()};
    }
    return static_cast<world::Significance>(std::distance(names.begin(), found));
}

template <typename Id, typename Def>
void append(std::vector<Def>& defs, std::map<std::string, Id, std::less<>>& index, Def def) {
    const auto id = detail::append_def<Id>(defs, std::move(def));
    index.emplace(defs.back().id, id);
}

void validate_references(const Ruleset& ruleset) {
    for (const auto& tenet : ruleset.tenets()) {
        for (const auto& conflict : tenet.conflicts) {
            if (!ruleset.find_tenet(conflict).has_value() || conflict == tenet.id) {
                throw std::runtime_error{"教義衝突引用不存在或指向自己：" + conflict};
            }
        }
    }
    for (const auto& deity : ruleset.deities()) {
        for (const auto& tenet : deity.tenets) {
            if (!ruleset.find_tenet(tenet).has_value()) {
                throw std::runtime_error{"神祇引用不存在的教義：" + tenet};
            }
        }
        for (const auto& hostile : deity.hostile_deities) {
            if (!ruleset.find_deity(hostile).has_value() || hostile == deity.id) {
                throw std::runtime_error{"敵對神祇引用不存在或指向自己：" + hostile};
            }
        }
    }
}

}  // namespace

void RulesetLoader::load_power_source_rules(
    Ruleset& result, const std::filesystem::path& data_directory,
    std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "power_sources.toml";
    const auto document = parse(path);
    const auto* strategic = document["strategic_magic"].as_table();
    if (strategic == nullptr) {
        throw std::runtime_error{"power_sources.toml 缺少 [strategic_magic]"};
    }
    auto& rules = result.power_source_rules_;
    rules.strategic_minimum_resource_cost =
        detail::require_int32(*strategic, "minimum_resource_cost", path);
    rules.strategic_minimum_ritual_xun =
        detail::require_int32(*strategic, "minimum_ritual_xun", path);
    rules.strategic_minimum_casters =
        detail::require_int32(*strategic, "minimum_casters", path);
    rules.strategic_minimum_cooldown_xun =
        detail::require_int32(*strategic, "minimum_cooldown_xun", path);
    rules.strategic_reference_resource_income_per_xun =
        detail::require_int32(*strategic, "reference_resource_income_per_xun", path);
    if (rules.strategic_minimum_resource_cost <= 0 ||
        rules.strategic_minimum_ritual_xun <= 1 || rules.strategic_minimum_casters <= 1 ||
        rules.strategic_minimum_cooldown_xun <= 0 ||
        rules.strategic_reference_resource_income_per_xun < 0 ||
        rules.strategic_reference_resource_income_per_xun >=
            rules.strategic_minimum_resource_cost) {
        throw std::runtime_error{"戰略魔法必須昂貴、跨旬且需要多名施法者"};
    }

    for (const auto& node : array(document, "schools", path)) {
        const auto& table = detail::require_table(node, path);
        SchoolDef def;
        def.id = detail::require_string(table, "id", path);
        def.name_key = detail::require_string(table, "name_key", path);
        detail::register_global_id(global_ids, def.id, "school.");
        def.source = source(table, PowerSourceKind::Magic, def.id, path);
        append(result.schools_, result.school_index_, std::move(def));
    }
    if (result.schools_.empty()) {
        throw std::runtime_error{"power_sources.toml 至少需要一個 SchoolDef"};
    }
    for (const auto& node : array(document, "tenets", path)) {
        const auto& table = detail::require_table(node, path);
        TenetDef def{detail::require_string(table, "id", path),
                     detail::require_string(table, "name_key", path),
                     strings(table, "conflicts", path)};
        detail::register_global_id(global_ids, def.id, "tenet.");
        append(result.tenets_, result.tenet_index_, std::move(def));
    }
    for (const auto& node : array(document, "deities", path)) {
        const auto& table = detail::require_table(node, path);
        DeityDef def;
        def.id = detail::require_string(table, "id", path);
        def.name_key = detail::require_string(table, "name_key", path);
        detail::register_global_id(global_ids, def.id, "deity.");
        def.significance = significance(detail::require_string(table, "significance", path), path);
        if (def.significance != world::Significance::World) {
            throw std::runtime_error{"神祇必須是 World 級實體：" + def.id};
        }
        def.domains = strings(table, "domains", path);
        def.tenets = strings(table, "tenets", path);
        def.hostile_deities = strings(table, "hostile_deities", path);
        def.source = source(table, PowerSourceKind::Faith, def.id, path);
        append(result.deities_, result.deity_index_, std::move(def));
    }
    for (const auto& node : array(document, "races", path)) {
        const auto& table = detail::require_table(node, path);
        RaceDef def;
        def.id = detail::require_string(table, "id", path);
        def.name_key = detail::require_string(table, "name_key", path);
        detail::register_global_id(global_ids, def.id, "race.");
        def.source = source(table, PowerSourceKind::Bloodline, def.id, path);
        if (def.source.tier_bonus != 0) {
            throw std::runtime_error{"血統影響位階上限而非起點：" + def.id};
        }
        def.tier_cap = significance(detail::require_string(table, "tier_cap", path), path);
        const auto lifespan = detail::require_integer(table, "lifespan_years", path);
        if (lifespan <= 0 || lifespan > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error{"種族壽命超出 uint32：" + def.id};
        }
        def.lifespan_years = static_cast<std::uint32_t>(lifespan);
        append(result.races_, result.race_index_, std::move(def));
    }
    validate_references(result);
    rules.loaded = true;
}

}  // namespace aetheria::rules
