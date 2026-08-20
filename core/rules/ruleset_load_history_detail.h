#pragma once

// 上古歷史規則載入各 TU 共用的內部函式；不是對外介面。

#include "core/rules/ruleset.h"

#include <toml++/toml.hpp>

#include <filesystem>

namespace aetheria::rules::detail {

void load_history_values(CivilizationRules::HistoryRules& history,
                         const toml::table& history_table,
                         const std::filesystem::path& civilization_path,
                         const CivilizationRules& civilization_rules);

void load_history_references(CivilizationRules::HistoryRules& history,
                             const toml::table& history_table,
                             const std::filesystem::path& civilization_path,
                             const Ruleset& ruleset);

}  // namespace aetheria::rules::detail
