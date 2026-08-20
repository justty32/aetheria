// core/rules/ruleset_load_history.cpp：上古歷史規則檔案的載入編排。

#include "core/rules/ruleset.h"
#include "core/rules/ruleset_load_history_detail.h"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>

namespace aetheria::rules {

void RulesetLoader::load_history_rules(Ruleset& result,
                                       const std::filesystem::path& data_directory) {
    const auto civilization_path = data_directory / "civilization.toml";
    if (!std::filesystem::is_regular_file(civilization_path)) {
        return;
    }

    toml::table civilization;
    try {
        civilization = toml::parse_file(civilization_path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + civilization_path.string() +
                                 "：" + std::string{error.description()}};
    }
    const auto* history_table = civilization["history"].as_table();
    if (history_table == nullptr) {
        throw std::runtime_error{"civilization.toml 缺少 [history]"};
    }

    auto& history = result.civilization_rules_.history;
    detail::load_history_values(history, *history_table, civilization_path,
                                result.civilization_rules_);
    detail::load_history_references(history, *history_table, civilization_path, result);
}

}  // namespace aetheria::rules
