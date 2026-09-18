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
#include "core/rules/ruleset_load_power_sources_detail.h"

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

    load_power_source_defs(result, {document, path, global_ids});
    validate_references(result);
    rules.loaded = true;
}

}  // namespace aetheria::rules
